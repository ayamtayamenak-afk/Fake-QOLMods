#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/CCParticleSystemQuad.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

using namespace geode::prelude;

// ===========================================================================
// Pengaturan fitur
// ===========================================================================
struct Cfg {
	// Gameplay
	bool noclip = false;
	bool jumphack = false;
	bool autoClicker = false;
	bool autoPractice = false;
	bool freezeAttempt = false;
	bool forceHidePlayer = false;
	bool safeMode = false;
	bool autoSafeMode = false;
	bool unlockIcons = false;
	// Visual
	bool showLayout = false;
	bool showHitboxes = false;
	bool hitboxTrail = false;
	bool coinTracers = false;
	bool rainbowTrail = false;
	bool rainbowIcon = false;
	bool hideParticles = false;
	bool noDeathEffect = false;
	bool lastDeath = false;
	bool spSwitcher = false;
	// Konfirmasi
	bool confirmPractice = false;
	bool confirmRestart = false;
	bool confirmPlay = false;
	// Editor
	bool nightMode = false;
	bool decoOnly = false;
	bool favorites = false;
	// Lainnya
	bool levelHistory = false;
	// Label
	bool lblCps = false;
	bool lblCheat = false;
	bool lblNcAcc = false;
	bool lblNcDeaths = false;
	bool lblAttempt = false;
	bool lblSession = false;
	bool lblBest = false;
	bool lblClock = false;
	bool lblCustom = false;
	// Macro
	bool replayRecord = false;
	bool replayPlay = false;

	int cps = 10;
	bool loaded = false;
};

struct Feature {
	const char* name;
	const char* key;
	bool Cfg::* ptr;
	int group; // 0 gameplay, 1 visual, 2 confirm, 3 editor, 4 lainnya, 5 label, 6 macro
};

