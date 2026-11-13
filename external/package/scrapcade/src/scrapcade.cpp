// ScrapCade para o T-OS - versao 0.7 (conversao do ScrapCade do iChroma para C++/SDL2)
//
// Modo jogo do T-OS: carrossel de capas com perspectiva e reflexo, filtros LB/RB (Todos, Favoritos,
// Mais jogados e cada console), barra de cima com logo do console, emulador, filtro, hora, data e IP,
// faixa com o nome do jogo e barra de botoes. Mesmas medidas e cores do iChroma (Carousel.java,
// CarouselGL.java, Theme.java, Home.java, HintBar.java).
//
// Jogos: ROMS_DIR/<console>/ (padrao /userdata/roms). Capas: <console>/images/<jogo>.png ou .jpg.
// Fundo do jogo: <console>/images/<jogo>-fundo.png. Licenca: GPLv3.
//
// Fase B: dados dos jogos pelo CRC (meta.h), capas e fundos baixados do acervo libretro (capas.h) e
// organizacao automatica da pasta "Jogos novos" (organizar.h), tudo numa thread de fundo.
#include "gfx.h"
#include "catalogo.h"
#include "meta.h"
#include "capas.h"
#include "organizar.h"
#include "envio.h"

#include <algorithm>
#include <arpa/inet.h>
#include <csignal>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <ifaddrs.h>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <sstream>
#include <strings.h>
#include <sys/stat.h>
#include <mutex>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/reboot.h>
#include <unistd.h>
#include <thread>


// ------------------------------------------------------------------ preferencias (favoritos, jogados, tema)

static std::map<std::string, std::string> prefs;

static void loadPrefs() {
    std::ifstream f(CONF);
    std::string l;
    while (std::getline(f, l)) {
        size_t p = l.find('=');
        if (p != std::string::npos) prefs[l.substr(0, p)] = l.substr(p + 1);
    }
}

static void savePrefs() {
    mkdirs(CONF.substr(0, CONF.rfind('/')));
    std::string tmp = CONF + ".tmp";
    {
        std::ofstream f(tmp);
        for (auto &kv : prefs) f << kv.first << "=" << kv.second << "\n";
    }
    rename(tmp.c_str(), CONF.c_str());
}

static bool isFav(const Game &g) { return prefs.count("fav|" + g.key()) > 0; }
static int plays(const Game &g) {
    auto it = prefs.find("plays|" + g.key());
    return it == prefs.end() ? 0 : atoi(it->second.c_str());
}

// ------------------------------------------------------------------ temas (Theme.java)

enum { CARBONO, NEON, ARCADE, CLARO, AURORA, NTEMAS };
static const char *THEME_NAMES[NTEMAS] = {"Carbono", "Neon", "Arcade", "Claro", "Aurora"};
static int theme = AURORA;

static uint32_t accent(uint32_t col) { return theme == AURORA ? 0xFF32E6A8 : lighten(col, 0.35f); }
static uint32_t band(uint32_t col) {
    if (theme == CLARO || theme == AURORA) return col;
    if (theme == NEON) return darken(col, 0.55f);
    if (theme == ARCADE) return blend(col, 0xFFFF3EA5, 0.45f);
    return darken(col, 0.30f);
}
static uint32_t cardText(uint32_t col) {
    if (theme == CLARO) return darken(col, 0.25f);
    if (theme == NEON) return lighten(col, 0.55f);
    return 0xFFFFFFFF;
}

// ------------------------------------------------------------------ estado da tela

static Gfx G;
static Text T;
static std::unordered_map<std::string, SDL_Texture *> images;   // logos, botoes, fundo
static std::unordered_map<std::string, SDL_Texture *> cards;    // capas prontas por jogo
static std::unordered_map<std::string, SDL_Texture *> backs;    // fundos por cor/tema

static SDL_Texture *image(const std::string &path) {
    auto it = images.find(path);
    if (it != images.end()) return it->second;
    SDL_Texture *t = IMG_LoadTexture(G.r, path.c_str());
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    images[path] = t;
    return t;
}

static SDL_Texture *whiteLogo(const Sys *s) {
    if (!s || s->logo.empty()) return nullptr;
    return image(DATA + "/logos_brancos/" + s->logo + ".png");
}

// Logo branco do console; sem logo, o nome escrito em negrito branco (Art.drawWhiteLogo)
static void drawWhiteLogo(const Sys *s, float x, float y, float w, float h, bool center) {
    if (SDL_Texture *t = whiteLogo(s)) {
        G.imageFit(t, x, y, w, h, center);
        return;
    }
    std::string n = s->name;
    for (auto &c : n) c = char(toupper((unsigned char)c));
    float units = h / G.u * 0.78f;
    while (units > 3 && T.width(n, units, true) > w) units *= 0.92f;
    T.draw(n, center ? x + w / 2 : x, y + h / 2, units, true, 0xFFFFFFFF, center ? 1 : 0);
}

// ------------------------------------------------------------------ medidas (Carousel.java)

static const float TITLE_H = 30, HINT_H = 22, TOP_H = 27, SIDE_SCALE = 0.68f;
static const int SIDE = 4;
static float areaH() { return G.H - (TITLE_H + HINT_H) * G.u; }
static float coverH() { return areaH() * 0.66f; }
static float coverW() { return std::min(G.W * 0.24f, coverH() * 0.72f); }
static float coverY() { return areaH() * 0.485f; }
static float coverScale(float a) { return 1 - (1 - SIDE_SCALE) * a; }
static float coverDrop(float a) { return coverH() * (1 - SIDE_SCALE) * 0.30f * a; }
static float coverX(float off) {
    float a = std::fabs(off), cw = coverW();
    float near_ = cw * (1 + SIDE_SCALE) / 2 + G.W * 0.010f, far_ = cw * SIDE_SCALE + G.W * 0.008f;
    float x = std::min(a, 1.0f) * near_ + std::max(a - 1, 0.0f) * far_;
    return off < 0 ? -x : x;
}
static float margin() { return float(int(3 * G.u)); }

// ------------------------------------------------------------------ fundo (Theme.background)

static SDL_Texture *background(uint32_t col) {
    std::string key = std::to_string(theme) + "|" + std::to_string(theme == AURORA ? 0 : col);
    auto it = backs.find(key);
    if (it != backs.end()) return it->second;
    SDL_Texture *t = SDL_CreateTexture(G.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, G.W, G.H);
    SDL_SetRenderTarget(G.r, t);
    float W = float(G.W), H = float(G.H), d = G.u;
    switch (theme) {
    case AURORA: {
        G.fillRect(0, 0, W, H, 0xFF0B1660);
        if (SDL_Texture *f = image(DATA + "/fundo.jpg")) {
            SDL_FRect r{0, 0, W, H};
            SDL_RenderCopyF(G.r, f, nullptr, &r);
        }
        break;
    }
    case NEON:
        G.fillRect(0, 0, W, H, 0xFF020205);
        G.radial(W / 2, H * 0.47f, W * 0.55f, W * 0.55f, alpha(col, 80));
        for (float x = 0; x < W; x += 32 * d) G.fillRect(x, 0, 1, H, alpha(lighten(col, 0.3f), 26));
        for (float y = 0; y < H; y += 32 * d) G.fillRect(0, y, W, 1, alpha(lighten(col, 0.3f), 26));
        break;
    case ARCADE:
        G.gradientV(0, 0, W, H * 0.62f, 0xFF12052A, blend(0xFF5B1466, col, 0.25f));
        G.gradientV(0, H * 0.62f, W, H * 0.38f, 0xFF1A0631, 0xFF050208);
        for (int i = 0; i < 7; i++) {
            float y = H * 0.62f - i * 9 * d;
            float half = std::sqrt(std::max(0.0, 1 - std::pow(i / 7.0, 2))) * W * 0.16f;
            G.fillRect(W / 2 - half, y - 3 * d, 2 * half, 3 * d, 0x55FF7AC8);
        }
        for (int i = 0; i < 8; i++) {
            float t2 = i / 8.0f, y = H * 0.62f + H * 0.38f * t2 * t2;
            G.fillRect(0, y, W, 1.5f * d, 0x66FF3EA5);
        }
        break;
    case CLARO:
        G.gradientV(0, 0, W, H, 0xFFF4F6F9, blend(0xFFF4F6F9, col, 0.18f));
        break;
    default:
        G.gradientV(0, 0, W, H, darken(col, 0.45f), 0xFF06070A);
        G.radial(W / 2, H * 0.47f, W * 0.42f, W * 0.42f, alpha(lighten(col, 0.25f), 110));
        break;
    }
    SDL_SetRenderTarget(G.r, nullptr);
    backs[key] = t;
    return t;
}

