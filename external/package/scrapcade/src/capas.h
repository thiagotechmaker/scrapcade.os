// ScrapCade - capa e imagem de fundo que faltam, baixadas do acervo libretro-thumbnails (sem conta).
// Conversao do Capas.java do iChroma. O que o acervo nao tem fica anotado em ROMS/.sem_capa e ROMS/.sem_fundo,
// para nao tentar de novo a cada vez. Licenca: GPLv3.
#pragma once
#include "catalogo.h"
#include "meta.h"

#include <cstdio>
#include <curl/curl.h>
#include <fstream>
#include <set>
#include <unistd.h>

namespace Capas {

static const std::string BASE = "https://thumbnails.libretro.com/";
static const char *COVER[] = {"Named_Boxarts", "Named_Titles"};   // sem caixa, serve a tela de titulo
static const char *BACKDROP[] = {"Named_Snaps", "Named_Titles"};  // uma cena do jogo

struct NetError {};   // rede caiu: para tudo e tenta mais tarde

static std::set<std::string> readMemo(const std::string &p) {
    std::set<std::string> s;
    std::ifstream in(p);
    std::string l;
    while (std::getline(in, l)) s.insert(l);
    return s;
}

static void addMemo(const std::string &p, const std::string &key) {
    FILE *f = fopen(p.c_str(), "a");
    if (f) { fprintf(f, "%s\n", key.c_str()); fclose(f); }
}

static std::string enc(const std::string &s) {
    char *e = curl_easy_escape(nullptr, s.c_str(), int(s.size()));
    std::string out = e ? e : "";
    curl_free(e);
    return out;
}

static std::string unenc(const std::string &s) {
    int n = 0;
    char *d = curl_easy_unescape(nullptr, s.c_str(), int(s.size()), &n);
    std::string out = d ? std::string(d, size_t(n)) : s;
    curl_free(d);
    return out;
}

// O acervo troca estes caracteres por "_" no nome do arquivo
static std::string safeName(std::string n) {
    for (auto &c : n) if (strchr("&*/:`<>?\\|\"", c)) c = '_';
    return n;
}

static size_t toFile(char *p, size_t s, size_t n, void *f) { return fwrite(p, s, n, (FILE *)f); }

// Baixa url em dest. 1 = baixou, 0 = nao existe no acervo; rede fora lanca NetError.
static int download(const std::string &url, const std::string &dest, long timeout = 20) {
    std::string tmp = dest + ".baixando";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) return 0;
    CURL *c = curl_easy_init();
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, toFile);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "ScrapCade/0.3 (ScrapCade)");
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    CURLcode r = curl_easy_perform(c);
    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(c);
    fflush(f);
    fsync(fileno(f));   // aparelho pode ser desligado da tomada: grava de verdade antes de renomear
    fclose(f);
    if (r == CURLE_OK && code == 200) {
        if (rename(tmp.c_str(), dest.c_str()) == 0) {
            fprintf(stderr, "scrapcade: baixou %s\n", dest.c_str());
            return 1;
        }
        remove(tmp.c_str());
        return 0;
    }
    remove(tmp.c_str());
    if (r == CURLE_HTTP_RETURNED_ERROR && code >= 400 && code < 500) return 0;   // nao tem no acervo
    fprintf(stderr, "scrapcade: capas: %s (%ld) %s\n", curl_easy_strerror(r), code, url.c_str());
    throw NetError{};
}

// Pasta do console no acervo, ou "" se o console nao tem arte la.
static std::string system(const Game &g) {
    const std::string &id = g.sys->id;
    if (id == "psx") return "Sony - PlayStation";
    if (id == "ps2") return "Sony - PlayStation 2";
    if (id == "psp") return "Sony - PlayStation Portable";
    if (id == "gamecube") return "Nintendo - GameCube";
    if (id == "wii") return "Nintendo - Wii";
    if (id == "nds") return "Nintendo - Nintendo DS";
    if (id == "3ds") return "Nintendo - Nintendo 3DS";
    if (id == "dos") return "DOS";
    std::string ix = Meta::indexId(g);
    if (ix == "gb") return "Nintendo - Game Boy";
    if (ix == "gbc") return "Nintendo - Game Boy Color";
    if (ix == "gba") return "Nintendo - Game Boy Advance";
    if (ix == "nes") return "Nintendo - Nintendo Entertainment System";
    if (ix == "snes") return "Nintendo - Super Nintendo Entertainment System";
    if (ix == "n64") return "Nintendo - Nintendo 64";
    if (ix == "sms") return "Sega - Master System - Mark III";
    if (ix == "gg") return "Sega - Game Gear";
    if (ix == "md") return "Sega - Mega Drive - Genesis";
    return "";
}

// Lista de capas de uma pasta do acervo, baixada uma vez e guardada em ROMS. "" se nao deu.
static std::string index(const std::string &sys, const std::string &file) {
    std::string idx = ROMS + "/" + file;
    if (!isFile(idx) && !download(BASE + enc(sys) + "/Named_Boxarts/", idx, 60)) return "";
    return idx;
}

// Nome do arquivo numa linha da lista do acervo, ou "".
static std::string entry(const std::string &l) {
    size_t a = l.find("href=\"");
    if (a == std::string::npos) return "";
    size_t b = l.find(".png\"", a + 6);
    if (b == std::string::npos) return "";
    return unenc(l.substr(a + 6, b - a - 6));
}

