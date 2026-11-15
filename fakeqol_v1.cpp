#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CCParticleSystemQuad.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace geode::prelude;

// ===========================================================================
// Pengaturan
// ===========================================================================
struct Cfg {
	bool noclip = false;
	bool jumphack = false;
	bool showLayout = false;
	bool showHitboxes = false;
	bool hitboxTrail = false;
	bool rainbowTrail = false;
	bool hideParticles = false;
	bool lastDeath = false;
	bool autoClicker = false;
	bool replayRecord = false;
	bool replayPlay = false;
	bool spSwitcher = false;
	bool levelHistory = false;
	bool nightMode = false;
	bool decoOnly = false;
	bool favorites = false;
	int cps = 10;
	bool loaded = false;
};

struct Feature {
	const char* name;
	const char* key;
	bool Cfg::* ptr;
};

static const std::vector<Feature>& features() {
	static const std::vector<Feature> list = {
		{"Noclip",               "noclip",        &Cfg::noclip},
		{"Jumphack",             "jumphack",      &Cfg::jumphack},
		{"Show Layout",          "showLayout",    &Cfg::showLayout},
		{"Show Hitboxes",        "showHitboxes",  &Cfg::showHitboxes},
		{"Hitbox Trail",         "hitboxTrail",   &Cfg::hitboxTrail},
		{"Rainbow Trail",        "rainbowTrail",  &Cfg::rainbowTrail},
		{"Hide Particles",       "hideParticles", &Cfg::hideParticles},
		{"Show Last Death %",    "lastDeath",     &Cfg::lastDeath},
		{"AutoClicker",          "autoClicker",   &Cfg::autoClicker},
		{"Replay: Record",       "replayRecord",  &Cfg::replayRecord},
		{"Replay: Play",         "replayPlay",    &Cfg::replayPlay},
		{"StartPos Switcher",    "spSwitcher",    &Cfg::spSwitcher},
		{"Level History",        "levelHistory",  &Cfg::levelHistory},
		{"Night Mode (Editor)",  "nightMode",     &Cfg::nightMode},
		{"Deco-Only (Editor)",   "decoOnly",      &Cfg::decoOnly},
		{"Favorites (Editor)",   "favorites",     &Cfg::favorites},
	};
	return list;
}

static Cfg& cfg() {
	static Cfg c;
	if (!c.loaded) {
		c.loaded = true;
		auto mod = Mod::get();
		for (auto const& f : features()) {
			c.*(f.ptr) = mod->getSavedValue<bool>(f.key);
		}
		int cps = mod->getSavedValue<int>("cps");
		c.cps = (cps >= 1 && cps <= 30) ? cps : 10;
	}
	return c;
}

// ===========================================================================
// Status runtime (replay bot, autoclicker)
// ===========================================================================
struct RInput {
	int step;
	bool hold;
	int button;
	bool p2;
};

static std::vector<RInput> g_rec;
static size_t g_idx = 0;
static int g_step = 0;
static bool g_injecting = false;

static std::string safeName(std::string s) {
	for (auto& ch : s) {
		if (!std::isalnum(static_cast<unsigned char>(ch))) ch = '_';
	}
	if (s.empty()) s = "level";
	return s;
}

static std::filesystem::path replayPath(std::string const& level) {
	return Mod::get()->getSaveDir() / ("replay_" + safeName(level) + ".txt");
}

static void saveReplay(std::string const& level) {
	std::ofstream out(replayPath(level));
	if (!out) return;
	for (auto const& in : g_rec) {
		out << in.step << ' ' << (in.hold ? 1 : 0) << ' ' << in.button << ' ' << (in.p2 ? 1 : 0) << '\n';
	}
}

static void loadReplay(std::string const& level) {
	g_rec.clear();
	std::ifstream in(replayPath(level));
	if (!in) return;
	int s, h, b, p;
	while (in >> s >> h >> b >> p) {
		g_rec.push_back(RInput{s, h != 0, b, p != 0});
	}
}

// ===========================================================================
// Fungsi bantu
// ===========================================================================
template <class T>
static std::string levelNameOf(T* layer) {
	if constexpr (requires { std::string(layer->m_level->m_levelName); }) {
		if (layer->m_level) return std::string(layer->m_level->m_levelName);
	}
	return "level";
}