// ------------------------------------------------------------------ capa (Carousel.drawCover)

struct Card { SDL_Texture *t; float top, side; };
static std::unordered_map<std::string, Card> cardInfo;
static bool showCovers = true;

// Nome do jogo centrado na capa sem imagem, em ate 5 linhas (Carousel.drawTitle)
static void drawCardTitle(const std::string &t, float l, float top, float r, float bottom, uint32_t col) {
    float w = (r - l) * 0.88f, avail = bottom - top, size = (r - l) * 0.125f / G.u;
    std::vector<std::string> words;
    std::stringstream ss(t);
    std::string wd;
    while (ss >> wd) words.push_back(wd);
    float longest = 0;
    for (auto &x : words) longest = std::max(longest, float(T.width(x, size, true)));
    if (longest > w) size *= w / longest;
    std::vector<std::string> lines;
    std::string cur;
    for (auto &x : words) {
        std::string next = cur.empty() ? x : cur + " " + x;
        if (T.width(next, size, true) <= w || cur.empty()) cur = next;
        else { lines.push_back(cur); cur = x; }
    }
    if (!cur.empty()) lines.push_back(cur);
    int maxl = std::max(1, std::min(5, int(avail / (size * G.u * 1.2f))));
    while (int(lines.size()) > maxl) {
        std::string a = lines.back();
        lines.pop_back();
        lines.back() += " " + a;
    }
    float lh = size * G.u * 1.2f, y = top + (avail - lh * lines.size()) / 2 + lh / 2;
    for (auto &s : lines) {
        T.draw(s, (l + r) / 2, y, size, true, col, 1, w);
        y += lh;
    }
}

static Card &card(const Game &g) {
    std::string key = g.key() + "|" + std::to_string(theme) + (showCovers ? "" : "|s");
    auto it = cardInfo.find(key);
    if (it != cardInfo.end()) return it->second;
    float cw = coverW(), ch = coverH(), m = margin(), u = G.u;
    int bw = int(cw + 2 * m), bh = int(ch + 2 * m);
    SDL_Texture *t = SDL_CreateTexture(G.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, bw, bh);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(G.r, t);
    SDL_SetRenderDrawColor(G.r, 0, 0, 0, 0);
    SDL_RenderClear(G.r);
    const Sys *s = g.sys;
    float rad = 2 * u, bandH = ch * 0.075f, inset = u, top = 0, side = 0;
    SDL_Texture *art = nullptr;
    int aw = 0, ah = 0;
    if (showCovers) {
        std::string p = coverOf(g);
        if (!p.empty()) art = IMG_LoadTexture(G.r, p.c_str());
        if (art) SDL_QueryTexture(art, nullptr, nullptr, &aw, &ah);
    }
    if (art && aw > 0 && ah > 0) {
        // a caixa segue o formato da propria capa, sem esticar nem cortar, apoiada no chao
        float maxW = cw - 2 * inset, maxH = ch - bandH - inset, ar = float(aw) / ah;
        float artW = maxW, artH = maxW / ar;
        if (artH > maxH) { artH = maxH; artW = maxH * ar; }
        top = ch - bandH - artH - inset;
        side = (maxW - artW) / 2;
    }
    float x0 = m + side, y0 = m + top, w = cw - 2 * side, h = ch - top;
    // corpo do cartao, por tema (Theme.card)
    switch (theme) {
    case AURORA:
        G.roundRect(x0, y0, w, h, rad, 0xFF15171D);
        G.roundRectStroke(x0, y0, w, h, rad, u, lighten(s->color, 0.15f));
        break;
    case NEON:
        G.roundRect(x0, y0, w, h, rad, 0xE6050508);
        G.roundRectStroke(x0 - 2 * u, y0 - 2 * u, w + 4 * u, h + 4 * u, rad + 2 * u, 4 * u, alpha(lighten(s->color, 0.35f), 70));
        G.roundRectStroke(x0, y0, w, h, rad, 2.5f * u, lighten(s->color, 0.45f));
        break;
    case CLARO:
        G.roundRect(x0, y0 + 4 * u, w, h, rad, 0x22000000);
        G.roundRect(x0, y0, w, h, rad, 0xFFFFFFFF);
        G.fillRect(x0 + rad, y0 + h - 3 * u, w - 2 * rad, 3 * u, s->color);
        break;
    case ARCADE:
        G.roundRect(x0, y0, w, h, rad, darken(s->color, 0.2f));
        G.roundRectStroke(x0, y0, w, h, rad, 2.5f * u, 0xFFFFD34E);
        break;
    default:
        G.roundRect(x0, y0 + 7 * u, w, h, rad, 0x66000000);
        G.roundRect(x0, y0, w, h, rad, s->color);
        G.roundRectStroke(x0, y0, w, h, rad, 2 * u, 0xA6FFFFFF);
        break;
    }
    // faixa do console no alto da capa, como a lombada de uma caixa de jogo
    G.roundRect(x0, y0, w, bandH + rad, rad, band(s->color));
    G.fillRect(x0, y0 + bandH, w, rad, theme == CLARO ? 0xFFFFFFFF : (theme == AURORA ? 0xFF15171D : s->color));
    drawWhiteLogo(s, m + cw / 2 - w * 0.40f, y0 + bandH * 0.20f, w * 0.80f, bandH * 0.60f, true);
    float ax = x0 + inset, ay = y0 + bandH, aW = w - 2 * inset, aH = h - bandH - inset;
    if (art) {
        SDL_FRect d{ax, ay, aW, aH};
        SDL_RenderCopyF(G.r, art, nullptr, &d);
        SDL_DestroyTexture(art);
    } else {
        drawCardTitle(g.title, ax, ay, ax + aW, ay + aH, cardText(s->color));
    }
    SDL_SetRenderTarget(G.r, nullptr);
    cards[key] = t;
    cardInfo[key] = Card{t, top, side};
    return cardInfo[key];
}

static void clearCards() {
    for (auto &kv : cardInfo) SDL_DestroyTexture(kv.second.t);
    cardInfo.clear();
    cards.clear();
}

static void clearBacks() {
    for (auto &kv : backs) SDL_DestroyTexture(kv.second);
    backs.clear();
}

// ------------------------------------------------------------------ carrossel (CarouselGL.drawCover)

static std::vector<Game> all, view;
static float pos = 0, animFrom = 0;
static int target = 0;
static Uint32 animStart = 0, lastMove = 0;
static bool animating = false;
static Uint32 ANIM_MS = 200;   // 130 com animacoes rapidas

static int mod(int a, int n) { int m = a % n; return m < 0 ? m + n : m; }
static const Game *current() { return view.empty() ? nullptr : &view[mod(target, int(view.size()))]; }