static const std::vector<Feature>& features() {
	static const std::vector<Feature> list = {
		{"Noclip",              "noclip",          &Cfg::noclip,          0},
		{"Jumphack",            "jumphack",        &Cfg::jumphack,        0},
		{"AutoClicker",         "autoClicker",     &Cfg::autoClicker,     0},
		{"Auto Practice",       "autoPractice",    &Cfg::autoPractice,    0},
		{"Freeze Attempt",      "freezeAttempt",   &Cfg::freezeAttempt,   0},
		{"Force Hide Player",   "forceHidePlayer", &Cfg::forceHidePlayer, 0},
		{"Safe Mode",           "safeMode",        &Cfg::safeMode,        0},
		{"Auto Safe Mode",      "autoSafeMode",    &Cfg::autoSafeMode,    0},
		{"Unlock All Icons",    "unlockIcons",     &Cfg::unlockIcons,     0},

		{"Show Layout",         "showLayout",      &Cfg::showLayout,      1},
		{"Show Hitboxes",       "showHitboxes",    &Cfg::showHitboxes,    1},
		{"Hitbox Trail",        "hitboxTrail",     &Cfg::hitboxTrail,     1},
		{"Coin Tracers",        "coinTracers",     &Cfg::coinTracers,     1},
		{"Rainbow Trail",       "rainbowTrail",    &Cfg::rainbowTrail,    1},
		{"Rainbow Icon",        "rainbowIcon",     &Cfg::rainbowIcon,     1},
		{"Hide Particles",      "hideParticles",   &Cfg::hideParticles,   1},
		{"No Death Effect",     "noDeathEffect",   &Cfg::noDeathEffect,   1},
		{"Show Last Death %",   "lastDeath",       &Cfg::lastDeath,       1},
		{"StartPos Switcher",   "spSwitcher",      &Cfg::spSwitcher,      1},

		{"Confirm Practice",    "confirmPractice", &Cfg::confirmPractice, 2},
		{"Confirm Restart",     "confirmRestart",  &Cfg::confirmRestart,  2},
		{"Confirm Play Level",  "confirmPlay",     &Cfg::confirmPlay,     2},

		{"Night Mode",          "nightMode",       &Cfg::nightMode,       3},
		{"Deco-Only View",      "decoOnly",        &Cfg::decoOnly,        3},
		{"Favorite Objects",    "favorites",       &Cfg::favorites,       3},

		{"Level History",       "levelHistory",    &Cfg::levelHistory,    4},

		{"CPS Counter",         "lblCps",          &Cfg::lblCps,          5},
		{"Cheat Indicator",     "lblCheat",        &Cfg::lblCheat,        5},
		{"Noclip Accuracy",     "lblNcAcc",        &Cfg::lblNcAcc,        5},
		{"Noclip Deaths",       "lblNcDeaths",     &Cfg::lblNcDeaths,     5},
		{"Attempt",             "lblAttempt",      &Cfg::lblAttempt,      5},
		{"Session Time",        "lblSession",      &Cfg::lblSession,      5},
		{"Best Run",            "lblBest",         &Cfg::lblBest,         5},
		{"Clock",               "lblClock",        &Cfg::lblClock,        5},
		{"Custom Label",        "lblCustom",       &Cfg::lblCustom,       5},

		{"Record",              "replayRecord",    &Cfg::replayRecord,    6},
		{"Play",                "replayPlay",      &Cfg::replayPlay,      6},
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

static bool cheatsActive() {
	auto& c = cfg();
	return c.noclip || c.jumphack || c.autoClicker || c.replayPlay;
}

static std::string& customText() {
	static std::string s;
	static bool loaded = false;
	if (!loaded) {
		loaded = true;
		s = Mod::get()->getSavedValue<std::string>("customText");
		if (s.empty()) s = "Custom label";
	}
	return s;
}

// ===========================================================================
// Tema dan bahasa
// ===========================================================================
struct FQTheme {
	int ar = 46, ag = 204, ab = 113;   // aksen (hijau kelapa)
	int br = 18, bg = 22, bb = 30;     // latar
	int tr = 255, tg = 255, tb = 255;  // teks
	int op = 215;                      // transparansi latar
	int lang = 1;                      // 0 EN, 1 ID, 2 MS, 3 FIL, 4 BN
};

struct FQThemeKey {
	const char* key;
	int FQTheme::* ptr;
};

static const std::vector<FQThemeKey>& themeKeys() {
	static const std::vector<FQThemeKey> k = {
		{"th_ar", &FQTheme::ar}, {"th_ag", &FQTheme::ag}, {"th_ab", &FQTheme::ab},
		{"th_br", &FQTheme::br}, {"th_bg", &FQTheme::bg}, {"th_bb", &FQTheme::bb},
		{"th_tr", &FQTheme::tr}, {"th_tg", &FQTheme::tg}, {"th_tb", &FQTheme::tb},
		{"th_op", &FQTheme::op}, {"th_lang", &FQTheme::lang},
	};
	return k;
}

static FQTheme& theme() {
	static FQTheme t;
	static bool loaded = false;
	if (!loaded) {
		loaded = true;
		auto mod = Mod::get();
		if (mod->getSavedValue<bool>("th_set")) {
			for (auto const& k : themeKeys()) t.*(k.ptr) = mod->getSavedValue<int>(k.key);
			t.lang = std::max(0, std::min(4, t.lang));
		}
	}
	return t;
}

static void saveTheme() {
	auto mod = Mod::get();
	auto& t = theme();
	for (auto const& k : themeKeys()) mod->setSavedValue<int>(k.key, t.*(k.ptr));
	mod->setSavedValue<bool>("th_set", true);
}

enum Str {
	S_TAB_HACKS, S_TAB_LABELS, S_TAB_MACRO, S_TAB_THEME, S_TAB_LANG,
	S_GAMEPLAY, S_VISUAL, S_CONFIRM, S_EDITOR, S_MISC,
	S_CLICK_SPEED, S_HISTORY, S_RESTART_NOW, S_CUSTOM_TEXT, S_EDIT_TEXT,
	S_RECORD, S_PLAY, S_LOADED, S_NONE, S_SAVE_MACRO, S_CLEAR_MACRO,
	S_SAVED_MACROS, S_LOAD, S_DELETE, S_NO_MACROS,
	S_ACCENT, S_BACKGROUND, S_TEXTCOL, S_OPACITY, S_RED, S_GREEN, S_BLUE,
	S_RESET_THEME, S_LANG_NOTE, S_MACRO_NAME, S_OK, S_CANCEL,
	S_COUNT
};

// Urutan bahasa: EN, ID, MS, FIL, BN
static const char* const STR[S_COUNT][5] = {
	{"Hacks", "Hack", "Hack", "Hacks", "Hack"},
	{"Labels", "Label", "Label", "Labels", "Label"},
	{"Macro", "Makro", "Makro", "Macro", "Makro"},
	{"FQTheme", "Tema", "Tema", "Tema", "Tema"},
	{"Language", "Bahasa", "Bahasa", "Wika", "Bahasa"},

	{"Gameplay", "Gameplay", "Permainan", "Gameplay", "Permainan"},
	{"Visual", "Visual", "Visual", "Visual", "Visual"},
	{"Confirm", "Konfirmasi", "Pengesahan", "Kumpirmasyon", "Pengesahan"},
	{"Editor", "Editor", "Editor", "Editor", "Editor"},
	{"Misc", "Lainnya", "Lain-lain", "Iba pa", "Lain-lain"},

	{"Click speed: %d CPS", "Kecepatan klik: %d CPS", "Kelajuan klik: %d CPS", "Bilis ng click: %d CPS", "Kelajuan klik: %d CPS"},
	{"Level History", "Riwayat Level", "Sejarah Level", "Kasaysayan ng Level", "Sejarah Level"},
	{"Restart now", "Ulangi sekarang", "Mula semula sekarang", "I-restart ngayon", "Mula semula sekarang"},
	{"Custom text", "Teks kustom", "Teks tersuai", "Custom na teksto", "Teks tersuai"},
	{"Edit text", "Ubah teks", "Ubah teks", "I-edit ang teksto", "Ubah teks"},

	{"Record", "Rekam", "Rakam", "Mag-record", "Rakam"},
	{"Play", "Putar", "Main", "I-play", "Main"},
	{"Loaded", "Dimuat", "Dimuatkan", "Naka-load", "Dimuatkan"},
	{"none", "kosong", "kosong", "wala", "kosong"},
	{"Save macro", "Simpan makro", "Simpan makro", "I-save ang macro", "Simpan makro"},
	{"Clear recording", "Hapus rekaman", "Padam rakaman", "Burahin ang recording", "Padam rakaman"},

	{"Saved macros", "Makro tersimpan", "Makro disimpan", "Mga na-save na macro", "Makro disimpan"},
	{"Load", "Muat", "Muat", "I-load", "Muat"},
	{"Delete", "Hapus", "Padam", "Burahin", "Padam"},
	{"No macros yet", "Belum ada makro", "Tiada makro lagi", "Wala pang macro", "Tiada makro lagi"},

	{"Accent color", "Warna aksen", "Warna aksen", "Kulay ng accent", "Warna aksen"},
	{"Background color", "Warna latar", "Warna latar", "Kulay ng background", "Warna latar"},
	{"Text color", "Warna teks", "Warna teks", "Kulay ng teksto", "Warna teks"},
	{"Background opacity", "Transparansi latar", "Ketelusan latar", "Opacity ng background", "Ketelusan latar"},
	{"Red", "Merah", "Merah", "Pula", "Merah"},
	{"Green", "Hijau", "Hijau", "Berde", "Hijau"},
	{"Blue", "Biru", "Biru", "Asul", "Biru"},

	{"Reset theme", "Reset tema", "Set semula tema", "I-reset ang tema", "Set semula tema"},
	{"Other languages need custom fonts (later)", "Bahasa lain butuh font khusus (menyusul)", "Bahasa lain perlukan fon khas (akan datang)", "Kailangan ng custom font ang ibang wika (susunod)", "Bahasa lain perlukan fon khas (akan datang)"},
	{"Macro name", "Nama makro", "Nama makro", "Pangalan ng macro", "Nama makro"},
	{"OK", "OK", "OK", "OK", "OK"},
	{"Cancel", "Batal", "Batal", "Kanselahin", "Batal"},
};

static const char* tr(int s) {
	return STR[s][theme().lang];
}

// ===========================================================================
// Status runtime: macro bot, autoclicker, statistik
// ===========================================================================
struct RInput {
	int step;
	bool hold;
	int button;
	bool p2;
};

static std::vector<RInput> g_rec;
static std::string g_macroName;
static size_t g_idx = 0;
static int g_step = 0;
static bool g_injecting = false;
static float g_clickAcc = 0.f;
static float g_holdLeft = 0.f;
static bool g_clickDown = false;

static int g_ncSteps = 0;
static int g_ncHitSteps = 0;
static int g_ncDeaths = 0;
static bool g_hitThisStep = false;
static bool g_prevHit = false;
static float g_bestPct = 0.f;

static std::deque<std::chrono::steady_clock::time_point> g_clicks;
static const auto g_sessionStart = std::chrono::steady_clock::now();

static void noteClick() {
	auto now = std::chrono::steady_clock::now();
	g_clicks.push_back(now);
	while (!g_clicks.empty() && now - g_clicks.front() > std::chrono::seconds(1)) g_clicks.pop_front();
}

static int currentCps() {
	auto now = std::chrono::steady_clock::now();
	while (!g_clicks.empty() && now - g_clicks.front() > std::chrono::seconds(1)) g_clicks.pop_front();
	return static_cast<int>(g_clicks.size());
}

static std::string safeName(std::string s) {
	for (auto& ch : s) {
		if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '_') ch = '_';
	}
	if (s.empty()) s = "macro";
	if (s.size() > 40) s.resize(40);
	return s;
}

static std::filesystem::path macroDir() {
	auto d = Mod::get()->getSaveDir() / "macros";
	std::error_code ec;
	std::filesystem::create_directories(d, ec);
	return d;
}

static std::vector<std::string> listMacros() {
	std::vector<std::string> out;
	std::error_code ec;
	for (auto const& e : std::filesystem::directory_iterator(macroDir(), ec)) {
		if (e.path().extension() == ".txt") out.push_back(e.path().stem().string());
	}
	std::sort(out.begin(), out.end());
	if (out.size() > 60) out.resize(60);
	return out;
}

static bool saveMacro(std::string const& rawName) {
	if (g_rec.empty()) return false;
	std::string name = safeName(rawName);
	std::ofstream out(macroDir() / (name + ".txt"));
	if (!out) return false;
	out << "FQMACRO 1\n";
	for (auto const& in : g_rec) {
		out << in.step << ' ' << (in.hold ? 1 : 0) << ' ' << in.button << ' ' << (in.p2 ? 1 : 0) << '\n';
	}
	return true;
}

static bool loadMacro(std::string const& name) {
	std::ifstream in(macroDir() / (name + ".txt"));
	if (!in) return false;
	std::string header;
	std::getline(in, header);
	std::vector<RInput> tmp;
	int s, h, b, p;
	while (in >> s >> h >> b >> p) {
		tmp.push_back(RInput{s, h != 0, b, p != 0});
	}
	g_rec = std::move(tmp);
	g_macroName = name;
	g_idx = 0;
	return true;
}

static void deleteMacro(std::string const& name) {
	std::error_code ec;
	std::filesystem::remove(macroDir() / (name + ".txt"), ec);
	if (g_macroName == name) g_macroName.clear();
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
static int attemptsOf(T* layer) {
	if constexpr (requires { layer->m_attempts; }) {
		return static_cast<int>(layer->m_attempts);
	}
	return 0;
}

template <class T>
static void setAttempts(T* layer, int v) {
	if constexpr (requires { layer->m_attempts = v; }) {
		layer->m_attempts = v;
	}
	if constexpr (requires { layer->updateAttempts(); }) {
		layer->updateAttempts();
	}
}

template <class T>
static bool testModeOf(T* layer) {
	if constexpr (requires { layer->m_isTestMode; }) {
		return static_cast<bool>(layer->m_isTestMode);
	}
	return false;
}

template <class T>
static void setTestMode(T* layer, bool v) {
	if constexpr (requires { layer->m_isTestMode = v; }) {
		layer->m_isTestMode = v;
	}
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
static void tintIcon(T* p, ccColor3B a, ccColor3B b) {
	if constexpr (requires { p->setColor(a); }) {
		p->setColor(a);
	}
	if constexpr (requires { p->setSecondColor(b); }) {
		p->setSecondColor(b);
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

static ccColor4F col4(int r, int g, int b, float a) {
	return ccc4f(static_cast<float>(r) / 255.f, static_cast<float>(g) / 255.f, static_cast<float>(b) / 255.f, a);
}

static void fillRect(CCDrawNode* node, float x, float y, float w, float h, ccColor4F c) {
	CCPoint v[4] = {
		CCPoint(x, y),
		CCPoint(x + w, y),
		CCPoint(x + w, y + h),
		CCPoint(x, y + h)
	};
	node->drawPolygon(v, 4, c, 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
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

static std::string fmtTime(long long secs) {
	char b[32];
	std::snprintf(b, sizeof(b), "%02lld:%02lld:%02lld", secs / 3600, (secs / 60) % 60, secs % 60);
	return b;
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

// ===========================================================================
// Popup: input teks
// ===========================================================================
class TextPopup : public Popup {
protected:
	TextInput* m_input = nullptr;
	std::function<void(std::string const&)> m_cb;

	bool init(std::string const& title, std::string const& initial, std::function<void(std::string const&)> cb) {
		if (!Popup::init(300.f, 150.f)) return false;
		m_cb = std::move(cb);
		this->setTitle(title.c_str());

		m_input = TextInput::create(240.f, "...", "bigFont.fnt");
		m_input->setString(initial);
		m_input->setPosition({150.f, 85.f});
		m_mainLayer->addChild(m_input);

		auto menu = CCMenu::create();
		menu->setPosition({0.f, 0.f});
		m_mainLayer->addChild(menu);

		auto okSpr = ButtonSprite::create(tr(S_OK));
		auto okBtn = CCMenuItemSpriteExtra::create(okSpr, this, menu_selector(TextPopup::onOk));
		okBtn->setPosition({150.f, 30.f});
		menu->addChild(okBtn);
		return true;
	}

	void onOk(CCObject*) {
		if (m_cb) m_cb(m_input->getString());
		this->onClose(nullptr);
	}

public:
	static TextPopup* create(std::string const& title, std::string const& initial, std::function<void(std::string const&)> cb) {
		auto ret = new TextPopup();
		if (ret->init(title, initial, std::move(cb))) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

// ===========================================================================
// Popup: Level History
// ===========================================================================
class HistoryPopup : public Popup {
protected:
	bool init() {
		if (!Popup::init(320.f, 260.f)) return false;
		this->setTitle(tr(S_HISTORY));

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
// Overlay menu: bisa di-drag, tampil di semua layar, warna bisa diganti
// ===========================================================================
enum class RowT { Header, Toggle, Stepper, Button, Slider, Info, Macro };

struct Row {
	RowT type = RowT::Info;
	std::string text;
	std::string value;
	std::function<bool()> getBool;
	std::function<void(bool)> setBool;
	std::function<void(int)> step;
	std::function<void()> click;
	std::function<void()> click2;
	std::function<void(float)> setSlider;
	float sliderValue = 0.f;
	bool hasSwatch = false;
	ccColor3B swatch = {0, 0, 0};
};

struct Hit {
	float x, y, w, h;
	int row;
	int kind; // 0 klik utama, 1 minus, 2 plus, 3 slider, 4 tombol kedua
	float tx, tw;
};

class FQOverlay : public CCLayer {
public:
	static FQOverlay* create() {
		auto ret = new FQOverlay();
		if (ret->init()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}

	bool init() override {
		if (!CCLayer::init()) return false;

		auto win = CCDirector::get()->getWinSize();
		auto mod = Mod::get();
		bool hasPos = mod->getSavedValue<bool>("ov_set");
		if (hasPos) {
			m_btn = ccp(static_cast<float>(mod->getSavedValue<int>("ov_bx")), static_cast<float>(mod->getSavedValue<int>("ov_by")));
			m_panel = ccp(static_cast<float>(mod->getSavedValue<int>("ov_px")), static_cast<float>(mod->getSavedValue<int>("ov_py")));
		}
		else {
			m_btn = ccp(30.f, win.height - 60.f);
			m_panel = ccp((win.width - m_size.width) / 2.f, (win.height - m_size.height) / 2.f);
		}
		this->clampPositions();

		m_draw = CCDrawNode::create();
		this->addChild(m_draw, 0);
		m_ui = CCNode::create();
		this->addChild(m_ui, 1);
		m_hud = CCNode::create();
		this->addChild(m_hud, 2);

		this->setTouchMode(kCCTouchesOneByOne);
		this->setTouchEnabled(true);
		this->scheduleUpdate();
		this->rebuild();
		return true;
	}

	void registerWithTouchDispatcher() override {
		CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -2000, true);
	}

	bool ccTouchBegan(CCTouch* t, CCEvent*) override {
		auto p = t->getLocation();
		m_start = p;
		m_last = p;
		m_moved = false;
		m_drag = Drag::None;
		m_sliderHit = -1;

		float dx = p.x - m_btn.x, dy = p.y - m_btn.y;
		if (dx * dx + dy * dy <= 26.f * 26.f) {
			m_drag = Drag::Button;
			return true;
		}
		if (!m_open) return false;
		if (!this->inPanel(p)) return false;

		float top = m_panel.y + m_size.height;
		if (p.y >= top - HEADER) {
			m_drag = Drag::Panel;
			return true;
		}
		if (p.y >= top - HEADER - TABH) {
			m_drag = Drag::Tabs;
			return true;
		}
		for (size_t i = 0; i < m_hits.size(); ++i) {
			auto const& h = m_hits[i];
			if (h.kind == 3 && p.x >= h.x && p.x <= h.x + h.w && p.y >= h.y && p.y <= h.y + h.h) {
				m_drag = Drag::Slider;
				m_sliderHit = static_cast<int>(i);
				this->applySlider(p);
				return true;
			}
		}
		m_drag = Drag::Body;
		return true;
	}

	void ccTouchMoved(CCTouch* t, CCEvent*) override {
		auto p = t->getLocation();
		float dx = p.x - m_last.x, dy = p.y - m_last.y;
		float sx = p.x - m_start.x, sy = p.y - m_start.y;
		if (sx * sx + sy * sy > 64.f) m_moved = true;

		switch (m_drag) {
			case Drag::Button:
				if (m_moved) {
					m_btn.x += dx;
					m_btn.y += dy;
					this->clampPositions();
					this->rebuild();
				}
				break;
			case Drag::Panel:
				if (m_moved) {
					m_panel.x += dx;
					m_panel.y += dy;
					this->clampPositions();
					this->rebuild();
				}
				break;
			case Drag::Body:
				if (m_moved) {
					m_scroll += dy;
					this->rebuild();
				}
				break;
			case Drag::Slider:
				this->applySlider(p);
				break;
			default:
				break;
		}
		m_last = p;
	}

	void ccTouchEnded(CCTouch* t, CCEvent*) override {
		auto p = t->getLocation();
		Drag d = m_drag;
		m_drag = Drag::None;

		if (d == Drag::Button) {
			if (!m_moved) m_open = !m_open;
			this->saveLayout();
			this->rebuild();
		}
		else if (d == Drag::Panel) {
			if (!m_moved && p.x > m_panel.x + m_size.width - 28.f) m_open = false;
			this->saveLayout();
			this->rebuild();
		}
		else if (d == Drag::Tabs) {
			if (!m_moved) {
				int idx = static_cast<int>((p.x - m_panel.x) / (m_size.width / 5.f));
				m_tab = std::max(0, std::min(4, idx));
				m_scroll = 0.f;
				this->rebuild();
			}
		}
		else if (d == Drag::Body) {
			if (!m_moved) this->tapBody(p);
		}
		else if (d == Drag::Slider) {
			saveTheme();
			this->rebuild();
		}
	}

	void ccTouchCancelled(CCTouch*, CCEvent*) override {
		m_drag = Drag::None;
	}

	void update(float) override {
		this->updateHud();
	}

	void showPanel(bool v) {
		m_open = v;
		this->rebuild();
	}

private:
	enum class Drag { None, Button, Panel, Tabs, Body, Slider };

	static constexpr float HEADER = 26.f;
	static constexpr float TABH = 22.f;

	bool m_open = false;
	CCPoint m_btn = ccp(30.f, 250.f);
	CCPoint m_panel = ccp(100.f, 40.f);
	CCSize m_size = CCSize(360.f, 250.f);
	int m_tab = 0;
	float m_scroll = 0.f;
	float m_contentH = 0.f;
	Drag m_drag = Drag::None;
	CCPoint m_start = ccp(0.f, 0.f);
	CCPoint m_last = ccp(0.f, 0.f);
	bool m_moved = false;
	int m_sliderHit = -1;

	CCDrawNode* m_draw = nullptr;
	CCNode* m_ui = nullptr;
	CCNode* m_hud = nullptr;
	std::vector<CCLabelBMFont*> m_hudLabels;
	std::vector<Row> m_rows;
	std::vector<Hit> m_hits;

	bool inPanel(CCPoint p) const {
		return p.x >= m_panel.x && p.x <= m_panel.x + m_size.width &&
			p.y >= m_panel.y && p.y <= m_panel.y + m_size.height;
	}

	void clampPositions() {
		auto win = CCDirector::get()->getWinSize();
		m_btn.x = std::max(24.f, std::min(win.width - 24.f, m_btn.x));
		m_btn.y = std::max(24.f, std::min(win.height - 24.f, m_btn.y));
		m_panel.x = std::max(0.f, std::min(win.width - m_size.width, m_panel.x));
		m_panel.y = std::max(0.f, std::min(win.height - m_size.height, m_panel.y));
	}

	void saveLayout() {
		auto mod = Mod::get();
		mod->setSavedValue<int>("ov_bx", static_cast<int>(m_btn.x));
		mod->setSavedValue<int>("ov_by", static_cast<int>(m_btn.y));
		mod->setSavedValue<int>("ov_px", static_cast<int>(m_panel.x));
		mod->setSavedValue<int>("ov_py", static_cast<int>(m_panel.y));
		mod->setSavedValue<bool>("ov_set", true);
	}

	CCLabelBMFont* addText(std::string s, float x, float y, float scale, ccColor3B col, float anchorX = 0.f, int opacity = 255) {
		if (s.size() > 34) s = s.substr(0, 32) + "..";
		auto l = CCLabelBMFont::create(s.c_str(), "bigFont.fnt");
		l->setScale(scale);
		l->setAnchorPoint({anchorX, .5f});
		l->setPosition({x, y});
		l->setColor(col);
		l->setOpacity(static_cast<GLubyte>(opacity));
		m_ui->addChild(l);
		return l;
	}

	void applySlider(CCPoint p) {
		if (m_sliderHit < 0 || m_sliderHit >= static_cast<int>(m_hits.size())) return;
		auto const& h = m_hits[m_sliderHit];
		if (h.row < 0 || h.row >= static_cast<int>(m_rows.size())) return;
		float v = (p.x - h.tx) / h.tw;
		v = std::max(0.f, std::min(1.f, v));
		if (m_rows[h.row].setSlider) m_rows[h.row].setSlider(v);
		this->rebuild();
	}

	void tapBody(CCPoint p) {
		for (auto const& h : m_hits) {
			if (h.kind == 3) continue;
			if (p.x < h.x || p.x > h.x + h.w || p.y < h.y || p.y > h.y + h.h) continue;
			if (h.row < 0 || h.row >= static_cast<int>(m_rows.size())) return;
			auto row = m_rows[h.row]; // salinan, karena rows dibangun ulang
			if (h.kind == 0) {
				if (row.type == RowT::Toggle && row.setBool && row.getBool) row.setBool(!row.getBool());
				else if (row.click) row.click();
			}
			else if (h.kind == 1) {
				if (row.step) row.step(-1);
			}
			else if (h.kind == 2) {
				if (row.step) row.step(1);
			}
			else if (h.kind == 4) {
				if (row.click2) row.click2();
			}
			this->rebuild();
			return;
		}
	}

	void openText(std::string const& title, std::string const& initial, std::function<void(std::string const&)> cb) {
		m_open = false;
		this->rebuild();
		TextPopup::create(title, initial, std::move(cb))->show();
	}

	// ---------------------------------------------------------------------
	// Isi tab
	// ---------------------------------------------------------------------
	void addToggle(Row& r, Feature const& f) {
		r.type = RowT::Toggle;
		r.text = f.name;
		auto ptr = f.ptr;
		std::string key = f.key;
		r.getBool = [ptr]() { return cfg().*ptr; };
		r.setBool = [ptr, key](bool v) {
			cfg().*ptr = v;
			Mod::get()->setSavedValue<bool>(key, v);
			// Record dan Play tidak boleh aktif bersamaan
			if (v && ptr == &Cfg::replayRecord) {
				cfg().replayPlay = false;
				Mod::get()->setSavedValue<bool>("replayPlay", false);
			}
			if (v && ptr == &Cfg::replayPlay) {
				cfg().replayRecord = false;
				Mod::get()->setSavedValue<bool>("replayRecord", false);
			}
		};
	}

	void addSliderRow(std::string const& label, int FQTheme::* field, float maxV) {
		Row r;
		r.type = RowT::Slider;
		r.text = label;
		r.sliderValue = static_cast<float>(theme().*field) / maxV;
		r.value = std::to_string(theme().*field);
		r.setSlider = [field, maxV](float v) {
			theme().*field = static_cast<int>(v * maxV + 0.5f);
		};
		m_rows.push_back(r);
	}

	void buildRows() {
		m_rows.clear();
		auto& th = theme();
		ccColor3B accent = ccc3(th.ar, th.ag, th.ab);

		if (m_tab == 0) {
			static const int groupStr[5] = {S_GAMEPLAY, S_VISUAL, S_CONFIRM, S_EDITOR, S_MISC};
			for (int g = 0; g < 5; ++g) {
				Row h;
				h.type = RowT::Header;
				h.text = tr(groupStr[g]);
				m_rows.push_back(h);

				for (auto const& f : features()) {
					if (f.group != g) continue;
					Row r;
					this->addToggle(r, f);
					m_rows.push_back(r);

					if (f.ptr == &Cfg::autoClicker) {
						Row s;
						s.type = RowT::Stepper;
						char buf[64];
						std::snprintf(buf, sizeof(buf), tr(S_CLICK_SPEED), cfg().cps);
						s.text = buf;
						s.step = [](int d) {
							int v = std::max(1, std::min(30, cfg().cps + d));
							cfg().cps = v;
							Mod::get()->setSavedValue<int>("cps", v);
						};
						m_rows.push_back(s);
					}
				}
				if (g == 0) {
					Row b;
					b.type = RowT::Button;
					b.text = tr(S_RESTART_NOW);
					b.click = []() {
						if (auto pl = PlayLayer::get()) pl->resetLevel();
					};
					m_rows.push_back(b);
				}
				if (g == 4) {
					Row b;
					b.type = RowT::Button;
					b.text = tr(S_HISTORY);
					b.click = [this]() {
						m_open = false;
						HistoryPopup::create()->show();
					};
					m_rows.push_back(b);
				}
			}
		}
		else if (m_tab == 1) {
			for (auto const& f : features()) {
				if (f.group != 5) continue;
				Row r;
				this->addToggle(r, f);
				m_rows.push_back(r);
			}
			Row i;
			i.type = RowT::Info;
			i.text = std::string(tr(S_CUSTOM_TEXT)) + ": " + customText();
			m_rows.push_back(i);

			Row b;
			b.type = RowT::Button;
			b.text = tr(S_EDIT_TEXT);
			b.click = [this]() {
				this->openText(tr(S_CUSTOM_TEXT), customText(), [this](std::string const& s) {
					customText() = s.empty() ? std::string("Custom label") : s;
					Mod::get()->setSavedValue<std::string>("customText", customText());
					this->showPanel(true);
				});
			};
			m_rows.push_back(b);
		}
		else if (m_tab == 2) {
			for (auto const& f : features()) {
				if (f.group != 6) continue;
				Row r;
				this->addToggle(r, f);
				r.text = f.ptr == &Cfg::replayRecord ? tr(S_RECORD) : tr(S_PLAY);
				m_rows.push_back(r);
			}
			Row i;
			i.type = RowT::Info;
			i.text = std::string(tr(S_LOADED)) + ": " + (g_macroName.empty() ? std::string(tr(S_NONE)) : g_macroName) +
				" (" + std::to_string(g_rec.size()) + ")";
			m_rows.push_back(i);

			Row save;
			save.type = RowT::Button;
			save.text = tr(S_SAVE_MACRO);
			save.click = [this]() {
				std::string base = g_macroName.empty() ? std::string("macro") : g_macroName;
				this->openText(tr(S_MACRO_NAME), base, [this](std::string const& s) {
					if (saveMacro(s)) g_macroName = safeName(s);
					this->showPanel(true);
				});
			};
			m_rows.push_back(save);

			Row clr;
			clr.type = RowT::Button;
			clr.text = tr(S_CLEAR_MACRO);
			clr.click = []() {
				g_rec.clear();
				g_macroName.clear();
				g_idx = 0;
			};
			m_rows.push_back(clr);

			Row h;
			h.type = RowT::Header;
			h.text = tr(S_SAVED_MACROS);
			m_rows.push_back(h);

			auto names = listMacros();
			if (names.empty()) {
				Row n;
				n.type = RowT::Info;
				n.text = tr(S_NO_MACROS);
				m_rows.push_back(n);
			}
			for (auto const& name : names) {
				Row m;
				m.type = RowT::Macro;
				m.text = name;
				m.click = [name]() { loadMacro(name); };
				m.click2 = [name]() { deleteMacro(name); };
				m_rows.push_back(m);
			}
		}
		else if (m_tab == 3) {
			struct Block { int title; int FQTheme::* r; int FQTheme::* g; int FQTheme::* b; };
			Block blocks[3] = {
				{S_ACCENT, &FQTheme::ar, &FQTheme::ag, &FQTheme::ab},
				{S_BACKGROUND, &FQTheme::br, &FQTheme::bg, &FQTheme::bb},
				{S_TEXTCOL, &FQTheme::tr, &FQTheme::tg, &FQTheme::tb},
			};
			for (auto const& bl : blocks) {
				Row h;
				h.type = RowT::Header;
				h.text = tr(bl.title);
				h.hasSwatch = true;
				h.swatch = ccc3(th.*(bl.r), th.*(bl.g), th.*(bl.b));
				m_rows.push_back(h);
				this->addSliderRow(tr(S_RED), bl.r, 255.f);
				this->addSliderRow(tr(S_GREEN), bl.g, 255.f);
				this->addSliderRow(tr(S_BLUE), bl.b, 255.f);
			}
			this->addSliderRow(tr(S_OPACITY), &FQTheme::op, 255.f);

			Row reset;
			reset.type = RowT::Button;
			reset.text = tr(S_RESET_THEME);
			reset.click = []() {
				int keepLang = theme().lang;
				theme() = FQTheme();
				theme().lang = keepLang;
				saveTheme();
			};
			m_rows.push_back(reset);
		}
		else {
			static const char* names[5] = {"English", "Indonesia", "Melayu", "Filipino", "Brunei"};
			for (int i = 0; i < 5; ++i) {
				Row b;
				b.type = RowT::Button;
				b.text = std::string(th.lang == i ? "> " : "") + names[i];
				b.click = [i]() {
					theme().lang = i;
					saveTheme();
				};
				m_rows.push_back(b);
			}
			Row n;
			n.type = RowT::Info;
			n.text = tr(S_LANG_NOTE);
			m_rows.push_back(n);
		}
		(void)accent;
	}

	static float rowHeight(Row const& r) {
		switch (r.type) {
			case RowT::Header: return 22.f;
			case RowT::Slider: return 28.f;
			case RowT::Button: return 26.f;
			case RowT::Info: return 20.f;
			default: return 23.f;
		}
	}

	// ---------------------------------------------------------------------
	// Menggambar ulang
	// ---------------------------------------------------------------------
	void rebuild() {
		m_draw->clear();
		m_ui->removeAllChildrenWithCleanup(true);
		m_hits.clear();

		auto& th = theme();
		ccColor3B tc = ccc3(th.tr, th.tg, th.tb);

		// Tombol melayang
		m_draw->drawDot(m_btn, 22.f, col4(255, 255, 255, 0.9f));
		m_draw->drawDot(m_btn, 19.f, col4(th.ar, th.ag, th.ab, 1.f));
		this->addText("FQ", m_btn.x, m_btn.y, .5f, ccc3(255, 255, 255), .5f);

		if (!m_open) return;

		float x = m_panel.x, y = m_panel.y, w = m_size.width, h = m_size.height;
		fillRect(m_draw, x, y, w, h, col4(th.br, th.bg, th.bb, static_cast<float>(th.op) / 255.f));
		fillRect(m_draw, x, y + h - HEADER, w, HEADER, col4(th.ar, th.ag, th.ab, 1.f));
		this->addText("Fake QOLMods", x + 10.f, y + h - HEADER / 2.f, .45f, tc);
		this->addText("X", x + w - 14.f, y + h - HEADER / 2.f, .5f, tc, .5f);

		// Tab
		const int nt = 5;
		float tw = w / static_cast<float>(nt);
		float ty = y + h - HEADER - TABH;
		for (int i = 0; i < nt; ++i) {
			if (i == m_tab) fillRect(m_draw, x + i * tw, ty, tw, TABH, col4(th.ar, th.ag, th.ab, 0.45f));
			this->addText(tr(S_TAB_HACKS + i), x + i * tw + tw / 2.f, ty + TABH / 2.f, .32f, tc, .5f);
		}

		// Isi
		this->buildRows();
		float bodyTop = ty - 4.f;
		float bodyBottom = y + 6.f;
		float bodyH = bodyTop - bodyBottom;

		m_contentH = 0.f;
		for (auto const& r : m_rows) m_contentH += rowHeight(r);
		float maxScroll = std::max(0.f, m_contentH - bodyH);
		m_scroll = std::max(0.f, std::min(maxScroll, m_scroll));

		float cursor = bodyTop + m_scroll;
		for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
			auto const& r = m_rows[i];
			float rh = rowHeight(r);
			float rowTop = cursor;
			float rowBottom = cursor - rh;
			cursor -= rh;
			if (rowBottom < bodyBottom || rowTop > bodyTop) continue;
			float cy = rowBottom + rh / 2.f;
			this->drawRow(r, i, x, w, rowBottom, rh, cy, th, tc);
		}

		// Scrollbar tipis
		if (maxScroll > 0.f) {
			float ratio = bodyH / m_contentH;
			float barH = std::max(16.f, bodyH * ratio);
			float barY = bodyTop - barH - (bodyH - barH) * (m_scroll / maxScroll);
			fillRect(m_draw, x + w - 4.f, barY, 2.f, barH, col4(th.ar, th.ag, th.ab, 0.8f));
		}
	}

	void drawRow(Row const& r, int index, float x, float w, float bottom, float rh, float cy, FQTheme& th, ccColor3B tc) {
		switch (r.type) {
			case RowT::Header: {
				fillRect(m_draw, x + 6.f, bottom + 1.f, w - 16.f, 1.f, col4(th.ar, th.ag, th.ab, 0.7f));
				this->addText(r.text, x + 10.f, cy + 2.f, .4f, ccc3(th.ar, th.ag, th.ab));
				if (r.hasSwatch) fillRect(m_draw, x + w - 40.f, bottom + 4.f, 26.f, rh - 8.f, col4(r.swatch.r, r.swatch.g, r.swatch.b, 1.f));
				break;
			}
			case RowT::Toggle: {
				this->addText(r.text, x + 12.f, cy, .38f, tc);
				bool on = r.getBool ? r.getBool() : false;
				float sx = x + w - 48.f;
				fillRect(m_draw, sx, cy - 7.f, 30.f, 14.f, on ? col4(th.ar, th.ag, th.ab, 1.f) : col4(70, 70, 80, 1.f));
				float kx = on ? sx + 22.f : sx + 8.f;
				m_draw->drawDot(ccp(kx, cy), 6.f, col4(255, 255, 255, 1.f));
				m_hits.push_back(Hit{x, bottom, w, rh, index, 0, 0.f, 0.f});
				break;
			}
			case RowT::Stepper: {
				this->addText(r.text, x + 12.f, cy, .36f, tc);
				float mx = x + w - 62.f, px = x + w - 36.f;
				fillRect(m_draw, mx, cy - 8.f, 20.f, 16.f, col4(th.ar, th.ag, th.ab, 0.8f));
				fillRect(m_draw, px, cy - 8.f, 20.f, 16.f, col4(th.ar, th.ag, th.ab, 0.8f));
				this->addText("-", mx + 10.f, cy, .45f, ccc3(255, 255, 255), .5f);
				this->addText("+", px + 10.f, cy, .45f, ccc3(255, 255, 255), .5f);
				m_hits.push_back(Hit{mx - 4.f, bottom, 28.f, rh, index, 1, 0.f, 0.f});
				m_hits.push_back(Hit{px - 4.f, bottom, 28.f, rh, index, 2, 0.f, 0.f});
				break;
			}
			case RowT::Button: {
				fillRect(m_draw, x + 12.f, bottom + 3.f, w - 34.f, rh - 6.f, col4(th.ar, th.ag, th.ab, 0.35f));
				this->addText(r.text, x + w / 2.f - 8.f, cy, .38f, tc, .5f);
				m_hits.push_back(Hit{x + 12.f, bottom, w - 34.f, rh, index, 0, 0.f, 0.f});
				break;
			}
			case RowT::Info: {
				this->addText(r.text, x + 12.f, cy, .32f, tc, 0.f, 170);
				break;
			}
			case RowT::Slider: {
				this->addText(r.text, x + 12.f, bottom + rh - 8.f, .32f, tc);
				this->addText(r.value, x + w - 16.f, bottom + rh - 8.f, .32f, tc, 1.f);
				float tx = x + 12.f, tw = w - 40.f, ty = bottom + 8.f;
				fillRect(m_draw, tx, ty - 2.f, tw, 4.f, col4(90, 90, 100, 1.f));
				fillRect(m_draw, tx, ty - 2.f, tw * r.sliderValue, 4.f, col4(th.ar, th.ag, th.ab, 1.f));
				m_draw->drawDot(ccp(tx + tw * r.sliderValue, ty), 6.f, col4(255, 255, 255, 1.f));
				m_hits.push_back(Hit{tx - 8.f, bottom, tw + 16.f, rh, index, 3, tx, tw});
				break;
			}
			case RowT::Macro: {
				this->addText(r.text, x + 12.f, cy, .36f, tc);
				float lx = x + w - 94.f, dx = x + w - 52.f;
				fillRect(m_draw, lx, cy - 8.f, 38.f, 16.f, col4(th.ar, th.ag, th.ab, 0.6f));
				fillRect(m_draw, dx, cy - 8.f, 38.f, 16.f, col4(200, 60, 60, 0.7f));
				this->addText(tr(S_LOAD), lx + 19.f, cy, .28f, ccc3(255, 255, 255), .5f);
				this->addText(tr(S_DELETE), dx + 19.f, cy, .28f, ccc3(255, 255, 255), .5f);
				m_hits.push_back(Hit{lx, bottom, 38.f, rh, index, 0, 0.f, 0.f});
				m_hits.push_back(Hit{dx, bottom, 38.f, rh, index, 4, 0.f, 0.f});
				break;
			}
		}
	}

	// ---------------------------------------------------------------------
	// HUD label (hanya muncul saat main level)
	// ---------------------------------------------------------------------
	void updateHud() {
		auto pl = PlayLayer::get();
		m_hud->setVisible(pl != nullptr);
		if (!pl) return;

		auto& c = cfg();
		auto& th = theme();
		ccColor3B tc = ccc3(th.tr, th.tg, th.tb);

		struct Line { std::string text; ccColor3B color; };
		std::vector<Line> lines;
		char buf[96];

		if (c.lblCps) {
			std::snprintf(buf, sizeof(buf), "CPS: %d", currentCps());
			lines.push_back({buf, tc});
		}
		if (c.lblCheat) {
			bool on = cheatsActive();
			lines.push_back({on ? "Cheats: ON" : "Cheats: OFF", on ? ccc3(255, 90, 90) : ccc3(90, 255, 120)});
		}
		if (c.lblNcAcc) {
			float acc = g_ncSteps > 0 ? 100.f * (1.f - static_cast<float>(g_ncHitSteps) / static_cast<float>(g_ncSteps)) : 100.f;
			std::snprintf(buf, sizeof(buf), "Noclip acc: %.1f%%", acc);
			lines.push_back({buf, tc});
		}
		if (c.lblNcDeaths) {
			std::snprintf(buf, sizeof(buf), "Noclip deaths: %d", g_ncDeaths);
			lines.push_back({buf, tc});
		}
		if (c.lblAttempt) {
			std::snprintf(buf, sizeof(buf), "Attempt: %d", attemptsOf(pl));
			lines.push_back({buf, tc});
		}
		if (c.lblSession) {
			auto secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - g_sessionStart).count();
			lines.push_back({"Session: " + fmtTime(static_cast<long long>(secs)), tc});
		}
		if (c.lblBest) {
			std::snprintf(buf, sizeof(buf), "Best: %.0f%%", g_bestPct);
			lines.push_back({buf, tc});
		}
		if (c.lblClock) {
			char tb[32] = {0};
			std::time_t t = std::time(nullptr);
			if (auto tmv = std::localtime(&t)) std::strftime(tb, sizeof(tb), "%H:%M:%S", tmv);
			lines.push_back({std::string("Clock: ") + tb, tc});
		}
		if (c.lblCustom) {
			lines.push_back({customText(), tc});
		}

		while (m_hudLabels.size() < lines.size()) {
			auto l = CCLabelBMFont::create("", "bigFont.fnt");
			l->setScale(.32f);
			l->setAnchorPoint({0.f, 1.f});
			m_hud->addChild(l);
			m_hudLabels.push_back(l);
		}

		auto win = CCDirector::get()->getWinSize();
		float y = win.height - 8.f;
		for (size_t i = 0; i < m_hudLabels.size(); ++i) {
			auto l = m_hudLabels[i];
			if (i < lines.size()) {
				l->setVisible(true);
				l->setString(lines[i].text.c_str());
				l->setColor(lines[i].color);
				l->setOpacity(210);
				l->setPosition({62.f, y});
				y -= 12.f;
			}
			else {
				l->setVisible(false);
			}
		}
	}
};

static FQOverlay* s_overlay = nullptr;

static void ensureOverlay() {
	if (s_overlay) return;
	s_overlay = FQOverlay::create();
	if (s_overlay) SceneManager::get()->keepAcrossScenes(s_overlay);
}

// ===========================================================================
// GJBaseGameLayer: macro bot + autoclicker
// ===========================================================================
class $modify(FQGameLayer, GJBaseGameLayer) {
	void processCommands(float dt, bool isHalfTick, bool isLastTick) {
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
					if (in.hold) noteClick();
					++g_idx;
				}
				g_injecting = false;
			}
			else if (c.autoClicker) {
				float period = 1.f / static_cast<float>(std::max(1, c.cps));
				if (g_clickDown) {
					g_holdLeft -= dt;
					if (g_holdLeft <= 0.f) {
						g_injecting = true;
						GJBaseGameLayer::handleButton(false, 1, false);
						g_injecting = false;
						g_clickDown = false;
					}
				}
				else {
					g_clickAcc += dt;
					if (g_clickAcc >= period) {
						g_clickAcc -= period;
						g_injecting = true;
						GJBaseGameLayer::handleButton(true, 1, false);
						g_injecting = false;
						noteClick();
						g_clickDown = true;
						g_holdLeft = std::min(0.03f, period * 0.5f);
					}
				}
			}
		}

		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

		if (inLevel) {
			++g_step;
			if (c.noclip) {
				++g_ncSteps;
				if (g_hitThisStep) {
					++g_ncHitSteps;
					if (!g_prevHit) ++g_ncDeaths;
				}
				g_prevHit = g_hitThisStep;
			}
			g_hitThisStep = false;
		}
	}

	void handleButton(bool hold, int button, bool player2) {
		auto pl = PlayLayer::get();
		bool inLevel = pl && static_cast<GJBaseGameLayer*>(pl) == static_cast<GJBaseGameLayer*>(this);
		auto& c = cfg();

		if (inLevel && !g_injecting) {
			if (c.replayPlay && !g_rec.empty()) return; // input manual diblokir saat macro dimainkan
			if (hold) noteClick();
			if (c.replayRecord) g_rec.push_back(RInput{g_step, hold, button, player2});
		}
		GJBaseGameLayer::handleButton(hold, button, player2);
	}
};

// ===========================================================================
// PlayLayer
// ===========================================================================
class $modify(FQPlayLayer, PlayLayer) {
	struct Fields {
		CCDrawNode* hitNode = nullptr;
		std::deque<CCRect> trail;
		CCLabelBMFont* deathLabel = nullptr;
		CCLabelBMFont* spLabel = nullptr;
		std::vector<StartPosObject*> startPositions;
		int startIdx = 0;
		bool origTest = false;
	};

	void addObject(GameObject* obj) {
		if (cfg().showLayout && obj && obj->m_objectType == GameObjectType::Decoration) return;
		PlayLayer::addObject(obj);
	}

	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();
		auto& c = cfg();
		auto win = CCDirector::get()->getWinSize();

		g_step = 0;
		g_idx = 0;
		g_clickAcc = 0.f;
		g_clickDown = false;
		g_ncSteps = 0;
		g_ncHitSteps = 0;
		g_ncDeaths = 0;
		g_hitThisStep = false;
		g_prevHit = false;
		g_bestPct = 0.f;

		m_fields->origTest = testModeOf(this);
		if (c.levelHistory) addHistory(levelNameOf(this));
		if (c.autoPractice && !m_isPracticeMode) this->togglePracticeMode(true);

		if (c.lastDeath) {
			m_fields->deathLabel = CCLabelBMFont::create("Last death: -", "bigFont.fnt");
			m_fields->deathLabel->setScale(.4f);
			m_fields->deathLabel->setOpacity(160);
			m_fields->deathLabel->setPosition({win.width / 2.f, win.height - 14.f});
			this->addChild(m_fields->deathLabel, 200);
		}
		if (c.spSwitcher) this->buildSwitcher();
	}

	void resetLevel() {
		int keep = attemptsOf(this);
		PlayLayer::resetLevel();
		if (cfg().freezeAttempt) setAttempts(this, keep);
		g_step = 0;
		g_idx = 0;
		g_clickAcc = 0.f;
		g_clickDown = false;
		if (cfg().replayRecord) g_rec.clear();
		m_fields->trail.clear();
	}

	void levelComplete() {
		if (cfg().replayRecord && !m_isPracticeMode && !g_rec.empty()) {
			char tb[32] = {0};
			std::time_t t = std::time(nullptr);
			if (auto tmv = std::localtime(&t)) std::strftime(tb, sizeof(tb), "%Y%m%d_%H%M%S", tmv);
			std::string name = safeName(levelNameOf(this)) + "_" + tb;
			if (saveMacro(name)) g_macroName = name;
		}
		auto& c = cfg();
		if (c.safeMode || (c.autoSafeMode && cheatsActive())) setTestMode(this, true);
		PlayLayer::levelComplete();
	}

	void destroyPlayer(PlayerObject* player, GameObject* object) {
		if (cfg().noclip) {
			g_hitThisStep = true;
			return;
		}
		float pct = this->getCurrentPercent();
		PlayLayer::destroyPlayer(player, object);
		if (cfg().lastDeath && m_fields->deathLabel && m_player1 && m_player1->m_isDead) {
			char buf[48];
			std::snprintf(buf, sizeof(buf), "Last death: %.0f%%", pct);
			m_fields->deathLabel->setString(buf);
		}
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);
		auto& c = cfg();

		if (m_player1 && !m_player1->m_isDead) {
			float p = this->getCurrentPercent();
			if (p > g_bestPct) g_bestPct = p;
		}

		if (c.safeMode || c.autoSafeMode) {
			bool want = m_fields->origTest || c.safeMode || (c.autoSafeMode && cheatsActive());
			setTestMode(this, want);
		}

		if (c.forceHidePlayer) {
			if (m_player1) m_player1->setVisible(false);
			if (m_player2) m_player2->setVisible(false);
		}

		this->drawDebug();
	}

	void drawDebug() {
		auto& c = cfg();

		if (!c.showHitboxes && !c.hitboxTrail && !c.coinTracers) {
			if (m_fields->hitNode) m_fields->hitNode->clear();
			m_fields->trail.clear();
			return;
		}
		if (!m_player1) return;

		if (!m_fields->hitNode) {
			m_fields->hitNode = CCDrawNode::create();
			m_objectLayer->addChild(m_fields->hitNode, 1000);
		}
		m_fields->hitNode->clear();

		float px = m_player1->getPositionX();
		auto playerRect = static_cast<GameObject*>(m_player1)->getObjectRect();

		if (c.showHitboxes || c.coinTracers) {
			for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
				if (!obj) continue;
				if (std::fabs(obj->getPositionX() - px) > 420.f) continue;
				if (c.showHitboxes) {
					if (obj->m_objectType == GameObjectType::Hazard) {
						drawBox(m_fields->hitNode, obj->getObjectRect(), ccc4f(1.f, 0.f, 0.f, 1.f));
					}
					else if (obj->m_objectType == GameObjectType::Solid) {
						drawBox(m_fields->hitNode, obj->getObjectRect(), ccc4f(0.f, 0.4f, 1.f, 1.f));
					}
				}
				if (c.coinTracers && (obj->m_objectID == 1329 || obj->m_objectID == 142)) {
					m_fields->hitNode->drawSegment(m_player1->getPosition(), obj->getPosition(), 0.7f, ccc4f(1.f, 0.85f, 0.f, 0.8f));
				}
			}
			if (c.showHitboxes) drawBox(m_fields->hitNode, playerRect, ccc4f(1.f, 1.f, 0.f, 1.f));
		}

		if (c.hitboxTrail) {
			m_fields->trail.push_back(playerRect);
			while (m_fields->trail.size() > 90) m_fields->trail.pop_front();
			float n = static_cast<float>(m_fields->trail.size());
			for (size_t i = 0; i < m_fields->trail.size(); ++i) {
				float a = 0.2f + 0.8f * (static_cast<float>(i) / n);
				drawBox(m_fields->hitNode, m_fields->trail[i], ccc4f(0.f, 1.f, 1.f, a));
			}
		}
		else {
			m_fields->trail.clear();
		}
	}

	void buildSwitcher() {
		m_fields->startPositions.clear();
		gatherStartPos(this, m_fields->startPositions);
		if (m_fields->startPositions.empty()) return;

		std::sort(m_fields->startPositions.begin(), m_fields->startPositions.end(), [](StartPosObject* a, StartPosObject* b) {
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

		m_fields->spLabel = CCLabelBMFont::create("", "bigFont.fnt");
		m_fields->spLabel->setScale(.35f);
		m_fields->spLabel->setPosition({win.width / 2.f, 24.f});
		this->addChild(m_fields->spLabel, 200);
		this->updateSpLabel();
	}

	void updateSpLabel() {
		if (!m_fields->spLabel) return;
		char buf[48];
		std::snprintf(buf, sizeof(buf), "StartPos %d/%d", m_fields->startIdx + 1, static_cast<int>(m_fields->startPositions.size()));
		m_fields->spLabel->setString(buf);
	}

	void changeStart(int d) {
		if (m_fields->startPositions.empty()) return;
		int n = static_cast<int>(m_fields->startPositions.size());
		m_fields->startIdx = ((m_fields->startIdx + d) % n + n) % n;
		trySwitchStartPos(this, m_fields->startPositions[m_fields->startIdx]);
		this->resetLevel();
		this->updateSpLabel();
	}

	void onSpPrev(CCObject*) { this->changeStart(-1); }
	void onSpNext(CCObject*) { this->changeStart(1); }
};

// ===========================================================================
// PlayerObject: jumphack, rainbow trail, rainbow icon, efek kematian
// ===========================================================================
class $modify(FQPlayer, PlayerObject) {
	void update(float dt) {
		PlayerObject::update(dt);
		if (!PlayLayer::get()) return;
		auto& c = cfg();
		if (c.jumphack) forceGround(this);
		if (c.rainbowTrail || c.rainbowIcon) {
			static float hue = 0.f;
			hue += dt * 0.4f;
			if (c.rainbowTrail) tintTrail(this, hueToColor(hue));
			if (c.rainbowIcon) tintIcon(this, hueToColor(hue), hueToColor(hue + 0.5f));
		}
	}

	void playDeathEffect() {
		if (cfg().noDeathEffect) return;
		PlayerObject::playDeathEffect();
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
// Unlock semua ikon
// ===========================================================================
class $modify(FQGameManager, GameManager) {
	bool isIconUnlocked(int id, IconType type) {
		if (cfg().unlockIcons) return true;
		return GameManager::isIconUnlocked(id, type);
	}

	bool isColorUnlocked(int id, UnlockType type) {
		if (cfg().unlockIcons) return true;
		return GameManager::isColorUnlocked(id, type);
	}
};

// ===========================================================================
// Menu utama (membuat overlay) dan konfirmasi Play Level
// ===========================================================================
class $modify(FQMenu, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;
		ensureOverlay();
		return true;
	}
};

static bool s_allowPlay = false;

class $modify(FQLevelInfo, LevelInfoLayer) {
	void onPlay(CCObject* sender) {
		if (!cfg().confirmPlay || s_allowPlay) return LevelInfoLayer::onPlay(sender);
		createQuickPopup(
			"Play Level", "Do you want to play this level?", "No", "Yes",
			[this, sender](FLAlertLayer*, bool yes) {
				if (yes) {
					s_allowPlay = true;
					this->onPlay(sender);
					s_allowPlay = false;
				}
			}
		);
	}
};

// ===========================================================================
// Menu pause: konfirmasi Practice dan Restart
// ===========================================================================
static bool s_allowPractice = false;
static bool s_allowRestart = false;

class $modify(FQPause, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();
		s_allowPractice = false;
		s_allowRestart = false;
	}

	void onPracticeMode(CCObject* sender) {
		if (!cfg().confirmPractice || s_allowPractice) return PauseLayer::onPracticeMode(sender);
		createQuickPopup(
			"Practice Mode", "Do you want to enter practice mode?", "No", "Yes",
			[this, sender](FLAlertLayer*, bool yes) {
				if (yes) {
					s_allowPractice = true;
					this->onPracticeMode(sender);
				}
			}
		);
	}

	void onRestart(CCObject* sender) {
		if (!cfg().confirmRestart || s_allowRestart) return PauseLayer::onRestart(sender);
		createQuickPopup(
			"Restart Level", "Do you want to restart the level?", "No", "Yes",
			[this, sender](FLAlertLayer*, bool yes) {
				if (yes) {
					s_allowRestart = true;
					this->onRestart(sender);
				}
			}
		);
	}
};

// ===========================================================================
// Editor: night mode, favorit objek, deco-only
// ===========================================================================
class $modify(FQEditor, EditorUI) {
	struct Fields {
		CCMenu* favMenu = nullptr;
		bool decoWasOn = false;
	};

	bool init(LevelEditorLayer* lel) {
		if (!EditorUI::init(lel)) return false;

		auto& c = cfg();
		if (c.nightMode && lel) {
			auto overlay = CCLayerColor::create(ccc4(0, 0, 0, 120));
			lel->addChild(overlay, 9999);
		}
		if (c.favorites) this->buildFav();
		this->schedule(schedule_selector(FQEditor::tick), 0.1f);
		return true;
	}

	void tick(float) {
		if (!m_editorLayer) return;
		if (!cfg().decoOnly) {
			if (m_fields->decoWasOn) {
				m_fields->decoWasOn = false;
				for (auto obj : CCArrayExt<GameObject*>(m_editorLayer->m_objects)) {
					if (obj) obj->setVisible(true);
				}
			}
			return;
		}
		m_fields->decoWasOn = true;
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

		float x = 100.f;
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