template <class T>
static void forceGround(T* p) {
	if constexpr (requires { p->m_isOnGround = true; }) {
		p->m_isOnGround = true;
	}
}

template <class T>
static void tintTrail(T* p, ccColor3B c) {
	if constexpr (requires { p->m_regularTrail->setColor(c); }) {
		if (p->m_regularTrail) p->m_regularTrail->setColor(c);
	}
}

template <class T>
static void trySwitchStartPos(T* pl, StartPosObject* sp) {
	if constexpr (requires { pl->switchToStartPosObject(sp); }) {
		pl->switchToStartPosObject(sp);
	}
}

template <class T>
static void gatherStartPos(T* pl, std::vector<StartPosObject*>& out) {
	if constexpr (requires { CCArrayExt<GameObject*>(pl->m_startPosObjects); }) {
		for (auto o : CCArrayExt<GameObject*>(pl->m_startPosObjects)) {
			if (auto sp = typeinfo_cast<StartPosObject*>(o)) out.push_back(sp);
		}
	}
	if (out.empty()) {
		for (auto o : CCArrayExt<GameObject*>(pl->m_objects)) {
			if (auto sp = typeinfo_cast<StartPosObject*>(o)) out.push_back(sp);
		}
	}
}

static ccColor3B hueToColor(float h) {
	h = h - std::floor(h);
	float f = h * 6.f;
	int i = static_cast<int>(f);
	float x = f - static_cast<float>(i);
	float r = 0.f, g = 0.f, b = 0.f;
	switch (i % 6) {
		case 0: r = 1.f; g = x; break;
		case 1: r = 1.f - x; g = 1.f; break;
		case 2: g = 1.f; b = x; break;
		case 3: g = 1.f - x; b = 1.f; break;
		case 4: r = x; b = 1.f; break;
		default: r = 1.f; b = 1.f - x; break;
	}
	return ccc3(
		static_cast<uint8_t>(r * 255.f),
		static_cast<uint8_t>(g * 255.f),
		static_cast<uint8_t>(b * 255.f)
	);
}

static void drawBox(CCDrawNode* node, CCRect const& r, ccColor4F color) {
	CCPoint v[4] = {
		CCPoint(r.origin.x, r.origin.y),
		CCPoint(r.origin.x + r.size.width, r.origin.y),
		CCPoint(r.origin.x + r.size.width, r.origin.y + r.size.height),
		CCPoint(r.origin.x, r.origin.y + r.size.height)
	};
	node->drawPolygon(v, 4, ccc4f(color.r, color.g, color.b, color.a * 0.15f), 1.f, color);
}

static void addHistory(std::string const& name) {
	auto mod = Mod::get();
	auto list = mod->getSavedValue<std::vector<std::string>>("history");
	std::string prefix = name + "\t";
	list.erase(
		std::remove_if(list.begin(), list.end(), [&](std::string const& e) {
			return e.rfind(prefix, 0) == 0;
		}),
		list.end()
	);
	char buf[32] = {0};
	std::time_t t = std::time(nullptr);
	if (auto tmv = std::localtime(&t)) {
		std::strftime(buf, sizeof(buf), "%d/%m %H:%M", tmv);
	}
	list.insert(list.begin(), prefix + buf);
	if (list.size() > 30) list.resize(30);
	mod->setSavedValue("history", list);
}

static void addFQButton(CCNode* parent, CCObject* target, SEL_MenuHandler cb, CCPoint pos) {
	auto spr = ButtonSprite::create("FQOL");
	spr->setScale(.6f);
	auto btn = CCMenuItemSpriteExtra::create(spr, target, cb);
	auto menu = CCMenu::create();
	menu->setPosition(pos);
	menu->addChild(btn);
	parent->addChild(menu, 100);
}