static void quad(SDL_Texture *t, float x0, float x1, float y0t, float y0b, float y1t, float y1b,
                 uint32_t c0, uint32_t c1, float u0, float u1, float v0, float v1) {
    SDL_Color a = cor(c0), b = cor(c1);
    SDL_Vertex v[4] = {{{x0, y0t}, a, {u0, v0}}, {{x1, y1t}, a, {u1, v0}},
                       {{x1, y1b}, b, {u1, v1}}, {{x0, y0b}, b, {u0, v1}}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(G.r, t, v, 4, idx, 6);
}

static void drawCover(const Game &g, float off) {
    float a = std::min(1.0f, std::fabs(off));
    float scale = coverScale(a), k = 0.05f * a, thin = 1 - 0.03f * a, dim = 1 - 0.12f * a;
    float s0 = off > 0 ? 1 + k : 1 - k, s1 = off > 0 ? 1 - k : 1 + k;
    float m = margin(), cw = coverW(), ch = coverH(), u = G.u;
    float x = G.W / 2.0f + coverX(off), cy = coverY() + coverDrop(a);
    float hw = (cw + 2 * m) / 2 * scale * thin, hh = (ch + 2 * m) / 2 * scale;
    float x0 = x - hw, x1 = x + hw;
    if (x1 < 0 || x0 > G.W) return;
    Card &c = card(g);
    float bw = cw + 2 * m, bh = ch + 2 * m;
    float y0t = cy - hh * s0, y0b = cy + hh * s0, y1t = cy - hh * s1, y1b = cy + hh * s1;
    // reflexo: a parte de baixo da capa, de ponta-cabeca, sumindo
    float rh = ch * 0.24f, gap = 1.5f * u, mu = (m + c.side) / bw, mv = m / bh, vb = 1 - mv;
    float rx0 = x0 + (m + c.side) * scale * thin, rx1 = x1 - (m + c.side) * scale * thin;
    float yb0 = y0b - m * scale * s0 + gap * s0, yb1 = y1b - m * scale * s1 + gap * s1;
    int ra = int(0.38f * dim * 255);
    quad(c.t, rx0, rx1, yb0, yb0 + rh * scale * s0, yb1, yb1 + rh * scale * s1, alpha(0xFFFFFF, ra),
         0x00FFFFFF, mu, 1 - mu, vb, vb - rh / bh);
    if (a < 0.98f) {
        // borda da selecionada: aparece conforme a capa chega ao centro
        uint32_t ac = accent(g.sys->color);
        float on = 1 - a, e = 2 * u;
        float px = (m - e + c.side) * scale * thin, py = (m - e) * scale, down = c.top * scale;
        uint32_t col = alpha(ac, int(on * 255));
        quad(nullptr, x0 + px, x1 - px, y0t + (py + down) * s0, y0b - py * s0, y1t + (py + down) * s1,
             y1b - py * s1, col, col, 0, 0, 0, 0);
    }
    int lum = int(dim * 255);
    uint32_t tint = 0xFF000000 | uint32_t(lum) << 16 | uint32_t(lum) << 8 | uint32_t(lum);
    quad(c.t, x0, x1, y0t, y0b, y1t, y1b, tint, tint, 0, 1, 0, 1);
}

// Fundo do jogo: cena borrada e escurecida quando o carrossel para
static std::string backdropKey;
static SDL_Texture *backdrop = nullptr;
static bool showBackdrop = true;

static void loadBackdrop(const Game *g) {
    std::string key = g ? g->key() : "";
    if (key == backdropKey) return;
    backdropKey = key;
    if (backdrop) SDL_DestroyTexture(backdrop);
    backdrop = nullptr;
    if (!g || !showBackdrop) return;
    std::string p = coverOf(*g, "-fundo");
    if (p.empty()) return;
    SDL_Surface *raw = IMG_Load(p.c_str());
    if (!raw) return;
    // as cenas do acervo costumam vir com paleta (4 ou 8 bits): converte antes de reduzir
    SDL_Surface *src = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(raw);
    if (!src) return;
    float ar = float(G.W) / G.H, sw = float(src->w), sh = float(src->h);
    float cw = std::min(sw, sh * ar), ch = cw / ar;
    SDL_Rect crop{int((sw - cw) / 2), int((sh - ch) / 2), int(cw), int(ch)};
    SDL_Surface *tiny = SDL_CreateRGBSurfaceWithFormat(0, 56, 34, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_BlitScaled(src, &crop, tiny, nullptr);
    SDL_FreeSurface(src);
    backdrop = SDL_CreateTextureFromSurface(G.r, tiny);
    SDL_FreeSurface(tiny);
    if (backdrop) SDL_SetTextureBlendMode(backdrop, SDL_BLENDMODE_BLEND);
}

// ------------------------------------------------------------------ filtros (Home.buildFilters)

static const std::string F_ALL = "", F_FAV = "*fav", F_TOP = "*top";
static std::vector<std::string> filters;
static std::string filter = F_ALL;

static void buildFilters() {
    filters = {F_ALL};
    bool fav = false, top = false;
    for (auto &g : all) { if (isFav(g)) fav = true; if (plays(g) > 0) top = true; }
    if (fav) filters.push_back(F_FAV);
    if (top) filters.push_back(F_TOP);
    for (auto &s : SYSTEMS)
        for (auto &g : all) if (g.sys == &s) { filters.push_back(s.id); break; }
    if (std::find(filters.begin(), filters.end(), filter) == filters.end()) filter = F_ALL;
}

static std::string filterName(const std::string &id) {
    if (id == F_FAV) return "Favoritos";
    if (id == F_TOP) return "Mais jogados";
    for (auto &s : SYSTEMS) if (s.id == id) return s.name;
    return "Todos";
}

static void applyFilter(int keepIndex = -1) {
    view.clear();
    for (auto &g : all) {
        if (filter == F_FAV && !isFav(g)) continue;
        if (filter == F_TOP && plays(g) <= 0) continue;
        if (!filter.empty() && filter[0] != '*' && g.sys->id != filter) continue;
        view.push_back(g);
    }
    if (filter == F_TOP)
        std::stable_sort(view.begin(), view.end(), [](const Game &a, const Game &b) { return plays(a) > plays(b); });
    target = keepIndex >= 0 && !view.empty() ? std::min(keepIndex, int(view.size()) - 1) : 0;
    pos = float(target);
    animating = false;
}

// ------------------------------------------------------------------ barras (Home.buildMain, HintBar)

static std::string ipAddr() {
    std::string ip = "sem rede";
    ifaddrs *ifs = nullptr;
    if (getifaddrs(&ifs) == 0) {
        for (ifaddrs *i = ifs; i; i = i->ifa_next) {
            if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
            char b[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((sockaddr_in *)i->ifa_addr)->sin_addr, b, sizeof b);
            if (strncmp(b, "127.", 4)) { ip = b; break; }
        }
        freeifaddrs(ifs);
    }
    return ip;
}

static void pill(const std::string &label, float x, float cy, float &outW) {
    float u = G.u, w = T.width(label, 7.5f, true) + 10 * u, h = T.get(label, 7.5f, true, 0xFF202329).h + 3 * u;
    G.roundRect(x, cy - h / 2, w, h, 4 * u, 0xEEFFFFFF);
    T.draw(label, x + w / 2, cy, 7.5f, true, 0xFF202329, 1);
    outW = w;
}

static void topBar(const Game *g) {
    float u = G.u, h = TOP_H * u, cy = h / 2;
    G.fillRect(0, 0, float(G.W), h, 0xB8303338);
    float x = 10 * u;
    if (g) drawWhiteLogo(g->sys, x, cy - 8.5f * u, 92 * u, 17 * u, false);
    x += 92 * u + 8 * u;
    G.fillRect(x, cy - 7 * u, std::max(1.0f, 0.7f * u), 14 * u, 0x55FFFFFF);
    x += 7 * u + std::max(1.0f, 0.7f * u);
    if (g) T.draw(g->sys->emu, x, cy, 8, false, 0xCCFFFFFF, 0, G.W * 0.30f - x);
    // filtro no meio
    if (filters.size() >= 2) {
        std::string f = filterName(filter) + "  (" + std::to_string(view.size()) + ")";
        float fw = T.width(f, 10.5f, true) + 14 * u, lb = T.width("LB", 7.5f, true) + 10 * u,
              rb = T.width("RB", 7.5f, true) + 10 * u;
        float sx = G.W / 2.0f - (lb + fw + rb) / 2, w1;
        pill("LB", sx, cy, w1);
        T.draw(f, sx + lb + fw / 2, cy, 10.5f, true, 0xFFFFFFFF, 1);
        pill("RB", sx + lb + fw, cy, w1);
    }
    // hora, data e IP a direita
    time_t now = time(nullptr);
    tm lt;
    localtime_r(&now, &lt);
    char hora[8], data[32];
    strftime(hora, sizeof hora, "%H:%M", &lt);
    static const char *DIAS[] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};
    snprintf(data, sizeof data, "%s, %02d/%02d", DIAS[lt.tm_wday], lt.tm_mday, lt.tm_mon + 1);
    static std::string ip;
    static Uint32 ipAt = 0;
    if (!ipAt || SDL_GetTicks() - ipAt > 5000) { ip = ipAddr(); ipAt = SDL_GetTicks(); }
    float rx = G.W - 10 * u;
    T.draw(hora, rx, cy - 3.6f * u, 12.5f, true, 0xFFFFFFFF, 2);
    T.draw(std::string(data) + "  •  " + ip, rx, cy + 7.2f * u, 7, false, 0xBBFFFFFF, 2);
}

static void titleBar(const Game *g) {
    float u = G.u, h = TITLE_H * u, y = G.H - (TITLE_H + HINT_H) * u;
    G.fillRect(0, y, float(G.W), h, 0xB8303338);
    if (!g) return;
    std::string t = (isFav(*g) ? "★  " : "") + g->title;
    T.draw(t, G.W / 2.0f, y + h * 0.36f, 13.5f, false, 0xFFFFFFFF, 1, G.W - 32 * u);
    std::string meta = Meta::line(*g);
    std::string sub = (meta.empty() ? "" : meta + "     ") + std::to_string(mod(target, int(view.size())) + 1) + " de " +
                      std::to_string(view.size());
    T.draw(sub, G.W / 2.0f, y + h * 0.78f, 8.5f, false, 0xDDFFFFFF, 1);
}

static void hintBar() {
    float u = G.u, h = HINT_H * u, y = G.H - h;
    bool white = theme == AURORA;
    G.fillRect(0, y, float(G.W), h, white ? 0xF7FFFFFF : 0xA0000000);
    static const char *H[] = {"lr", "escolher", "a", "jogar", "x", "favorito", "y", "detalhes",
                              "lb rb", "filtrar", "select", "sistema"};
    float x = 12 * u, cy = y + h / 2, s = 15.5f * u;
    for (int i = 0; i < 12; i += 2) {
        std::stringstream ss(H[i]);
        std::string k;
        while (ss >> k) {
            if (SDL_Texture *t = image(DATA + "/botoes/" + k + ".png")) {
                SDL_FRect d{x, cy - s / 2, s, s};
                SDL_RenderCopyF(G.r, t, nullptr, &d);
            }
            x += s + u;
        }
        T.draw(H[i + 1], x + 2 * u, cy, 9.5f, false, white ? 0xFF5E6269 : 0xFFE6E9F0);
        x += 2 * u + T.width(H[i + 1], 9.5f, false) + 13 * u;
    }
}

static std::string toastMsg;
static Uint32 toastUntil = 0;
static void toast(const std::string &s, Uint32 ms = 2600) { toastMsg = s; toastUntil = SDL_GetTicks() + ms; }

static void drawToast() {
    if (toastMsg.empty() || SDL_GetTicks() > toastUntil) return;
    float u = G.u, maxw = G.W - 40 * u, w = std::min(maxw, T.width(toastMsg, 10, false) + 24 * u), h = 22 * u;
    float x = (G.W - w) / 2, y = G.H - (TITLE_H + HINT_H) * u - h - 8 * u;
    G.roundRect(x, y, w, h, 6 * u, 0xE6202329);
    T.draw(toastMsg, G.W / 2.0f, y + h / 2, 10, false, 0xFFFFFFFF, 1, w - 16 * u);
}

static void drawEmpty() {
    float u = G.u, cy = areaH() * 0.45f;
    std::string web = "http://" + ipAddr() + ":" + std::to_string(Envio::PORT);
    const char *L[] = {"Nenhum jogo ainda.", "",
                       "No navegador do PC ou do celular, na mesma rede, abra:", web.c_str(), "",
                       "e arraste os jogos. Eles vão sozinhos para o console certo, com capa e dados."};
    for (int i = 0; i < 6; i++) {
        uint32_t c = i == 3 ? 0xFFFFD479 : 0xFFFFFFFF;
        T.draw(L[i], G.W / 2.0f, cy + (i - 2.5f) * 16 * u, i == 0 ? 14 : 10, i == 0, c, 1);
    }
}

// ------------------------------------------------------------------ detalhes (Y), como o Home.showDetails

static bool details = false;

static std::string sizeText(long long b) {
    char s[32];
    if (b >= 1024LL * 1024 * 1024) snprintf(s, sizeof s, "%.1f GB", b / 1073741824.0);
    else if (b >= 1024 * 1024) snprintf(s, sizeof s, "%.1f MB", b / 1048576.0);
    else snprintf(s, sizeof s, "%lld KB", (b + 1023) / 1024);
    return s;
}

static void drawDetails() {
    const Game *g = current();
    if (!details || !g) return;
    float u = G.u;
    G.fillRect(0, 0, float(G.W), float(G.H), 0xA0000000);
    std::vector<std::string> L;
    L.push_back("Console: " + g->sys->name + "     Emulador: " + g->sys->emu);
    Meta::Info m;
    bool has = Meta::get(g->key(), m);
    if (has && (!m.year.empty() || !m.dev.empty() || !m.genre.empty())) {
        std::string s;
        if (!m.year.empty()) s += "Ano: " + m.year + "     ";
        if (!m.dev.empty()) s += "Empresa: " + m.dev + "     ";
        if (!m.genre.empty()) s += "Gênero: " + Meta::genrePt(m.genre);
        L.push_back(s);
    } else {
        L.push_back(has ? "Ano, empresa e gênero: sem dados para este jogo" : "Procurando os dados do jogo...");
    }
    if (has && !m.name.empty() && m.name != g->title) L.push_back("Versão: " + m.name);
    L.push_back("");
    struct stat st;
    std::string f = "Arquivo: " + g->file;
    if (stat((g->dir + "/" + g->file).c_str(), &st) == 0) f += "  (" + sizeText(st.st_size) + ")";
    L.push_back(f);
    L.push_back("Pasta: " + g->dir);
    L.push_back("");
    int n = plays(*g);
    L.push_back(n == 0 ? "Ainda não jogado" : (n == 1 ? "Jogado 1 vez" : "Jogado " + std::to_string(n) + " vezes"));
    L.push_back(std::string("Favorito: ") + (isFav(*g) ? "sim" : "não") + "  (o botão X marca e desmarca)");
    float w = std::min(G.W - 40 * u, 430 * u), lh = 13 * u, h = 44 * u + L.size() * lh;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    G.roundRect(x, y, w, h, 8 * u, 0xF2202329);
    G.roundRectStroke(x, y, w, h, 8 * u, std::max(1.0f, u), alpha(accent(g->sys->color), 0xC0));
    T.draw(g->title, x + 14 * u, y + 16 * u, 13, true, 0xFFFFFFFF, 0, w - 28 * u);
    for (size_t i = 0; i < L.size(); i++)
        T.draw(L[i], x + 14 * u, y + 34 * u + i * lh, 9, false, 0xFFDDE1E8, 0, w - 28 * u);
    T.draw("B fecha", x + w - 14 * u, y + h - 10 * u, 8, false, 0x99FFFFFF, 2);
}

// ------------------------------------------------------------------ painel Sistema (Select), como o Home.buildOptions

enum { OPT_THEME, OPT_ANIM, OPT_COVERS, OPT_BG, OPT_ENGINE, OPT_DESKTOP, OPT_HELP, OPT_INFO, OPT_REBOOT, OPT_OFF, OPT_COUNT };
static bool sysOpen = false;
static bool restartMenu = false;
static const std::string DESKTOP = "/usr/bin/tos-desktop";
static int optSel = 0;
static std::string textTitle;                 // painel de texto (Como jogar, Informacoes)
static std::vector<std::string> textLines;
static int cpuPct = 0, memPct = 0, diskPct = 0;
static long long cpuTotal = 0, cpuIdle = 0;
static Uint32 barsAt = 0;
static std::string glName;                    // placa de video, lida uma vez

// Motor grafico: os desenhos que o SDL tem neste sistema (lidos na hora, so aparece o que existe)
static std::string engineNow;                 // o que esta desenhando agora
static std::string engineWant;                // escolhido no painel (vale ao reabrir)
static std::vector<std::string> engines() {
    std::vector<std::string> v;
    for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
        SDL_RendererInfo ri;
        if (SDL_GetRenderDriverInfo(i, &ri) == 0) v.push_back(ri.name);
    }
    return v;
}
static std::string engineName(const std::string &id) {
    if (id == "opengles2") return "GPU (OpenGL ES)";
    if (id == "opengles") return "GPU (OpenGL ES 1)";
    if (id == "opengl") return "GPU (OpenGL)";
    if (id == "vulkan") return "GPU (Vulkan)";
    if (id == "software") return "CPU (sem placa de vídeo)";
    return id;
}

