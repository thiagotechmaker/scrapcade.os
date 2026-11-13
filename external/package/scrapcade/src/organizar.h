// ScrapCade (T-OS) - caixa de entrada: tudo que cai em "Jogos novos" vai sozinho para a pasta do console.
// Conversao do Organizer.java do iChroma. Licenca: GPLv3.
//
//  - ROM solta: vai para o console pela extensao. .bin grande ou citado por um .cue: CD de PlayStation.
//  - Imagem de disco (.iso, .rvz...): o console sai do cabecalho do disco (GameCube, Wii, PSP, PS1/PS2).
//  - .zip com ROM dentro: tira a ROM (ou o .cue com as faixas) e apaga o zip.
//  - .zip de Neo Geo (tem c1 e m1): vai para neogeo/, e a BIOS que vier junto vira neogeo/neogeo.zip.
//  - .zip de arcade: vai para arcade/ se o FBNeo conhece o conjunto (fbneo.idx). Com .exe dentro: PC (DOS).
//  - O resto vai para nao_reconhecidos/, para o usuario ver o que nao entrou.
#pragma once
#include "catalogo.h"
#include "meta.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <map>
#include <zip.h>

namespace Org {

static std::string base(const std::string &p) {
    size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
}

static bool isDir(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static long long sizeOf(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 ? (long long)st.st_size : -1;
}

// Tamanho e hora em que cada arquivo foi visto pela primeira vez com esse tamanho.
static std::map<std::string, std::pair<long long, long long>> seen;

static long long nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Arquivo ainda sendo copiado (rede, pen drive) nao se mexe: so entra depois de 10 s com o mesmo tamanho.
// A hora do arquivo nao serve: as copias mantem a hora do original. Em 10/11/2026, no iChroma, faixas de CD
// foram movidas no meio da copia e, ainda pequenas, foram parar no Mega Drive.
static bool settled(const std::string &p) {
    long long len = sizeOf(p), now = nowMs();
    auto it = seen.find(p);
    if (it == seen.end() || it->second.first != len) {
        seen[p] = {len, now};
        return false;
    }
    return now - it->second.second >= 10000;
}

// Windows, navegadores e copias pela metade deixam arquivos que nao sao jogo.
static bool ready(const std::string &p) {
    std::string n = lower(base(p));
    if (n.empty() || n[0] == '.' || n == "desktop.ini" || n == "thumbs.db") return false;
    for (const char *e : {".baixando", ".envio", ".part", ".crdownload", ".tmp", ".partial"})
        if (endsWith(n, e)) return false;
    return settled(p);
}

// Tudo que esta na caixa de entrada, inclusive em subpastas: da para arrastar uma pasta inteira.
static void collect(const std::string &d, int depth, std::vector<std::string> &out) {
    DIR *dir = opendir(d.c_str());
    if (!dir) return;
    while (dirent *e = readdir(dir)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string p = d + "/" + n;
        if (isDir(p)) {
            if (depth < 3) collect(p, depth + 1, out);
        } else if (ready(p)) {
            out.push_back(p);
        }
    }
    closedir(dir);
}

static void removeEmptyFolders(const std::string &d) {
    DIR *dir = opendir(d.c_str());
    if (!dir) return;
    std::vector<std::string> subs;
    while (dirent *e = readdir(dir)) {
        std::string n = e->d_name;
        if (n != "." && n != ".." && isDir(d + "/" + n)) subs.push_back(d + "/" + n);
    }
    closedir(dir);
    for (auto &s : subs) {
        removeEmptyFolders(s);
        rmdir(s.c_str());   // so apaga se estiver vazia
    }
}

static uint32_t be32(const unsigned char *b) { return uint32_t(b[0]) << 24 | b[1] << 16 | b[2] << 8 | b[3]; }

// Console de uma imagem de disco, pelo cabecalho. "" se nao reconheceu.
static std::string discKind(const std::string &p) {
    FILE *f = fopen(p.c_str(), "rb");
    if (!f) return "";
    unsigned char h[0x60] = {0}, v[0x30] = {0};
    size_t nh = fread(h, 1, sizeof h, f);
    bool iso9660 = fseek(f, 0x8000, SEEK_SET) == 0 && fread(v, 1, sizeof v, f) == sizeof v &&
                   memcmp(v + 1, "CD001", 5) == 0;
    fclose(f);
    if (nh >= 0x4C && memcmp(h, "RVZ\x01", 4) == 0) {   // RVZ: tipo do disco logo depois do primeiro bloco
        uint32_t t = be32(h + 0x48);
        return t == 2 ? "wii" : (t == 1 ? "gamecube" : "");
    }
    if (nh >= 0x20 && be32(h + 0x18) == 0x5D1C9EA3) return "wii";
    if (nh >= 0x20 && be32(h + 0x1C) == 0xC2339F3D) return "gamecube";
    if (iso9660) {
        std::string sys(reinterpret_cast<char *>(v + 8), 32);
        if (sys.compare(0, 8, "PSP GAME") == 0) return "psp";
        if (sys.compare(0, 11, "PLAYSTATION") == 0)   // CD de PS1 tem no maximo ~700 MB; DVD e PS2
            return sizeOf(p) > 900LL * 1024 * 1024 ? "ps2" : "psx";
    }
    return "";
}

// Console pela extensao do arquivo. "?" = imagem de disco (precisa olhar o cabecalho), "" = nao e jogo.
static std::string folder(const std::string &file) {
    std::string n = lower(file);
    static const std::vector<std::pair<std::string, std::vector<const char *>>> T = {
        {"gba", {".gba"}},
        {"gb", {".gb"}},
        {"gbc", {".gbc"}},
        {"nes", {".nes", ".fds"}},
        {"snes", {".smc", ".sfc"}},
        {"n64", {".z64", ".n64", ".v64"}},
        {"megadrive", {".md", ".gen", ".smd", ".sms", ".gg"}},
        {"psx", {".cue", ".pbp", ".m3u", ".chd"}},
        {"nds", {".nds"}},
        {"3ds", {".3ds", ".cci", ".cxi", ".3dsx"}},
        {"gamecube", {".gcm", ".ciso"}},
        {"wii", {".wbfs"}},
        {"psp", {".cso"}},
        {"dos", {".dosz"}},
        {"?", {".iso", ".rvz"}},
    };
    for (auto &t : T)
        for (auto e : t.second)
            if (endsWith(n, e)) return t.first;
    return "";
}

// "Sonic (1).md", do navegador baixando de novo, e o mesmo jogo: vira "Sonic.md", e nao entra repetido.
static std::string noCopySuffix(const std::string &name) {
    size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot < 4 || name[dot - 1] != ')') return name;
    size_t open = name.rfind(" (", dot);
    if (open == std::string::npos) return name;
    for (size_t i = open + 2; i < dot - 1; i++)
        if (!isdigit((unsigned char)name[i])) return name;
    if (open + 2 == dot - 1) return name;
    return name.substr(0, open) + name.substr(dot);
}

// Move para ROMS/<dir>/, com o nome limpo. Mesmo disco: rename; outro disco: copia e apaga.
static bool moveTo(const std::string &from, const std::string &dir, std::string name = "") {
    std::string d = ROMS + "/" + dir;
    mkdirs(d);
    if (name.empty()) name = noCopySuffix(base(from));
    std::string to = d + "/" + name;
    if (isFile(to)) remove(to.c_str());
    if (rename(from.c_str(), to.c_str()) == 0) return true;
    std::ifstream in(from, std::ios::binary);
    std::ofstream out(to + ".baixando", std::ios::binary);
    out << in.rdbuf();
    out.close();
    if (!in || !out || sizeOf(to + ".baixando") != sizeOf(from)) { remove((to + ".baixando").c_str()); return false; }
    rename((to + ".baixando").c_str(), to.c_str());
    remove(from.c_str());
    return true;
}

// Algum .cue (na mesma pasta ou ja em ROMS/psx) cita esse arquivo: e faixa de CD.
static bool inSomeCue(const std::string &bin) {
    std::string want = lower(base(bin));
    size_t s = bin.rfind('/');
    for (const std::string &d : {bin.substr(0, s == std::string::npos ? 0 : s), ROMS + "/psx"}) {
        DIR *dir = opendir(d.c_str());
        if (!dir) continue;
        bool hit = false;
        while (dirent *e = readdir(dir)) {
            std::string c = d + "/" + e->d_name;
            if (!endsWith(c, ".cue") || sizeOf(c) > 64 * 1024) continue;
            std::ifstream in(c);
            std::string l;
            while (!hit && std::getline(in, l)) hit = lower(l).find(want) != std::string::npos;
            if (hit) break;
        }
        closedir(dir);
        if (hit) return true;
    }
    return false;
}

// ------------------------------------------------------------ zip

struct Entry { zip_uint64_t idx; std::string name; zip_uint64_t size; };

static std::vector<Entry> entries(zip_t *z) {
    std::vector<Entry> v;
    zip_int64_t n = zip_get_num_entries(z, 0);
    for (zip_int64_t i = 0; i < n; i++) {
        zip_stat_t st;
        if (zip_stat_index(z, zip_uint64_t(i), 0, &st) != 0 || !st.name) continue;
        std::string nm = st.name;
        if (nm.empty() || nm.back() == '/') continue;
        v.push_back({zip_uint64_t(i), nm, st.size});
    }
    return v;
}

static bool extract(zip_t *z, const Entry &e, const std::string &destDir) {
    mkdirs(destDir);
    std::string name = base(e.name), to = destDir + "/" + noCopySuffix(name), tmp = to + ".baixando";
    zip_file_t *zf = zip_fopen_index(z, e.idx, 0);
    if (!zf) return false;
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) { zip_fclose(zf); return false; }
    std::vector<char> b(1 << 16);
    zip_int64_t r;
    bool ok = true;
    while ((r = zip_fread(zf, b.data(), b.size())) > 0)
        if (fwrite(b.data(), 1, size_t(r), f) != size_t(r)) { ok = false; break; }
    if (r < 0) ok = false;
    zip_fclose(zf);
    if (fclose(f) != 0) ok = false;
    if (!ok) { remove(tmp.c_str()); return false; }
    if (isFile(to)) remove(to.c_str());
    return rename(tmp.c_str(), to.c_str()) == 0;
}

