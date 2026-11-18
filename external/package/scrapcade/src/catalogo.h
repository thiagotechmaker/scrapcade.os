// ScrapCade - consoles, jogos e pastas. Licenca: GPLv3.
#pragma once
#include <algorithm>
#include <cstdio>
#include <set>
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
    {"megadrive", "md", "Mega Drive", "Genesis Plus GX", 0xFF2B68C8, {".md", ".gen", ".smd", ".bin", ".zip"}},
    {"sg1000", "", "SG-1000", "Genesis Plus GX", 0xFF6D7B8A, {".sg", ".zip"}},
    {"mastersystem", "", "Master System", "Genesis Plus GX", 0xFF1F5FBF, {".sms", ".zip"}},
    {"gamegear", "", "Game Gear", "Genesis Plus GX", 0xFF3B3B3B, {".gg", ".zip"}},
    {"segacd", "", "Sega CD", "Genesis Plus GX", 0xFF4A4A8C, {".cue", ".chd", ".m3u"}},
    {"sega32x", "", "32X", "PicoDrive", 0xFFB8862B, {".32x", ".zip"}},
    {"pcengine", "", "PC Engine", "Beetle PCE Fast", 0xFFE2E2E2, {".pce", ".cue", ".chd", ".zip"}},
    {"atari2600", "", "Atari 2600", "Stella 2014", 0xFF8B5A2B, {".a26", ".bin", ".zip"}},
    {"atari5200", "", "Atari 5200", "a5200", 0xFF7A4A2A, {".a52", ".bin", ".zip"}},
    {"atari7800", "", "Atari 7800", "ProSystem", 0xFF6B3E2E, {".a78", ".bin", ".zip"}},
    {"lynx", "", "Atari Lynx", "Handy", 0xFF5D6D7E, {".lnx", ".zip"}},
    {"jaguar", "", "Atari Jaguar", "Virtual Jaguar", 0xFFB22222, {".j64", ".jag", ".zip"}},
    {"colecovision", "", "ColecoVision", "Gearcoleco", 0xFF2C3E50, {".col", ".cv", ".zip"}},
    {"intellivision", "", "Intellivision", "FreeIntv", 0xFF8E6B23, {".int", ".bin", ".rom", ".zip"}},
    {"odyssey2", "", "Odyssey²", "O2EM", 0xFF1F3A93, {".bin", ".zip"}},
    {"vectrex", "", "Vectrex", "vecx", 0xFF4A4A4A, {".vec", ".bin", ".zip"}},
    {"3do", "", "3DO", "Opera", 0xFFC0392B, {".iso", ".cue", ".chd"}},
    {"ngp", "", "Neo Geo Pocket", "Beetle NeoPop", 0xFF2E86C1, {".ngp", ".ngc", ".zip"}},
    {"wswan", "", "WonderSwan", "Beetle Cygne", 0xFF7D3C98, {".ws", ".wsc", ".zip"}},
    {"virtualboy", "", "Virtual Boy", "Beetle VB", 0xFFB03A2E, {".vb", ".vboy", ".zip"}},
    {"dreamcast", "", "Dreamcast", "Flycast", 0xFFE8742B, {".cdi", ".gdi", ".chd", ".cue", ".m3u"}},
    {"saturn", "", "Sega Saturn", "Beetle Saturn", 0xFF3A3F8F, {".cue", ".chd", ".ccd", ".m3u"}},
    {"psx", "psx", "PlayStation", "PCSX ReARMed", 0xFF7D8494, {".cue", ".chd", ".pbp", ".m3u", ".iso"}},
    {"ps2", "", "PlayStation 2", "PCSX2", 0xFF2C3E9A, {".iso", ".chd", ".cso"}},
    {"ps3", "", "PlayStation 3", "RPCS3", 0xFF1B1B1B, {".ps3"}},
    {"xbox", "", "Xbox", "xemu", 0xFF107C10, {".iso", ".xiso"}},
    {"gamecube", "", "GameCube", "Dolphin", 0xFF5B3FA8, {".iso", ".rvz", ".gcm", ".ciso"}},
    {"wii", "", "Wii", "Dolphin", 0xFF8A97A3, {".iso", ".rvz", ".wbfs"}},
    {"psp", "", "PSP", "PPSSPP", 0xFF34495E, {".iso", ".cso", ".pbp", ".chd"}},
    {"nds", "", "Nintendo DS", "melonDS", 0xFF9AA3AA, {".nds", ".zip"}},
    {"3ds", "", "Nintendo 3DS", "Citra", 0xFFD35400, {".3ds", ".cci", ".cxi", ".3dsx"}},
    {"arcade", "arcade", "Fliperama", "FBNeo", 0xFFD9822B, {".zip"}},
    {"neogeo", "neogeo", "Neo Geo", "FBNeo", 0xFF2F7DD1, {".zip"}},
    {"neogeocd", "", "Neo Geo CD", "NeoCD", 0xFF1E5AA8, {".cue", ".chd"}},
    {"dos", "", "PC (DOS)", "DOSBox Pure", 0xFF8C2F2F, {".zip", ".dosz", ".exe"}},
    {"msx", "", "MSX", "blueMSX", 0xFF5D6D7E, {".rom", ".mx1", ".mx2", ".dsk", ".cas", ".zip"}},
    {"c64", "", "Commodore 64", "VICE", 0xFF6C5B7B, {".d64", ".t64", ".prg", ".crt", ".tap", ".zip"}},
    {"amiga", "", "Amiga", "PUAE", 0xFFE74C3C, {".adf", ".hdf", ".lha", ".m3u", ".zip"}},
    {"atarist", "", "Atari ST", "Hatari", 0xFF34495E, {".st", ".msa", ".stx", ".zip"}},
    {"scummvm", "", "ScummVM", "ScummVM", 0xFF27AE60, {".scummvm"}},
    {"easyrpg", "", "RPG Maker", "EasyRPG", 0xFFD35400, {".easyrpg"}},
    {"doom", "", "Doom", "PrBoom", 0xFF9B2D20, {".wad"}},
    {"quake", "", "Quake", "TyrQuake", 0xFF6E2C00, {".quake"}},
    {"cavestory", "", "Cave Story", "NXEngine", 0xFF2471A3, {".cavestory"}},
    {"halflife", "", "Half-Life", "Xash3D", 0xFFE07B1A, {".xash"}},
    {"ports", "", "Ports", "PortMaster", 0xFF8E44AD, {".sh"}},
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