static void readUsage() {
    std::ifstream st("/proc/stat");
    std::string cpu;
    long long v[8] = {0}, total = 0;
    st >> cpu;
    for (int i = 0; i < 8; i++) { st >> v[i]; total += v[i]; }
    long long idle = v[3] + v[4], dt = total - cpuTotal, di = idle - cpuIdle;
    cpuPct = dt > 0 ? int(100 - di * 100 / dt) : 0;
    cpuTotal = total;
    cpuIdle = idle;
    std::ifstream mi("/proc/meminfo");
    std::string k;
    long long val, memTotal = 0, memAvail = 0;
    std::string unit;
    while (mi >> k >> val >> unit) {
        if (k == "MemTotal:") memTotal = val;
        if (k == "MemAvailable:") memAvail = val;
    }
    memPct = memTotal > 0 ? int((memTotal - memAvail) * 100 / memTotal) : 0;
    struct statvfs fs;
    diskPct = statvfs(ROMS.c_str(), &fs) == 0 && fs.f_blocks ? int(100 - fs.f_bavail * 100 / fs.f_blocks) : 0;
}

static std::string optLabel(int i) {
    switch (i) {
    case OPT_THEME: return std::string("Tema:   ‹ ") + THEME_NAMES[theme] + " ›";
    case OPT_ANIM: return std::string("Animações:   ‹ ") + (ANIM_MS < 200 ? "Rápidas" : "Normais") + " ›";
    case OPT_COVERS: return std::string("Capas dos jogos:   ‹ ") + (showCovers ? "Mostrar" : "Ocultar") + " ›";
    case OPT_BG: return std::string("Fundo do jogo:   ‹ ") + (showBackdrop ? "Mostrar" : "Ocultar") + " ›";
    case OPT_ENGINE:
        return "Motor gráfico:   ‹ " + engineName(engineWant) + " ›" + (engineWant != engineNow ? "   A aplica" : "");
    case OPT_DESKTOP: return "Modo desktop";
    case OPT_HELP: return "Como jogar (botões)";
    case OPT_INFO: return "Informações do sistema";
    case OPT_REBOOT: return "Reiniciar";
    case OPT_OFF: return "Desligar";
    }
    return "";
}