// ===========================================================================
// Popup: Level History
// ===========================================================================
class HistoryPopup : public Popup {
protected:
	bool init() {
		if (!Popup::init(320.f, 260.f)) return false;
		this->setTitle("Level History");

		auto size = m_mainLayer->getContentSize();
		float viewW = size.width - 30.f;
		float viewH = size.height - 60.f;
		auto scroll = ScrollLayer::create({viewW, viewH});
		scroll->setPosition({15.f, 15.f});
		m_mainLayer->addChild(scroll);

		auto list = Mod::get()->getSavedValue<std::vector<std::string>>("history");
		const float rowH = 26.f;
		float contentH = std::max(viewH, static_cast<float>(list.size()) * rowH + 10.f);
		scroll->m_contentLayer->setContentSize({viewW, contentH});

		if (list.empty()) {
			auto empty = CCLabelBMFont::create("No history yet", "bigFont.fnt");
			empty->setScale(.5f);
			empty->setPosition({viewW / 2.f, contentH - 30.f});
			scroll->m_contentLayer->addChild(empty);
		}

		for (size_t i = 0; i < list.size(); ++i) {
			float y = contentH - 20.f - static_cast<float>(i) * rowH;
			std::string name = list[i];
			std::string when;
			auto tab = name.find('\t');
			if (tab != std::string::npos) {
				when = name.substr(tab + 1);
				name = name.substr(0, tab);
			}
			if (name.size() > 20) name = name.substr(0, 20) + "..";

			auto l1 = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
			l1->setScale(.4f);
			l1->setAnchorPoint({0.f, .5f});
			l1->setPosition({8.f, y});
			scroll->m_contentLayer->addChild(l1);

			auto l2 = CCLabelBMFont::create(when.c_str(), "bigFont.fnt");
			l2->setScale(.3f);
			l2->setAnchorPoint({1.f, .5f});
			l2->setPosition({viewW - 8.f, y});
			scroll->m_contentLayer->addChild(l2);
		}
		scroll->moveToTop();
		return true;
	}

public:
	static HistoryPopup* create() {
		auto ret = new HistoryPopup();
		if (ret->init()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

// ===========================================================================
// Popup: menu utama Fake QOLMods
// ===========================================================================
class FQPopup : public Popup {
protected:
	CCLabelBMFont* m_cpsLabel = nullptr;
	std::vector<CCMenuItemToggler*> m_togglers;

	bool init() {
		if (!Popup::init(360.f, 290.f)) return false;
		this->setTitle("Fake QOLMods");

		auto size = m_mainLayer->getContentSize();
		float viewW = size.width - 30.f;
		float viewH = size.height - 60.f;
		auto scroll = ScrollLayer::create({viewW, viewH});
		scroll->setPosition({15.f, 15.f});
		m_mainLayer->addChild(scroll);

		auto const& fs = features();
		const float rowH = 38.f;
		int rows = static_cast<int>(fs.size()) + 2;
		float contentH = std::max(viewH, static_cast<float>(rows) * rowH + 10.f);
		scroll->m_contentLayer->setContentSize({viewW, contentH});

		auto menu = CCMenu::create();
		menu->setContentSize({viewW, contentH});
		menu->setPosition({0.f, 0.f});
		scroll->m_contentLayer->addChild(menu);

		for (int i = 0; i < static_cast<int>(fs.size()); ++i) {
			float y = contentH - 24.f - static_cast<float>(i) * rowH;

			auto label = CCLabelBMFont::create(fs[i].name, "bigFont.fnt");
			label->setScale(.45f);
			label->setAnchorPoint({0.f, .5f});
			label->setPosition({12.f, y});
			scroll->m_contentLayer->addChild(label);

			auto off = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
			auto on = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
			auto toggler = CCMenuItemToggler::create(off, on, this, menu_selector(FQPopup::onToggle));
			toggler->setTag(i);
			toggler->setScale(.8f);
			toggler->setPosition({viewW - 34.f, y});
			toggler->toggle(cfg().*(fs[i].ptr));
			menu->addChild(toggler);
			m_togglers.push_back(toggler);
		}

		// Baris AutoClicker CPS
		float yCps = contentH - 24.f - static_cast<float>(fs.size()) * rowH;
		m_cpsLabel = CCLabelBMFont::create("", "bigFont.fnt");
		m_cpsLabel->setScale(.45f);
		m_cpsLabel->setAnchorPoint({0.f, .5f});
		m_cpsLabel->setPosition({12.f, yCps});
		scroll->m_contentLayer->addChild(m_cpsLabel);
		this->refreshCps();

		auto minusSpr = ButtonSprite::create("-", 30, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .6f);
		auto minusBtn = CCMenuItemSpriteExtra::create(minusSpr, this, menu_selector(FQPopup::onCps));
		minusBtn->setTag(0);
		minusBtn->setPosition({viewW - 80.f, yCps});
		menu->addChild(minusBtn);

		auto plusSpr = ButtonSprite::create("+", 30, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .6f);
		auto plusBtn = CCMenuItemSpriteExtra::create(plusSpr, this, menu_selector(FQPopup::onCps));
		plusBtn->setTag(1);
		plusBtn->setPosition({viewW - 34.f, yCps});
		menu->addChild(plusBtn);

		// Tombol Level History
		float yHist = yCps - rowH;
		auto histSpr = ButtonSprite::create("Level History", 160, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .6f);
		auto histBtn = CCMenuItemSpriteExtra::create(histSpr, this, menu_selector(FQPopup::onHistory));
		histBtn->setPosition({viewW / 2.f, yHist});
		menu->addChild(histBtn);

		scroll->moveToTop();
		return true;
	}

	void refreshCps() {
		char buf[48];
		std::snprintf(buf, sizeof(buf), "Click speed: %d CPS", cfg().cps);
		m_cpsLabel->setString(buf);
	}

	void onToggle(CCObject* sender) {
		int idx = static_cast<CCNode*>(sender)->getTag();
		auto const& fs = features();
		if (idx < 0 || idx >= static_cast<int>(fs.size())) return;

		bool nowOn = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
		auto& c = cfg();
		auto mod = Mod::get();
		c.*(fs[idx].ptr) = nowOn;
		mod->setSavedValue<bool>(fs[idx].key, nowOn);

		// Replay record dan play tidak boleh aktif bersamaan
		if (nowOn && fs[idx].ptr == &Cfg::replayRecord) {
			c.replayPlay = false;
			mod->setSavedValue<bool>("replayPlay", false);
		}
		if (nowOn && fs[idx].ptr == &Cfg::replayPlay) {
			c.replayRecord = false;
			mod->setSavedValue<bool>("replayRecord", false);
		}
		for (int i = 0; i < static_cast<int>(m_togglers.size()); ++i) {
			if (i == idx) continue;
			m_togglers[i]->toggle(c.*(fs[i].ptr));
		}
	}

	void onCps(CCObject* sender) {
		int dir = static_cast<CCNode*>(sender)->getTag() == 1 ? 1 : -1;
		int v = std::max(1, std::min(30, cfg().cps + dir));
		cfg().cps = v;
		Mod::get()->setSavedValue<int>("cps", v);
		this->refreshCps();
	}

	void onHistory(CCObject*) {
		HistoryPopup::create()->show();
	}

public:
	static FQPopup* create() {
		auto ret = new FQPopup();
		if (ret->init()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

// ===========================================================================
// GJBaseGameLayer: replay bot + autoclicker
// ===========================================================================
class $modify(FQGameLayer, GJBaseGameLayer) {
	void processCommands(float dt) {
		auto pl = PlayLayer::get();
		bool inLevel = pl && static_cast<GJBaseGameLayer*>(pl) == static_cast<GJBaseGameLayer*>(this);
		bool active = inLevel && m_player1 && !m_player1->m_isDead && !m_levelEndAnimationStarted;
		auto& c = cfg();

		if (active) {
			if (c.replayPlay) {
				g_injecting = true;
				while (g_idx < g_rec.size() && g_rec[g_idx].step <= g_step) {
					auto const& in = g_rec[g_idx];
					GJBaseGameLayer::handleButton(in.hold, in.button, in.p2);
					++g_idx;
				}
				g_injecting = false;
			}
			else if (c.autoClicker) {
				int period = std::max(2, 240 / std::max(1, c.cps));
				int hold = std::max(1, period / 4);
				int phase = g_step % period;
				if (phase == 0) {
					g_injecting = true;
					GJBaseGameLayer::handleButton(true, 1, false);
					g_injecting = false;
				}
				else if (phase == hold) {
					g_injecting = true;
					GJBaseGameLayer::handleButton(false, 1, false);
					g_injecting = false;
				}
			}
		}

		GJBaseGameLayer::processCommands(dt);

		if (inLevel) ++g_step;
	}

	void handleButton(bool hold, int button, bool player2) {
		auto pl = PlayLayer::get();
		bool inLevel = pl && static_cast<GJBaseGameLayer*>(pl) == static_cast<GJBaseGameLayer*>(this);
		auto& c = cfg();

		if (inLevel && !g_injecting) {
			if (c.replayPlay && !g_rec.empty()) return; // input manual diblokir saat replay
			if (c.replayRecord) g_rec.push_back(RInput{g_step, hold, button, player2});
		}
		GJBaseGameLayer::handleButton(hold, button, player2);
	}
};

// ===========================================================================
// PlayLayer: noclip, layout, hitbox, last death, startpos, riwayat
// ===========================================================================
class $modify(FQPlayLayer, PlayLayer) {
	struct Fields {
		CCDrawNode* hitNode = nullptr;
		std::deque<CCRect> trail;
		CCLabelBMFont* deathLabel = nullptr;
		CCLabelBMFont* spLabel = nullptr;
		std::vector<StartPosObject*> startPositions;
		int startIdx = 0;
	};

	void addObject(GameObject* obj) {
		if (cfg().showLayout && obj && obj->m_objectType == GameObjectType::Decoration) return;
		PlayLayer::addObject(obj);
	}

	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();
		auto& c = cfg();
		auto& f = *m_fields;
		auto win = CCDirector::get()->getWinSize();

		g_step = 0;
		g_idx = 0;
		if (c.replayPlay) loadReplay(levelNameOf(this));
		if (c.levelHistory) addHistory(levelNameOf(this));

		if (c.lastDeath) {
			f.deathLabel = CCLabelBMFont::create("Last death: -", "bigFont.fnt");
			f.deathLabel->setScale(.4f);
			f.deathLabel->setOpacity(160);
			f.deathLabel->setPosition({win.width / 2.f, win.height - 14.f});
			this->addChild(f.deathLabel, 200);
		}
		if (c.spSwitcher) this->buildSwitcher();
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		g_step = 0;
		g_idx = 0;
		if (cfg().replayRecord) g_rec.clear();
		m_fields->trail.clear();
	}

	void levelComplete() {
		if (cfg().replayRecord && !m_isPracticeMode && !g_rec.empty()) {
			saveReplay(levelNameOf(this));
		}
		PlayLayer::levelComplete();
	}

	void destroyPlayer(PlayerObject* player, GameObject* object) {
		if (cfg().noclip) return;
		float pct = this->getCurrentPercent();
		PlayLayer::destroyPlayer(player, object);
		auto& f = *m_fields;
		if (cfg().lastDeath && f.deathLabel && m_player1 && m_player1->m_isDead) {
			char buf[48];
			std::snprintf(buf, sizeof(buf), "Last death: %.0f%%", pct);
			f.deathLabel->setString(buf);
		}
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);
		this->drawDebug();
	}

	void drawDebug() {
		auto& c = cfg();
		auto& f = *m_fields;

		if (!c.showHitboxes && !c.hitboxTrail) {
			if (f.hitNode) f.hitNode->clear();
			f.trail.clear();
			return;
		}
		if (!m_player1) return;

		if (!f.hitNode) {
			f.hitNode = CCDrawNode::create();
			m_objectLayer->addChild(f.hitNode, 1000);
		}
		f.hitNode->clear();

		float px = m_player1->getPositionX();
		auto playerRect = static_cast<GameObject*>(m_player1)->getObjectRect();

		if (c.showHitboxes) {
			for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
				if (!obj) continue;
				if (std::fabs(obj->getPositionX() - px) > 420.f) continue;
				if (obj->m_objectType == GameObjectType::Hazard) {
					drawBox(f.hitNode, obj->getObjectRect(), ccc4f(1.f, 0.f, 0.f, 1.f));
				}
				else if (obj->m_objectType == GameObjectType::Solid) {
					drawBox(f.hitNode, obj->getObjectRect(), ccc4f(0.f, 0.4f, 1.f, 1.f));
				}
			}
			drawBox(f.hitNode, playerRect, ccc4f(1.f, 1.f, 0.f, 1.f));
		}

		if (c.hitboxTrail) {
			f.trail.push_back(playerRect);
			while (f.trail.size() > 90) f.trail.pop_front();
			float n = static_cast<float>(f.trail.size());
			for (size_t i = 0; i < f.trail.size(); ++i) {
				float a = 0.2f + 0.8f * (static_cast<float>(i) / n);
				drawBox(f.hitNode, f.trail[i], ccc4f(0.f, 1.f, 1.f, a));
			}
		}
		else {
			f.trail.clear();
		}
	}

	void buildSwitcher() {
		auto& f = *m_fields;
		f.startPositions.clear();
		gatherStartPos(this, f.startPositions);
		if (f.startPositions.empty()) return;

		std::sort(f.startPositions.begin(), f.startPositions.end(), [](StartPosObject* a, StartPosObject* b) {
			return a->getPositionX() < b->getPositionX();
		});

		auto win = CCDirector::get()->getWinSize();
		auto menu = CCMenu::create();
		menu->setPosition({0.f, 0.f});
		this->addChild(menu, 200);

		auto prevSpr = ButtonSprite::create("<");
		prevSpr->setScale(.5f);
		auto prevBtn = CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(FQPlayLayer::onSpPrev));
		prevBtn->setPosition({win.width / 2.f - 80.f, 24.f});
		menu->addChild(prevBtn);

		auto nextSpr = ButtonSprite::create(">");
		nextSpr->setScale(.5f);
		auto nextBtn = CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(FQPlayLayer::onSpNext));
		nextBtn->setPosition({win.width / 2.f + 80.f, 24.f});
		menu->addChild(nextBtn);

		f.spLabel = CCLabelBMFont::create("", "bigFont.fnt");
		f.spLabel->setScale(.35f);
		f.spLabel->setPosition({win.width / 2.f, 24.f});
		this->addChild(f.spLabel, 200);
		this->updateSpLabel();
	}

	void updateSpLabel() {
		auto& f = *m_fields;
		if (!f.spLabel) return;
		char buf[48];
		std::snprintf(buf, sizeof(buf), "StartPos %d/%d", f.startIdx + 1, static_cast<int>(f.startPositions.size()));
		f.spLabel->setString(buf);
	}

	void changeStart(int d) {
		auto& f = *m_fields;
		if (f.startPositions.empty()) return;
		int n = static_cast<int>(f.startPositions.size());
		f.startIdx = ((f.startIdx + d) % n + n) % n;
		trySwitchStartPos(this, f.startPositions[f.startIdx]);
		this->resetLevel();
		this->updateSpLabel();
	}

	void onSpPrev(CCObject*) { this->changeStart(-1); }
	void onSpNext(CCObject*) { this->changeStart(1); }
};

// ===========================================================================
// PlayerObject: jumphack + rainbow trail
// ===========================================================================
class $modify(FQPlayer, PlayerObject) {
	void update(float dt) {
		PlayerObject::update(dt);
		if (!PlayLayer::get()) return;
		auto& c = cfg();
		if (c.jumphack) forceGround(this);
		if (c.rainbowTrail) {
			static float hue = 0.f;
			hue += dt * 0.4f;
			tintTrail(this, hueToColor(hue));
		}
	}
};

// ===========================================================================
// Partikel
// ===========================================================================
class $modify(FQParticles, CCParticleSystemQuad) {
	void draw() {
		if (cfg().hideParticles && PlayLayer::get()) return;
		CCParticleSystemQuad::draw();
	}
};

// ===========================================================================
// Menu utama dan pause
// ===========================================================================
class $modify(FQMenu, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;
		auto win = CCDirector::get()->getWinSize();
		addFQButton(this, this, menu_selector(FQMenu::onFQ), ccp(win.width - 58.f, win.height - 22.f));
		return true;
	}

