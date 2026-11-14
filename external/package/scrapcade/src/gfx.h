// ScrapCade - funcoes de desenho sobre o SDL2.
// Cores no formato 0xAARRGGBB, como no ScrapCade do iChroma. Licenca: GPLv3.
#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

static inline SDL_Color cor(uint32_t c) {
    return SDL_Color{Uint8(c >> 16), Uint8(c >> 8), Uint8(c), Uint8(c >> 24)};
}

static inline uint32_t blend(uint32_t a, uint32_t b, float t) {
    auto ch = [&](int s) {
        float x = float((a >> s) & 255), y = float((b >> s) & 255);
        return uint32_t(x + (y - x) * t + 0.5f) & 255;
    };
    return ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
}
static inline uint32_t lighten(uint32_t c, float t) { return blend(c, 0xFFFFFFFF, t); }
static inline uint32_t darken(uint32_t c, float t) { return blend(c, 0xFF000000, t); }
static inline uint32_t alpha(uint32_t c, int a) {
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    return (c & 0x00FFFFFF) | uint32_t(a) << 24;
}

struct Gfx {
    SDL_Renderer *r = nullptr;
    int W = 0, H = 0;
    float u = 1;   // unidade de tela: 1,5 pixel em 800x480 (igual ao iChroma)

    void fillRect(float x, float y, float w, float h, uint32_t c) {
        SDL_Color k = cor(c);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, k.r, k.g, k.b, k.a);
        SDL_FRect f{x, y, w, h};
        SDL_RenderFillRectF(r, &f);
    }

    // degrade vertical
    void gradientV(float x, float y, float w, float h, uint32_t top, uint32_t bottom) {
        SDL_Color a = cor(top), b = cor(bottom);
        SDL_Vertex v[4] = {{{x, y}, a, {0, 0}}, {{x + w, y}, a, {1, 0}},
                           {{x + w, y + h}, b, {1, 1}}, {{x, y + h}, b, {0, 1}}};
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
    }

    // brilho redondo: cor no centro sumindo para a borda
    void radial(float cx, float cy, float rx, float ry, uint32_t inner) {
        const int N = 48;
        std::vector<SDL_Vertex> v;
        std::vector<int> idx;
        SDL_Color in = cor(inner), out = in;
        out.a = 0;
        v.push_back({{cx, cy}, in, {0, 0}});
        for (int i = 0; i <= N; i++) {
            float a = float(i) / N * 6.2831853f;
            v.push_back({{cx + std::cos(a) * rx, cy + std::sin(a) * ry}, out, {0, 0}});
        }
        for (int i = 1; i <= N; i++) { idx.push_back(0); idx.push_back(i); idx.push_back(i + 1); }
        SDL_RenderGeometry(r, nullptr, v.data(), int(v.size()), idx.data(), int(idx.size()));
    }

    // contorno de retangulo arredondado (sentido horario)
    static void outline(std::vector<SDL_FPoint> &p, float x, float y, float w, float h, float rad) {
        p.clear();
        if (rad < 0) rad = 0;
        if (rad * 2 > w) rad = w / 2;
        if (rad * 2 > h) rad = h / 2;
        const int S = 8;
        float cx[4] = {x + w - rad, x + w - rad, x + rad, x + rad};
        float cy[4] = {y + rad, y + h - rad, y + h - rad, y + rad};
        float a0[4] = {-1.5707963f, 0, 1.5707963f, 3.1415926f};
        for (int c = 0; c < 4; c++)
            for (int i = 0; i <= S; i++) {
                float a = a0[c] + 1.5707963f * i / S;
                p.push_back({cx[c] + std::cos(a) * rad, cy[c] + std::sin(a) * rad});
            }
    }

    void roundRect(float x, float y, float w, float h, float rad, uint32_t c) {
        std::vector<SDL_FPoint> p;
        outline(p, x, y, w, h, rad);
        SDL_Color k = cor(c);
        std::vector<SDL_Vertex> v;
        std::vector<int> idx;
        v.push_back({{x + w / 2, y + h / 2}, k, {0, 0}});
        for (auto &q : p) v.push_back({q, k, {0, 0}});
        int n = int(p.size());
        for (int i = 0; i < n; i++) { idx.push_back(0); idx.push_back(1 + i); idx.push_back(1 + (i + 1) % n); }
        SDL_RenderGeometry(r, nullptr, v.data(), int(v.size()), idx.data(), int(idx.size()));
    }

    // borda de largura lw (por dentro do retangulo)
    void roundRectStroke(float x, float y, float w, float h, float rad, float lw, uint32_t c) {
        std::vector<SDL_FPoint> a, b;
        outline(a, x, y, w, h, rad);
        outline(b, x + lw, y + lw, w - 2 * lw, h - 2 * lw, rad - lw);
        SDL_Color k = cor(c);
        std::vector<SDL_Vertex> v;
        std::vector<int> idx;
        int n = int(a.size());
        for (int i = 0; i < n; i++) { v.push_back({a[i], k, {0, 0}}); v.push_back({b[i], k, {0, 0}}); }
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            idx.insert(idx.end(), {2 * i, 2 * j, 2 * i + 1, 2 * j, 2 * j + 1, 2 * i + 1});
        }
        SDL_RenderGeometry(r, nullptr, v.data(), int(v.size()), idx.data(), int(idx.size()));
    }

    // textura num retangulo, cabendo inteira e centrada
    void imageFit(SDL_Texture *t, float x, float y, float w, float h, bool center = true) {
        int tw, th;
        if (!t || SDL_QueryTexture(t, nullptr, nullptr, &tw, &th)) return;
        float k = std::min(w / tw, h / th), dw = tw * k, dh = th * k;
        SDL_FRect d{center ? x + (w - dw) / 2 : x, y + (h - dh) / 2, dw, dh};
        SDL_RenderCopyF(r, t, nullptr, &d);
    }
};