static void drawBar(const std::string &name, int pct, float x, float y, float w) {
    float u = G.u, h = 7 * u;
    T.draw(name, x, y + h / 2, 8.5f, false, 0xFF5E6269);
    float bx = x + 46 * u, bw = w - 46 * u - 30 * u;
    G.roundRect(bx, y, bw, h, h / 2, 0xFFE3E5E8);
    if (pct > 0) G.roundRect(bx, y, std::max(h, bw * std::min(pct, 100) / 100.0f), h, h / 2, pct > 85 ? 0xFFE0564A : 0xFF2CCF9A);
    T.draw(std::to_string(pct) + "%", x + w, y + h / 2, 8.5f, true, 0xFF2B2E33, 2);
}

// Painel branco com faixa cinza no alto, como o whitePanel do iChroma. Devolve o y do corpo.
static float whitePanel(const std::string &title, float x, float y, float w, float h) {
    float u = G.u, th = 22 * u;
    G.fillRect(0, 0, float(G.W), float(G.H), 0x99000000);
    G.fillRect(x, y, w, h, 0xFFFFFFFF);
    G.fillRect(x, y, w, th, 0xFF4A4D52);
    T.draw(title, x + 12 * u, y + th / 2, 13, false, 0xFFFFFFFF);
    return y + th;
}

static void panelHints(const char *const *H, int n, float x, float y, float w) {
    float u = G.u, s = 13 * u, cx = x + 12 * u, cy = y - 11 * u;
    G.fillRect(x, y - 22 * u, w, 22 * u, 0xFFF1F2F4);
    for (int i = 0; i < n; i += 2) {
        std::stringstream ss(H[i]);
        std::string k;
        while (ss >> k) {
            if (SDL_Texture *t = image(DATA + "/botoes/" + k + ".png")) {
                SDL_FRect d{cx, cy - s / 2, s, s};
                SDL_RenderCopyF(G.r, t, nullptr, &d);
            }
            cx += s + u;
        }
        T.draw(H[i + 1], cx + 2 * u, cy, 8.5f, false, 0xFF5E6269);
        cx += 2 * u + T.width(H[i + 1], 8.5f, false) + 12 * u;
    }
}

static std::string ipAddr();

static void drawSystem() {
    if (!sysOpen) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 420 * u), rowH = 15 * u;
    float h = 22 * u + 10 * u + OPT_COUNT * (rowH + 1.6f * u) + 14 * u + 22 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel("Sistema", x, y, w, h) + 8 * u;
    float lx = x + 12 * u, lw = (w - 24 * u) * 0.53f, rx = lx + lw + 14 * u, rw = x + w - 12 * u - rx;
    T.draw("Opções", lx, top + 5 * u, 11, false, 0xFF2B2E33);
    for (int i = 0; i < OPT_COUNT; i++) {
        float ry = top + 13 * u + i * (rowH + 1.6f * u);
        G.fillRect(lx, ry, lw, rowH, i == optSel ? 0xFF2CCF9A : 0xFF4A4D52);
        T.draw(optLabel(i), lx + 8 * u, ry + rowH / 2, 9.5f, false, 0xFFFFFFFF, 0, lw - 16 * u);
    }
    float ry = top + 5 * u;
    T.draw("Endereço IP", rx, ry, 11, false, 0xFF2B2E33);
    T.draw(ipAddr(), rx, ry + 14 * u, 10, false, 0xFF5E6269, 0, rw);
    ry += 34 * u;
    T.draw("Enviar jogos do PC", rx, ry, 11, false, 0xFF2B2E33);
    T.draw("http://" + ipAddr() + ":" + std::to_string(Envio::PORT), rx, ry + 14 * u, 9.5f, false, 0xFF5E6269, 0, rw);
    ry += 34 * u;
    T.draw("Uso do sistema", rx, ry, 11, false, 0xFF2B2E33);
    if (SDL_GetTicks() - barsAt > 1000) { readUsage(); barsAt = SDL_GetTicks(); }
    drawBar("CPU", cpuPct, rx, ry + 12 * u, rw);
    drawBar("Memória", memPct, rx, ry + 25 * u, rw);
    drawBar("Disco", diskPct, rx, ry + 38 * u, rw);
    static const char *H[] = {"ud", "escolher", "lr", "mudar", "a", "abrir", "b", "fechar"};
    panelHints(H, 8, x, y + h, w);
}