	void onFQ(CCObject*) {
		FQPopup::create()->show();
	}
};

class $modify(FQPause, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();
		auto win = CCDirector::get()->getWinSize();
		addFQButton(this, this, menu_selector(FQPause::onFQ), ccp(58.f, win.height - 22.f));
	}

	void onFQ(CCObject*) {
		FQPopup::create()->show();
	}
};

// ===========================================================================
// Editor: tombol, night mode, favorit objek, deco-only
// ===========================================================================
class $modify(FQEditor, EditorUI) {
	struct Fields {
		CCMenu* favMenu = nullptr;
		bool decoWasOn = false;
	};

	bool init(LevelEditorLayer* lel) {
		if (!EditorUI::init(lel)) return false;
		auto win = CCDirector::get()->getWinSize();
		addFQButton(this, this, menu_selector(FQEditor::onFQ), ccp(50.f, win.height - 25.f));

		auto& c = cfg();
		if (c.nightMode && lel) {
			auto overlay = CCLayerColor::create(ccc4(0, 0, 0, 120));
			lel->addChild(overlay, 9999);
		}
		if (c.favorites) this->buildFav();
		this->schedule(schedule_selector(FQEditor::tick), 0.1f);
		return true;
	}

	void onFQ(CCObject*) {
		FQPopup::create()->show();
	}

