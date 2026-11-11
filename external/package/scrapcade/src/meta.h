// ScrapCade (T-OS) - dados de cada jogo: nome oficial, ano, empresa e genero. Licenca: GPLv3.
//
// Conversao do Meta.java do iChroma. Vem dos indices em DATA/crc/*.idx e DATA/fbneo.idx (gzip), sem rede
// e sem conta: o jogo e reconhecido pelo CRC do arquivo. O resultado fica em ROMS/.meta, para so procurar
// uma vez. Linha do .meta: chave \t nome \t ano \t empresa \t genero.
#pragma once
#include "catalogo.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <unordered_map>
#include <zip.h>
#include <zlib.h>

namespace Meta {

struct Info {
    std::string name, year, dev, genre;
};

static std::mutex mu;
static std::unordered_map<std::string, Info> cache;
static bool loaded = false;

static std::vector<std::string> split(const std::string &l, char sep) {
    std::vector<std::string> f;
    size_t a = 0;
    while (true) {
        size_t b = l.find(sep, a);
        f.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return f;
}

static std::string file() { return ROMS + "/.meta"; }

static void loadLocked() {
    if (loaded) return;
    loaded = true;
    std::ifstream in(file());
    std::string l;
    while (std::getline(in, l)) {
        auto f = split(l, '\t');
        if (f.size() < 5) continue;
        cache[f[0]] = Info{f[1], f[2], f[3], f[4]};
    }
}

// O que ja se sabe do jogo; false se ele ainda nao foi procurado.
static bool get(const std::string &key, Info &out) {
    std::lock_guard<std::mutex> l(mu);
    loadLocked();
    auto it = cache.find(key);
    if (it == cache.end()) return false;
    out = it->second;
    return true;
}

static bool known(const std::string &key) {
    std::lock_guard<std::mutex> l(mu);
    loadLocked();
    return cache.count(key) > 0;
}

static void put(const std::string &key, const Info &i) {
    std::lock_guard<std::mutex> l(mu);
    loadLocked();
    cache[key] = i;
    FILE *f = fopen(file().c_str(), "a");
    if (!f) return;   // sem disco: procura de novo na proxima vez
    fprintf(f, "%s\t%s\t%s\t%s\t%s\n", key.c_str(), i.name.c_str(), i.year.c_str(), i.dev.c_str(), i.genre.c_str());
    fclose(f);
}

static const char *GENRES[][2] = {
    {"platform", "Plataforma"}, {"racing", "Corrida"}, {"driving", "Corrida"}, {"fighting", "Luta"},
    {"beat", "Briga de rua"}, {"shoot", "Tiro"}, {"role-playing", "RPG"}, {"rpg", "RPG"}, {"sport", "Esporte"},
    {"puzzle", "Quebra-cabeça"}, {"adventure", "Aventura"}, {"strategy", "Estratégia"}, {"simulation", "Simulação"},
    {"pinball", "Pinball"}, {"board", "Tabuleiro"}, {"card", "Cartas"}, {"music", "Música"}, {"quiz", "Perguntas"},
    {"educational", "Educativo"}, {"compilation", "Coletânea"}, {"action", "Ação"},
};

static std::string genrePt(const std::string &g) {
    std::string low = lower(g);
    for (auto &p : GENRES) if (low.find(p[0]) != std::string::npos) return p[1];
    return g;
}

// "1991  •  Nintendo  •  Plataforma", so com o que existir.
static std::string line(const Game &g) {
    Info i;
    if (!get(g.key(), i)) return "";
    std::string s;
    for (const std::string &p : {i.year, i.dev, genrePt(i.genre)}) {
        if (p.empty()) continue;
        if (!s.empty()) s += "  •  ";
        s += p;
    }
    return s;
}

// ------------------------------------------------------------ arcade e Neo Geo (fbneo.idx: zip \t nome \t ano \t empresa)

struct Set { std::string name, year, dev; };
static std::unordered_map<std::string, Set> fbneo;
static bool fbneoLoaded = false;

static const Set *arcadeSet(const std::string &zipBase) {
    std::lock_guard<std::mutex> l(mu);
    if (!fbneoLoaded) {
        fbneoLoaded = true;
        gzFile z = gzopen((DATA + "/fbneo.idx").c_str(), "rb");
        if (z) {
            char buf[1024];
            while (gzgets(z, buf, sizeof buf)) {
                std::string ln = buf;
                while (!ln.empty() && (ln.back() == '\n' || ln.back() == '\r')) ln.pop_back();
                auto f = split(ln, '\t');
                if (f.size() < 2) continue;
                fbneo[f[0]] = Set{f[1], f.size() > 2 ? f[2] : "", f.size() > 3 ? f[3] : ""};
            }
            gzclose(z);
        }
    }
    auto it = fbneo.find(lower(zipBase));
    return it == fbneo.end() ? nullptr : &it->second;
}

// ------------------------------------------------------------ busca por CRC (le o arquivo do jogo: fora da tela)

static std::string extOf(const std::string &f) {
    size_t d = f.rfind('.');
    return d == std::string::npos ? "" : lower(f.substr(d + 1));
}

// Qual indice de CRC serve para o jogo, ou "" (CD, arcade e PC nao tem).
static std::string indexId(const Game &g) {
    const std::string &id = g.sys->id, e = extOf(g.file);
    if (id == "gba" || id == "nes" || id == "snes" || id == "n64" || id == "gb") return id;
    if (id == "gbc") return e == "gb" ? "gb" : "gbc";
    if (id == "megadrive") return e == "sms" ? "sms" : (e == "gg" ? "gg" : "md");
    return "";
}

// CRC do jogo, sem o cabecalho que as bases nao contam (.smc de 512 bytes, NES de 16).
static std::string romCrc(const std::string &path) {
    char hex[16];
    if (endsWith(path, ".zip")) {
        int err = 0;
        zip_t *z = zip_open(path.c_str(), ZIP_RDONLY, &err);
        if (!z) return "";
        zip_int64_t n = zip_get_num_entries(z, 0);
        zip_uint64_t bigSize = 0;
        uint32_t crc = 0;
        bool found = false;
        for (zip_int64_t i = 0; i < n; i++) {
            zip_stat_t st;
            if (zip_stat_index(z, zip_uint64_t(i), 0, &st) != 0) continue;
            if (!found || st.size > bigSize) { bigSize = st.size; crc = st.crc; found = true; }
        }
        zip_close(z);
        if (!found) return "";
        snprintf(hex, sizeof hex, "%08X", crc);
        return hex;
    }
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || st.st_size > 64L * 1024 * 1024) return "";
    long skip = extOf(path) == "nes" ? 16 : (st.st_size % 1024 == 512 ? 512 : 0);
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return "";
    fseek(f, skip, SEEK_SET);
    uLong c = crc32(0L, Z_NULL, 0);
    std::vector<unsigned char> b(65536);
    size_t r;
    while ((r = fread(b.data(), 1, b.size(), f)) > 0) c = crc32(c, b.data(), uInt(r));
    fclose(f);
    snprintf(hex, sizeof hex, "%08lX", (unsigned long)c);
    return hex;
}

// Linha "CRC nome\tano\tempresa\tgenero" do indice, sem o CRC.
static std::string lookup(const std::string &id, const std::string &crc) {
    if (crc.empty()) return "";
    gzFile z = gzopen((DATA + "/crc/" + id + ".idx").c_str(), "rb");
    if (!z) return "";
    char buf[1024];
    std::string out;
    while (gzgets(z, buf, sizeof buf)) {
        if (strncmp(buf, crc.c_str(), 8) == 0 && strlen(buf) > 9) {
            out = buf + 9;
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
            break;
        }
    }
    gzclose(z);
    return out;
}

static Info find(const Game &g) {
    Info i;
    const std::string &id = g.sys->id;
    if (id == "arcade" || id == "neogeo") {
        if (const Set *s = arcadeSet(g.base)) {
            i.name = s->name;
            i.year = s->year;
            i.dev = s->dev;
        }
        return i;
    }
    std::string ix = indexId(g);
    if (ix.empty()) return i;
    std::string rec = lookup(ix, romCrc(g.dir + "/" + g.file));
    if (!rec.empty()) {
        auto f = split(rec, '\t');
        i.name = f[0];
        if (f.size() > 1) i.year = f[1];
        if (f.size() > 2) i.dev = f[2];
        if (f.size() > 3) i.genre = f[3];
    }
    return i;
}

// Procura o que falta. true se achou algo novo.
static bool fill(const std::vector<Game> &games, const volatile bool &stop) {
    bool any = false;
    for (auto &g : games) {
        if (stop) break;
        if (known(g.key())) continue;
        put(g.key(), find(g));
        any = true;
    }
    return any;
}

}  // namespace Meta