static void drawTextPanel() {
    if (textTitle.empty()) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 460 * u), lh = 12.5f * u;
    float h = 22 * u + 16 * u + textLines.size() * lh + 22 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel(textTitle, x, y, w, h) + 12 * u;
    for (size_t i = 0; i < textLines.size(); i++)
        T.draw(textLines[i], x + 14 * u, top + i * lh + lh / 2, 9, false, 0xFF2B2E33, 0, w - 28 * u);
    static const char *H[] = {"b", "voltar"};
    panelHints(H, 2, x, y + h, w);
}

static std::string firstLine(const char *path, const char *key) {
    std::ifstream in(path);
    std::string l;
    while (std::getline(in, l))
        if (l.compare(0, strlen(key), key) == 0) {
            size_t c = l.find(':');
            std::string v = c == std::string::npos ? l : l.substr(c + 1);
            while (!v.empty() && (v[0] == ' ' || v[0] == '\t')) v.erase(0, 1);
            return v;
        }
    return "";
}

static void openInfo() {
    struct utsname un;
    uname(&un);
    long long kbTotal = 0, kbAvail = 0;
    {
        std::ifstream mi("/proc/meminfo");
        std::string k, unit;
        long long v;
        while (mi >> k >> v >> unit) {
            if (k == "MemTotal:") kbTotal = v;
            if (k == "MemAvailable:") kbAvail = v;
        }
    }
    struct statvfs fs;
    double freeG = 0, totG = 0;
    if (statvfs(ROMS.c_str(), &fs) == 0) {
        freeG = double(fs.f_bavail) * fs.f_frsize / 1073741824.0;
        totG = double(fs.f_blocks) * fs.f_frsize / 1073741824.0;
    }
    double up = 0;
    std::ifstream("/proc/uptime") >> up;
    std::string emus;
    for (auto &s : SYSTEMS)
        if (emus.find(s.emu) == std::string::npos) emus += (emus.empty() ? "" : ", ") + s.emu;
    std::string ver = firstLine("/etc/os-release", "VERSION=");
    ver.erase(std::remove(ver.begin(), ver.end(), '"'), ver.end());
    if (!ver.empty() && ver[0] == '=') ver.erase(0, 1);
    char buf[160];
    textTitle = "Informações do sistema";
    textLines.clear();
    textLines.push_back("Processador: " + firstLine("/proc/cpuinfo", "model name") + "  (" +
                        std::to_string(sysconf(_SC_NPROCESSORS_ONLN)) + " núcleos)");
    textLines.push_back("Vídeo: " + (glName.empty() ? std::string("desenho por software") : glName));
    snprintf(buf, sizeof buf, "Memória: %lld MB livres de %lld MB", kbAvail / 1024, kbTotal / 1024);
    textLines.push_back(buf);
    snprintf(buf, sizeof buf, "Disco dos jogos: %.1f GB livres de %.1f GB", freeG, totG);
    textLines.push_back(buf);
    textLines.push_back("Tela: " + std::to_string(G.W) + " x " + std::to_string(G.H) + "     Rede: " + ipAddr());
    snprintf(buf, sizeof buf, "  •  ligado há %d min", int(up / 60));
    textLines.push_back("Sistema: T-OS " + ver.substr(ver.find('=') + 1) + " (Linux " + un.release + ")  •  menu: ScrapCade 0.7" + buf);
    textLines.push_back("Jogos: " + std::to_string(all.size()) + ", em " + ROMS);
    textLines.push_back("Emuladores previstos: " + emus);
    textLines.push_back("");
    textLines.push_back("Botões: Xelu's Free Controller Prompts (CC0)  •  Logos: es-theme-carbon");
    textLines.push_back("Capas: libretro-thumbnails  •  Dados dos jogos: libretro-database e FBNeo");
}

static void openHelp() {
    textTitle = "Como jogar (botões)";
    textLines = {
        "No menu:",
        "   Direcional ou analógico: escolher o jogo        A: jogar",
        "   X: marcar ou desmarcar favorito        Y: detalhes do jogo",
        "   LB / RB: filtrar (Todos, Favoritos, Mais jogados, cada console)",
        "   Select: este painel Sistema",
        "",
        "Durante o jogo:",
        "   Select + Start: menu rápido (salvar, carregar, sair do jogo)",
        "",
        "No teclado: setas, Enter (jogar), F (favorito), D (detalhes),",
        "Q / E (filtrar), Tab (Sistema), Esc (voltar).",
        "",
        "Jogos novos: no navegador do PC ou celular, abra http://" + ipAddr() + ":" + std::to_string(Envio::PORT),
        "e arraste os jogos. Eles vão sozinhos para o console certo, com capa e dados.",
    };
}

static void powerOff(bool restart) {
    toast(restart ? "Reiniciando..." : "Desligando...", 10000);
    sync();
    // o init do BusyBox fecha os servicos e desmonta os discos antes de desligar
    if (system(restart ? "/sbin/reboot" : "/sbin/poweroff") != 0) reboot(restart ? RB_AUTOBOOT : RB_POWER_OFF);
}

static void clearCards();
static void clearBacks();

// dir: -1/+1 = mudar valor (esquerda/direita); 0 = confirmar (A)
static void optAction(int dir) {
    int d = dir == 0 ? 1 : dir;
    switch (optSel) {
    case OPT_THEME:
        theme = (theme + d + NTEMAS) % NTEMAS;
        prefs["theme"] = std::to_string(theme);
        clearCards();
        clearBacks();
        break;
    case OPT_ANIM:
        ANIM_MS = ANIM_MS < 200 ? 200 : 130;
        prefs["anim"] = ANIM_MS < 200 ? "rapida" : "normal";
        break;
    case OPT_COVERS:
        showCovers = !showCovers;
        prefs["covers"] = showCovers ? "1" : "0";
        clearCards();
        break;
    case OPT_BG:
        showBackdrop = !showBackdrop;
        prefs["backdrop"] = showBackdrop ? "1" : "0";
        backdropKey = "\x01";
        break;
    case OPT_ENGINE: {
        auto v = engines();
        if (v.empty()) return;
        if (dir != 0) {
            int i = int(std::find(v.begin(), v.end(), engineWant) - v.begin());
            engineWant = v[(i + dir + int(v.size()) * 2) % int(v.size())];
            return;
        }
        if (engineWant == engineNow) return;
        prefs["engine"] = engineWant;
        savePrefs();
        restartMenu = true;   // o laco principal fecha a tela e reabre o ScrapCade com o motor novo
        return;
    }
    case OPT_DESKTOP:
        if (dir != 0) return;
        // o desktop (Fluxbox, no estilo do antiX) chega como /usr/bin/tos-desktop; ao sair dele, volta o ScrapCade
        if (isFile(DESKTOP)) {
            prefs["voltar"] = "desktop";
            savePrefs();
            restartMenu = true;
        } else {
            toast("O modo desktop entra numa próxima versão do T-OS", 4000);
        }
        return;
    case OPT_HELP: if (dir == 0) { sysOpen = false; openHelp(); } return;
    case OPT_INFO: if (dir == 0) { sysOpen = false; openInfo(); } return;
    case OPT_REBOOT: if (dir == 0) powerOff(true); return;
    case OPT_OFF: if (dir == 0) powerOff(false); return;
    }
    savePrefs();
}

// Teclas e botoes enquanto um painel esta aberto. true = o painel tratou.
enum Btn { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_SELECT, B_OTHER };
static bool panelInput(Btn b) {
    if (!textTitle.empty()) {
        if (b == B_B || b == B_A || b == B_SELECT) { textTitle.clear(); sysOpen = true; }
        return true;
    }
    if (!sysOpen) return false;
    switch (b) {
    case B_UP: optSel = (optSel + OPT_COUNT - 1) % OPT_COUNT; break;
    case B_DOWN: optSel = (optSel + 1) % OPT_COUNT; break;
    case B_LEFT: optAction(-1); break;
    case B_RIGHT: optAction(1); break;
    case B_A: optAction(0); break;
    case B_B: case B_SELECT: sysOpen = false; break;
    default: break;
    }
    return true;
}

