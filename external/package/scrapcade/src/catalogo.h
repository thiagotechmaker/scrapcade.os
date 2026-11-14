// ScrapCade (T-OS) - consoles, jogos e pastas. Licenca: GPLv3.
#pragma once
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sstream>
#include <string>
#include <strings.h>
#include <sys/stat.h>
#include <vector>

static std::string DATA = "/usr/share/scrapcade";
static std::string ROMS = "/userdata/roms";
static std::string INBOX = "/userdata/Jogos novos";
static std::string CONF_FILE = "/userdata/system/scrapcade.conf";

// ------------------------------------------------------------------ consoles

struct Sys {
    std::string id, logo, name, emu;
    uint32_t color;
    std::vector<std::string> exts;
};

// Mesmas cores do iChroma onde o console ja existia la
static std::vector<Sys> SYSTEMS = {
    {"nes", "nes", "NES", "Mesen", 0xFFC93636, {".nes", ".fds", ".zip"}},
    {"snes", "snes", "Super Nintendo", "Snes9x", 0xFF5560C2, {".smc", ".sfc", ".zip"}},
    {"n64", "n64", "Nintendo 64", "Mupen64Plus-Next", 0xFF1E9C4A, {".z64", ".n64", ".v64", ".zip"}},
    {"gb", "gbc", "Game Boy", "Gambatte", 0xFF7F8C55, {".gb", ".zip"}},
    {"gbc", "gbc", "Game Boy Color", "Gambatte", 0xFF2E9E62, {".gbc", ".zip"}},
    {"gba", "gba", "Game Boy Advance", "mGBA", 0xFF6C47C9, {".gba", ".zip"}},
    {"megadrive", "md", "Mega Drive", "Genesis Plus GX", 0xFF2B68C8, {".md", ".gen", ".smd", ".bin", ".sms", ".gg", ".zip"}},
    {"saturn", "", "Sega Saturn", "Beetle Saturn", 0xFF3A3F8F, {".cue", ".chd", ".ccd", ".m3u"}},
    {"psx", "psx", "PlayStation", "PCSX ReARMed", 0xFF7D8494, {".cue", ".chd", ".pbp", ".m3u", ".iso"}},
    {"ps2", "", "PlayStation 2", "PCSX2", 0xFF2C3E9A, {".iso", ".chd", ".cso"}},
    {"gamecube", "", "GameCube", "Dolphin", 0xFF5B3FA8, {".iso", ".rvz", ".gcm", ".ciso"}},
    {"wii", "", "Wii", "Dolphin", 0xFF8A97A3, {".iso", ".rvz", ".wbfs"}},
    {"psp", "", "PSP", "PPSSPP", 0xFF34495E, {".iso", ".cso", ".pbp", ".chd"}},
    {"nds", "", "Nintendo DS", "melonDS", 0xFF9AA3AA, {".nds", ".zip"}},
    {"3ds", "", "Nintendo 3DS", "Azahar", 0xFFD35400, {".3ds", ".cci", ".cxi", ".3dsx"}},
    {"arcade", "arcade", "Fliperama", "FBNeo", 0xFFD9822B, {".zip"}},
    {"neogeo", "neogeo", "Neo Geo", "FBNeo", 0xFF2F7DD1, {".zip"}},
    {"dos", "", "PC (DOS)", "DOSBox Pure", 0xFF8C2F2F, {".zip", ".dosz", ".exe"}},
};

struct Game {
    const Sys *sys;
    std::string dir, file, base, title;
    std::string key() const { return sys->id + "/" + file; }
};

static std::string lower(std::string s) {
    for (auto &c : s) c = char(tolower((unsigned char)c));
    return s;
}

static bool endsWith(const std::string &s, const std::string &e) {
    return s.size() >= e.size() && lower(s.substr(s.size() - e.size())) == e;
}

// Nome do jogo a partir do arquivo: sem extensao e sem as marcas "(U) [!]"
static std::string titleOf(const std::string &base) {
    std::string t = base;
    size_t p = t.find(" (");
    size_t q = t.find(" [");
    size_t cut = std::min(p, q);
    if (cut != std::string::npos && cut > 0) t = t.substr(0, cut);
    std::replace(t.begin(), t.end(), '_', ' ');
    // "Legend of Zelda, The - Link's Awakening" -> "The Legend of Zelda - Link's Awakening"
    size_t the = t.find(", The");
    if (the != std::string::npos && the > 0) {
        std::string rest = t.substr(the + 5);
        if (rest.empty() || rest.compare(0, 3, " - ") == 0) t = "The " + t.substr(0, the) + rest;
    }
    return t;
}

static bool isFile(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static void mkdirs(const std::string &p) {
    std::string cur;
    std::stringstream ss(p);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (part.empty()) continue;
        cur += "/" + part;
        mkdir(cur.c_str(), 0755);
    }
}

static std::vector<Game> scanGames() {
    std::vector<Game> out;
    for (auto &s : SYSTEMS) {
        std::string dir = ROMS + "/" + s.id;
        mkdirs(dir);
        DIR *d = opendir(dir.c_str());
        if (!d) continue;
        std::vector<Game> here;
        while (dirent *e = readdir(d)) {
            std::string f = e->d_name;
            if (f.empty() || f[0] == '.' || e->d_type == DT_DIR) continue;
            bool ok = false;
            for (auto &x : s.exts) if (endsWith(f, x)) ok = true;
            if (!ok) continue;
            // CD: so o .cue/.m3u aparece, as faixas .bin ficam escondidas
            if (s.id == "psx" && endsWith(f, ".bin")) continue;
            Game g{&s, dir, f, f.substr(0, f.rfind('.')), ""};
            g.title = titleOf(g.base);
            here.push_back(g);
        }
        closedir(d);
        std::sort(here.begin(), here.end(),
                  [](const Game &a, const Game &b) { return strcasecmp(a.title.c_str(), b.title.c_str()) < 0; });
        out.insert(out.end(), here.begin(), here.end());
    }
    return out;
}

// Capa: images/<jogo>.png (ou -image/-thumb, .jpg), igual as pastas do Batocera e do iChroma
static std::string coverOf(const Game &g, const std::string &suffix = "") {
    const char *subs[] = {"images", "media", "covers"};
    const char *ends[] = {".png", "-image.png", "-thumb.png", ".jpg", "-image.jpg", "-thumb.jpg"};
    for (auto sd : subs) {
        if (!suffix.empty()) {
            std::string p = g.dir + "/" + sd + "/" + g.base + suffix + ".png";
            if (isFile(p)) return p;
            continue;
        }
        for (auto en : ends) {
            std::string p = g.dir + "/" + sd + "/" + g.base + en;
            if (isFile(p)) return p;
        }
    }
    return "";
}