	void tick(float) {
		if (!m_editorLayer) return;
		auto& f = *m_fields;
		if (!cfg().decoOnly) {
			if (f.decoWasOn) {
				f.decoWasOn = false;
				for (auto obj : CCArrayExt<GameObject*>(m_editorLayer->m_objects)) {
					if (obj) obj->setVisible(true);
				}
			}
			return;
		}
		f.decoWasOn = true;
		for (auto obj : CCArrayExt<GameObject*>(m_editorLayer->m_objects)) {
			if (obj && obj->m_objectType != GameObjectType::Decoration) obj->setVisible(false);
		}
	}

	void buildFav() {
		auto win = CCDirector::get()->getWinSize();
		auto menu = CCMenu::create();
		menu->setPosition({0.f, 0.f});
		this->addChild(menu, 100);
		m_fields->favMenu = menu;

		float x = 130.f;
		const float y = win.height - 25.f;

		auto addSpr = ButtonSprite::create("+");
		addSpr->setScale(.5f);
		auto addBtn = CCMenuItemSpriteExtra::create(addSpr, this, menu_selector(FQEditor::onAddFav));
		addBtn->setPosition({x, y});
		menu->addChild(addBtn);
		x += 30.f;

		auto clrSpr = ButtonSprite::create("X");
		clrSpr->setScale(.5f);
		auto clrBtn = CCMenuItemSpriteExtra::create(clrSpr, this, menu_selector(FQEditor::onClearFav));
		clrBtn->setPosition({x, y});
		menu->addChild(clrBtn);
		x += 32.f;

		auto favs = Mod::get()->getSavedValue<std::vector<int>>("favorites");
		for (int id : favs) {
			auto obj = GameObject::createWithKey(id);
			if (!obj) continue;
			auto sz = obj->getContentSize();
			float m = std::max(sz.width, sz.height);
			if (m <= 0.f) continue;
			obj->setScale(26.f / m);
			auto btn = CCMenuItemSpriteExtra::create(obj, this, menu_selector(FQEditor::onFav));
			btn->setTag(id);
			btn->setPosition({x, y});
			menu->addChild(btn);
			x += 32.f;
		}
	}