static void openSystem() {
    sysOpen = true;
    optSel = 0;
    readUsage();
    barsAt = SDL_GetTicks();
}

// ------------------------------------------------------------------ desenho da tela

static void render() {
    int n = int(view.size());
    Uint32 now = SDL_GetTicks();
    if (animating) {
        float t = float(now - animStart) / ANIM_MS;
        if (t >= 1) { t = 1; animating = false; }
        float e = 1 - (1 - t) * (1 - t) * (1 - t);
        pos = animFrom + (target - animFrom) * e;
    } else {
        pos = float(target);
    }
    const Game *g = current();
    // fundo do tema, misturando a cor dos dois consoles vizinhos durante o giro
    uint32_t c0 = 0xFF2B68C8, c1 = c0;
    float fr = 0;
    if (n) {
        int i0 = int(std::floor(pos));
        fr = pos - i0;
        c0 = view[mod(i0, n)].sys->color;
        c1 = view[mod(i0 + 1, n)].sys->color;
    }
    SDL_RenderCopy(G.r, background(c0), nullptr, nullptr);
    if (fr > 0.01f && c1 != c0 && theme != AURORA) {
        SDL_Texture *b1 = background(c1);
        SDL_SetTextureAlphaMod(b1, Uint8(fr * 255));
        SDL_RenderCopy(G.r, b1, nullptr, nullptr);
        SDL_SetTextureAlphaMod(b1, 255);
    }
    // fundo do jogo, aparecendo devagar depois que o carrossel para
    if (showBackdrop && g && !animating && now - lastMove > 250) {
        loadBackdrop(g);
        if (backdrop) {
            float t = std::min(1.0f, (now - lastMove - 250) / 300.0f);
            SDL_SetTextureAlphaMod(backdrop, Uint8(t * 255));
            SDL_RenderCopy(G.r, backdrop, nullptr, nullptr);
            G.fillRect(0, 0, float(G.W), float(G.H), alpha(0, int(t * 0x70)));
        }
    }
    if (n) {
        int base = int(std::lround(pos));
        for (int s = SIDE; s >= 0; s--)
            for (int sign = -1; sign <= 1; sign += 2) {
                if (s == 0 && sign > 0) continue;
                int k = s * sign;
                if (k < -(n - 1) / 2 || k > n / 2) continue;   // poucos jogos: sem capa repetida
                int i = base + k;
                drawCover(view[mod(i, n)], i - pos);
            }
        // guarda so as capas perto da selecionada
        if (cardInfo.size() > size_t(4 * SIDE + 6)) clearCards();
    } else {
        drawEmpty();
    }
    topBar(g);
    titleBar(g);
    hintBar();
    drawDetails();
    drawSystem();
    drawTextPanel();
    drawToast();
    SDL_RenderPresent(G.r);
}

// ------------------------------------------------------------------ acoes

static void move(int dir) {
    details = false;
    if (view.size() < 2) return;
    animFrom = pos;
    target += dir;
    animStart = lastMove = SDL_GetTicks();
    animating = true;
}

static void nextFilter(int dir) {
    if (filters.size() < 2) return;
    int i = int(std::find(filters.begin(), filters.end(), filter) - filters.begin());
    filter = filters[mod(i + dir, int(filters.size()))];
    applyFilter();
    lastMove = SDL_GetTicks();
}

static void toggleFav() {
    const Game *g = current();
    if (!g) return;
    std::string k = "fav|" + g->key();
    bool on = !isFav(*g);
    if (on) prefs[k] = "1"; else prefs.erase(k);
    savePrefs();
    int at = mod(target, int(view.size()));
    buildFilters();
    applyFilter(at);
    toast(on ? "Adicionado aos favoritos" : "Tirado dos favoritos");
}

static void play() {
    const Game *g = current();
    if (!g) return;
    toast("O " + g->sys->emu + " entra na próxima versão do T-OS");
}

// ------------------------------------------------------------------ principal

static volatile sig_atomic_t quit = 0;
static void onSignal(int) { quit = 1; }

// ------------------------------------------------------------------ trabalho de fundo (Capas.start do iChroma)

static Uint32 EV_TOAST = 0, EV_META = 0, EV_ART = 0, EV_WEB = 0;
static volatile bool workStop = false;
static std::mutex workMu;
static std::string workMsg;

static void post(Uint32 type) {
    SDL_Event e;
    SDL_zero(e);
    e.type = type;
    SDL_PushEvent(&e);
}

// Arcade e Neo Geo: o nome do zip ("mslug") vira o nome oficial
static void nameGames(std::vector<Game> &games) {
    for (auto &g : games) {
        if (g.sys->id != "arcade" && g.sys->id != "neogeo") continue;
        Meta::Info m;
        if (Meta::get(g.key(), m) && !m.name.empty()) g.title = titleOf(m.name);
    }
}

static void worker() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    long long netRetry = 0;
    while (!quit && !workStop) {
        std::string msg = Org::run();
        if (!msg.empty()) {
            { std::lock_guard<std::mutex> l(workMu); workMsg = msg; }
            post(EV_TOAST);
        }
        auto games = scanGames();
        if (Meta::fill(games, workStop)) post(EV_META);
        if (Org::nowMs() >= netRetry && getenv("SCRAPCADE_OFFLINE") == nullptr) {
            try {
                if (Capas::work(games, showBackdrop, workStop)) post(EV_ART);
            } catch (Capas::NetError &) {
                netRetry = Org::nowMs() + 60000;   // sem rede: tenta de novo em 1 minuto
                post(EV_ART);                      // o que ja baixou aparece
            }
        }
        for (int i = 0; i < 30 && !quit && !workStop; i++) SDL_Delay(100);
    }
    curl_global_cleanup();
}

// Lista nova de jogos, ficando no mesmo jogo se ele ainda existir
static void setGames(std::vector<Game> fresh) {
    nameGames(fresh);
    std::stable_sort(fresh.begin(), fresh.end(), [](const Game &a, const Game &b) {
        if (a.sys != b.sys) return a.sys < b.sys;
        return strcasecmp(a.title.c_str(), b.title.c_str()) < 0;
    });
    const Game *g = current();
    std::string keep = g ? g->key() : "";
    all = fresh;
    Envio::setGames(all);
    buildFilters();
    applyFilter();
    for (size_t i = 0; i < view.size(); i++)
        if (view[i].key() == keep) { target = int(i); pos = float(target); break; }
}

static std::string signature(const std::vector<Game> &games) {
    std::string s;
    for (auto &g : games) s += g.key() + "\n";
    return s;
}