static void halfLifeAtalhos() {
    std::string hl = ROMS + "/halflife";
    std::set<std::string> pastas;
    if (DIR *d = opendir(hl.c_str())) {
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            if (n.empty() || n[0] == '.') continue;
            std::string lib = hl + "/" + n + "/liblist.gam";
            FILE *f = fopen(lib.c_str(), "r");
            if (!f) continue;
            pastas.insert(n);
            std::string titulo = n == "valve" ? "Half-Life" : n;
            char linha[256];
            while (fgets(linha, sizeof linha, f)) {   // game "Half-Life"
                std::string l = linha;
                size_t g = l.find("game"), a1 = l.find('"'), a2 = l.find('"', a1 + 1);
                if (g != std::string::npos && g < a1 && a2 != std::string::npos && l.compare(0, 4, "game") == 0) {
                    titulo = l.substr(a1 + 1, a2 - a1 - 1);
                    break;
                }
            }
            fclose(f);
            for (char &c : titulo) if (strchr(":/\\?*\"<>|", c)) c = ' ';
            std::string atalho = hl + "/" + titulo + ".xash";
            struct stat st;
            if (stat(atalho.c_str(), &st) != 0)
                if (FILE *o = fopen(atalho.c_str(), "w")) { fprintf(o, "%s\n", n.c_str()); fclose(o); }
        }
        closedir(d);
    }
    if (DIR *d = opendir(hl.c_str())) {   // atalho de pasta que saiu
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() < 6 || n.compare(n.size() - 5, 5, ".xash") != 0) continue;
            std::string c = hl + "/" + n;
            char buf[256] = {0};
            if (FILE *f = fopen(c.c_str(), "r")) { if (!fgets(buf, sizeof buf, f)) buf[0] = 0; fclose(f); }
            std::string pasta = buf;
            while (!pasta.empty() && (pasta.back() == '\n' || pasta.back() == '\r' || pasta.back() == ' ')) pasta.pop_back();
            if (!pastas.count(pasta)) remove(c.c_str());
        }
        closedir(d);
    }
}

// Jogo que e uma pasta (ScummVM, RPG Maker, PS3): o menu mostra um atalho "<pasta>.<ext>" com o nome da pasta
// dentro; o tos-jogar abre o arquivo certo dela. Pasta que saiu leva o atalho junto. So olha as pastas que tem
// a marca do console (temJogo), para nao listar pastas de saves ou de capas.
static bool temArquivo(const std::string &dir, const char *nome) {
    DIR *d = opendir(dir.c_str());
    if (!d) return false;
    bool achou = false;
    while (dirent *e = readdir(d)) if (strcasecmp(e->d_name, nome) == 0) { achou = true; break; }
    closedir(d);
    return achou;
}
static bool ehPasta(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
static void atalhosDePastas(const std::string &id, const std::string &ext, bool (*temJogo)(const std::string &)) {
    std::string raiz = ROMS + "/" + id;
    std::set<std::string> pastas;
    if (DIR *d = opendir(raiz.c_str())) {
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            if (n.empty() || n[0] == '.' || n == "images" || !ehPasta(raiz + "/" + n) || !temJogo(raiz + "/" + n)) continue;
            pastas.insert(n);
            std::string atalho = raiz + "/" + n + ext;
            if (!isFile(atalho))
                if (FILE *o = fopen(atalho.c_str(), "w")) { fprintf(o, "%s\n", n.c_str()); fclose(o); }
        }
        closedir(d);
    }
    if (DIR *d = opendir(raiz.c_str())) {
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() <= ext.size() || lower(n.substr(n.size() - ext.size())) != ext) continue;
            if (!pastas.count(n.substr(0, n.size() - ext.size()))) remove((raiz + "/" + n).c_str());
        }
        closedir(d);
    }
}
// ScummVM: qualquer pasta com arquivos (o proprio ScummVM reconhece o jogo pelo conteudo)
static bool pastaScummvm(const std::string &p) {
    DIR *d = opendir(p.c_str());
    if (!d) return false;
    bool tem = false;
    while (dirent *e = readdir(d)) if (e->d_name[0] != '.') { tem = true; break; }
    closedir(d);
    return tem;
}
static bool pastaRpgMaker(const std::string &p) { return temArquivo(p, "RPG_RT.ldb"); }
static bool pastaPs3(const std::string &p) {
    return isFile(p + "/PS3_GAME/USRDIR/EBOOT.BIN") || isFile(p + "/USRDIR/EBOOT.BIN");
}

static std::vector<Game> scanGames() {
    std::vector<Game> out;
    halfLifeAtalhos();
    atalhosDePastas("scummvm", ".scummvm", pastaScummvm);
    atalhosDePastas("easyrpg", ".easyrpg", pastaRpgMaker);
    atalhosDePastas("ps3", ".ps3", pastaPs3);
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