// Nome sem os parenteses do fim, como o acervo grava ("/" vira "_"), em minusculas.
static std::string stem(std::string n) {
    size_t p = n.find(" (");
    if (p == std::string::npos) p = n.find(" [");
    if (p != std::string::npos && p > 0) n = n.substr(0, p);
    std::replace(n.begin(), n.end(), '/', '_');
    while (!n.empty() && n.back() == ' ') n.pop_back();
    return lower(n);
}

static bool has(const std::string &s, const char *w) { return s.find(w) != std::string::npos; }

// Caixa do Neo Geo de casa com o mesmo nome do jogo. Prefere a de casa (NGH), depois a de fliperama (NGM);
// pirata e versao coreana so se o jogo for um deles.
static std::string neoBox(const std::string &official) {
    std::string idx = index("SNK - Neo Geo", ".capas_neogeo");
    if (idx.empty()) return "";
    std::string want = stem(official), off = lower(official);
    bool pirate = has(off, "bootleg"), korean = has(off, "korea");
    std::string best;
    int bestScore = -1;
    std::ifstream in(idx);
    std::string l;
    while (std::getline(in, l)) {
        std::string name = entry(l);
        if (name.empty() || stem(name) != want) continue;
        std::string low = lower(name);
        if (!pirate && has(low, "bootleg")) continue;
        if (!korean && has(low, "korea")) continue;
        int score = (has(low, "ngh") ? 4 : 0) + (has(low, "ngm") ? 2 : 0) +
                    (has(low, "prototype") || has(low, "alternate") ? 0 : 1);
        if (score > bestScore) { best = name; bestScore = score; }
    }
    return best;
}

// O acervo de arcade usa nomes mais novos; pega o mais curto que comeca com o nome do jogo (mundial primeiro).
static std::string fromIndex(const std::string &title) {
    std::string idx = index("MAME", ".capas_arcade");
    if (idx.empty()) return "";
    std::string safe = lower(safeName(title)), best;
    auto world = [](const std::string &n) { std::string l = lower(n); return has(l, "world") || has(l, "us"); };
    std::ifstream in(idx);
    std::string l;
    while (std::getline(in, l)) {
        std::string name = entry(l);
        if (name.empty()) continue;
        std::string low = lower(name);
        if (low.compare(0, safe.size(), safe) != 0) continue;
        if (low.size() > safe.size() && !strchr(" ,(", low[safe.size()])) continue;
        bool w = world(name), bw = !best.empty() && world(best);
        if (best.empty() || (w && !bw) || (w == bw && name.size() < best.size())) best = name;
    }
    return best;
}

// Onde procurar a arte: pares {pasta do acervo, nome}, do mais confiavel para o menos.
static std::vector<std::pair<std::string, std::string>> candidates(const Game &g) {
    std::vector<std::pair<std::string, std::string>> l;
    const std::string &id = g.sys->id;
    Meta::Info m;
    std::string official = Meta::get(g.key(), m) ? m.name : "";
    if (id == "arcade" || id == "neogeo") {
        // Neo Geo: primeiro a caixa do cartucho de casa (AES, em pe). O acervo de arcade costuma ter so o cartaz.
        if (id == "neogeo" && !official.empty()) {
            std::string home = neoBox(official);
            if (!home.empty()) l.push_back({"SNK - Neo Geo", home});
        }
        if (!official.empty()) l.push_back({"FBNeo - Arcade Games", official});
        std::string near = fromIndex(official.empty() ? g.title : titleOf(official));
        if (!near.empty()) l.push_back({"MAME", near});
        return l;
    }
    std::string sys = system(g);
    if (sys.empty()) return l;
    if (!official.empty()) l.push_back({sys, official});
    if (g.base != official) l.push_back({sys, g.base});
    return l;
}

static bool fetchAny(const std::vector<std::pair<std::string, std::string>> &where, const char *const *kinds,
                     const std::string &dest) {
    for (auto &w : where)
        for (int k = 0; k < 2; k++)
            if (download(BASE + enc(w.first) + "/" + kinds[k] + "/" + enc(safeName(w.second)) + ".png", dest)) return true;
    return false;
}

// Baixa o que falta. true se chegou capa ou fundo novo. Rede fora lanca NetError.
static bool work(const std::vector<Game> &games, bool backdrops, const volatile bool &stop) {
    std::string memoC = ROMS + "/.sem_capa", memoF = ROMS + "/.sem_fundo";
    auto missC = readMemo(memoC), missF = readMemo(memoF);
    bool any = false;
    for (auto &g : games) {
        if (stop) break;
        bool needC = coverOf(g).empty() && !missC.count(g.key());
        bool needF = backdrops && coverOf(g, "-fundo").empty() && !missF.count(g.key());
        if (!needC && !needF) continue;
        auto where = candidates(g);
        if (where.empty()) continue;
        std::string dir = g.dir + "/images";
        mkdirs(dir);
        if (needC) {
            if (fetchAny(where, COVER, dir + "/" + g.base + ".png")) any = true;
            else addMemo(memoC, g.key());
        }
        if (needF) {
            if (fetchAny(where, BACKDROP, dir + "/" + g.base + "-fundo.png")) any = true;
            else addMemo(memoF, g.key());
        }
    }
    return any;
}

}  // namespace Capas