int main() {
    if (getenv("ROMS_DIR")) ROMS = getenv("ROMS_DIR");
    if (getenv("SCRAPCADE_DATA")) DATA = getenv("SCRAPCADE_DATA");
    if (getenv("SCRAPCADE_CONF")) CONF = getenv("SCRAPCADE_CONF");
    if (getenv("INBOX_DIR")) INBOX = getenv("INBOX_DIR");
    signal(SIGTERM, onSignal);
    signal(SIGINT, onSignal);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    loadPrefs();
    if (!prefs["engine"].empty()) SDL_SetHint(SDL_HINT_RENDER_DRIVER, prefs["engine"].c_str());
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "scrapcade: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    SDL_Window *win = SDL_CreateWindow("ScrapCade", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
                                       SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) { fprintf(stderr, "scrapcade: janela: %s\n", SDL_GetError()); return 1; }
    G.r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!G.r) G.r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
    if (!G.r) { fprintf(stderr, "scrapcade: renderer: %s\n", SDL_GetError()); return 1; }
    SDL_RendererInfo info;
    SDL_GetRendererInfo(G.r, &info);
    engineNow = engineWant = info.name;
    SDL_GetRendererOutputSize(G.r, &G.W, &G.H);
    G.u = std::min(std::max(G.W, G.H) / 533.33f, std::min(G.W, G.H) / 320.0f);
    fprintf(stderr, "scrapcade: %dx%d, desenho %s, u=%.2f\n", G.W, G.H, info.name, G.u);
    SDL_ShowCursor(SDL_DISABLE);
    T.g = &G;
    T.regular = "/usr/share/fonts/dejavu/DejaVuSans.ttf";
    T.bold = "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf";

    loadPrefs();
    if (prefs.count("theme")) theme = std::max(0, std::min(NTEMAS - 1, atoi(prefs["theme"].c_str())));
    if (prefs["anim"] == "rapida") ANIM_MS = 130;
    if (prefs["covers"] == "0") showCovers = false;
    if (prefs["backdrop"] == "0") showBackdrop = false;
    if (std::string(info.name).find("opengl") != std::string::npos) {   // nome da placa, para as Informacoes
        typedef const unsigned char *(*GetStr)(unsigned);
        if (GetStr gs = (GetStr)SDL_GL_GetProcAddress("glGetString"))
            if (const unsigned char *r = gs(0x1F01)) glName = (const char *)r;   // GL_RENDERER
    }
    mkdirs(ROMS);
    setGames(scanGames());
    std::string sig = signature(scanGames());
    lastMove = SDL_GetTicks();
    EV_TOAST = SDL_RegisterEvents(4);
    EV_META = EV_TOAST + 1;
    EV_ART = EV_TOAST + 2;
    EV_WEB = EV_TOAST + 3;
    std::thread work(worker);
    Envio::changed = [] { post(EV_WEB); };
    Envio::start();

    Uint32 holdDir = 0, holdNext = 0, lastScan = SDL_GetTicks();
    int axisDir = 0;
    while (!quit && !restartMenu) {
        SDL_Event e;
        bool any = false;
        while (SDL_PollEvent(&e)) {
            any = true;
            switch (e.type) {
            case SDL_QUIT: quit = 1; break;
            default:
                if (e.type == EV_TOAST) {
                    std::string m;
                    { std::lock_guard<std::mutex> l(workMu); m = workMsg; }
                    toast(m, 6000);
                    lastScan = 0;   // procura os jogos de novo agora
                } else if (e.type == EV_META) {
                    setGames(scanGames());
                } else if (e.type == EV_WEB) {   // jogo, capa ou apagado pela pagina
                    clearCards();
                    backdropKey = "\x01";
                    lastScan = 0;
                } else if (e.type == EV_ART) {
                    clearCards();
                    backdropKey = "\x01";   // recarrega o fundo do jogo selecionado
                }
                break;
            case SDL_CONTROLLERDEVICEADDED: SDL_GameControllerOpen(e.cdevice.which); break;
            case SDL_KEYDOWN: {
                SDL_Keycode kc = e.key.keysym.sym;
                Btn pb = kc == SDLK_UP ? B_UP : kc == SDLK_DOWN ? B_DOWN : kc == SDLK_LEFT ? B_LEFT :
                         kc == SDLK_RIGHT ? B_RIGHT : (kc == SDLK_RETURN || kc == SDLK_SPACE) ? B_A :
                         (kc == SDLK_ESCAPE || kc == SDLK_BACKSPACE) ? B_B : kc == SDLK_TAB ? B_SELECT : B_OTHER;
                if (panelInput(pb)) break;
                switch (kc) {
                case SDLK_LEFT: move(-1); break;
                case SDLK_RIGHT: move(1); break;
                case SDLK_RETURN: case SDLK_SPACE: play(); break;
                case SDLK_f: toggleFav(); break;
                case SDLK_d: details = !details; break;
                case SDLK_ESCAPE: case SDLK_BACKSPACE: details = false; break;
                case SDLK_q: case SDLK_PAGEUP: nextFilter(-1); break;
                case SDLK_e: case SDLK_PAGEDOWN: nextFilter(1); break;
                case SDLK_TAB: details = false; openSystem(); break;
                }
                break;
            }
            case SDL_CONTROLLERBUTTONDOWN: {
                Uint8 cb = e.cbutton.button;
                Btn pb = cb == SDL_CONTROLLER_BUTTON_DPAD_UP ? B_UP : cb == SDL_CONTROLLER_BUTTON_DPAD_DOWN ? B_DOWN :
                         cb == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? B_LEFT : cb == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ? B_RIGHT :
                         cb == SDL_CONTROLLER_BUTTON_A ? B_A : cb == SDL_CONTROLLER_BUTTON_B ? B_B :
                         cb == SDL_CONTROLLER_BUTTON_BACK ? B_SELECT : B_OTHER;
                if (panelInput(pb)) break;
                switch (cb) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: move(-1); holdDir = Uint32(-1); holdNext = SDL_GetTicks() + 380; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: move(1); holdDir = 1; holdNext = SDL_GetTicks() + 380; break;
                case SDL_CONTROLLER_BUTTON_A: play(); break;
                case SDL_CONTROLLER_BUTTON_X: toggleFav(); break;
                case SDL_CONTROLLER_BUTTON_Y: details = !details; break;
                case SDL_CONTROLLER_BUTTON_B: details = false; break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: nextFilter(-1); break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: nextFilter(1); break;
                case SDL_CONTROLLER_BUTTON_BACK: details = false; openSystem(); break;
                }
                break;
            }
            case SDL_CONTROLLERBUTTONUP:
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT || e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
                    holdDir = 0;
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (sysOpen || !textTitle.empty()) {
                    static int ly = 0;
                    if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                        int d = e.caxis.value < -20000 ? -1 : (e.caxis.value > 20000 ? 1 : 0);
                        if (d != ly) { ly = d; if (d) panelInput(d < 0 ? B_UP : B_DOWN); }
                    }
                    break;
                }
                if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                    int d = e.caxis.value < -20000 ? -1 : (e.caxis.value > 20000 ? 1 : 0);
                    if (d != axisDir) {
                        axisDir = d;
                        if (d) { move(d); holdDir = Uint32(d); holdNext = SDL_GetTicks() + 380; }
                        else holdDir = 0;
                    }
                }
                break;
            }
        }
        Uint32 now = SDL_GetTicks();
        // segurando o direcional: anda sozinho
        if (holdDir && (sysOpen || !textTitle.empty())) holdDir = 0;
        if (holdDir && now >= holdNext) { move(int(holdDir)); holdNext = now + 90; any = true; }
        // jogos novos na pasta: procura de novo a cada 5 s
        if (now - lastScan > 5000) {
            lastScan = now;
            auto fresh = scanGames();
            std::string s2 = signature(fresh);
            if (s2 != sig) {
                sig = s2;
                setGames(fresh);
                any = true;
            }
        }
        bool backdropFading = showBackdrop && now - lastMove < 700;
        bool toastOn = !toastMsg.empty() && now <= toastUntil + 100;
        static int minute = -1;
        time_t tt = time(nullptr);
        int mnow = localtime(&tt)->tm_min;
        static Uint32 lastBars = 0;
        bool barsDue = sysOpen && now - lastBars > 1000;
        if (barsDue) lastBars = now;
        if (any || animating || backdropFading || toastOn || mnow != minute || barsDue) {
            minute = mnow;
            render();
        } else {
            SDL_Delay(30);
        }
    }
    workStop = true;
    if (restartMenu) {
        // troca de motor grafico: solta a tela e o processo vira um ScrapCade novo
        SDL_Quit();
        if (prefs["voltar"] == "desktop") {
            prefs.erase("voltar");
            savePrefs();
            fprintf(stderr, "scrapcade: abrindo o modo desktop\n");
            execl(DESKTOP.c_str(), "tos-desktop", (char *)nullptr);   // o script reabre o ScrapCade ao sair
        }
        fprintf(stderr, "scrapcade: reabrindo com o motor %s\n", prefs["engine"].c_str());
        execl("/proc/self/exe", "scrapcade", (char *)nullptr);
    }
    work.join();
    SDL_Quit();
    return 0;
}