// Conjunto de Neo Geo: tem as ROMs de sprites (c1) e de som (m1).
static bool isNeoGeo(const std::vector<Entry> &es) {
    bool c1 = false, m1 = false;
    for (auto &e : es) {
        std::string n = lower(e.name);
        if (endsWith(n, ".c1") || n.find("-c1.") != std::string::npos) c1 = true;
        if (endsWith(n, ".m1") || n.find("-m1.") != std::string::npos) m1 = true;
    }
    return c1 && m1;
}

static bool isBios(const std::string &file) {
    std::string n = lower(base(file));
    return n == "000-lo.lo" || n == "sfix.sfix" || n == "sm1.sm1" || endsWith(n, ".sp1") || n.rfind("uni-bios", 0) == 0 ||
           n.rfind("sp-1v1", 0) == 0 || n == "vs-bios.rom" || n == "asia-s3.rom" || n == "japan-j3.bin" || n == "v2.bin" ||
           n == "sp1.jipan.1024";
}

// Muitos zips atuais trazem a BIOS junto: separa uma vez em neogeo/neogeo.zip, que os jogos de Neo Geo pedem.
static void neoBios(zip_t *z, const std::vector<Entry> &es) {
    std::string dest = ROMS + "/neogeo/neogeo.zip";
    if (isFile(dest)) return;
    std::vector<Entry> b;
    bool sfix = false, lo = false;
    for (auto &e : es)
        if (isBios(e.name)) {
            b.push_back(e);
            if (lower(base(e.name)) == "sfix.sfix") sfix = true;
            if (lower(base(e.name)) == "000-lo.lo") lo = true;
        }
    if (!sfix || !lo) return;
    mkdirs(ROMS + "/neogeo");
    std::string tmp = dest + ".baixando";
    int err = 0;
    zip_t *out = zip_open(tmp.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (!out) return;
    for (auto &e : b) {
        zip_file_t *zf = zip_fopen_index(z, e.idx, 0);
        if (!zf) continue;
        void *buf = malloc(size_t(e.size) + 1);
        zip_int64_t r = zip_fread(zf, buf, e.size);
        zip_fclose(zf);
        if (r != zip_int64_t(e.size)) { free(buf); continue; }
        zip_source_t *src = zip_source_buffer(out, buf, e.size, 1);   // a fonte libera o buffer
        if (!src || zip_file_add(out, base(e.name).c_str(), src, ZIP_FL_OVERWRITE) < 0) {
            if (src) zip_source_free(src);
            else free(buf);
        }
    }
    if (zip_close(out) == 0) rename(tmp.c_str(), dest.c_str());
    else { zip_discard(out); remove(tmp.c_str()); }
}

// "mslug (1).zip" -> "mslug": o FBNeo acha o jogo pelo nome do zip.
static std::string gameName(const std::string &file) {
    std::string n = lower(base(file));
    if (endsWith(n, ".zip")) n = n.substr(0, n.size() - 4);
    size_t cut = n.find(" (");
    if (cut != std::string::npos && cut > 0) n = n.substr(0, cut);
    while (!n.empty() && n.back() == ' ') n.pop_back();
    return n;
}

// 1 = jogo novo no menu, 0 = tirou algo para a caixa de entrada (entra na proxima volta), -1 = nao reconheceu
static int fromZip(const std::string &path) {
    int err = 0;
    zip_t *z = zip_open(path.c_str(), ZIP_RDONLY, &err);
    if (!z) return -1;
    auto es = entries(z);
    std::vector<Entry> roms, discs;
    bool cd = false, exe = false;
    for (auto &e : es) {
        std::string f = folder(e.name), n = lower(e.name);
        if (f == "?") discs.push_back(e);
        else if (!f.empty()) roms.push_back(e);
        if (endsWith(n, ".cue")) cd = true;
        if (endsWith(n, ".exe") || endsWith(n, ".com") || endsWith(n, ".bat")) exe = true;
    }
    int res = -1;
    std::string moveAs;   // zip vai inteiro para esta pasta
    if (!roms.empty() && !isNeoGeo(es)) {
        bool ok = true;
        if (cd) {   // CD: leva o .cue e as faixas juntos
            for (auto &e : es) ok = extract(z, e, ROMS + "/psx") && ok;
        } else {    // cartucho: so a ROM (a maior, se vierem varias versoes)
            const Entry *big = &roms[0];
            for (auto &e : roms) if (e.size > big->size) big = &e;
            ok = extract(z, *big, ROMS + "/" + folder(big->name));
        }
        res = ok ? 1 : -1;
    } else if (!discs.empty()) {
        // imagem de disco zipada: sai para a caixa de entrada e e reconhecida pelo cabecalho na proxima volta
        bool ok = true;
        for (auto &e : es) ok = extract(z, e, INBOX) && ok;
        res = ok ? 0 : -1;
    } else if (isNeoGeo(es)) {
        neoBios(z, es);
        moveAs = "neogeo";
    } else if (Meta::arcadeSet(gameName(path))) {
        moveAs = "arcade";
    } else if (exe) {
        moveAs = "dos";
    }
    zip_close(z);
    if (!moveAs.empty()) {
        std::string name = moveAs == "dos" ? noCopySuffix(base(path)) : gameName(path) + ".zip";
        return moveTo(path, moveAs, name) ? 1 : -1;
    }
    if (res >= 0) remove(path.c_str());
    return res;
}

// ------------------------------------------------------------ organizar

static std::string join(const std::vector<std::string> &l) {
    std::string s;
    for (auto &x : l) s += (s.empty() ? "" : ", ") + x;
    return s;
}

// Organiza tudo. Devolve o aviso para mostrar na tela, ou "" se nada aconteceu.
static std::string run() {
    mkdirs(INBOX);
    std::vector<std::string> fs;
    collect(INBOX, 0, fs);
    if (fs.empty()) return "";
    // .cue antes das faixas: assim o .bin sabe que e CD mesmo pequeno
    std::stable_sort(fs.begin(), fs.end(), [](const std::string &a, const std::string &b) {
        return endsWith(a, ".cue") && !endsWith(b, ".cue");
    });
    int ok = 0;
    std::vector<std::string> failed;
    for (auto &f : fs) {
        if (!isFile(f)) continue;   // ja foi junto com outro
        std::string n = base(f), kind = folder(n);
        bool good = false, counted = true;
        if (endsWith(n, ".zip")) {
            int r = fromZip(f);
            good = r >= 0;
            counted = r == 1;
        } else if (endsWith(n, ".bin")) {
            // .bin de CD (com .cue junto ou grande) ou cartucho de Mega Drive
            std::string cue = f.substr(0, f.size() - 4) + ".cue";
            bool isCd = sizeOf(f) > 16LL * 1024 * 1024 || isFile(cue) || inSomeCue(f);
            good = moveTo(f, isCd ? "psx" : "megadrive");
            counted = !isCd;
        } else if (kind == "?") {
            std::string k = discKind(f);
            good = !k.empty() && moveTo(f, k);
        } else if (!kind.empty()) {
            good = moveTo(f, kind);
        }
        if (good) {
            if (counted) ok++;
        } else {
            failed.push_back(n);
            if (isFile(f)) moveTo(f, "nao_reconhecidos");
        }
    }
    removeEmptyFolders(INBOX);
    std::string s;
    if (ok > 0) s = ok == 1 ? "1 jogo novo organizado" : std::to_string(ok) + " jogos novos organizados";
    if (!failed.empty()) s += (s.empty() ? "" : "  •  ") + std::string("Não reconhecidos: ") + join(failed);
    return s;
}

}  // namespace Org