	void refreshFav() {
		if (m_fields->favMenu) {
			m_fields->favMenu->removeFromParent();
			m_fields->favMenu = nullptr;
		}
		if (cfg().favorites) this->buildFav();
	}

	void onAddFav(CCObject*) {
		auto sel = this->getSelectedObjects();
		if (!sel || sel->count() == 0) {
			FLAlertLayer::create("Favorites", "Select an object first, then tap +.", "OK")->show();
			return;
		}
		auto favs = Mod::get()->getSavedValue<std::vector<int>>("favorites");
		for (auto obj : CCArrayExt<GameObject*>(sel)) {
			if (!obj) continue;
			int id = obj->m_objectID;
			if (std::find(favs.begin(), favs.end(), id) == favs.end() && favs.size() < 12) {
				favs.push_back(id);
			}
		}
		Mod::get()->setSavedValue("favorites", favs);
		this->refreshFav();
	}

	void onClearFav(CCObject*) {
		std::vector<int> empty;
		Mod::get()->setSavedValue("favorites", empty);
		this->refreshFav();
	}

	void onFav(CCObject* sender) {
		int id = static_cast<CCNode*>(sender)->getTag();
		auto lel = m_editorLayer;
		if (!lel || id <= 0) return;
		auto win = CCDirector::get()->getWinSize();
		auto p = lel->m_objectLayer->convertToNodeSpace(ccp(win.width / 2.f, win.height / 2.f));
		p.x = std::round(p.x / 30.f) * 30.f;
		p.y = std::round(p.y / 30.f) * 30.f;
		lel->createObject(id, p, false);
	}
};