// ------------------------------------------------------------------ texto

struct Text {
    Gfx *g = nullptr;
    std::string regular, bold;
    std::unordered_map<int, TTF_Font *> fonts;
    struct T { SDL_Texture *t; int w, h; };
    std::unordered_map<std::string, T> cache;
    float escala = 1;   // "Tamanho do texto" do painel Sistema (Normal 1, Grande 1,15, Maior 1,3)

    TTF_Font *font(int px, bool b) {
        int key = px * 2 + (b ? 1 : 0);
        auto it = fonts.find(key);
        if (it != fonts.end()) return it->second;
        TTF_Font *f = TTF_OpenFont((b ? bold : regular).c_str(), px);
        if (!f && b) f = TTF_OpenFont(regular.c_str(), px);
        fonts[key] = f;
        return f;
    }

    // tamanho em unidades de tela, como no iChroma (text(8.5f, ...))
    int px(float units) const { return std::max(6, int(units * g->u * escala + 0.5f)); }

    int width(const std::string &s, float units, bool b) {
        TTF_Font *f = font(px(units), b);
        int w = 0, h = 0;
        if (f && !s.empty()) TTF_SizeUTF8(f, s.c_str(), &w, &h);
        return w;
    }

    T get(const std::string &s, float units, bool b, uint32_t c) {
        std::string key = std::to_string(px(units)) + (b ? "b" : "r") + std::to_string(c) + "|" + s;
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        if (cache.size() > 400) clear();
        T out{nullptr, 0, 0};
        TTF_Font *f = font(px(units), b);
        if (f && !s.empty()) {
            SDL_Surface *sf = TTF_RenderUTF8_Blended(f, s.c_str(), cor(c | 0xFF000000));
            if (sf) {
                out.t = SDL_CreateTextureFromSurface(g->r, sf);
                out.w = sf->w;
                out.h = sf->h;
                SDL_FreeSurface(sf);
                if (out.t) SDL_SetTextureAlphaMod(out.t, Uint8(c >> 24));
            }
        }
        cache[key] = out;
        return out;
    }

    void clear() {
        for (auto &kv : cache) if (kv.second.t) SDL_DestroyTexture(kv.second.t);
        cache.clear();
    }

    // align: 0 esquerda, 1 centro, 2 direita; cy = meio vertical da linha
    void draw(const std::string &s, float x, float cy, float units, bool b, uint32_t c, int align = 0,
              float maxw = 0) {
        T t = get(s, units, b, c);
        if (!t.t) return;
        float w = float(t.w), h = float(t.h);
        if (maxw > 0 && w > maxw) {   // corta com reticencias
            std::string cut = s;
            while (cut.size() > 1 && width(cut + "...", units, b) > maxw) {
                cut.pop_back();
                while (!cut.empty() && (cut.back() & 0xC0) == 0x80) cut.pop_back();
            }
            draw(cut + "...", x, cy, units, b, c, align, 0);
            return;
        }
        float dx = align == 1 ? x - w / 2 : (align == 2 ? x - w : x);
        SDL_FRect d{dx, cy - h / 2, w, h};
        SDL_RenderCopyF(g->r, t.t, nullptr, &d);
    }
};
