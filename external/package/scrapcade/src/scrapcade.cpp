// ScrapCade - o menu do ScrapCade OS (conversao do ScrapCade do iChroma para C++/SDL2); versao no arquivo VERSION
//
// Modo jogo do ScrapCade: carrossel de capas com perspectiva e reflexo, filtros LB/RB (Todos, Favoritos,
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
#include "bios.h"
#include "usb.h"
#include "rede.h"
#include "pixel_fonte.h"

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
#include <functional>
#include <thread>
#include <atomic>
#include <cmath>
#include <SDL_mixer.h>


// ------------------------------------------------------------------ preferencias (favoritos, jogados, tema)

static std::map<std::string, std::string> prefs;

static void loadPrefs() {
    std::ifstream f(CONF_FILE);
    std::string l;
    while (std::getline(f, l)) {
        size_t p = l.find('=');
        if (p != std::string::npos) prefs[l.substr(0, p)] = l.substr(p + 1);
    }
}

static void savePrefs() {
    mkdirs(CONF_FILE.substr(0, CONF_FILE.rfind('/')));
    std::string tmp = CONF_FILE + ".tmp";
    {
        std::ofstream f(tmp);
        for (auto &kv : prefs) f << kv.first << "=" << kv.second << "\n";
    }
    rename(tmp.c_str(), CONF_FILE.c_str());
}

// perfis de jogador (definidos mais abaixo, na secao "perfis de jogador")
struct Perfil { std::string id, nome, hash; int avatar; };
extern std::vector<Perfil> perfis;
extern int perfilAtual;
static void drawAvatar(const Perfil *p, int av, float x, float y, float s);
static std::string emuNome(const Sys *s);
static bool servico(const std::string &nome, const char *padrao);
static void redraw();
// Consoles com mais de um emulador: o primeiro e o padrao
struct Nucleo { const char *so, *nome; };
static std::vector<Nucleo> nucleos(const std::string &id) {
    // o primeiro instalado e o padrao; a imagem de 32 bits (PCs antigos) so leva os mais leves
    if (id == "snes")
        return {{"snes9x", "Snes9x"}, {"bsnes", "bsnes (mais preciso, mais pesado)"}, {"snes9x2010", "Snes9x 2010 (mais leve)"}};
    if (id == "nes") return {{"mesen", "Mesen"}, {"fceumm", "FCEUmm (mais leve)"}};
    if (id == "megadrive") return {{"genesis_plus_gx", "Genesis Plus GX"}, {"picodrive", "PicoDrive (mais leve)"}};
    if (id == "gba") return {{"mgba", "mGBA"}, {"gpsp", "gpSP (mais leve)"}};
    return {};
}

// ------------------------------------------------------------------ bateria (notebooks) e modos de energia
// Le direto do kernel (/sys/class/power_supply e /sys/devices/system/cpu), sem programa extra.

struct Bateria { bool tem = false, carregando = false; int pct = 0; };
static std::string lerSys(const std::string &p) {
    std::ifstream in(p);
    std::string v;
    std::getline(in, v);
    return v;
}
static Bateria lerBateria() {
    Bateria b;
    DIR *d = opendir("/sys/class/power_supply");
    if (!d) return b;
    while (dirent *e = readdir(d)) {
        std::string n = e->d_name, base = "/sys/class/power_supply/" + n;
        if (n[0] == '.' || lerSys(base + "/type") != "Battery") continue;
        std::string c = lerSys(base + "/capacity");
        if (c.empty()) continue;
        b.tem = true;
        b.pct = atoi(c.c_str());
        std::string st = lerSys(base + "/status");
        b.carregando = st == "Charging" || st == "Full";
        break;
    }
    closedir(d);
    return b;
}

static const char *ENERGIA[] = {"Economia", "Moderado", "Máximo"};
static int energia = 1;
static const std::string ENERGIA_FILE = "/userdata/system/configs/energia";

static bool energiaDisponivel() { return isFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"); }

static void escreverSys(const std::string &p, const std::string &v) {
    FILE *f = fopen(p.c_str(), "w");
    if (f) { fputs(v.c_str(), f); fclose(f); }
}

// Economia: menos frequencia e sem turbo. Moderado: equilibrado. Maximo: frequencia alta o tempo todo.
static void aplicarEnergia(int m) {
    energia = std::max(0, std::min(2, m));
    std::string driver = lerSys("/sys/devices/system/cpu/cpu0/cpufreq/scaling_driver");
    bool pstate = driver == "intel_pstate" || driver == "amd-pstate-epp";
    std::string disp = lerSys("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors");
    std::string gov;
    if (pstate) gov = energia == 2 ? "performance" : "powersave";   // nesses drivers o EPP faz o ajuste fino
    else if (energia == 0) gov = "powersave";
    else if (energia == 2) gov = "performance";
    else gov = disp.find("schedutil") != std::string::npos ? "schedutil" : "ondemand";
    static const char *EPP[] = {"power", "balance_performance", "performance"};
    DIR *d = opendir("/sys/devices/system/cpu");
    while (d) {
        dirent *e = readdir(d);
        if (!e) break;
        std::string n = e->d_name;
        if (n.compare(0, 3, "cpu") != 0 || !isdigit((unsigned char)n[3 < n.size() ? 3 : 0])) continue;
        std::string c = "/sys/devices/system/cpu/" + n + "/cpufreq/";
        escreverSys(c + "scaling_governor", gov);
        if (pstate) escreverSys(c + "energy_performance_preference", EPP[energia]);
    }
    if (d) closedir(d);
    escreverSys("/sys/devices/system/cpu/intel_pstate/no_turbo", energia == 0 ? "1" : "0");
    escreverSys("/sys/devices/system/cpu/cpufreq/boost", energia == 0 ? "0" : "1");
    mkdirs("/userdata/system/configs");
    std::ofstream(ENERGIA_FILE) << energia << "\n";
}

static bool isFav(const Game &g) { return prefs.count("fav|" + g.key()) > 0; }
static long quando(const Game &g) {   // ultima vez que o jogo foi aberto (para o "Continuar jogando")
    auto it = prefs.find("quando|" + g.key());
    return it == prefs.end() ? 0 : atol(it->second.c_str());
}
static int plays(const Game &g) {
    auto it = prefs.find("plays|" + g.key());
    return it == prefs.end() ? 0 : atoi(it->second.c_str());
}

// ------------------------------------------------------------------ temas (Theme.java)

// Temas proprios, desenhados aqui (sem imagem de marca): os ultimos tres lembram epocas dos videogames
enum { CARBONO, NEON, ARCADE, CLARO, AURORA, PIXEL, PORTATIL, OCEANO, NTEMAS };
static const char *THEME_NAMES[NTEMAS] = {"Carbono", "Neon", "Arcade", "Claro", "Aurora", "8 bits", "Portátil", "Oceano"};
static int theme = AURORA;

// Temas do usuario: cada pasta em SCRAP-DADOS/themes/<pasta> vira um tema a mais na lista.
//   fundo.png / fundo.jpg  imagem de fundo (preenche a tela)
//   tema.txt               nome=Meu tema | base=Aurora (cartoes e cores de um tema pronto) | destaque=#32E6A8
//   avatares/*.png         avatares a mais para escolher ao criar um perfil (tambem em themes/avatares)
// As imagens ficam so no aparelho de quem colocou: o ScrapCade nao traz nenhuma.
static const std::string THEMES_DIR = "/userdata/themes";
struct TemaUser { std::string pasta, nome, fundo; int base; uint32_t destaque; bool temDestaque; };
static std::vector<TemaUser> temasUser;
static int temaUser = -1;                       // -1: tema pronto; senao, indice em temasUser
static std::vector<std::string> avatarExtras;   // caminhos das imagens de avatar do usuario

static uint32_t corHex(std::string h, bool &ok) {
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    ok = h.size() == 6 && strspn(h.c_str(), "0123456789abcdefABCDEF") == 6;
    return ok ? 0xFF000000u | uint32_t(strtoul(h.c_str(), nullptr, 16)) : 0;
}

static void lerAvatares(const std::string &dir) {
    std::vector<std::string> v;
    if (DIR *d = opendir(dir.c_str())) {
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            if (!n.empty() && n[0] != '.' && (endsWith(n, ".png") || endsWith(n, ".jpg") || endsWith(n, ".jpeg")))
                v.push_back(dir + "/" + n);
        }
        closedir(d);
    }
    std::sort(v.begin(), v.end());
    for (auto &x : v) if (avatarExtras.size() < 24) avatarExtras.push_back(x);
}

// Le a pasta themes de novo (temas colocados com o ScrapCade ligado aparecem sem reiniciar)
static void scanTemas() {
    std::string atual = temaUser >= 0 && temaUser < int(temasUser.size()) ? temasUser[temaUser].pasta : "";
    temasUser.clear();
    avatarExtras.clear();
    mkdirs(THEMES_DIR + "/avatares");
    std::vector<std::string> dirs;
    if (DIR *d = opendir(THEMES_DIR.c_str())) {
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            struct stat st;
            if (!n.empty() && n[0] != '.' && stat((THEMES_DIR + "/" + n).c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                dirs.push_back(n);
        }
        closedir(d);
    }
    std::sort(dirs.begin(), dirs.end());
    for (auto &n : dirs) {
        std::string p = THEMES_DIR + "/" + n;
        if (n == "avatares") { lerAvatares(p); continue; }
        lerAvatares(p + "/avatares");
        TemaUser t{n, n, "", CARBONO, 0, false};
        for (const char *f : {"/fundo.png", "/fundo.jpg", "/fundo.jpeg"})
            if (isFile(p + f)) { t.fundo = p + f; break; }
        std::ifstream in(p + "/tema.txt");
        std::string l;
        while (std::getline(in, l)) {
            if (!l.empty() && l.back() == '\r') l.pop_back();
            size_t q = l.find('=');
            if (q == std::string::npos) continue;
            std::string k = lower(l.substr(0, q)), v = l.substr(q + 1);
            while (!v.empty() && v[0] == ' ') v.erase(0, 1);
            if (k == "nome" && !v.empty()) {
                t.nome = v.substr(0, 24);
            } else if (k == "base") {
                for (int i = 0; i < NTEMAS; i++)
                    if (lower(v) == lower(THEME_NAMES[i])) t.base = i;
            } else if (k == "destaque") {
                t.destaque = corHex(v, t.temDestaque);
            }
        }
        if (!t.fundo.empty() || isFile(p + "/tema.txt")) temasUser.push_back(t);
    }
    temaUser = -1;
    for (size_t i = 0; i < temasUser.size(); i++)
        if (temasUser[i].pasta == atual) temaUser = int(i);
}

static std::string temaNome() { return temaUser >= 0 ? temasUser[temaUser].nome : THEME_NAMES[theme]; }

// Tema salvo no perfil: "theme" = tema pronto (ou a base do tema do usuario), "tema_usuario" = pasta
static void aplicarTemaPrefs() {
    theme = prefs.count("theme") ? std::max(0, std::min(NTEMAS - 1, atoi(prefs["theme"].c_str()))) : AURORA;
    temaUser = -1;
    auto it = prefs.find("tema_usuario");
    if (it == prefs.end() || it->second.empty()) return;
    for (size_t i = 0; i < temasUser.size(); i++)
        if (temasUser[i].pasta == it->second) { temaUser = int(i); theme = temasUser[i].base; }
}

static uint32_t accent(uint32_t col) {
    if (temaUser >= 0 && temasUser[temaUser].temDestaque) return temasUser[temaUser].destaque;
    if (theme == AURORA) return 0xFF32E6A8;
    if (theme == PIXEL) return 0xFFFFD34E;
    if (theme == PORTATIL) return 0xFFB8DC48;
    if (theme == OCEANO) return 0xFF5CE1E6;
    return lighten(col, 0.35f);
}
static uint32_t band(uint32_t col) {
    if (theme == CLARO || theme == AURORA) return col;
    if (theme == NEON) return darken(col, 0.55f);
    if (theme == ARCADE) return blend(col, 0xFFFF3EA5, 0.45f);
    if (theme == PIXEL) return col;
    if (theme == PORTATIL) return blend(col, 0xFF2F5A1F, 0.65f);
    if (theme == OCEANO) return blend(col, 0xFF0A4C6E, 0.45f);
    return darken(col, 0.30f);
}
static uint32_t cardText(uint32_t col) {
    if (theme == CLARO) return darken(col, 0.25f);
    if (theme == NEON) return lighten(col, 0.55f);
    if (theme == PORTATIL) return 0xFFE4F6C8;
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
    bool fu = temaUser >= 0 && !temasUser[temaUser].fundo.empty();
    std::string key = std::to_string(theme) + "|" + std::to_string(temaUser) + "|" + std::to_string(theme == AURORA || fu ? 0 : col);
    auto it = backs.find(key);
    if (it != backs.end()) return it->second;
    SDL_Texture *t = SDL_CreateTexture(G.r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, G.W, G.H);
    SDL_SetRenderTarget(G.r, t);
    float W = float(G.W), H = float(G.H), d = G.u;
    SDL_Texture *fimg = fu ? image(temasUser[temaUser].fundo) : nullptr;
    if (fimg) {   // fundo do usuario: preenche a tela cortando as sobras, sem esticar
        int iw = 1, ih = 1;
        SDL_QueryTexture(fimg, nullptr, nullptr, &iw, &ih);
        float esc = std::max(W / iw, H / ih), sw = W / esc, sh = H / esc;
        SDL_Rect src{int((iw - sw) / 2), int((ih - sh) / 2), int(sw), int(sh)};
        SDL_FRect dst{0, 0, W, H};
        G.fillRect(0, 0, W, H, 0xFF000000);
        SDL_RenderCopyF(G.r, fimg, &src, &dst);
    } else
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
    case PIXEL: {   // ceu noturno com estrelas quadradas e chao quadriculado
        G.gradientV(0, 0, W, H * 0.66f, 0xFF07061A, 0xFF1B1550);
        unsigned seed = 7;
        for (int i = 0; i < 90; i++) {
            seed = seed * 1103515245u + 12345u;
            float x = float(seed % 10000) / 10000 * W;
            seed = seed * 1103515245u + 12345u;
            float y = float(seed % 10000) / 10000 * H * 0.6f, q = (seed % 3 + 1) * d;
            G.fillRect(std::floor(x), std::floor(y), q, q, (seed % 5) ? 0xAAFFFFFF : 0xFFFFD34E);
        }
        float hz = H * 0.66f, q = 16 * d;
        G.fillRect(0, hz, W, H - hz, 0xFF100D30);
        for (float y = hz; y < H; y += q)
            for (float x = 0; x < W; x += q)
                if ((int(x / q) + int((y - hz) / q)) % 2) G.fillRect(x, y, q, q, 0xFF1A1648);
        G.fillRect(0, hz, W, 2 * d, alpha(lighten(col, 0.3f), 200));
        break;
    }
    case PORTATIL:   // tela de LCD verde com linhas de varredura
        G.gradientV(0, 0, W, H, 0xFF3C5A22, 0xFF0F200C);
        G.radial(W / 2, H * 0.45f, W * 0.6f, H * 0.6f, 0x40C8E880);
        for (float y = 0; y < H; y += 3 * d) G.fillRect(0, y, W, d, 0x18000000);
        break;
    case OCEANO:   // azul profundo, raios de luz vindos de cima e bolhas
        G.gradientV(0, 0, W, H, 0xFF06486A, 0xFF010A16);
        for (int i = 0; i < 5; i++) {
            float cx = W * (0.15f + 0.18f * i);
            SDL_Color c0 = cor(0x30A8F0FF), c1 = cor(0x00A8F0FF);
            SDL_Vertex v[3] = {{{cx - 18 * d, 0}, c0, {0, 0}}, {{cx + 18 * d, 0}, c0, {0, 0}},
                               {{cx + (i - 2) * 60 * d, H * 0.9f}, c1, {0, 0}}};
            SDL_RenderGeometry(G.r, nullptr, v, 3, nullptr, 0);
        }
        {
            unsigned seed = 3;
            for (int i = 0; i < 26; i++) {
                seed = seed * 1103515245u + 12345u;
                float x = float(seed % 10000) / 10000 * W, y = H * (0.3f + float((seed >> 8) % 700) / 1000);
                G.radial(x, y, (2 + seed % 5) * d, (2 + seed % 5) * d, 0x50C8F4FF);
            }
        }
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

static std::string fotoDoPonto(const Game &g);
static bool cartoesPonto = false;   // "Continuar jogando" aberto: as capas viram a foto do ponto salvo
static void render();
static Card &card(const Game &g) {
    std::string ponto = cartoesPonto ? fotoDoPonto(g) : "";
    std::string key = g.key() + "|" + std::to_string(theme) + (showCovers ? "" : "|s") + (ponto.empty() ? "" : "|ponto");
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
        std::string p = ponto.empty() ? coverOf(g) : ponto;   // "Continuar jogando": a foto de onde voce parou
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
    // (recuado do contorno: senao cobre a borda da capa e aparecem "pontinhos" nos cantos da faixa)
    G.fillRect(x0 + 2.5f * u, y0 + bandH, w - 5 * u, rad, theme == CLARO ? 0xFFFFFFFF : (theme == AURORA ? 0xFF15171D : s->color));
    // contorno por cima da faixa: a borda de cima fica inteira e lisa (sem tracinhos nem pontinhos nos cantos)
    switch (theme) {
    case AURORA: G.roundRectStroke(x0, y0, w, h, rad, u, lighten(s->color, 0.15f)); break;
    case NEON: G.roundRectStroke(x0, y0, w, h, rad, 2.5f * u, lighten(s->color, 0.45f)); break;
    case ARCADE: G.roundRectStroke(x0, y0, w, h, rad, 2.5f * u, 0xFFFFD34E); break;
    case CLARO: break;
    default: G.roundRectStroke(x0, y0, w, h, rad, 2 * u, 0xA6FFFFFF); break;
    }
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

static std::string fotoDoPonto(const Game &g) {
    std::string pid = perfilAtual >= 0 && perfilAtual < int(perfis.size()) ? perfis[perfilAtual].id : "1";
    std::string dir = "/userdata/saves/" + g.sys->id + (pid != "1" && !pid.empty() ? "/perfil-" + pid : "");
    std::string f = dir + "/" + g.base + ".state.auto.png";
    return isFile(f) ? f : "";
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
    // Em faixas verticais finas, com a textura corrigida pela perspectiva: com so dois triangulos a imagem
    // "dobra" na diagonal (capas tortas nas laterais do carrossel)
    const int N = 24;
    SDL_Color a = cor(c0), b = cor(c1);
    SDL_Vertex v[2 * (N + 1)];
    int idx[6 * N];
    float h0 = std::max(0.01f, y0b - y0t), h1 = std::max(0.01f, y1b - y1t);
    for (int i = 0; i <= N; i++) {
        float f = float(i) / N, x = x0 + (x1 - x0) * f;
        float pf = (f * h1) / ((1 - f) * h0 + f * h1);   // o lado mais baixo (mais longe) fica mais apertado
        float uu = u0 + (u1 - u0) * pf;
        v[2 * i] = {{x, y0t + (y1t - y0t) * f}, a, {uu, v0}};
        v[2 * i + 1] = {{x, y0b + (y1b - y0b) * f}, b, {uu, v1}};
    }
    for (int i = 0; i < N; i++) {
        int k = 2 * i, *q = idx + 6 * i;
        q[0] = k; q[1] = k + 2; q[2] = k + 3; q[3] = k; q[4] = k + 3; q[5] = k + 1;
    }
    SDL_RenderGeometry(G.r, t, v, 2 * (N + 1), idx, 6 * N);
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

// ------------------------------------------------------------------ opcoes do painel que valem no carrossel e no jogo
static const char *ORDENS[] = {"Nome", "Ano", "Mais jogados", "Últimos jogados"};
static int ordemAtual() { int v = atoi(prefs["ordem"].c_str()); return v < 0 || v > 3 ? 0 : v; }
static const int PROTECAO_MIN[] = {0, 5, 10, 30};
static int protecaoIdx() { auto it = prefs.find("protecao"); int v = it == prefs.end() || it->second.empty() ? 2 : atoi(it->second.c_str()); return v < 0 || v > 3 ? 2 : v; }
static const int DESLIGAR_MIN[] = {0, 30, 60, 120};
static int desligarIdx() { int v = atoi(prefs["desligar"].c_str()); return v < 0 || v > 3 ? 0 : v; }
static Uint32 ultimaTecla = 0;
// musica (da pasta music da SCRAP-DADOS: .ogg, .mp3, .flac, .wav) e sons do menu, desligados por padrao
static const std::string MUSICA_DIR = "/userdata/music";
static bool mixerAberto = false;
static Mix_Music *musicaAtual = nullptr;
static std::atomic<bool> musicaAcabou{false};
static Mix_Chunk *somMover = nullptr, *somOk = nullptr, *somVoltar = nullptr;
static bool abrirMixer() {
    if (mixerAberto) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) != 0) { SDL_QuitSubSystem(SDL_INIT_AUDIO); return false; }
    Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3 | MIX_INIT_FLAC);
    mixerAberto = true;
    return true;
}
// bipe curto e suave (sem arquivo): frequencia que desliza de f0 para f1 em ms milissegundos
static Mix_Chunk *bipe(float f0, float f1, int ms) {
    int n = 44100 * ms / 1000;
    Sint16 *b = (Sint16 *)SDL_malloc(n * 4);
    double fase = 0;
    for (int i = 0; i < n; i++) {
        float t = float(i) / n, f = f0 + (f1 - f0) * t, env = t < 0.1f ? t / 0.1f : (1 - t) / 0.9f;
        fase += 2 * M_PI * f / 44100;
        Sint16 v = Sint16(sin(fase) * 6000 * env);
        b[2 * i] = b[2 * i + 1] = v;
    }
    Mix_Chunk *c = Mix_QuickLoad_RAW((Uint8 *)b, n * 4);
    return c;
}
static void tocarSom(Mix_Chunk *c) {
    if (prefs["sons"] != "1" || !c || !mixerAberto) return;
    Mix_PlayChannel(-1, c, 0);
}
static std::vector<std::string> musicas() {
    std::vector<std::string> v;
    if (DIR *d = opendir(MUSICA_DIR.c_str())) {
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            if (endsWith(n, ".ogg") || endsWith(n, ".mp3") || endsWith(n, ".flac") || endsWith(n, ".wav")) v.push_back(MUSICA_DIR + "/" + n);
        }
        closedir(d);
    }
    return v;
}
static void tocarMusica() {   // uma musica sorteada da pasta; quando acaba, a proxima
    if (prefs["musica"] != "1" || !abrirMixer()) return;
    auto v = musicas();
    if (v.empty()) return;
    if (musicaAtual) { Mix_HaltMusic(); Mix_FreeMusic(musicaAtual); musicaAtual = nullptr; }
    musicaAtual = Mix_LoadMUS(v[rand() % v.size()].c_str());
    if (!musicaAtual) return;
    Mix_VolumeMusic(MIX_MAX_VOLUME * 35 / 100);
    Mix_HookMusicFinished([] { musicaAcabou = true; });
    Mix_PlayMusic(musicaAtual, 1);
}
static void pararMusica() {
    if (musicaAtual) { Mix_HaltMusic(); Mix_FreeMusic(musicaAtual); musicaAtual = nullptr; }
}
static void prepararSons() {
    if (prefs["sons"] != "1" && prefs["musica"] != "1") return;
    if (!abrirMixer()) return;
    if (!somMover) { somMover = bipe(1250, 1250, 22); somOk = bipe(880, 1320, 70); somVoltar = bipe(990, 660, 60); }
}
// menu do botao de energia (o acpid manda SIGUSR1 ao ScrapCade)
static volatile sig_atomic_t botaoEnergia = 0;
static void onBotaoEnergia(int) { botaoEnergia = 1; }
static int energiaMenu = 0;   // 0 fechado; senao 1 + opcao destacada
static const char *ENERGIA_OPCOES[] = {"Suspender", "Reiniciar", "Desligar", "Cancelar"};      // ultimo botao ou tecla (para a protecao de tela e o desligar sozinho)
static bool protecaoOn = false;
static const char *FILTROS_IMG[] = {"Nenhum", "Liso", "TV de tubo"};
static int filtroImg() { int v = atoi(prefs["filtro"].c_str()); return v < 0 || v > 2 ? 0 : v; }
static const char *PROPORCOES[] = {"Original", "4:3", "Tela cheia", "Escala inteira"};
static int proporcao() { int v = atoi(prefs["proporcao"].c_str()); return v < 0 || v > 3 ? 0 : v; }
static bool prefLigada(const char *k, bool padrao) {
    auto it = prefs.find(k);
    return it == prefs.end() || it->second.empty() ? padrao : it->second != "0";
}
// O que o painel JOGANDO escolheu vai para o RetroArch por este arquivo (o tos-jogar acrescenta a configuracao)
static void gravarJogoCfg() {
    mkdirs("/userdata/system/configs");
    std::ofstream o("/userdata/system/configs/scrapcade-jogo.cfg");
    bool resume = prefLigada("resume", true), run = prefLigada("runahead", false), fps = prefLigada("fps", false);
    int f = filtroImg(), pr = proporcao();
    o << "savestate_auto_save = \"" << (resume ? "true" : "false") << "\"\n";
    o << "savestate_auto_load = \"" << (resume ? "true" : "false") << "\"\n";
    o << "video_smooth = \"" << (f == 1 ? "true" : "false") << "\"\n";
    o << "video_shader_enable = \"" << (f == 2 ? "true" : "false") << "\"\n";
    if (f == 2) o << "video_shader = \"/usr/share/tos/shaders/tv.glslp\"\n";
    o << "aspect_ratio_index = \"" << (pr == 1 ? 0 : pr == 2 ? 24 : 22) << "\"\n";   // 0 = 4:3, 22 = do console, 24 = tela cheia
    o << "video_scale_integer = \"" << (pr == 3 ? "true" : "false") << "\"\n";
    o << "run_ahead_enabled = \"" << (run ? "true" : "false") << "\"\n";
    o << "run_ahead_frames = \"1\"\n";
    o << "fps_show = \"" << (fps ? "true" : "false") << "\"\n";
}

static const std::string F_ALL = "", F_FAV = "*fav", F_TOP = "*top", F_REC = "*rec";
static std::vector<std::string> filters;
static std::string filter = F_ALL;

static void buildFilters() {
    filters = {F_ALL};
    bool fav = false, top = false, rec = false;
    for (auto &g : all) { if (isFav(g)) fav = true; if (plays(g) > 0) top = true; if (quando(g) > 0) rec = true; }
    if (rec) filters.push_back(F_REC);   // logo depois de Todos: os ultimos jogos abertos
    if (fav) filters.push_back(F_FAV);
    if (top) filters.push_back(F_TOP);
    bool vazios = prefLigada("vazios", false);   // painel: mostrar tambem os consoles sem jogo
    for (auto &s : SYSTEMS) {
        bool tem = false;
        for (auto &g : all) if (g.sys == &s) { tem = true; break; }
        if (tem || vazios) filters.push_back(s.id);
    }
    if (std::find(filters.begin(), filters.end(), filter) == filters.end()) filter = F_ALL;
}

static void pixelText(const std::string &txt, float cx, float cy, float units, uint32_t cor, float maxw);

static std::string filterName(const std::string &id) {
    if (id == F_REC) return "Continuar jogando";
    if (id == F_FAV) return "Favoritos";
    if (id == F_TOP) return "Mais jogados";
    for (auto &s : SYSTEMS) if (s.id == id) return s.name;
    return "Todos";
}

static void applyFilter(int keepIndex = -1) {
    cartoesPonto = filter == F_REC;
    view.clear();
    for (auto &g : all) {
        if (filter == F_FAV && !isFav(g)) continue;
        if (filter == F_TOP && plays(g) <= 0) continue;
        if (filter == F_REC && quando(g) <= 0) continue;
        if (!filter.empty() && filter[0] != '*' && g.sys->id != filter) continue;
        view.push_back(g);
    }
    if (filter != F_TOP && filter != F_REC) {   // Ordenar jogos (painel): nome e a ordem de sempre
        int o = ordemAtual();
        if (o == 1) std::stable_sort(view.begin(), view.end(), [](const Game &a, const Game &b) {
            Meta::Info ma, mb;
            std::string ya = Meta::get(a.key(), ma) ? ma.year : "", yb = Meta::get(b.key(), mb) ? mb.year : "";
            if (ya.empty() != yb.empty()) return !ya.empty();   // sem ano: no fim
            return ya < yb;
        });
        else if (o == 2) std::stable_sort(view.begin(), view.end(), [](const Game &a, const Game &b) { return plays(a) > plays(b); });
        else if (o == 3) std::stable_sort(view.begin(), view.end(), [](const Game &a, const Game &b) { return quando(a) > quando(b); });
    }
    if (filter == F_TOP)
        std::stable_sort(view.begin(), view.end(), [](const Game &a, const Game &b) { return plays(a) > plays(b); });
    if (filter == F_REC) {   // do mais recente para o mais antigo, no maximo 12
        std::stable_sort(view.begin(), view.end(), [](const Game &a, const Game &b) { return quando(a) > quando(b); });
        if (view.size() > 12) view.resize(12);
    }
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
    if (g) T.draw(emuNome(g->sys), x, cy, 8, false, 0xCCFFFFFF, 0, G.W * 0.30f - x);
    // filtro no meio
    if (filters.size() >= 2) {
        std::string f = filterName(filter) + "  (" + std::to_string(view.size()) + ")";
        float bpx = std::max(1.0f, std::floor(9.5f * G.u / 7.0f + 0.5f));   // bloco das letras de pixel (tema 8 bits)
        float fw = (theme == PIXEL ? Pixel::largura(Pixel::maiuscula(f), bpx) : T.width(f, 10.5f, true)) + 14 * u,
              lb = T.width("LB", 7.5f, true) + 10 * u,
              rb = T.width("RB", 7.5f, true) + 10 * u;
        float sx = G.W / 2.0f - (lb + fw + rb) / 2, w1;
        pill("LB", sx, cy, w1);
        if (theme == PIXEL) pixelText(f, sx + lb + fw / 2, cy, 9.5f, 0xFFFFFFFF, fw);
        else T.draw(f, sx + lb + fw / 2, cy, 10.5f, true, 0xFFFFFFFF, 1);
        pill("RB", sx + lb + fw, cy, w1);
    }
    // hora, data e IP a direita
    time_t now = time(nullptr);
    tm lt;
    localtime_r(&now, &lt);
    char hora[16], data[32];
    strftime(hora, sizeof hora, prefs["relogio"] == "12" ? "%I:%M %p" : "%H:%M", &lt);
    static const char *DIAS[] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};
    snprintf(data, sizeof data, "%s, %02d/%02d", DIAS[lt.tm_wday], lt.tm_mday, lt.tm_mon + 1);
    static std::string ip;
    static Uint32 ipAt = 0;
    if (!ipAt || SDL_GetTicks() - ipAt > 5000) { ip = ipAddr(); ipAt = SDL_GetTicks(); }
    float rx = G.W - 10 * u;
    static Bateria bat;
    static Uint32 batAt = 0;
    if (!batAt || SDL_GetTicks() - batAt > 30000) { bat = lerBateria(); batAt = SDL_GetTicks(); }
    float bloco = std::max(T.width(hora, 12.5f, true), T.width(std::string(data) + "  •  " + ip, 7, false));
    float ax = rx - bloco - 12 * u;
    if (bat.tem) {   // bateria do notebook: icone, porcentagem e raio quando esta na tomada
        std::string pc = std::to_string(bat.pct) + "%";
        float tw = T.width(pc, 9, true), bw = 16 * u, bh = 8 * u, bx = ax - tw - bw - 8 * u, by = cy - bh / 2;
        uint32_t cb = bat.pct <= 15 && !bat.carregando ? 0xFFE0564A : 0xFFFFFFFF;
        G.roundRectStroke(bx, by, bw, bh, 1.5f * u, std::max(1.0f, 0.8f * u), cb);
        G.fillRect(bx + bw, by + bh * 0.3f, 1.6f * u, bh * 0.4f, cb);
        G.fillRect(bx + 1.6f * u, by + 1.6f * u, (bw - 3.2f * u) * std::min(100, bat.pct) / 100.0f, bh - 3.2f * u, cb);
        T.draw(pc, bx + bw + 3 * u + tw, cy, 9, true, cb, 2);
        if (bat.carregando) T.draw("⚡", bx - 2 * u, cy, 9, true, 0xFFFFD34E, 2);
        ax = bx - (bat.carregando ? 14 : 6) * u;
    }
    if (perfilAtual >= 0) {   // quem esta jogando, ao lado do relogio
        ax -= 19 * u;
        drawAvatar(&perfis[perfilAtual], 0, ax, cy - 9.5f * u, 19 * u);
        T.draw(perfis[perfilAtual].nome, ax - 6 * u, cy, 9.5f, true, 0xFFFFFFFF, 2, 120 * u);
    }
    T.draw(hora, rx, cy - 3.6f * u, 12.5f, true, 0xFFFFFFFF, 2);
    T.draw(std::string(data) + "  •  " + ip, rx, cy + 7.2f * u, 7, false, 0xBBFFFFFF, 2);
}

// Texto em letras de pixel (tema "8 bits"): blocos inteiros de tela, com sombra; corta o que nao couber
static void pixelText(const std::string &txt, float cx, float cy, float units, uint32_t cor, float maxw) {
    std::string up = Pixel::maiuscula(txt);
    float b = std::max(1.0f, std::floor(units * G.u / 7.0f + 0.5f));
    while (!up.empty() && Pixel::largura(up, b) > maxw) up.pop_back();
    float x0 = std::floor(cx - Pixel::largura(up, b) / 2), y0 = std::floor(cy - 3.5f * b);
    for (int camada = 0; camada < 2; camada++) {
        float d = camada == 0 ? std::max(1.0f, std::floor(b / 2)) : 0;
        uint32_t c = camada == 0 ? 0xA0000000 : cor;
        for (size_t i = 0; i < up.size(); i++) {
            auto it = Pixel::G.find(up[i]);
            if (it == Pixel::G.end()) continue;
            for (int k = 0; k < 35; k++)
                if (it->second[k] == '#') G.fillRect(x0 + (i * 6 + k % 5) * b + d, y0 + (k / 5) * b + d, b, b, c);
        }
    }
}

static void titleBar(const Game *g) {
    float u = G.u, h = TITLE_H * u, y = G.H - (TITLE_H + HINT_H) * u;
    G.fillRect(0, y, float(G.W), h, 0xB8303338);
    if (!g) return;
    std::string t = (isFav(*g) ? "★  " : "") + g->title;
    if (theme == PIXEL) pixelText((isFav(*g) ? "+ " : "") + g->title, G.W / 2.0f, y + h * 0.36f, 13, 0xFFFFD34E, G.W - 32 * u);
    else T.draw(t, G.W / 2.0f, y + h * 0.36f, 13.5f, false, 0xFFFFFFFF, 1, G.W - 32 * u);
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
    L.push_back("Console: " + g->sys->name + "     Emulador: " + emuNome(g->sys) +
                (nucleos(g->sys->id).size() > 1 ? "   (X troca)" : ""));
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

enum { OPT_PERFIL, OPT_JOGOS, OPT_ARQUIVOS, OPT_ENERGIA, OPT_THEME, OPT_FONTE, OPT_ANIM, OPT_COVERS, OPT_BG, OPT_ENGINE, OPT_VOLUME, OPT_REDE, OPT_BLUETOOTH, OPT_SAMBA, OPT_SSH, OPT_CONTROLE, OPT_DESKTOP, OPT_HELP, OPT_BIOS, OPT_ATUALIZAR, OPT_INSTALL, OPT_INFO, OPT_GRATIS, OPT_BACKUP, OPT_EJETAR, OPT_AUTOUPD, OPT_REBOOT, OPT_OFF,
       OPT_ORDENAR, OPT_VAZIOS, OPT_CAPAS, OPT_LIMPAR, OPT_RESUME, OPT_FILTRO, OPT_PROPORCAO, OPT_RUNAHEAD, OPT_FPS, OPT_NUCLEO, OPT_RELOGIO, OPT_ENVIAR, OPT_PADRAO, OPT_PROTECAO, OPT_DESLIGAR, OPT_MUSICA, OPT_SONS, OPT_SUSPENDER, OPT_COUNT };
static bool sysOpen = false;

// Painel em secoes, como o menu de ajustes de um console de mesa: o dia a dia primeiro, o tecnico no fim.
// (A bateria de testes conta as opcoes nesta ordem: tests/testar.sh, funcao opt.)
struct Secao { const char *nome; std::vector<int> opts; };
static const std::vector<Secao> SECOES = {
    {"JOGOS", {OPT_PERFIL, OPT_GRATIS, OPT_JOGOS, OPT_ARQUIVOS, OPT_EJETAR, OPT_ORDENAR, OPT_VAZIOS, OPT_CAPAS, OPT_LIMPAR, OPT_BIOS, OPT_BACKUP}},
    {"JOGANDO", {OPT_RESUME, OPT_FILTRO, OPT_PROPORCAO, OPT_RUNAHEAD, OPT_FPS, OPT_NUCLEO}},
    {"APARÊNCIA", {OPT_THEME, OPT_FONTE, OPT_ANIM, OPT_COVERS, OPT_BG, OPT_PROTECAO}},
    {"CONTROLE E SOM", {OPT_CONTROLE, OPT_VOLUME, OPT_BLUETOOTH, OPT_MUSICA, OPT_SONS}},
    {"TELA E ENERGIA", {OPT_ENERGIA, OPT_DESLIGAR, OPT_RELOGIO}},
    {"REDE", {OPT_REDE, OPT_SAMBA, OPT_SSH, OPT_ENVIAR}},
    {"SISTEMA", {OPT_ATUALIZAR, OPT_AUTOUPD, OPT_INSTALL, OPT_DESKTOP, OPT_ENGINE, OPT_HELP, OPT_INFO, OPT_PADRAO, OPT_SUSPENDER, OPT_REBOOT, OPT_OFF}},
};
static std::vector<int> painelLinhas;   // -(secao + 1) = titulo da secao; >= 0 = opcao
static std::vector<int> painelOpts;     // so as opcoes, na ordem da tela
static int optSel = 0;                  // opcao destacada (OPT_...)
static void montarPainel() {
    painelLinhas.clear();
    painelOpts.clear();
    for (size_t i = 0; i < SECOES.size(); i++) {
        painelLinhas.push_back(-int(i) - 1);
        for (int o : SECOES[i].opts) { painelLinhas.push_back(o); painelOpts.push_back(o); }
    }
}
static int painelPos(int opt) {
    for (size_t i = 0; i < painelLinhas.size(); i++) if (painelLinhas[i] == opt) return int(i);
    return 0;
}
static const char *secaoDe(int opt) {
    for (auto &sc : SECOES) for (int o : sc.opts) if (o == opt) return sc.nome;
    return "";
}
static void painelMover(int d) {   // pula os titulos das secoes
    if (painelOpts.empty()) montarPainel();
    int n = int(painelOpts.size()), i = 0;
    while (i < n && painelOpts[i] != optSel) i++;
    optSel = painelOpts[(i + d + n) % n];
}
static bool restartMenu = false;
static std::string perfilNome();
static void abrirPerfis();
static int volume = -2;   // -2 ainda nao leu, -1 sem placa de som, 0..100
static const std::string DESKTOP = "/usr/bin/tos-desktop";
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

static const char *FONTES[] = {"Normal", "Grande", "Maior"};
static int nivelFonte() { return std::max(0, std::min(2, atoi(prefs["fonte"].c_str()))); }
static float escalaFonte() { return nivelFonte() == 2 ? 1.3f : nivelFonte() == 1 ? 1.15f : 1.0f; }

static std::string emuNome(const Sys *s);

static std::string optLabel(int i) {
    switch (i) {
    case OPT_ORDENAR: return std::string("Ordenar jogos:   ‹ ") + ORDENS[ordemAtual()] + " ›";
    case OPT_VAZIOS: return std::string("Consoles vazios:   ‹ ") + (prefLigada("vazios", false) ? "Mostrar" : "Esconder") + " ›";
    case OPT_CAPAS: return "Capas e dados dos jogos:   baixar de novo";
    case OPT_LIMPAR: return "Limpar \"Continuar jogando\"";
    case OPT_RESUME: return std::string("Voltar de onde parou:   ‹ ") + (prefLigada("resume", true) ? "Ligado" : "Desligado") + " ›";
    case OPT_FILTRO: return std::string("Filtro de imagem:   ‹ ") + FILTROS_IMG[filtroImg()] + " ›";
    case OPT_PROPORCAO: return std::string("Proporção da imagem:   ‹ ") + PROPORCOES[proporcao()] + " ›";
    case OPT_RUNAHEAD: return std::string("Menos atraso nos controles:   ‹ ") + (prefLigada("runahead", false) ? "Ligado" : "Desligado") + " ›";
    case OPT_FPS: return std::string("Mostrar FPS no jogo:   ‹ ") + (prefLigada("fps", false) ? "Sim" : "Não") + " ›";
    case OPT_NUCLEO: {
        const Game *g = current();
        if (!g) return "Emulador do console:   (escolha um jogo)";
        return "Emulador do " + g->sys->name + ":   ‹ " + emuNome(g->sys) + " ›";
    }
    case OPT_RELOGIO: return std::string("Relógio:   ‹ ") + (prefs["relogio"] == "12" ? "12 h (AM/PM)" : "24 h") + " ›";
    case OPT_ENVIAR: return "Enviar jogos do PC:   http://" + ipAddr() + ":" + std::to_string(Envio::PORT);
    case OPT_PADRAO: return "Voltar tudo ao padrão";
    case OPT_MUSICA: {
        size_t n = musicas().size();
        return std::string("Música do menu:   ‹ ") + (prefs["musica"] == "1" ? "Ligada" : "Desligada") + " ›" +
               (n ? "   (" + std::to_string(n) + (n == 1 ? " música)" : " músicas)") : "   (pasta music vazia)");
    }
    case OPT_SONS: return std::string("Sons do menu:   ‹ ") + (prefs["sons"] == "1" ? "Ligados" : "Desligados") + " ›";
    case OPT_SUSPENDER: return "Suspender (continua de onde parou)";
    case OPT_PROTECAO: { int m = PROTECAO_MIN[protecaoIdx()]; return std::string("Proteção de tela:   ‹ ") + (m ? std::to_string(m) + " min" : "Desligada") + " ›"; }
    case OPT_DESLIGAR: {
        int m = DESLIGAR_MIN[desligarIdx()];
        return std::string("Desligar sozinho:   ‹ ") + (m == 0 ? "Nunca" : m < 60 ? std::to_string(m) + " min" : std::to_string(m / 60) + " h") + " parado ›";
    }
    case OPT_PERFIL: return "Perfil:   " + perfilNome() + "   (A troca)";
    case OPT_ENERGIA: return energiaDisponivel() ? std::string("Energia:   ‹ ") + ENERGIA[energia] + " ›"
                                                 : "Energia:   (fixa neste computador)";
    case OPT_JOGOS: return "Trazer jogos do pen drive";
    case OPT_ARQUIVOS: return "Gerenciador de arquivos";
    case OPT_GRATIS: return "Jogos grátis (homebrew livres)";
    case OPT_BACKUP: return "Cópia dos saves no pen drive";
    case OPT_EJETAR: return "Ejetar pen drive";
    case OPT_AUTOUPD: return std::string("Atualizar sozinho:   ‹ ") + (prefs["autoupd"] == "0" ? "Desligado" : "Ligado") + " ›";
    case OPT_FONTE: return std::string("Tamanho do texto:   ‹ ") + FONTES[nivelFonte()] + " ›";
    case OPT_THEME: return std::string("Tema:   ‹ ") + temaNome() + " ›   (A: imagem)";
    case OPT_ANIM: return std::string("Animações:   ‹ ") + (ANIM_MS < 200 ? "Rápidas" : "Normais") + " ›";
    case OPT_COVERS: return std::string("Capas dos jogos:   ‹ ") + (showCovers ? "Mostrar" : "Ocultar") + " ›";
    case OPT_BG: return std::string("Fundo do jogo:   ‹ ") + (showBackdrop ? "Mostrar" : "Ocultar") + " ›";
    case OPT_ENGINE:
        return "Motor gráfico:   ‹ " + engineName(engineWant) + " ›" + (engineWant != engineNow ? "   A aplica" : "");
    case OPT_VOLUME: return volume < 0 ? "Volume:   (sem placa de som)" :
                            std::string("Volume:   ‹ ") + (volume == 0 ? "mudo" : std::to_string(volume) + "%") + " ›   A testa";
    case OPT_REDE: return "Rede e Wi-Fi";
    case OPT_BLUETOOTH: return "Bluetooth (controles sem fio)";
    case OPT_SAMBA: return std::string("Pasta na rede do Windows:   ‹ ") + (servico("samba", "on") ? "Ligada" : "Desligada") + " ›";
    case OPT_SSH: return std::string("Acesso remoto (SSH):   ‹ ") + (servico("ssh", "off") ? "Ligado" : "Desligado") + " ›";
    case OPT_CONTROLE: return "Configurar controle";
    case OPT_DESKTOP: return "Modo desktop";
    case OPT_HELP: return "Como jogar (botões)";
    case OPT_BIOS: return "BIOS dos consoles";
    case OPT_ATUALIZAR: return "Atualizar o ScrapCade OS";
    case OPT_INSTALL: return "Instalar no disco do computador";
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

static const char *optAjuda(int o) {
    switch (o) {
    case OPT_ORDENAR: return "Ordem dos jogos no carrossel: pelo nome, pelo ano (os sem ano ficam no fim), pelos mais jogados ou pelos últimos que você abriu.";
    case OPT_VAZIOS: return "Mostrar: os consoles sem jogo também aparecem nos filtros LB/RB. Esconder: só os consoles que têm jogo.";
    case OPT_CAPAS: return "Procura de novo, na internet, as capas e os dados dos jogos que ficaram sem. Bom depois de ligar a rede ou de arrumar o nome de um jogo.";
    case OPT_LIMPAR: return "Esvazia a lista \"Continuar jogando\" (os últimos jogos abertos). Favoritos e saves não são mexidos.";
    case OPT_RESUME: return "Ligado: ao sair de um jogo o ponto é guardado, e na próxima vez ele continua dali. Desligado: o jogo sempre começa do início (os saves do próprio jogo continuam valendo).";
    case OPT_FILTRO: return "Nenhum: pixels como o console mostrava. Liso: suaviza a imagem. TV de tubo: linhas de varredura leves, como numa TV antiga.";
    case OPT_PROPORCAO: return "Original: a proporção que o console usava. 4:3: como numa TV antiga. Tela cheia: estica até as bordas. Escala inteira: pixels sempre do mesmo tamanho, sem borrar.";
    case OPT_RUNAHEAD: return "Ligado: o emulador adianta um quadro para o botão responder mais rápido, como no console de verdade. Gasta mais processador: desligue se o jogo engasgar.";
    case OPT_FPS: return "Mostra os quadros por segundo no canto do jogo. Útil para ver se o computador está dando conta.";
    case OPT_NUCLEO: return "Troca o emulador do console do jogo selecionado, quando há mais de um (por exemplo Snes9x ou bsnes no Super Nintendo). Vale para todos os jogos desse console.";
    case OPT_RELOGIO: return "Formato da hora na barra de cima: 24 h (14:30) ou 12 h com AM e PM (02:30 PM).";
    case OPT_ENVIAR: return "No PC ou no celular, na mesma rede, abra este endereço no navegador e arraste os jogos. Eles vão sozinhos para o console certo.";
    case OPT_MUSICA: return "Toca as músicas que você colocar na pasta music da SCRAP-DADOS (.ogg, .mp3, .flac ou .wav), em ordem sorteada, só no menu. O ScrapCade não traz músicas.";
    case OPT_SONS: return "Um som curto ao andar pelo carrossel, ao escolher e ao voltar.";
    case OPT_SUSPENDER: return "Desliga a tela e quase tudo, como um console em descanso. Ao apertar o botão de energia (ou abrir a tampa) volta exatamente onde estava. O botão de energia também abre este menu.";
    case OPT_PROTECAO: return "Parado no menu por esse tempo, a tela escurece e a logo com a hora muda de lugar devagar (a imagem não marca a TV). Qualquer botão volta, sem fazer nada.";
    case OPT_DESLIGAR: return "Parado no menu por esse tempo, o computador desliga sozinho (avisa 1 minuto antes; qualquer botão cancela). Jogando, nunca desliga.";
    case OPT_PADRAO: return "Volta tema, filtros, opções de jogo e do menu ao padrão de fábrica. Favoritos, saves, perfis e jogos ficam como estão.";
    }
    if (o == OPT_GRATIS) return "Jogos feitos por fãs com licença livre (pode baixar, jogar e passar adiante). Com A o ScrapCade baixa do site oficial do autor, confere e coloca no console certo.";
    if (o == OPT_EJETAR) return "Solta o pen drive com segurança antes de tirar (grava o que ainda faltava). Para usar de novo, tire e coloque.";
    if (o == OPT_BACKUP) return "Copia os saves, os perfis e as configurações para uma pasta ScrapCade-saves no pen drive, com a data. Se o pen drive do sistema estragar, os saves estão guardados.";
    if (o == OPT_AUTOUPD) return "Ligado: quando sai versão nova, o ScrapCade OS baixa sozinho enquanto você joga e só avisa para reiniciar. Desligado: só avisa, e você atualiza em Atualizar o ScrapCade OS.";
    if (o == OPT_ARQUIVOS) return "Copiar, mover, renomear e apagar jogos, saves, BIOS e o que mais estiver na SCRAP-DADOS, e copiar do pen drive. Apagar manda para a pasta Lixeira, de onde da para trazer de volta.";
    if (o == OPT_FONTE) return "Deixa as letras do ScrapCade maiores: bom para TV longe do sofá ou para quem enxerga pouco. Vale para o menu, o painel e os nomes dos jogos.";
    switch (o) {
    case OPT_PERFIL: return "Troca quem está jogando. Cada perfil tem os seus favoritos, saves, tema e preferências. A abre a tela \"Quem vai jogar?\".";
    case OPT_JOGOS: return "Abre os arquivos do pen drive aqui mesmo: escolha um jogo (ou uma pasta inteira) e ele vai para \"Jogos novos\". O ScrapCade leva cada jogo ao console certo, com capa e dados.";
    case OPT_ENERGIA: return "Economia: gasta menos bateria e esquenta menos, mas jogos pesados podem engasgar. Moderado: o meio-termo. Máximo: o processador sempre no talo, para PS2, GameCube e PSP.";
    case OPT_THEME: return "Muda as cores, o fundo e o jeito das capas. Com A você escolhe uma imagem (do pen drive ou da SCRAP-DADOS) e ela vira o fundo de um tema seu.";
    case OPT_ANIM: return "Normais: o carrossel desliza suave. Rápidas: anda mais depressa, bom para listas grandes ou computadores mais fracos.";
    case OPT_COVERS: return "Mostrar: desenha a capa de cada jogo. Ocultar: só o nome e a cor do console, mais leve para computadores antigos.";
    case OPT_BG: return "Mostrar: a cena do jogo selecionado aparece borrada atrás do carrossel. Ocultar: fica só o fundo do tema.";
    case OPT_ENGINE: return "GPU (OpenGL ES): usa a placa de vídeo, mais suave. Software: desenha pelo processador; use se a tela ficar preta ou piscando. Vale ao reiniciar o ScrapCade.";
    case OPT_VOLUME: return "Volume de tudo: menu e jogos. A toca um som de teste para conferir as caixas ou o fone.";
    case OPT_REDE: return "Mostra o cabo de rede e as redes Wi-Fi. Escolha a sua e digite a senha com o teclado da tela. Com internet o ScrapCade acha capas e atualizações.";
    case OPT_BLUETOOTH: return "Pareia controles sem fio (PS4, PS5, Switch, Xbox) e fones. Coloque o controle em modo de pareamento antes.";
    case OPT_SAMBA: return "Ligada: a pasta dos jogos aparece na rede do Windows (\\\\ e o endereço IP). Dá para copiar jogos e saves direto pelo Explorador de Arquivos.";
    case OPT_SSH: return "Acesso pelo terminal de outro computador, para manutenção. Deixe desligado se não usa: é mais seguro.";
    case OPT_CONTROLE: return "Ensina ao ScrapCade os botões de um controle que ele não conhece (os genéricos). Aperte cada botão que aparecer na tela.";
    case OPT_DESKTOP: return "Abre uma área de trabalho com gerenciador de arquivos, navegador e editor de texto. Para sair, feche a sessão e o ScrapCade volta.";
    case OPT_HELP: return "Mostra os atalhos que valem em todo jogo: Select + Start sai, Select + X abre o menu do emulador, salvar e carregar.";
    case OPT_BIOS: return "Confere as BIOS de cada console (PS1, PS2, Saturn, DS...). Verde: certa. Vermelho: falta ou é o arquivo errado.";
    case OPT_ATUALIZAR: return "Procura a versão nova do ScrapCade na internet, baixa e confere. Jogos e saves não são mexidos, e se a nova não ligar ele volta sozinho para a anterior.";
    case OPT_INSTALL: return "Copia o ScrapCade para o HD ou SSD do computador, para ligar sem o pen drive. ATENÇÃO: apaga tudo o que estiver no disco escolhido.";
    case OPT_INFO: return "Processador, placa de vídeo, memória, versão do ScrapCade e do kernel. Útil para pedir ajuda.";
    case OPT_REBOOT: return "Reinicia o computador.";
    case OPT_OFF: return "Desliga o computador. Os saves já ficam guardados.";
    }
    return "";
}

// Texto corrido quebrado em linhas que cabem na largura
static float drawParagrafo(const std::string &t, float x, float y, float w, float units, uint32_t col, float lh, float yMax) {
    std::stringstream ss(t);
    std::string pal, linha;
    auto sai = [&] { if (!linha.empty() && y + lh / 2 < yMax) T.draw(linha, x, y + lh / 2, units, false, col); y += lh; linha.clear(); };
    while (ss >> pal) {
        std::string tenta = linha.empty() ? pal : linha + " " + pal;
        if (T.width(tenta, units, false) > w && !linha.empty()) { sai(); linha = pal; }
        else linha = tenta;
    }
    sai();
    return y;
}

static void drawSystem() {
    if (!sysOpen) return;
    if (painelLinhas.empty()) montarPainel();
    float u = G.u, w = std::min(G.W - 24.0f * u, 440 * u * T.escala);   // letra maior: painel mais largo
    float rowH = 15 * u, passo = rowH + 1.6f * u;
    // secoes e opcoes numa lista so: mostra as linhas que cabem e rola junto com a selecao
    int total = int(painelLinhas.size());
    int vis = std::max(6, std::min(total, int((G.H - 110 * u) / passo)));
    int first = std::max(0, std::min(total - vis, painelPos(optSel) - vis / 2));
    float h = 22 * u + 10 * u + vis * passo + 14 * u + 22 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel("Sistema", x, y, w, h) + 8 * u;
    float lx = x + 12 * u, lw = (w - 24 * u) * 0.55f, rx = lx + lw + 14 * u, rw = x + w - 12 * u - rx;
    T.draw(secaoDe(optSel), lx, top + 5 * u, 11, false, 0xFF2B2E33);
    if (first > 0) T.draw("▲", lx + lw, top + 5 * u, 8, false, 0xFF5E6269, 2);
    if (first + vis < total) T.draw("▼", lx + lw, top + 13 * u + vis * passo + 3 * u, 8, false, 0xFF5E6269, 2);
    for (int i = first; i < first + vis && i < total; i++) {
        float ry = top + 13 * u + (i - first) * passo;
        int o = painelLinhas[i];
        if (o < 0) {   // titulo da secao: letra pequena cinza com um fio embaixo
            G.fillRect(lx, ry + rowH - 1.5f * u, lw, std::max(1.0f, u * 0.6f), 0xFFD5D8DC);
            T.draw(SECOES[-o - 1].nome, lx + 2 * u, ry + rowH / 2 + u, 7.5f, true, 0xFF8A9099);
            continue;
        }
        G.fillRect(lx, ry, lw, rowH, o == optSel ? 0xFF2CCF9A : 0xFF4A4D52);
        T.draw(optLabel(o), lx + 8 * u, ry + rowH / 2, 9.5f, false, 0xFFFFFFFF, 0, lw - 16 * u);
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
    // o que a opcao destacada faz
    ry += 58 * u;
    G.fillRect(rx, ry - 6 * u, rw, std::max(1.0f, u * 0.6f), 0xFFE3E5E8);
    T.draw("Sobre esta opção", rx, ry + 4 * u, 11, false, 0xFF2B2E33);
    drawParagrafo(optAjuda(optSel), rx, ry + 12 * u, rw, 9, 0xFF5E6269, 12.5f * u, y + h - 24 * u);
    static const char *H[] = {"ud", "escolher", "lr", "mudar", "a", "abrir", "b", "fechar"};
    panelHints(H, 8, x, y + h, w);
}

// Caixa "tem certeza?" generica: A faz, B cancela
static std::string confTitulo, confTexto;
static std::function<void()> confAcao;
static void abrirConfirmacao(const std::string &t, const std::string &x, std::function<void()> f) { confTitulo = t; confTexto = x; confAcao = f; }
static void drawConfirmacao() {
    if (confTitulo.empty()) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 380 * u * T.escala), lh = 12.5f * u * T.escala;
    float h = 22 * u + 12 * u + 4 * lh + 10 * u + 22 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel(confTitulo, x, y, w, h) + 12 * u;
    drawParagrafo(confTexto, x + 14 * u, top, w - 28 * u, 9.5f, 0xFF2B2E33, lh, y + h - 24 * u);
    static const char *H[] = {"a", "sim", "b", "não"};
    panelHints(H, 4, x, y + h, w);
}

static void drawProtecao() {
    if (!protecaoOn) return;
    G.fillRect(0, 0, float(G.W), float(G.H), 0xFF000000);
    // modo atracao (como os fliperamas parados): a cena de um jogo sorteado a cada 10 s, escurecida, com nome e console
    static long cenaVez = -1;
    static SDL_Texture *cena = nullptr;
    static std::string cenaNome, cenaSis;
    long vez = long(time(nullptr) / 10);
    if (vez != cenaVez) {
        cenaVez = vez;
        if (cena) { SDL_DestroyTexture(cena); cena = nullptr; }
        std::vector<const Game *> com;
        for (auto &g : all) if (!coverOf(g, "-fundo").empty()) com.push_back(&g);
        if (!com.empty()) {
            const Game *g = com[(unsigned(vez) * 2654435761u) % com.size()];
            cena = IMG_LoadTexture(G.r, coverOf(*g, "-fundo").c_str());
            cenaNome = g->title;
            cenaSis = g->sys->name;
        }
    }
    if (cena) {
        int cw = 0, chh = 0;
        SDL_QueryTexture(cena, nullptr, nullptr, &cw, &chh);
        float esc = std::max(G.W / float(std::max(1, cw)), G.H / float(std::max(1, chh)));
        SDL_FRect d{(G.W - cw * esc) / 2, (G.H - chh * esc) / 2, cw * esc, chh * esc};
        SDL_SetTextureColorMod(cena, 110, 110, 110);
        SDL_RenderCopyF(G.r, cena, nullptr, &d);
        float u = G.u;
        G.fillRect(0, G.H - 46 * u, float(G.W), 46 * u, 0x99000000);
        T.draw(cenaNome, 18 * u, G.H - 30 * u, 13, true, 0xFFFFFFFF, 0, G.W * 0.6f);
        T.draw(cenaSis, 18 * u, G.H - 13 * u, 9, false, 0xFFB8BEC8, 0);
    }
    float u = G.u, lw = 300 * u, lh = lw * 200 / 1489;   // logo de pixel (1489x200)
    unsigned semente = unsigned(time(nullptr) / 10);   // muda de lugar a cada 10 s
    semente = semente * 1103515245u + 12345u;
    float x = float(semente % 1000) / 1000 * std::max(1.0f, G.W - lw - 40 * u) + 20 * u;
    semente = semente * 1103515245u + 12345u;
    float y = float(semente % 1000) / 1000 * std::max(1.0f, G.H - lh - 60 * u) + 20 * u;
    if (SDL_Texture *t = image(DATA + "/web/logo.png")) {
        SDL_FRect d{x, y, lw, lh};
        SDL_RenderCopyF(G.r, t, nullptr, &d);
    }
    time_t now = time(nullptr);
    tm lt;
    localtime_r(&now, &lt);
    char hora[16];
    strftime(hora, sizeof hora, prefs["relogio"] == "12" ? "%I:%M %p" : "%H:%M", &lt);
    T.draw(hora, x + lw / 2, y + lh + 18 * u, 16, true, 0xCCFFFFFF, 1);
}

static void drawEnergia() {
    if (!energiaMenu) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 300 * u * T.escala), rh = 17 * u * T.escala;
    float h = 22 * u + 8 * u + 4 * rh + 8 * u + 22 * u, x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel("Energia", x, y, w, h) + 8 * u;
    for (int i = 0; i < 4; i++) {
        bool sel = energiaMenu - 1 == i;
        if (sel) G.fillRect(x, top + i * rh, w, rh, 0xFF2FD09A);
        T.draw(ENERGIA_OPCOES[i], x + 14 * u, top + i * rh + rh / 2, 10, sel, 0xFF2B2E33);
    }
    static const char *H[] = {"ud", "escolher", "a", "fazer", "b", "cancelar"};
    panelHints(H, 6, x, y + h, w);
}

static void drawTextPanel() {
    if (textTitle.empty()) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 460 * u), lh = 12.5f * u;
    float h = 22 * u + 16 * u + textLines.size() * lh + 22 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel(textTitle, x, y, w, h) + 12 * u;
    for (size_t i = 0; i < textLines.size(); i++) {
        // tela de BIOS: verde = certo, vermelho = falta ou arquivo errado, ambar = opcional
        const std::string &l = textLines[i];
        uint32_t c = l.rfind("✓", 0) == 0 ? 0xFF1E9C4A : l.rfind("✗", 0) == 0 ? 0xFFD0362B
                   : l.rfind("•", 0) == 0 ? 0xFFB7791F : 0xFF2B2E33;
        T.draw(l, x + 14 * u, top + i * lh + lh / 2, 9, false, c, 0, w - 28 * u);
    }
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
    textLines.push_back("Sistema: ScrapCade OS v" + ver.substr(ver.find('=') + 1) + " (Linux " + un.release + ")" + buf);
    textLines.push_back("Jogos: " + std::to_string(all.size()) + ", em " + ROMS);
    textLines.push_back("Emuladores: " + emus);
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
        "   Select + Start: sair do jogo (salva sozinho e continua de onde parou)",
        "   Select + X: menu do emulador     Select + RB / LB: salvar / carregar",
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

// ------------------------------------------------------------------ Samba e SSH (liga e desliga)

static bool servico(const std::string &nome, const char *padrao) {
    std::ifstream in("/userdata/system/configs/" + nome);
    std::string v;
    if (!(in >> v)) v = padrao;
    return v == "on";
}
static void trocarServico(const std::string &nome, const char *padrao) {
    bool on = !servico(nome, padrao);
    mkdirs("/userdata/system/configs");
    std::ofstream("/userdata/system/configs/" + nome) << (on ? "on" : "off") << "\n";
    std::thread([] { if (system("/etc/init.d/S91tos-rede restart") != 0) {} }).detach();
    if (nome == "samba") toast(on ? "Pasta na rede ligada: no Windows, abra \\\\ScrapCade\\jogos" : "Pasta na rede desligada.", 4000);
    else toast(on ? "SSH ligado: entre com root e a senha do administrador." : "SSH desligado.", 4000);
}

// ------------------------------------------------------------------ Bluetooth (bluetoothctl)

struct Disp { std::string mac, nome; bool pareado; };
static int btStage = 0;   // 0 fechado, 1 lista, 2 conectando, 3 resultado
static std::vector<Disp> btLista;
static bool btBusy = false;
static int btSel = 0;
static std::string btMsg;
static std::mutex btMu;

static bool temBluetooth() {
    DIR *d = opendir("/sys/class/bluetooth");
    if (!d) return false;
    bool tem = false;
    while (dirent *e = readdir(d)) if (e->d_name[0] != '.') tem = true;
    closedir(d);
    return tem;
}

static std::vector<Disp> btDevices(const std::string &filtro) {
    std::vector<Disp> v;
    std::stringstream ss(Rede::run("bluetoothctl devices " + filtro));
    std::string l;
    while (std::getline(ss, l)) {   // "Device AA:BB:CC:DD:EE:FF Nome"
        if (l.compare(0, 7, "Device ") != 0 || l.size() < 25) continue;
        std::string mac = l.substr(7, 17), nome = l.size() > 25 ? l.substr(25) : mac;
        v.push_back({mac, nome, false});
    }
    return v;
}

static void btScan() {
    if (btBusy) return;
    btBusy = true;
    std::thread([] {
        Rede::run("bluetoothctl power on");
        Rede::run("bluetoothctl --timeout 10 scan on");
        auto todos = btDevices("");
        auto par = btDevices("Paired");
        for (auto &d : todos)
            for (auto &p : par)
                if (p.mac == d.mac) d.pareado = true;
        std::sort(todos.begin(), todos.end(), [](const Disp &a, const Disp &b) { return a.pareado > b.pareado; });
        std::lock_guard<std::mutex> l(btMu);
        btLista = todos;
        btBusy = false;
        redraw();
    }).detach();
}

static void btConnect() {
    Disp d;
    {
        std::lock_guard<std::mutex> l(btMu);
        if (btSel >= int(btLista.size())) return;
        d = btLista[btSel];
    }
    btStage = 2;
    std::thread([d] {
        if (!d.pareado) Rede::run("bluetoothctl --timeout 15 pair " + d.mac);
        Rede::run("bluetoothctl trust " + d.mac);
        std::string r = Rede::run("bluetoothctl --timeout 10 connect " + d.mac);
        bool ok = r.find("Connection successful") != std::string::npos ||
                  Rede::run("bluetoothctl info " + d.mac).find("Connected: yes") != std::string::npos;
        std::lock_guard<std::mutex> l(btMu);
        btMsg = ok ? d.nome + " conectado. Nas próximas vezes ele conecta sozinho."
                   : "Não conectou em " + d.nome + ". Deixe o controle em modo de pareamento e tente de novo.";
        btStage = 3;
        redraw();
    }).detach();
}

static void drawBluetooth() {
    if (!btStage) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 460 * u), lh = 14 * u;
    std::vector<std::string> L;
    std::vector<uint32_t> C;
    auto add = [&](const std::string &t, uint32_t cc = 0xFF2B2E33) { L.push_back(t); C.push_back(cc); };
    std::vector<Disp> v;
    bool busy;
    {
        std::lock_guard<std::mutex> l(btMu);
        v = btLista;
        busy = btBusy;
    }
    const char *H1[] = {"ud", "escolher", "a", "conectar", "x", "procurar de novo", "b", "voltar"};
    const char *H3[] = {"b", "voltar"};
    const char *const *H = H1;
    int nh = 8;
    if (btStage == 1) {
        if (!temBluetooth()) {
            add("Nenhum Bluetooth encontrado neste computador.", 0xFFD0362B);
            add("Um adaptador USB de Bluetooth resolve (são baratos).", 0xFF5E6269);
        } else {
            add("Coloque o controle em modo de pareamento:", 0xFF5E6269);
            add("PS4/PS5: Share + PS   •   Switch: botão do pareamento   •   Xbox: botão de conectar", 0xFF5E6269);
            add("");
            if (busy && v.empty()) add("Procurando...");
            else if (v.empty()) add("Nenhum aparelho encontrado. Aperte X para procurar de novo.");
            for (size_t i = 0; i < v.size() && i < 9; i++)
                add(std::string(int(i) == btSel ? "▶  " : "    ") + v[i].nome + (v[i].pareado ? "   (pareado)" : ""),
                    int(i) == btSel ? 0xFF1E9C4A : 0xFF2B2E33);
            if (busy && !v.empty()) add("procurando mais...", 0xFF5E6269);
        }
    } else {
        add(btStage == 2 ? "Conectando..." : btMsg, btMsg.rfind("Não", 0) == 0 ? 0xFFD0362B : 0xFF1E9C4A);
        H = H3;
        nh = btStage == 2 ? 0 : 2;
    }
    float h = 22 * u + 16 * u + L.size() * lh + 26 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel("Bluetooth", x, y, w, h) + 10 * u;
    for (size_t i = 0; i < L.size(); i++) T.draw(L[i], x + 14 * u, top + i * lh + lh / 2, 9.5f, false, C[i], 0, w - 28 * u);
    if (nh) panelHints(H, nh, x, y + h, w);
}

// ------------------------------------------------------------------ som (amixer) e teste de som


static void readVolume() {
    std::string o = Rede::run("amixer sget Master");
    size_t a = o.find('['), b = o.find("%]");
    if (a == std::string::npos || b == std::string::npos || b < a) { volume = -1; return; }
    volume = atoi(o.c_str() + a + 1);
    if (o.find("[off]") != std::string::npos) volume = 0;
}

static void setVolume(int v) {
    if (volume < 0) return;
    volume = std::max(0, std::min(100, v));
    std::string pct = std::to_string(volume) + "%";
    Rede::run("amixer -q sset Master " + pct + (volume == 0 ? " mute" : " unmute"));
    Rede::run("amixer -q sset PCM 100% unmute");
    mkdirs("/userdata/system/configs");
    std::ofstream("/userdata/system/configs/volume") << pct << (volume == 0 ? " mute" : " unmute") << "\n";
}

// Toca dois tons curtos (o som do ScrapCade) para conferir se o som sai
static void testSound() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { toast("Sem som: " + std::string(SDL_GetError()), 4000); return; }
    SDL_AudioSpec want{}, got{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (!dev) { toast("Sem som: " + std::string(SDL_GetError()), 4000); SDL_QuitSubSystem(SDL_INIT_AUDIO); return; }
    std::vector<int16_t> buf;
    for (int nota = 0; nota < 2; nota++) {
        float f = nota ? 880.0f : 659.25f;
        int n = got.freq * 28 / 100;
        for (int i = 0; i < n; i++) {
            float env = std::min(1.0f, std::min(i, n - i) / (got.freq * 0.02f));
            int16_t v = int16_t(std::sin(i * 6.2831853f * f / got.freq) * 9000 * env);
            buf.push_back(v);
            buf.push_back(v);
        }
    }
    SDL_QueueAudio(dev, buf.data(), Uint32(buf.size() * sizeof(int16_t)));
    SDL_PauseAudioDevice(dev, 0);
    SDL_Delay(650);
    SDL_CloseAudioDevice(dev);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

// ------------------------------------------------------------------ rede e Wi-Fi (rede.h)

static void redraw();   // pede um desenho novo (chamado de outras threads)
static int netStage = 0;    // 0 fechado, 1 lista, 2 senha, 3 conectando, 4 resultado
static std::mutex netMu;
static std::vector<Rede::Wifi> nets;
static bool netBusy = false;
static int netSel = 0;
static std::string netMsg, senha;
static int oskRow = 0, oskCol = 0;
static bool oskUpper = false, oskShow = false;
static const char *OSK[] = {"1234567890", "qwertyuiop", "asdfghjkl@", "zxcvbnm.-_", "!#$%&*+=?/"};
static const char *OSK_ESP[] = {"Aa", "espaço", "apagar", "ver", "conectar"};

static void netScan() {
    if (netBusy) return;
    netBusy = true;
    std::thread([] {
        auto v = Rede::scan();
        std::lock_guard<std::mutex> l(netMu);
        nets = v;
        netBusy = false;
        redraw();   // redesenha
    }).detach();
}

static void openRede() {
    netStage = 1;
    netSel = 0;
    netMsg.clear();
    netScan();
}

static void netConnect() {
    Rede::Wifi w;
    {
        std::lock_guard<std::mutex> l(netMu);
        if (netSel >= int(nets.size())) return;
        w = nets[netSel];
    }
    netStage = 3;
    std::string pw = senha;
    std::thread([w, pw] {
        bool ok = Rede::connect(w, pw);
        std::lock_guard<std::mutex> l(netMu);
        netMsg = ok ? "Conectado em " + w.name + "." : "Não conectou em " + w.name + ". Confira a senha e tente de novo.";
        netStage = 4;
        redraw();
    }).detach();
}

static char oskChar(int r, int c) {
    char ch = OSK[r][c];
    return oskUpper ? char(toupper((unsigned char)ch)) : ch;
}

static void drawRede() {
    if (!netStage) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 460 * u), lh = 14 * u;
    std::vector<std::string> L;
    std::vector<uint32_t> C;
    auto add = [&](const std::string &t, uint32_t c = 0xFF2B2E33) { L.push_back(t); C.push_back(c); };
    std::vector<Rede::Wifi> v;
    bool busy;
    {
        std::lock_guard<std::mutex> l(netMu);
        v = nets;
        busy = netBusy;
    }
    const char *H1[] = {"ud", "escolher", "a", "conectar", "x", "procurar de novo", "b", "voltar"};
    const char *H2[] = {"ud lr", "teclas", "a", "digitar", "b", "apagar", "start", "conectar"};
    const char *H4[] = {"b", "voltar"};
    const char *const *H = H1;
    int nh = 8;
    std::string ip = ipAddr();
    if (netStage == 1) {
        for (auto &c : Rede::cabos())
            add("Cabo (" + c.dev + "): " + (c.link ? "ligado" : "sem cabo"), c.link ? 0xFF1E9C4A : 0xFF5E6269);
        add(ip.empty() || ip == "sem rede" ? "Sem conexão." : "Conectado. Endereço: " + ip, 0xFF2B2E33);
        add("");
        if (!Rede::temWifi()) {
            add("Nenhuma placa de Wi-Fi encontrada (ou ela precisa de um driver que o ScrapCade", 0xFFD0362B);
            add("ainda não tem). Veja Registros/diagnostico.txt na página de Arquivos.", 0xFFD0362B);
        } else if (busy && v.empty()) {
            add("Procurando redes Wi-Fi...");
        } else if (v.empty()) {
            add("Nenhuma rede Wi-Fi encontrada. Aperte X para procurar de novo.");
        }
        for (size_t i = 0; i < v.size() && i < 9; i++)
            add(std::string(int(i) == netSel ? "▶  " : "    ") + v[i].name + (v[i].connected ? "   (conectado)" : v[i].saved ? "   (salva)" : ""),
                int(i) == netSel ? 0xFF1E9C4A : 0xFF2B2E33);
    } else if (netStage == 2) {
        std::string nome;
        {
            std::lock_guard<std::mutex> l(netMu);
            if (netSel < int(v.size())) nome = v[netSel].name;
        }
        add("Senha do Wi-Fi \"" + nome + "\":");
        add("  " + (oskShow ? senha : std::string(senha.size(), '*')) + "_", 0xFF1E9C4A);
        add("(o teclado do notebook também funciona: digite e aperte Enter)", 0xFF5E6269);
        H = H2;
    } else {
        add(netStage == 3 ? "Conectando..." : netMsg, netMsg.rfind("Não", 0) == 0 ? 0xFFD0362B : 0xFF1E9C4A);
        H = H4;
        nh = netStage == 3 ? 0 : 2;
    }
    float kh = netStage == 2 ? 6 * 18 * u : 0;
    float h = 22 * u + 16 * u + L.size() * lh + kh + 26 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel("Rede e Wi-Fi", x, y, w, h) + 10 * u;
    for (size_t i = 0; i < L.size(); i++) T.draw(L[i], x + 14 * u, top + i * lh + lh / 2, 9.5f, false, C[i], 0, w - 28 * u);
    if (netStage == 2) {   // teclado na tela
        float ky = top + L.size() * lh + 4 * u, kw = (w - 28 * u) / 10;
        for (int rr = 0; rr < 6; rr++) {
            int cols = rr < 5 ? 10 : 5;
            float cw = rr < 5 ? kw : (w - 28 * u) / 5;
            for (int cc = 0; cc < cols; cc++) {
                bool sel = rr == oskRow && cc == oskCol;
                float bx = x + 14 * u + cc * cw, by = ky + rr * 18 * u;
                G.roundRect(bx + u, by + u, cw - 2 * u, 16 * u, 3 * u, sel ? 0xFF2CCF9A : 0xFF4A4D52);
                std::string t = rr < 5 ? std::string(1, oskChar(rr, cc)) : OSK_ESP[cc];
                T.draw(t, bx + cw / 2, by + 9 * u, rr < 5 ? 10 : 8.5f, rr < 5, 0xFFFFFFFF, 1);
            }
        }
    }
    if (nh) panelHints(H, nh, x, y + h, w);
}

// ------------------------------------------------------------------ configurar controle (controle generico)

struct Passo { const char *key, *nome; bool eixo; };
static const Passo PASSOS[] = {
    {"a", "A  (o botão de baixo: confirmar)", false}, {"b", "B  (o botão da direita: voltar)", false},
    {"x", "X  (o botão da esquerda)", false}, {"y", "Y  (o botão de cima)", false},
    {"back", "Select", false}, {"start", "Start", false},
    {"leftshoulder", "LB  (ombro esquerdo, L1)", false}, {"rightshoulder", "RB  (ombro direito, R1)", false},
    {"lefttrigger", "LT  (gatilho esquerdo, L2)", false}, {"righttrigger", "RT  (gatilho direito, R2)", false},
    {"dpup", "Direcional para CIMA", false}, {"dpdown", "Direcional para BAIXO", false},
    {"dpleft", "Direcional para a ESQUERDA", false}, {"dpright", "Direcional para a DIREITA", false},
    {"leftx", "Analógico esquerdo: empurre para a DIREITA", true}, {"lefty", "Analógico esquerdo: empurre para BAIXO", true},
    {"rightx", "Analógico direito: empurre para a DIREITA", true}, {"righty", "Analógico direito: empurre para BAIXO", true},
};
static const int NPASSOS = int(sizeof(PASSOS) / sizeof(PASSOS[0]));
static int ctlStep = -1;   // -1 fechado
static SDL_JoystickID ctlId = -1;
static SDL_Joystick *ctlJoy = nullptr;
static std::vector<std::string> ctlMap;
static Uint32 ctlWait = 0;   // espera soltar antes do proximo passo

static void ctlStart(SDL_Joystick *j) {
    ctlJoy = j;
    ctlId = SDL_JoystickInstanceID(j);
    ctlMap.assign(NPASSOS, "");
    ctlStep = 0;
    ctlWait = SDL_GetTicks() + 600;
}

static void ctlFinish() {
    char guid[64];
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(ctlJoy), guid, sizeof guid);
    std::string nome = SDL_JoystickName(ctlJoy) ? SDL_JoystickName(ctlJoy) : "Controle";
    std::replace(nome.begin(), nome.end(), ',', ' ');
    std::string m = std::string(guid) + "," + nome + ",";
    for (int i = 0; i < NPASSOS; i++)
        if (!ctlMap[i].empty()) m += std::string(PASSOS[i].key) + ":" + ctlMap[i] + ",";
    m += "platform:Linux,";
    if (SDL_GameControllerAddMapping(m.c_str()) < 0) {
        toast("Não deu para salvar o controle: " + std::string(SDL_GetError()), 4000);
    } else {
        mkdirs("/userdata/system/configs");
        std::ofstream("/userdata/system/configs/controles.txt", std::ios::app) << m << "\n";
        std::ofstream("/tmp/gamecontrollerdb.txt", std::ios::app) << m << "\n";   // o RetroArch le este
        for (int i = 0; i < SDL_NumJoysticks(); i++)
            if (SDL_JoystickGetDeviceInstanceID(i) == ctlId && SDL_IsGameController(i)) SDL_GameControllerOpen(i);
        toast("Controle configurado: vale no menu e nos jogos.", 4000);
    }
    ctlStep = -1;
}

// Um botao, direcional (hat) ou eixo do controle que esta sendo configurado
static void ctlInput(const SDL_Event &e) {
    if (ctlStep < 0 || SDL_GetTicks() < ctlWait) return;
    std::string v;
    if (e.type == SDL_JOYBUTTONDOWN && e.jbutton.which == ctlId) v = "b" + std::to_string(e.jbutton.button);
    if (e.type == SDL_JOYHATMOTION && e.jhat.which == ctlId && e.jhat.value)
        v = "h" + std::to_string(e.jhat.hat) + "." + std::to_string(e.jhat.value);
    if (e.type == SDL_JOYAXISMOTION && e.jaxis.which == ctlId && std::abs(int(e.jaxis.value)) > 24000) {
        int a = e.jaxis.axis;
        if (PASSOS[ctlStep].eixo) v = "a" + std::to_string(a) + (e.jaxis.value < 0 ? "~" : "");
        else v = std::string(e.jaxis.value < 0 ? "-" : "+") + "a" + std::to_string(a);
    }
    if (v.empty()) return;
    for (int i = 0; i < ctlStep; i++)
        if (ctlMap[i] == v) return;   // ja usado: ignora (evita pegar o mesmo botao duas vezes)
    ctlMap[ctlStep] = v;
    ctlWait = SDL_GetTicks() + 450;
    if (++ctlStep >= NPASSOS) ctlFinish();
}

static void ctlSkip() {
    if (ctlStep < 0) return;
    if (++ctlStep >= NPASSOS) ctlFinish();
}

static void drawControle() {
    if (ctlStep < 0) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 440 * u), h = 150 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel("Configurar controle", x, y, w, h) + 14 * u;
    T.draw(SDL_JoystickName(ctlJoy) ? SDL_JoystickName(ctlJoy) : "Controle", x + 14 * u, top, 9, false, 0xFF5E6269, 0, w - 28 * u);
    T.draw("Passo " + std::to_string(ctlStep + 1) + " de " + std::to_string(NPASSOS), x + w - 14 * u, top, 9, false,
           0xFF5E6269, 2);
    T.draw("Aperte no controle:", x + 14 * u, top + 24 * u, 10, false, 0xFF2B2E33);
    T.draw(PASSOS[ctlStep].nome, x + 14 * u, top + 44 * u, 13, true, 0xFF1E9C4A, 0, w - 28 * u);
    T.draw("Se o controle não tiver esse botão: Espaço no teclado pula.  Esc cancela.", x + 14 * u, top + 72 * u, 8.5f,
           false, 0xFF5E6269, 0, w - 28 * u);
    float bw = w - 28 * u;
    G.roundRect(x + 14 * u, top + 88 * u, bw, 6 * u, 3 * u, 0xFFE3E5E8);
    G.roundRect(x + 14 * u, top + 88 * u, std::max(6 * u, bw * ctlStep / NPASSOS), 6 * u, 3 * u, 0xFF2CCF9A);
}

// ------------------------------------------------------------------ navegador de arquivos (pen drive e SCRAP-DADOS)
// Dentro do proprio ScrapCade: escolher uma imagem para um tema seu (modo 1) ou trazer jogos do pen drive (modo 2).

struct ItemNav { std::string nome, caminho; bool pasta; long long tam; };
static int navModo = 0;   // 0 fechado, 1 imagem do tema, 2 jogos, 3 gerenciador de arquivos
static std::string navClip;          // arquivo/pasta copiado ou recortado
static bool navClipMover = false;
static int navAcao = 0;              // 0 fechado, 1 lista de acoes, 2 confirmar apagar
static int navAcaoSel = 0;
static std::vector<std::string> navAcoes;
static std::atomic<bool> navRecarregar{false};
static const std::string LIXEIRA = "/userdata/Lixeira";
static void arqRenomear();
static void arqNovaPasta();
static void arqColar();
static void arqApagar();
static void arqClonar();
static std::string navDir;   // "" = escolher entre pen drive e SCRAP-DADOS
static std::vector<std::string> navRaizes;
static std::vector<ItemNav> navItens;
static int navSel = 0;
static std::atomic<bool> navCopiando{false};
static std::mutex navMu;
static std::string navMsg;

static bool navImagem(const std::string &n) { return endsWith(n, ".png") || endsWith(n, ".jpg") || endsWith(n, ".jpeg"); }

static void navListar() {
    navItens.clear();
    navSel = 0;
    if (navDir.empty()) {
        navRaizes.clear();
        auto raiz = [](const std::string &nome, const std::string &c) { navItens.push_back({nome, c, true, 0}); navRaizes.push_back(c); };
        if (Usb::attach()) raiz("Pen drive", Usb::MOUNT);
        if (navModo == 3) {   // gerenciador: direto nas pastas de jogos, como no iChroma
            raiz("roms", ROMS);
            raiz("Jogos novos", INBOX);
            raiz("saves", "/userdata/saves");
            raiz("bios", "/userdata/bios");
            raiz("themes", THEMES_DIR);
            if (Org::isDir(LIXEIRA)) raiz("Lixeira", LIXEIRA);
            raiz("SCRAP-DADOS (tudo)", "/userdata");
        } else raiz("SCRAP-DADOS (jogos, temas, saves)", "/userdata");
        return;
    }
    std::vector<ItemNav> ps, fs;
    if (DIR *d = opendir(navDir.c_str())) {
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            if (n.empty() || n[0] == '.') continue;
            std::string c = navDir + "/" + n;
            struct stat st;
            if (stat(c.c_str(), &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) ps.push_back({n, c, true, 0});
            else if (navModo >= 2 || navImagem(n)) fs.push_back({n, c, false, (long long)st.st_size});
        }
        closedir(d);
    }
    auto cmp = [](const ItemNav &a, const ItemNav &b) { return strcasecmp(a.nome.c_str(), b.nome.c_str()) < 0; };
    std::sort(ps.begin(), ps.end(), cmp);
    std::sort(fs.begin(), fs.end(), cmp);
    navItens.insert(navItens.end(), ps.begin(), ps.end());
    navItens.insert(navItens.end(), fs.begin(), fs.end());
}

static void abrirNavegador(int modo) {
    navModo = modo;
    navDir = "";
    { std::lock_guard<std::mutex> l(navMu); navMsg = ""; }
    navListar();
    // um lugar so (sem pen drive) no modo jogos: avisa; no modo imagem ja entra na SCRAP-DADOS
    if (navItens.size() == 1 && modo == 1) { navDir = navItens[0].caminho; navListar(); }
}

static void navAviso(const std::string &m) { std::lock_guard<std::mutex> l(navMu); navMsg = m; redraw(); }

static std::string tamBonito(long long b) {
    char t[32];
    if (b >= 1073741824LL) snprintf(t, sizeof t, "%.1f GB", b / 1073741824.0);
    else if (b >= 1048576) snprintf(t, sizeof t, "%.1f MB", b / 1048576.0);
    else snprintf(t, sizeof t, "%lld KB", (b + 1023) / 1024);
    return t;
}

// Imagem escolhida -> tema novo em themes/<nome da imagem>/fundo.<ext>, ja aplicado
static void navUsarImagem(const ItemNav &it) {
    std::string base = it.nome.substr(0, it.nome.rfind('.'));
    std::string ext = lower(it.nome.substr(it.nome.rfind('.')));
    if (ext == ".jpeg") ext = ".jpg";
    std::string dir = Arquivos::freeName(THEMES_DIR, base);
    mkdirs(dir);
    if (!Arquivos::copy(it.caminho, dir + "/fundo" + ext)) { navAviso("Não deu para copiar a imagem."); return; }
    std::ofstream(dir + "/tema.txt") << "nome=" << dir.substr(dir.rfind('/') + 1) << "\nbase=" << THEME_NAMES[theme] << "\n";
    sync();
    temaUser = -1;
    scanTemas();
    std::string pasta = dir.substr(dir.rfind('/') + 1);
    for (size_t i = 0; i < temasUser.size(); i++)
        if (temasUser[i].pasta == pasta) { temaUser = int(i); theme = temasUser[i].base; }
    prefs["tema_usuario"] = pasta;
    prefs["theme"] = std::to_string(theme);
    savePrefs();
    clearCards();
    clearBacks();
    navModo = 0;
    toast("Tema \"" + pasta + "\" criado. Mude o nome ou a cor em themes/" + pasta + "/tema.txt", 6000);
}

// Jogo (arquivo ou pasta) -> "Jogos novos": o organizador leva ao console certo, com capa e dados
static void navCopiarJogo(const ItemNav &it) {
    if (navCopiando) return;
    navCopiando = true;
    navAviso("Copiando " + it.nome + "...");
    std::thread([it] {
        std::string tmp = "/userdata/.copiando";
        mkdirs(tmp);
        std::string dst = tmp + "/" + it.nome;
        Arquivos::del(dst);
        bool ok = Arquivos::copy(it.caminho, dst);
        if (ok) ok = rename(dst.c_str(), Arquivos::freeName(INBOX, it.nome).c_str()) == 0;
        if (!ok) Arquivos::del(dst);
        sync();
        navCopiando = false;
        navAviso(ok ? it.nome + " copiado: vai aparecer no console certo em alguns segundos."
                    : "Não deu para copiar " + it.nome + " (falta espaço?).");
    }).detach();
}

static void drawNavegador() {
    if (!navModo) return;
    if (navRecarregar.exchange(false)) { int sel = navSel; navListar(); navSel = std::min(sel, std::max(0, int(navItens.size()) - 1)); }
    // visual do gerenciador do iChroma: linhas largas, a escolhida em verde, o tipo a direita, contagem embaixo
    float u = G.u, e = T.escala, w = std::min(G.W - 24.0f * u, 560 * u * e), rh = 17 * u * e;
    int vis = std::max(5, std::min(12, int((G.H - 160 * u) / rh)));
    std::string msg;
    { std::lock_guard<std::mutex> l(navMu); msg = navMsg; }
    int n = int(navItens.size()), first = std::max(0, std::min(n - vis, navSel - vis / 2));
    std::string onde = navDir.empty() ? (navModo == 3 ? "pastas de jogos" : "onde procurar?")
                       : navDir.rfind(Usb::MOUNT, 0) == 0 ? "Pen drive" + navDir.substr(Usb::MOUNT.size())
                                                          : "SCRAP-DADOS" + navDir.substr(9);
    if (onde.size() > 46) onde = "..." + onde.substr(onde.size() - 43);
    std::string titulo = navModo == 3 ? "Arquivos" : navModo == 1 ? "Imagem para o tema" : "Trazer jogos";
    float barra = 18 * u, h = 22 * u + 6 * u + vis * rh + 6 * u + barra + 22 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel(titulo + "  •  " + onde, x, y, w, h) + 6 * u;
    if (n == 0)
        T.draw(navModo == 1 ? "Nenhuma imagem (.png ou .jpg) aqui." : "Pasta vazia.", x + 14 * u, top + rh / 2, 9.5f, false, 0xFF5E6269);
    for (int i = first; i < n && i < first + vis; i++) {
        const ItemNav &it = navItens[i];
        float ry = top + (i - first) * rh;
        bool sel = i == navSel;
        if (sel) G.fillRect(x, ry, w, rh, 0xFF2FD09A);
        std::string tipo = it.pasta ? "pasta" : tamBonito(it.tam);
        float tw = T.width(tipo, 8.5f, false);
        // pasta: barra no fim (a fonte nao tem o desenho de pasta)
        T.draw(it.nome + (it.pasta ? "  /" : ""), x + 14 * u, ry + rh / 2, 9.5f, true, 0xFF2B2E33, 0, w - tw - 44 * u);
        T.draw(tipo, x + w - 14 * u, ry + rh / 2, 8.5f, false, sel ? 0xFF2B2E33 : 0xFF8A9099, 2);
    }
    // barra de baixo: aviso, o que esta na memoria ou quantos itens
    float by = y + h - 22 * u - barra;
    G.fillRect(x, by, w, barra, 0xFFE6E8EB);
    std::string info = msg;
    uint32_t ic = msg.rfind("Não", 0) == 0 ? 0xFFD0362B : 0xFF1E9C4A;
    if (info.empty()) {
        ic = 0xFF5E6269;
        if (navDir.empty() && navModo != 3 && navRaizes.size() == 1) info = "Nenhum pen drive encontrado: coloque um e abra de novo.";
        else if (navModo == 3 && !navClip.empty()) info = "Na memória: " + navClip.substr(navClip.rfind('/') + 1) + "  •  X cola aqui";
        else if (navModo == 1) info = "Escolha uma imagem: ela vira o fundo de um tema seu.";
        else if (navModo == 2 && !navDir.empty()) info = "A copia o jogo (ou a pasta) para \"Jogos novos\"; ele vai sozinho ao console certo.";
        else info = n == 1 ? "1 item" : std::to_string(n) + " itens";
    }
    T.draw(info, x + 12 * u, by + barra / 2, 8.5f, false, ic, 0, w - 24 * u);
    bool naPasta = navSel < n && navItens[navSel].pasta;
    bool pastaJogo = navModo == 2 && naPasta && !navDir.empty();
    const char *H1[] = {"ud", "escolher", "a", naPasta ? "abrir" : (navModo == 1 ? "usar esta imagem" : "copiar"), "b", "voltar"};
    const char *H2[] = {"ud", "escolher", "a", "abrir", "x", "copiar a pasta toda", "b", "voltar"};
    const char *H3[] = {"ud", "escolher", "a", "abrir", "y", "ações", "b", "voltar"};
    const char *H4[] = {"ud", "escolher", "a", "abrir", "y", "ações", "x", "colar aqui", "b", "voltar"};
    if (navModo == 3) {
        if (navClip.empty() || navDir.empty()) panelHints(H3, 8, x, y + h, w);
        else panelHints(H4, 10, x, y + h, w);
    } else panelHints(pastaJogo ? H2 : H1, pastaJogo ? 8 : 6, x, y + h, w);
    // lista de acoes do gerenciador (botao Y) e a confirmacao de apagar
    if (navAcao) {
        std::vector<std::string> A = navAcoes;
        std::string alvo = navSel < int(navItens.size()) ? navItens[navSel].nome : "";
        if (navAcao == 2) A = {navDir == LIXEIRA ? "Apagar de vez: " + alvo : "Mandar para a Lixeira: " + alvo};
        float aw = std::min(w * 0.7f, 320 * u * e), ah = 22 * u + 8 * u + A.size() * rh + 8 * u + 22 * u;
        float ax = (G.W - aw) / 2, ay = (G.H - ah) / 2;
        float at = whitePanel(navAcao == 2 ? "Confirmar" : "O que fazer?", ax, ay, aw, ah) + 8 * u;
        for (size_t i = 0; i < A.size(); i++) {
            bool sel = navAcao == 1 && int(i) == navAcaoSel;
            if (sel) G.fillRect(ax, at + i * rh, aw, rh, 0xFF2FD09A);
            T.draw(A[i], ax + 12 * u, at + i * rh + rh / 2, 9.5f, sel, navAcao == 2 ? 0xFFD0362B : 0xFF2B2E33, 0, aw - 24 * u);
        }
        const char *HA[] = {"a", "fazer", "b", "cancelar"};
        const char *HC[] = {"a", "sim", "b", "nao"};
        panelHints(navAcao == 2 ? HC : HA, 4, ax, ay + ah, aw);
    }
}


// ------------------------------------------------------------------ jogos gratis (homebrew com licenca livre)
// Baixados do release oficial de cada autor no GitHub e conferidos pelo SHA-256 antes de entrar.
// sis: pasta do console (a capa, feita da tela do proprio jogo, vem do sistema e vai para <sis>/images)
// dentro: quando o autor publica um .zip, o arquivo do jogo que sai de dentro dele (o SHA-256 e o do .zip)
struct Gratis { const char *nome, *console, *licenca, *autor, *url, *arquivo, *sha, *sis, *dentro; };
static const Gratis GRATIS[] = {
    {"µCity", "Game Boy Color", "GPLv3", "AntonioND", "https://github.com/AntonioND/ucity/releases/download/v1.3/ucity.gbc",
     "µCity.gbc", "9422ee2ca7b7ea1d46b58b2a429fff3f354dfd3e732dee1e7ae6220f148ce6e0", "gbc", ""},
    {"Nova the Squirrel", "NES", "GPLv3", "NovaSquirrel",
     "https://github.com/NovaSquirrel/NovaTheSquirrel/releases/download/v1.0.6a/nova.nes", "Nova the Squirrel.nes",
     "e4780e90b9d1587489bfb797d2ca395be21371ea9262fa9f87f99324ec6960ab", "nes", ""},
    {"Libbet and the Magic Floor", "Game Boy", "zlib", "pinobatch", "https://github.com/pinobatch/libbet/releases/download/v0.08/libbet.gb",
     "Libbet and the Magic Floor.gb", "3607412031c8287cf878299ce96e581e85b852dde703806343b95576fa3ff1a9", "gb", ""},
    {"Freedoom: Phase 1", "Doom", "BSD", "Freedoom", "https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip",
     "Freedoom - Phase 1.wad", "3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59", "doom",
     "freedoom-0.13.0/freedoom1.wad"},
    {"Freedoom: Phase 2", "Doom", "BSD", "Freedoom", "https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip",
     "Freedoom - Phase 2.wad", "3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59", "doom",
     "freedoom-0.13.0/freedoom2.wad"},
};
static const int NGRATIS = int(sizeof(GRATIS) / sizeof(GRATIS[0]));
static bool gratisOn = false;
static int gratisSel = 0;
static std::atomic<bool> gratisBaixando{false};
static std::mutex gratisMu;
static std::string gratisMsg;

static void abrirGratis() { gratisOn = true; gratisSel = 0; std::lock_guard<std::mutex> l(gratisMu); gratisMsg = ""; }

static long long sizeOfFile(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 ? (long long)st.st_size : -1;
}

static bool gratisTem(const Gratis &g) {   // ja esta na biblioteca (em qualquer console) ou chegando
    for (auto &sys : SYSTEMS) if (isFile(ROMS + "/" + sys.id + "/" + g.arquivo)) return true;
    return isFile(INBOX + "/" + g.arquivo);
}

static void gratisBaixar(int i) {
    if (gratisBaixando) return;
    gratisBaixando = true;
    { std::lock_guard<std::mutex> l(gratisMu); gratisMsg = std::string("Baixando ") + GRATIS[i].nome + "..."; }
    redraw();
    std::thread([i] {
        const Gratis &g = GRATIS[i];
        std::string tmp = "/userdata/.copiando";
        mkdirs(tmp);
        std::string f = tmp + "/" + g.arquivo;
        bool zip = g.dentro[0] != 0;
        std::string baixa = zip ? tmp + "/gratis.zip" : f;   // o SHA-256 confere o que veio do autor
        std::string r = Rede::run("curl -fsSL -m 600 -o '" + baixa + "' '" + g.url + "' && sha256sum '" + baixa + "'");
        bool ok = r.rfind(g.sha, 0) == 0;
        if (ok && zip) {
            Rede::run("unzip -p '" + baixa + "' '" + g.dentro + "' > '" + f + "'");
            ok = sizeOfFile(f) > 0;
        }
        if (zip) remove(baixa.c_str());
        if (ok) {
            // a capa vai antes do jogo: quando ele aparecer no carrossel, ja aparece com capa
            std::string nome = g.arquivo, stem = nome.substr(0, nome.rfind('.'));
            std::string de = DATA + "/capas-gratis/" + stem + ".png", para = ROMS + "/" + g.sis + "/images/" + stem + ".png";
            if (isFile(de) && !isFile(para)) {
                mkdirs(ROMS + "/" + g.sis + "/images");
                std::ifstream a(de, std::ios::binary);
                std::ofstream b(para, std::ios::binary);
                b << a.rdbuf();
            }
            ok = rename(f.c_str(), Arquivos::freeName(INBOX, g.arquivo).c_str()) == 0;
        } else remove(f.c_str());
        sync();
        std::lock_guard<std::mutex> l(gratisMu);
        gratisMsg = ok ? std::string(g.nome) + " baixado: aparece no " + g.console + " em alguns segundos."
                       : r.empty() ? "Não deu para baixar: confira a internet (Rede e Wi-Fi)."
                                   : "O arquivo baixado não confere (SHA-256): nada foi colocado.";
        gratisBaixando = false;
        redraw();
    }).detach();
}

static void drawGratis() {
    if (!gratisOn) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 520 * u * T.escala), lh = 14 * u * T.escala;
    std::string msg;
    { std::lock_guard<std::mutex> l(gratisMu); msg = gratisMsg; }
    std::vector<std::string> L;
    std::vector<uint32_t> C;
    auto add = [&](const std::string &t, uint32_t c = 0xFF2B2E33) { L.push_back(t); C.push_back(c); };
    add("Jogos de fãs com licença livre: pode baixar, jogar e passar adiante.", 0xFF5E6269);
    add("");
    for (int i = 0; i < NGRATIS; i++) {
        const Gratis &g = GRATIS[i];
        add(std::string(i == gratisSel ? "▶  " : "    ") + g.nome + "   (" + g.console + ", " + g.licenca + ", de " + g.autor + ")" +
                (gratisTem(g) ? "   - já tem" : ""),
            i == gratisSel ? 0xFF1E9C4A : 0xFF2B2E33);
    }
    add("");
    add(msg.empty() ? "A baixa do site oficial do autor e confere antes de colocar no console." : msg,
        msg.rfind("Não", 0) == 0 || msg.rfind("O arquivo", 0) == 0 ? 0xFFD0362B : 0xFF1E9C4A);
    float h = 22 * u + 16 * u + L.size() * lh + 26 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel("Jogos grátis", x, y, w, h) + 10 * u;
    for (size_t i = 0; i < L.size(); i++) T.draw(L[i], x + 14 * u, top + i * lh + lh / 2, 9.5f, false, C[i], 0, w - 28 * u);
    static const char *H[] = {"ud", "escolher", "a", "baixar", "b", "voltar"};
    panelHints(H, 6, x, y + h, w);
}

// Copia saves, perfis e configuracoes para o pen drive (pasta ScrapCade-saves/<data>)
static void copiarSaves() {
    if (!Usb::attach()) { toast("Coloque um pen drive para guardar a cópia dos saves.", 5000); return; }
    toast("Copiando os saves para o pen drive...", 60000);
    std::thread([] {
        char d[32];
        time_t t = time(nullptr);
        tm lt;
        localtime_r(&t, &lt);
        strftime(d, sizeof d, "%Y-%m-%d_%Hh%M", &lt);
        std::string dst = Usb::MOUNT + "/ScrapCade-saves/" + d;
        bool ok = system(("mount -o remount,rw " + Usb::MOUNT).c_str()) == 0;
        if (ok) {
            mkdirs(dst);
            ok = Arquivos::copy("/userdata/saves", dst + "/saves");
            if (Arquivos::isDirP("/userdata/system/perfis")) Arquivos::copy("/userdata/system/perfis", dst + "/perfis");
            if (isFile("/userdata/system/perfis.txt")) Arquivos::copy("/userdata/system/perfis.txt", dst + "/perfis.txt");
            if (Arquivos::isDirP("/userdata/system/configs")) Arquivos::copy("/userdata/system/configs", dst + "/configs");
            sync();
        }
        if (system(("mount -o remount,ro " + Usb::MOUNT).c_str()) != 0) {}
        toast(ok ? std::string("Saves copiados para o pen drive: ScrapCade-saves/") + d : "Não deu para gravar no pen drive (cheio ou protegido?).",
              8000);
        redraw();
    }).detach();
}

// ------------------------------------------------------------------ atualizar o ScrapCade (tos-atualizar, A/B)

static int updStage = 0;   // 0 fechado, 1 procurando, 2 resultado da procura, 3 baixando, 4 terminou
static std::string updAtual, updNova, updMB, updMsg;
static int updPct = 0;
static std::mutex updMu;

static std::string versaoBonita(const std::string &b) {   // 0.20.0-beta -> v0.20.0-beta; 202611132145 -> 13/11/2026 21:45
    bool data = b.size() == 12 && b.find_first_not_of("0123456789") == std::string::npos;
    if (!data) return !b.empty() && isdigit((unsigned char)b[0]) ? "v" + b : b;
    return b.substr(6, 2) + "/" + b.substr(4, 2) + "/" + b.substr(0, 4) + " " + b.substr(8, 2) + ":" + b.substr(10, 2);
}

static void abrirAtualizar() {
    updStage = 1;
    std::thread([] {
        std::string r = Rede::run("/usr/bin/tos-atualizar verificar");
        while (!r.empty() && (r.back() == '\n' || r.back() == '\r')) r.pop_back();
        std::vector<std::string> f;
        std::stringstream ss(r);
        std::string x;
        while (std::getline(ss, x, '|')) f.push_back(x);
        std::lock_guard<std::mutex> l(updMu);
        updAtual = f.size() > 0 ? f[0] : "";
        updNova = f.size() > 1 ? f[1] : "erro";
        updMB = f.size() > 2 ? f[2] : "0";
        updStage = 2;
        redraw();
    }).detach();
}

static void instalarAtualizacao() {
    remove("/tmp/tos-atualizar.txt");
    updPct = 0;
    updMsg = "Começando";
    updStage = 3;
    std::thread([] {
        if (system("/usr/bin/tos-atualizar instalar >> /userdata/system/logs/atualizar.log 2>&1") != 0) {}
    }).detach();
}

static void drawAtualizar() {
    if (!updStage) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 440 * u), lh = 15 * u;
    std::vector<std::string> L;
    std::vector<uint32_t> C;
    auto add = [&](const std::string &t, uint32_t cc = 0xFF2B2E33) { L.push_back(t); C.push_back(cc); };
    std::string atual, nova, mb;
    {
        std::lock_guard<std::mutex> l(updMu);
        atual = updAtual; nova = updNova; mb = updMB;
    }
    const char *H2[] = {"a", "atualizar", "b", "voltar"};
    const char *H1[] = {"b", "voltar"};
    const char *H3[] = {"a", "reiniciar agora", "b", "depois"};
    const char *const *H = H1;
    int nh = 2;
    if (updStage == 1) {
        add("Procurando a versão mais nova...");
    } else if (updStage == 2) {
        add("Versão instalada: " + versaoBonita(atual));
        if (nova == "erro") {
            add("Não deu para procurar: confira a internet (Rede e Wi-Fi).", 0xFFD0362B);
        } else if (nova == "pronta") {
            add("A versão nova já foi baixada: reinicie para usar.", 0xFF1E9C4A);
            add("A versão de agora fica guardada: se a nova não ligar, o ScrapCade OS volta para ela sozinho.", 0xFF5E6269);
            H = H3;
            nh = 4;
        } else if (nova == "baixando") {
            add("A versão nova está sendo baixada em segundo plano.", 0xFF1E9C4A);
            add("Pode continuar jogando: quando terminar, aparece um aviso para reiniciar.", 0xFF5E6269);
        } else if (nova.empty()) {
            add("O ScrapCade já está na versão mais nova.", 0xFF1E9C4A);
        } else {
            add("Versão nova: " + versaoBonita(nova) + "   (" + mb + " MB)", 0xFF1E9C4A);
            add("");
            add("Os jogos e os saves não são mexidos. A versão de agora fica guardada:");
            add("se a nova não ligar, o ScrapCade volta para ela sozinho.", 0xFF5E6269);
            H = H2;
            nh = 4;
        }
    } else {
        if (updStage == 3) {
            std::ifstream in("/tmp/tos-atualizar.txt");
            int pc = 0;
            std::string msg;
            if (in >> pc) { std::getline(in, msg); updPct = pc; if (!msg.empty()) updMsg = msg.substr(1); }
            if (updPct >= 100 || updMsg.rfind("ERRO", 0) == 0) updStage = 4;
        }
        bool erro = updMsg.rfind("ERRO", 0) == 0;
        add(updStage == 3 ? "Atualizando... não desligue." : (erro ? "A atualização não terminou." : "ScrapCade atualizado!"),
            erro ? 0xFFD0362B : 0xFF2B2E33);
        add(updMsg + (updStage == 3 ? "  (" + std::to_string(updPct) + "%)" : ""), erro ? 0xFFD0362B : 0xFF1E9C4A);
        nh = updStage == 4 ? 2 : 0;
        if (updStage == 4 && !erro) { H = H3; nh = 4; }
    }
    float h = 22 * u + 16 * u + L.size() * lh + 22 * u + (updStage == 3 ? 14 * u : 0);
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel("Atualizar o ScrapCade OS", x, y, w, h) + 12 * u;
    for (size_t i = 0; i < L.size(); i++) T.draw(L[i], x + 14 * u, top + i * lh + lh / 2, 9.5f, false, C[i], 0, w - 28 * u);
    if (updStage == 3) {
        float by = top + L.size() * lh, bw = w - 28 * u;
        G.roundRect(x + 14 * u, by, bw, 8 * u, 4 * u, 0xFFE3E5E8);
        G.roundRect(x + 14 * u, by, std::max(8 * u, bw * updPct / 100.0f), 8 * u, 4 * u, 0xFF2CCF9A);
    }
    if (nh) panelHints(H, nh, x, y + h, w);
}

// ------------------------------------------------------------------ instalar no disco (tos-instalar)

static int instStage = 0;   // 0 fechado, 1 escolher o disco, 2 confirmar, 3 instalando, 4 terminou
struct Disk { std::string dev, gb, model; };
static std::vector<Disk> disks;
static int diskSel = 0;
static bool copyGames = true;
static int instPct = 0;
static std::string instMsg;

static void openInstall() {
    disks.clear();
    FILE *f = popen("/usr/bin/tos-instalar listar 2>/dev/null", "r");
    char line[512];
    while (f && fgets(line, sizeof line, f)) {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        size_t a = l.find('|'), b = l.find('|', a + 1);
        if (a == std::string::npos || b == std::string::npos) continue;
        disks.push_back({l.substr(0, a), l.substr(a + 1, b - a - 1), l.substr(b + 1)});
    }
    if (f) pclose(f);
    diskSel = 0;
    instStage = 1;
}

static void startInstall() {
    std::string dev = disks[diskSel].dev;
    remove("/tmp/tos-instalar.txt");
    instPct = 0;
    instMsg = "Começando";
    std::string cmd = "/usr/bin/tos-instalar '" + dev + "' " + (copyGames ? "jogos" : "") +
                      " >> /userdata/system/logs/instalar.log 2>&1";
    std::thread([cmd] { if (system(cmd.c_str()) != 0) fprintf(stderr, "scrapcade: tos-instalar terminou com erro\n"); }).detach();
    instStage = 3;
}

static void drawInstall() {
    if (!instStage) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 440 * u), lh = 15 * u;
    std::vector<std::string> L;
    std::vector<uint32_t> C;
    auto add = [&](const std::string &t, uint32_t c = 0xFF2B2E33) { L.push_back(t); C.push_back(c); };
    const char *H1[] = {"ud", "escolher", "a", "continuar", "b", "voltar"};
    const char *H2[] = {"lr", "jogos sim/não", "a", "instalar", "b", "cancelar"};
    const char *H4[] = {"b", "fechar"};
    const char *const *H = H1;
    int nh = 6;
    if (instStage == 1) {
        add("Escolha o disco onde o ScrapCade OS vai ficar:");
        add("");
        if (disks.empty()) add("Nenhum outro disco encontrado (mínimo 4 GB).", 0xFFD0362B);
        for (size_t i = 0; i < disks.size(); i++)
            add(std::string(int(i) == diskSel ? "▶  " : "    ") + disks[i].model + "   " + disks[i].gb + " GB   (" +
                    disks[i].dev + ")",
                int(i) == diskSel ? 0xFF1E9C4A : 0xFF2B2E33);
        add("");
        add("O pen drive de onde o ScrapCade está rodando não aparece na lista.", 0xFF5E6269);
    } else if (instStage == 2) {
        const Disk &d = disks[diskSel];
        add("ATENÇÃO: isso APAGA TUDO o que está no disco:", 0xFFD0362B);
        add("    " + d.model + "   " + d.gb + " GB   (" + d.dev + ")", 0xFFD0362B);
        add("");
        add(std::string("Levar também os jogos, saves e BIOS deste pen drive:   ‹ ") + (copyGames ? "Sim" : "Não") + " ›");
        add("");
        add("Aperte A para instalar, ou B para cancelar.");
        H = H2;
    } else {
        if (instStage == 3) {
            std::ifstream in("/tmp/tos-instalar.txt");
            int pc = 0;
            std::string msg;
            if (in >> pc) { std::getline(in, msg); instPct = pc; instMsg = msg.empty() ? instMsg : msg.substr(1); }
            if (instPct >= 100 || instMsg.rfind("ERRO", 0) == 0) instStage = 4;
        }
        bool erro = instMsg.rfind("ERRO", 0) == 0;
        add(instStage == 3 ? "Instalando... não desligue o computador." : (erro ? "A instalação não terminou." : "ScrapCade instalado!"),
            erro ? 0xFFD0362B : 0xFF2B2E33);
        add("");
        add(instMsg + (instStage == 3 ? "  (" + std::to_string(instPct) + "%)" : ""), erro ? 0xFFD0362B : 0xFF1E9C4A);
        add("");
        if (instStage == 4 && !erro) add("Desligue, tire o pen drive e ligue de novo: o ScrapCade abre direto do disco.");
        if (instStage == 4 && erro) add("Detalhes em Registros/instalar.log, na página de Arquivos.", 0xFF5E6269);
        H = H4;
        nh = instStage == 4 ? 2 : 0;
    }
    float h = 22 * u + 16 * u + L.size() * lh + 22 * u + (instStage == 3 ? 14 * u : 0);
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel("Instalar no disco do computador", x, y, w, h) + 12 * u;
    for (size_t i = 0; i < L.size(); i++) T.draw(L[i], x + 14 * u, top + i * lh + lh / 2, 9.5f, false, C[i], 0, w - 28 * u);
    if (instStage == 3) {   // barra de progresso
        float by = top + L.size() * lh, bw = w - 28 * u;
        G.roundRect(x + 14 * u, by, bw, 8 * u, 4 * u, 0xFFE3E5E8);
        G.roundRect(x + 14 * u, by, std::max(8 * u, bw * instPct / 100.0f), 8 * u, 4 * u, 0xFF2CCF9A);
    }
    if (nh) panelHints(H, nh, x, y + h, w);
}

// true = a tela do instalador tratou o botao
enum Btn : int;
static bool installInput(int b);

// ------------------------------------------------------------------ pergunta: instalar no HD? (uma vez so)
// Rodando de um pen drive USB e com um HD/SSD no computador, pergunta uma vez se quer instalar.
// A resposta fica guardada; a opcao continua em Sistema > Instalar no disco do computador.
static const std::string PERGUNTOU_HD = "/userdata/system/configs/perguntou-instalar";
static std::atomic<int> hdPode{0};   // 0 vendo, 1 pode perguntar, 2 nao pergunta
static int hdStage = 0;              // 0 fechado, 1 perguntando
static int hdDiscos = 0;

static void verSePergunta() {
    std::thread([] {
        bool usb = false;
        std::string d = Usb::bootDisk();
        char buf[512];
        ssize_t n = d.empty() ? -1 : readlink(("/sys/block/" + d).c_str(), buf, sizeof buf - 1);
        if (n > 0) { buf[n] = 0; usb = std::string(buf).find("/usb") != std::string::npos; }
        int discos = 0;
        if (usb && !isFile(PERGUNTOU_HD)) {
            std::string l = Rede::run("/usr/bin/tos-instalar listar");
            for (char c : l) if (c == '\n') discos++;
        }
        hdDiscos = discos;
        hdPode = (usb && discos > 0 && !isFile(PERGUNTOU_HD)) ? 1 : 2;
    }).detach();
}

static void respondeuHD() {
    mkdirs("/userdata/system/configs");
    std::ofstream(PERGUNTOU_HD) << "sim\n";
    sync();
    hdStage = 0;
}

static void drawPerguntaHD() {
    if (!hdStage) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 460 * u * T.escala), lh = 15 * u * T.escala;
    const char *L[] = {"O ScrapCade está rodando do pen drive.",
                       "Dá para instalar no HD ou SSD deste computador e ligar sem o pen drive",
                       "(os jogos podem ir junto). Você escolhe o disco na próxima tela.",
                       "",
                       "Atenção: o disco escolhido é apagado.",
                       "Depois, se quiser: Sistema > Instalar no disco do computador."};
    uint32_t C[] = {0xFF2B2E33, 0xFF2B2E33, 0xFF2B2E33, 0xFF2B2E33, 0xFFD0362B, 0xFF5E6269};
    float h = 22 * u + 16 * u + 6 * lh + 26 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel("Instalar no computador?", x, y, w, h) + 10 * u;
    for (int i = 0; i < 6; i++) T.draw(L[i], x + 14 * u, top + i * lh + lh / 2, 9.5f, false, C[i], 0, w - 28 * u);
    static const char *H[] = {"a", "escolher o disco", "b", "agora não"};
    panelHints(H, 4, x, y + h, w);
}

// dir: -1/+1 = mudar valor (esquerda/direita); 0 = confirmar (A)
static void trocarNucleo(const Game *g);

static void optAction(int dir) {
    int d = dir == 0 ? 1 : dir;
    switch (optSel) {
    case OPT_ORDENAR: {
        prefs["ordem"] = std::to_string((ordemAtual() + d + 4) % 4);
        const Game *g = current();
        std::string k = g ? g->key() : "";
        applyFilter();
        for (size_t i = 0; i < view.size(); i++) if (view[i].key() == k) { target = int(i); pos = float(target); }
        break;
    }
    case OPT_VAZIOS: prefs["vazios"] = prefLigada("vazios", false) ? "0" : "1"; buildFilters(); break;
    case OPT_CAPAS:
        if (dir == 0) {
            remove((ROMS + "/.sem_capa").c_str());
            remove((ROMS + "/.sem_fundo").c_str());
            toast("As capas e os dados que faltavam vão ser procurados de novo (precisa de internet).", 5000);
        }
        return;
    case OPT_LIMPAR:
        if (dir == 0) {
            for (auto it = prefs.begin(); it != prefs.end();) it = it->first.rfind("quando|", 0) == 0 ? prefs.erase(it) : std::next(it);
            savePrefs();
            buildFilters();
            applyFilter();
            toast("\"Continuar jogando\" limpo.", 3000);
        }
        return;
    case OPT_RESUME: prefs["resume"] = prefLigada("resume", true) ? "0" : "1"; gravarJogoCfg(); break;
    case OPT_FILTRO: prefs["filtro"] = std::to_string((filtroImg() + d + 3) % 3); gravarJogoCfg(); break;
    case OPT_PROPORCAO: prefs["proporcao"] = std::to_string((proporcao() + d + 4) % 4); gravarJogoCfg(); break;
    case OPT_RUNAHEAD: prefs["runahead"] = prefLigada("runahead", false) ? "0" : "1"; gravarJogoCfg(); break;
    case OPT_FPS: prefs["fps"] = prefLigada("fps", false) ? "0" : "1"; gravarJogoCfg(); break;
    case OPT_NUCLEO: if (current()) trocarNucleo(current()); return;
    case OPT_RELOGIO: prefs["relogio"] = prefs["relogio"] == "12" ? "24" : "12"; break;
    case OPT_PROTECAO: prefs["protecao"] = std::to_string((protecaoIdx() + d + 4) % 4); break;
    case OPT_MUSICA:
        prefs["musica"] = prefs["musica"] == "1" ? "0" : "1";
        if (prefs["musica"] == "1") {
            if (musicas().empty()) toast("Coloque músicas (.ogg, .mp3, .flac, .wav) na pasta music da SCRAP-DADOS.", 6000);
            prepararSons();
            tocarMusica();
        } else pararMusica();
        break;
    case OPT_SONS: prefs["sons"] = prefs["sons"] == "1" ? "0" : "1"; prepararSons(); tocarSom(somOk); break;
    case OPT_SUSPENDER:
        if (dir == 0) { sysOpen = false; savePrefs(); render(); if (system("/usr/bin/tos-suspender") != 0) {} ultimaTecla = SDL_GetTicks(); }
        return;
    case OPT_DESLIGAR: prefs["desligar"] = std::to_string((desligarIdx() + d + 4) % 4); break;
    case OPT_ENVIAR:
        if (dir == 0) toast("No PC ou no celular, na mesma rede, abra http://" + ipAddr() + ":" + std::to_string(Envio::PORT) + " e arraste os jogos.", 6000);
        return;
    case OPT_PADRAO:
        if (dir == 0)
            abrirConfirmacao("Voltar tudo ao padrão",
                             "Tema, filtros, opções de jogo e do menu voltam ao padrão. Favoritos, saves, perfis e jogos ficam como estão. O ScrapCade reabre.", [] {
                for (auto it = prefs.begin(); it != prefs.end();) {
                    const std::string &k = it->first;
                    bool fica = k.rfind("fav|", 0) == 0 || k.rfind("plays|", 0) == 0 || k.rfind("quando|", 0) == 0 || k == "ultimo";
                    it = fica ? std::next(it) : prefs.erase(it);
                }
                savePrefs();
                gravarJogoCfg();
                restartMenu = true;   // reabre com os padroes
            });
        return;
    case OPT_JOGOS: if (dir == 0) { sysOpen = false; abrirNavegador(2); } return;
    case OPT_ARQUIVOS: if (dir == 0) { sysOpen = false; abrirNavegador(3); } return;
    case OPT_GRATIS: if (dir == 0) { sysOpen = false; abrirGratis(); } return;
    case OPT_BACKUP: if (dir == 0) copiarSaves(); return;
    case OPT_EJETAR:
        if (dir == 0) toast(Usb::eject() ? "Pode tirar o pen drive." : "Nenhum pen drive em uso.", 4000);
        return;
    case OPT_AUTOUPD:
        prefs["autoupd"] = prefs["autoupd"] == "0" ? "1" : "0";
        break;
    case OPT_FONTE:
        prefs["fonte"] = std::to_string((nivelFonte() + d + 3) % 3);
        T.escala = escalaFonte();
        T.clear();
        break;
    case OPT_THEME:
        if (dir == 0) { sysOpen = false; abrirNavegador(1); return; }
    {
        scanTemas();
        int total = NTEMAS + int(temasUser.size());
        int cur = temaUser >= 0 ? NTEMAS + temaUser : theme;
        cur = (cur + d + total) % total;
        if (cur < NTEMAS) { theme = cur; temaUser = -1; prefs["tema_usuario"] = ""; }
        else { temaUser = cur - NTEMAS; theme = temasUser[temaUser].base; prefs["tema_usuario"] = temasUser[temaUser].pasta; }
        prefs["theme"] = std::to_string(theme);
    }
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
    case OPT_PERFIL: if (dir == 0) { sysOpen = false; abrirPerfis(); } return;
    case OPT_ENERGIA:
        if (!energiaDisponivel()) return;
        aplicarEnergia((energia + (dir == 0 ? 1 : dir) + 3) % 3);
        toast(std::string("Energia: ") + ENERGIA[energia] +
              (energia == 0 ? " (bateria dura mais)" : energia == 2 ? " (o mais rápido possível)" : ""), 2500);
        return;
    case OPT_VOLUME:
        if (volume == -2) readVolume();
        if (dir == 0) testSound();
        else setVolume((volume < 0 ? 0 : volume) + dir * 10);
        return;
    case OPT_REDE: if (dir == 0) { sysOpen = false; openRede(); } return;
    case OPT_BLUETOOTH:
        if (dir != 0) return;
        sysOpen = false;
        btStage = 1;
        btSel = 0;
        btScan();
        return;
    case OPT_SAMBA: trocarServico("samba", "on"); return;
    case OPT_SSH: trocarServico("ssh", "off"); return;
    case OPT_CONTROLE:
        if (dir != 0) return;
        if (SDL_NumJoysticks() == 0) { toast("Nenhum controle ligado: ligue um controle USB ou pareie pelo Bluetooth.", 6000); return; }
        sysOpen = false;
        ctlStart(SDL_JoystickOpen(SDL_NumJoysticks() - 1));
        return;
    case OPT_DESKTOP:
        if (dir != 0) return;
        // o desktop (Fluxbox, no estilo do antiX) chega como /usr/bin/tos-desktop; ao sair dele, volta o ScrapCade
        if (isFile(DESKTOP)) {
            prefs["voltar"] = "desktop";
            savePrefs();
            restartMenu = true;
        } else {
            toast("O modo desktop entra numa próxima versão do ScrapCade", 4000);
        }
        return;
    case OPT_HELP: if (dir == 0) { sysOpen = false; openHelp(); } return;
    case OPT_INSTALL: if (dir == 0) { sysOpen = false; openInstall(); } return;
    case OPT_ATUALIZAR: if (dir == 0) { sysOpen = false; abrirAtualizar(); } return;
    case OPT_BIOS:
        if (dir != 0) return;
        sysOpen = false;
        textTitle = "BIOS dos consoles";
        textLines = Bios::report();
        for (auto &l : textLines) {
            size_t p = l.find("%IP%");
            if (p != std::string::npos) l.replace(p, 4, ipAddr());
        }
        return;
    case OPT_INFO: if (dir == 0) { sysOpen = false; openInfo(); } return;
    case OPT_REBOOT: if (dir == 0) powerOff(true); return;
    case OPT_OFF: if (dir == 0) powerOff(false); return;
    }
    savePrefs();
}

// Teclas e botoes enquanto um painel esta aberto. true = o painel tratou.
enum Btn : int { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_SELECT, B_OTHER, B_X, B_START, B_Y };

static bool tecladoInput(int b);
static bool perfisInput(int b);

static bool netInput(int b) {
    if (!netStage) return false;
    int n;
    {
        std::lock_guard<std::mutex> l(netMu);
        n = int(nets.size());
    }
    if (netStage == 1) {
        if (b == B_UP && n) netSel = (netSel + n - 1) % n;
        if (b == B_DOWN && n) netSel = (netSel + 1) % n;
        if (b == B_X) netScan();
        if (b == B_A && n) { senha.clear(); oskRow = oskCol = 0; netStage = 2; }
        if (b == B_B || b == B_SELECT) { netStage = 0; sysOpen = true; }
    } else if (netStage == 2) {
        int cols = oskRow < 5 ? 10 : 5;
        if (b == B_UP) oskRow = (oskRow + 5) % 6;
        if (b == B_DOWN) oskRow = (oskRow + 1) % 6;
        if (b == B_UP || b == B_DOWN) oskCol = std::min(oskCol, (oskRow < 5 ? 10 : 5) - 1);
        if (b == B_LEFT) oskCol = (oskCol + cols - 1) % cols;
        if (b == B_RIGHT) oskCol = (oskCol + 1) % cols;
        if (b == B_B && !senha.empty()) senha.pop_back();
        if (b == B_START) netConnect();
        if (b == B_SELECT) netStage = 1;
        if (b == B_A) {
            if (oskRow < 5) senha += oskChar(oskRow, oskCol);
            else if (oskCol == 0) oskUpper = !oskUpper;
            else if (oskCol == 1) senha += ' ';
            else if (oskCol == 2) { if (!senha.empty()) senha.pop_back(); }
            else if (oskCol == 3) oskShow = !oskShow;
            else netConnect();
        }
    } else if (netStage == 4) {
        if (b == B_B || b == B_A) { netStage = 1; netScan(); }
    }
    return true;
}

static bool navInput(int b) {
    if (!navModo) return false;
    int n = int(navItens.size());
    if (navAcao == 2) {   // confirmar apagar
        if (b == B_A) arqApagar();
        if (b == B_A || b == B_B) navAcao = 0;
        return true;
    }
    if (navAcao == 1) {   // lista de acoes
        int na = int(navAcoes.size());
        if (b == B_UP && na) navAcaoSel = (navAcaoSel + na - 1) % na;
        if (b == B_DOWN && na) navAcaoSel = (navAcaoSel + 1) % na;
        if (b == B_B) navAcao = 0;
        if (b == B_A && navAcaoSel < na) {
            std::string a = navAcoes[navAcaoSel];
            navAcao = 0;
            const ItemNav &it = navItens[navSel];
            if (a == "Copiar" || a == "Mover") {
                navClip = it.caminho;
                navClipMover = a == "Mover";
                navAviso(std::string(navClipMover ? "Recortado: " : "Copiado: ") + it.nome + ". Abra a pasta de destino e aperte X para colar.");
            } else if (a == "Colar aqui") arqColar();
            else if (a == "Clonar") arqClonar();
            else if (a == "Renomear") arqRenomear();
            else if (a == "Nova pasta") arqNovaPasta();
            else if (a.rfind("Apagar", 0) == 0) navAcao = 2;
        }
        return true;
    }
    bool mexe = navDir.rfind("/userdata", 0) == 0;   // pen drive: so leitura
    if (navModo == 3 && b == B_X && !navDir.empty()) {   // colar aqui (como no iChroma)
        if (navClip.empty()) navAviso("Não há nada na memória: use Y > Copiar ou Mover.");
        else if (!mexe) navAviso("Não dá para colar no pen drive: ele é só leitura.");
        else arqColar();
        return true;
    }
    if (navModo == 3 && b == B_Y && !navDir.empty()) {   // acoes do gerenciador
        bool valido = navSel < n && !navItens[navSel].caminho.empty();
        navAcoes.clear();
        if (valido) navAcoes.push_back("Copiar");
        if (valido && mexe) { navAcoes.push_back("Mover"); navAcoes.push_back("Clonar"); navAcoes.push_back("Renomear"); }
        if (!navClip.empty() && mexe) navAcoes.push_back("Colar aqui");
        if (mexe) navAcoes.push_back("Nova pasta");
        if (valido && mexe) navAcoes.push_back(navDir == LIXEIRA ? "Apagar de vez" : "Apagar (vai para a Lixeira)");
        navAcaoSel = 0;
        if (!navAcoes.empty()) navAcao = 1;
        return true;
    }
    if (b == B_UP && n) navSel = (navSel + n - 1) % n;
    if (b == B_DOWN && n) navSel = (navSel + 1) % n;
    auto subir = [] {
        bool raiz = std::find(navRaizes.begin(), navRaizes.end(), navDir) != navRaizes.end();
        navDir = raiz ? "" : navDir.substr(0, navDir.rfind('/'));
        navListar();
    };
    if (b == B_B) {
        if (navDir.empty()) { navModo = 0; sysOpen = true; }
        else subir();
        return true;
    }
    if (b == B_A && navSel < n) {
        ItemNav it = navItens[navSel];
        if (it.pasta && it.caminho.empty()) subir();
        else if (it.pasta) { navDir = it.caminho; navListar(); }
        else if (navModo == 1) navUsarImagem(it);
        else if (navModo == 3) navAviso("Use Y para copiar, mover, clonar, renomear ou apagar.");
        else navCopiarJogo(it);
    }
    if (b == B_X && navModo == 2 && navSel < n && !navDir.empty() && navItens[navSel].pasta && !navItens[navSel].caminho.empty())
        navCopiarJogo(navItens[navSel]);
    return true;
}

static bool hdInput(int b) {
    if (!hdStage) return false;
    if (b == B_A) { respondeuHD(); openInstall(); }
    else if (b == B_B || b == B_SELECT) { respondeuHD(); toast("Fica para depois: Sistema > Instalar no disco do computador", 5000); }
    return true;
}

static bool gratisInput(int b) {
    if (!gratisOn) return false;
    if (b == B_UP) gratisSel = (gratisSel + NGRATIS - 1) % NGRATIS;
    if (b == B_DOWN) gratisSel = (gratisSel + 1) % NGRATIS;
    if (b == B_A) gratisBaixar(gratisSel);
    if (b == B_B) { gratisOn = false; sysOpen = true; }
    return true;
}

static bool updInput(int b) {
    if (!updStage) return false;
    std::string nova;
    {
        std::lock_guard<std::mutex> l(updMu);
        nova = updNova;
    }
    if (updStage == 2 && b == B_A && nova == "pronta") powerOff(true);
    else if (updStage == 2 && b == B_A && !nova.empty() && nova != "erro" && nova != "baixando") instalarAtualizacao();
    else if (updStage == 4 && b == B_A && updMsg.rfind("ERRO", 0) != 0) powerOff(true);
    else if ((updStage == 2 || updStage == 4) && (b == B_B || b == B_A)) { updStage = 0; sysOpen = true; }
    return true;
}

static bool btInput(int b) {
    if (!btStage) return false;
    int n;
    {
        std::lock_guard<std::mutex> l(btMu);
        n = int(btLista.size());
    }
    if (btStage == 1) {
        if (b == B_UP && n) btSel = (btSel + n - 1) % n;
        if (b == B_DOWN && n) btSel = (btSel + 1) % n;
        if (b == B_X) btScan();
        if (b == B_A && n) btConnect();
        if (b == B_B || b == B_SELECT) { btStage = 0; sysOpen = true; }
    } else if (btStage == 3) {
        if (b == B_A || b == B_B) { btStage = 1; btScan(); }
    }
    return true;
}

static bool installInput(int b) {
    if (!instStage) return false;
    if (instStage == 1) {
        if (b == B_UP && !disks.empty()) diskSel = (diskSel + int(disks.size()) - 1) % int(disks.size());
        if (b == B_DOWN && !disks.empty()) diskSel = (diskSel + 1) % int(disks.size());
        if (b == B_A && !disks.empty()) instStage = 2;
        if (b == B_B || b == B_SELECT) { instStage = 0; sysOpen = true; }
    } else if (instStage == 2) {
        if (b == B_LEFT || b == B_RIGHT) copyGames = !copyGames;
        if (b == B_A) startInstall();
        if (b == B_B) instStage = 1;
    } else if (instStage == 4) {
        if (b == B_B || b == B_A) { instStage = 0; sysOpen = true; }
    }
    return true;   // instalando: nada mexe no menu
}

static bool confirmacaoInput(int b) {
    if (confTitulo.empty()) return false;
    if (b == B_A) { auto f = confAcao; confTitulo.clear(); confAcao = nullptr; if (f) f(); }
    else if (b == B_B) confTitulo.clear();
    return true;
}

static bool energiaInput(int b) {
    if (!energiaMenu) return false;
    if (b == B_UP) energiaMenu = (energiaMenu + 2) % 4 + 1;
    if (b == B_DOWN) energiaMenu = energiaMenu % 4 + 1;
    if (b == B_B) energiaMenu = 0;
    if (b == B_A) {
        int o = energiaMenu - 1;
        energiaMenu = 0;
        if (o == 0) { savePrefs(); render(); if (system("/usr/bin/tos-suspender") != 0) {} ultimaTecla = SDL_GetTicks(); }
        else if (o == 1) powerOff(true);
        else if (o == 2) powerOff(false);
    }
    return true;
}

static bool panelInput(Btn b) {
    if (ctlStep >= 0) return true;   // configurando controle: o menu nao mexe
    if (energiaInput(b)) return true;
    if (confirmacaoInput(b)) return true;
    if (tecladoInput(b)) return true;
    if (perfisInput(b)) return true;
    if (netInput(b)) return true;
    if (btInput(b)) return true;
    if (updInput(b)) return true;
    if (hdInput(b)) return true;
    if (navInput(b)) return true;
    if (gratisInput(b)) return true;
    if (installInput(b)) return true;
    if (!textTitle.empty()) {
        if (b == B_B || b == B_A || b == B_SELECT) { textTitle.clear(); sysOpen = true; }
        return true;
    }
    if (!sysOpen) return false;
    switch (b) {
    case B_UP: painelMover(-1); break;
    case B_DOWN: painelMover(1); break;
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
    readVolume();
    optSel = 0;
    readUsage();
    barsAt = SDL_GetTicks();
}

// ------------------------------------------------------------------ perfis de jogador (tela "Quem vai jogar?")
//
// Cada perfil tem as suas preferencias (favoritos, mais jogados, tema, ultimo jogo) e os seus saves.
// Os jogos sao os mesmos para todos. Dados em SCRAP-DADOS/system/perfis.txt e system/perfis/<id>/.
// O perfil 1 usa os saves que ja existiam (saves/<console>); os outros, saves/<console>/perfil-<id>.

std::vector<Perfil> perfis;
int perfilAtual = -1;
static const std::string PERFIS_FILE = "/userdata/system/perfis.txt";
static const std::string PERFIS_DIR = "/userdata/system/perfis";
static const std::string PERFIL_AGORA = "/tmp/tos-perfil";   // some ao desligar: ao ligar, pergunta de novo

struct Avatar { uint32_t cor; const char *simbolo; };
static const Avatar AVATARES[] = {
    {0xFF32E6A8, "★"}, {0xFF2B68C8, "♫"}, {0xFFC93636, "♥"}, {0xFFD9822B, "⚡"}, {0xFF6C47C9, "♛"},
    {0xFF1E9C4A, "♣"}, {0xFF2E9EC8, "✈"}, {0xFFE0B020, "☀"}, {0xFFE0356B, "♦"}, {0xFF4A4D52, "♠"},
    {0xFF8A5A2B, "☂"}, {0xFF19B0B0, "☺"},
};
static const int NAVATARES = int(sizeof(AVATARES) / sizeof(AVATARES[0]));
static int totalAvatares() { return NAVATARES + int(avatarExtras.size()); }

static std::string sha256(const std::string &t) {
    unsigned char d[EVP_MAX_MD_SIZE];
    unsigned int n = 0;
    EVP_Digest(t.data(), t.size(), d, &n, EVP_sha256(), nullptr);
    std::string h;
    char b[3];
    for (unsigned i = 0; i < n; i++) { snprintf(b, sizeof b, "%02x", d[i]); h += b; }
    return h;
}

static void loadPerfis() {
    perfis.clear();
    std::ifstream f(PERFIS_FILE);
    std::string l;
    while (std::getline(f, l)) {
        std::vector<std::string> c;
        std::stringstream ss(l);
        std::string x;
        while (std::getline(ss, x, '|')) c.push_back(x);
        if (c.size() >= 3) perfis.push_back({c[0], c[1], c.size() > 3 ? c[3] : "", std::max(0, atoi(c[2].c_str()))});
    }
}

static void savePerfis() {
    mkdirs(PERFIS_DIR);
    std::string tmp = PERFIS_FILE + ".tmp";
    {
        std::ofstream f(tmp);
        for (auto &p : perfis) f << p.id << "|" << p.nome << "|" << p.avatar << "|" << p.hash << "\n";
    }
    rename(tmp.c_str(), PERFIS_FILE.c_str());
    sync();
}

static std::string perfilNome() { return perfilAtual >= 0 ? perfis[perfilAtual].nome : "nenhum"; }

// Foto propria: system/perfis/<id>/avatar.png (ou .jpg), colocada pela pagina de Arquivos
static std::string fotoPerfil(const Perfil &p) {
    for (const char *e : {"/avatar.png", "/avatar.jpg", "/avatar.jpeg"})
        if (isFile(PERFIS_DIR + "/" + p.id + e)) return PERFIS_DIR + "/" + p.id + e;
    return "";
}

static void drawAvatar(const Perfil *p, int av, float x, float y, float s) {
    float u = G.u;
    std::string foto = p ? fotoPerfil(*p) : "";
    if (!foto.empty()) {
        if (SDL_Texture *t = image(foto)) {
            G.roundRect(x, y, s, s, 6 * u, 0xFF101428);
            G.imageFit(t, x, y, s, s);
            return;
        }
    }
    int idx = (p ? p->avatar : av) % totalAvatares();
    if (idx >= NAVATARES) {   // avatar do usuario (pasta themes)
        if (SDL_Texture *t = image(avatarExtras[idx - NAVATARES])) {
            G.roundRect(x, y, s, s, 6 * u, 0xFF101428);
            G.imageFit(t, x, y, s, s);
            return;
        }
        idx %= NAVATARES;
    }
    const Avatar &a = AVATARES[idx];
    G.gradientV(x, y, s, s, lighten(a.cor, 0.25f), darken(a.cor, 0.25f));
    G.roundRectStroke(x, y, s, s, 2 * u, std::max(1.0f, u), 0x40FFFFFF);
    T.draw(a.simbolo, x + s / 2, y + s * 0.47f, s / u * 0.5f, true, 0xFFFFFFFF, 1);
}

static void login(int i) {
    if (perfilAtual >= 0) savePrefs();
    perfilAtual = i;
    const Perfil &p = perfis[i];
    std::string dir = PERFIS_DIR + "/" + p.id;
    mkdirs(dir);
    std::string novo = dir + "/scrapcade.conf";
    // primeiro perfil: herda as preferencias de antes dos perfis (favoritos, tema...)
    if (!isFile(novo) && p.id == "1" && isFile("/userdata/system/scrapcade.conf")) {
        std::ifstream in("/userdata/system/scrapcade.conf", std::ios::binary);
        std::ofstream(novo, std::ios::binary) << in.rdbuf();
    }
    std::string engine = prefs["engine"];
    CONF_FILE = novo;
    prefs.clear();
    loadPrefs();
    if (!engine.empty() && prefs["engine"].empty()) prefs["engine"] = engine;
    aplicarTemaPrefs();
    T.escala = escalaFonte();
    T.clear();
    ANIM_MS = prefs["anim"] == "rapida" ? 130 : 200;
    showCovers = prefs["covers"] != "0";
    showBackdrop = prefs["backdrop"] != "0";
    clearCards();
    clearBacks();
    buildFilters();
    applyFilter();
    for (size_t k = 0; k < view.size(); k++)
        if (view[k].key() == prefs["ultimo"]) { target = int(k); pos = float(target); break; }
    std::ofstream(PERFIL_AGORA) << p.id << "|" << p.nome << "\n";   // o tos-jogar usa (saves do perfil)
    toast("Olá, " + p.nome + "!", 2500);
}

// ---- teclado na tela (nome e senha do perfil; o teclado do PC tambem funciona)
static bool tkOn = false, tkOculto = false, tkUp = false, tkVer = false;
static std::string tkTitulo, tkTexto;
static int tkR = 1, tkC = 0;
static std::function<void(bool)> tkFim;

static void abrirTeclado(const std::string &titulo, bool oculto, std::function<void(bool)> fim) {
    tkOn = true;
    tkTitulo = titulo;
    tkTexto.clear();
    tkOculto = oculto;
    tkVer = false;
    tkR = 1;
    tkC = 0;
    tkFim = fim;
}

static void fecharTeclado(bool ok) {
    tkOn = false;
    auto f = tkFim;
    if (f) f(ok);
}

static void drawTeclado() {
    if (!tkOn) return;
    float u = G.u, w = std::min(G.W - 24.0f * u, 440 * u), kh = 6 * 18 * u;
    float h = 22 * u + 16 * u + 2 * 14 * u + kh + 26 * u;
    float x = (G.W - w) / 2, y = (G.H - h) / 2;
    float top = whitePanel(tkTitulo, x, y, w, h) + 10 * u;
    std::string mostra = tkOculto && !tkVer ? std::string(tkTexto.size(), '*') : tkTexto;
    G.roundRect(x + 14 * u, top, w - 28 * u, 16 * u, 3 * u, 0xFFF1F2F4);
    T.draw(mostra + "_", x + 20 * u, top + 8 * u, 10, false, 0xFF1E9C4A, 0, w - 40 * u);
    float ky = top + 26 * u, kw = (w - 28 * u) / 10;
    static const char *ESP[] = {"Aa", "espaço", "apagar", "ver", "OK"};
    for (int rr = 0; rr < 6; rr++) {
        int cols = rr < 5 ? 10 : 5;
        float cw = rr < 5 ? kw : (w - 28 * u) / 5;
        for (int cc = 0; cc < cols; cc++) {
            bool sel = rr == tkR && cc == tkC;
            float bx = x + 14 * u + cc * cw, by = ky + rr * 18 * u;
            G.roundRect(bx + u, by + u, cw - 2 * u, 16 * u, 3 * u, sel ? 0xFF2CCF9A : 0xFF4A4D52);
            char ch = rr < 5 ? (tkUp ? char(toupper((unsigned char)OSK[rr][cc])) : OSK[rr][cc]) : 0;
            T.draw(rr < 5 ? std::string(1, ch) : ESP[cc], bx + cw / 2, by + 9 * u, rr < 5 ? 10 : 8.5f, rr < 5, 0xFFFFFFFF, 1);
        }
    }
    static const char *H[] = {"ud lr", "teclas", "a", "digitar", "b", "apagar", "start", "pronto"};
    panelHints(H, 8, x, y + h, w);
}

// ---- tela de perfis
static int profStage = 0;    // 0 fechada, 1 escolher, 2 avatar do perfil novo, 3 confirmar apagar
static int profSel = 0, profAvatar = 0;
static std::string profNomeNovo;

static void abrirPerfis() {
    loadPerfis();
    profStage = 1;
    profSel = perfilAtual >= 0 ? perfilAtual : 0;
}

static void criarPerfil() {
    abrirTeclado("Novo perfil: escreva o nome", false, [](bool ok) {
        while (!tkTexto.empty() && tkTexto.back() == ' ') tkTexto.pop_back();
        if (!ok || tkTexto.empty()) return;
        std::string n = tkTexto;
        std::replace(n.begin(), n.end(), '|', ' ');
        profNomeNovo = n.substr(0, 24);
        scanTemas();   // avatares colocados agora na pasta themes
        profAvatar = int(perfis.size()) % NAVATARES;
        profStage = 2;
    });
}

static void salvarPerfilNovo(const std::string &senha) {
    int maior = 0;
    for (auto &p : perfis) maior = std::max(maior, atoi(p.id.c_str()));
    perfis.push_back({std::to_string(maior + 1), profNomeNovo, senha.empty() ? "" : sha256(senha), profAvatar});
    savePerfis();
    profStage = 0;
    login(int(perfis.size()) - 1);
}

static void entrar(int i) {
    if (perfis[i].hash.empty()) { profStage = 0; login(i); return; }
    abrirTeclado("Senha de " + perfis[i].nome, true, [i](bool ok) {
        if (!ok) return;
        if (sha256(tkTexto) == perfis[i].hash) { profStage = 0; login(i); }
        else toast("Senha errada.", 2500);
    });
}

static void drawPerfis() {
    if (!profStage) return;
    float u = G.u;
    SDL_RenderCopy(G.r, background(0xFF2B68C8), nullptr, nullptr);
    G.fillRect(0, 0, float(G.W), float(G.H), 0x50000000);
    int n = int(perfis.size()) + 1;   // + "Novo perfil"
    if (profStage == 1 || profStage == 3) {
        if (SDL_Texture *lg = image(DATA + "/web/logo.png")) {   // logo do ScrapCade OS em cima (fundo transparente)
            float lh = G.H * 0.075f, lw = lh * 1489 / 200;
            G.imageFit(lg, (G.W - lw) / 2, G.H * 0.02f, lw, lh, true);
        }
        T.draw(perfis.empty() ? "Bem-vindo ao ScrapCade OS!" : "Quem vai jogar?", G.W / 2.0f, G.H * 0.18f, 20, true, 0xFFFFFFFF, 1);
        T.draw(perfis.empty() ? "Crie o seu perfil: cada pessoa tem os seus favoritos e os seus saves." :
                                "Cada perfil tem os seus favoritos e os seus saves.",
               G.W / 2.0f, G.H * 0.18f + 22 * u, 10, false, 0xDDFFFFFF, 1);
        float s = 86 * u, gap = 18 * u, tw = n * s + (n - 1) * gap;
        float x0 = std::min((G.W - tw) / 2, G.W / 2.0f - s / 2 - profSel * (s + gap)), y0 = G.H * 0.40f;
        if (tw <= G.W - 40 * u) x0 = (G.W - tw) / 2;
        for (int i = 0; i < n; i++) {
            bool sel = i == profSel;
            float k = sel ? 1.12f : 1.0f, ss = s * k, x = x0 + i * (s + gap) - (ss - s) / 2, y = y0 - (ss - s) / 2;
            if (x + ss < 0 || x > G.W) continue;
            if (i < int(perfis.size())) drawAvatar(&perfis[i], 0, x, y, ss);
            else {
                G.roundRect(x, y, ss, ss, 4 * u, 0xC0303338);
                T.draw("+", x + ss / 2, y + ss * 0.45f, ss / u * 0.5f, false, 0xFFFFFFFF, 1);
            }
            if (sel) G.roundRectStroke(x - 3 * u, y - 3 * u, ss + 6 * u, ss + 6 * u, 5 * u, 2.5f * u, 0xFF32E6A8);
            std::string nome = i < int(perfis.size()) ? perfis[i].nome : "Novo perfil";
            T.draw(nome, x + ss / 2, y + ss + 14 * u, sel ? 11 : 9.5f, sel, 0xFFFFFFFF, 1, s + gap);
            if (i < int(perfis.size()) && !perfis[i].hash.empty())
                T.draw("com senha", x + ss / 2, y + ss + 28 * u, 7.5f, false, 0xAAFFFFFF, 1);
        }
        if (profStage == 3 && profSel < int(perfis.size())) {
            float w = std::min(G.W - 24.0f * u, 400 * u), h = 92 * u, x = (G.W - w) / 2, y = (G.H - h) / 2;
            float top = whitePanel("Apagar perfil", x, y, w, h) + 14 * u;
            T.draw("Apagar o perfil " + perfis[profSel].nome + "?", x + 14 * u, top, 10, true, 0xFF2B2E33, 0, w - 28 * u);
            T.draw("Os jogos e os saves continuam guardados.", x + 14 * u, top + 16 * u, 9, false, 0xFF5E6269);
            static const char *HC[] = {"a", "apagar", "b", "voltar"};
            panelHints(HC, 4, x, y + h, w);
        }
        // barra de botoes no estilo Aurora
        float hh = HINT_H * u, hy = G.H - hh;
        G.fillRect(0, hy, float(G.W), hh, 0xF7FFFFFF);
        static const char *H[] = {"lr", "escolher", "a", "entrar", "y", "apagar perfil"};
        float hx = 12 * u, cy = hy + hh / 2, is = 15.5f * u;
        for (int i = 0; i < (perfis.empty() ? 4 : 6); i += 2) {
            std::stringstream ss2(H[i]);
            std::string kk;
            while (ss2 >> kk) {
                if (SDL_Texture *t = image(DATA + "/botoes/" + kk + ".png")) {
                    SDL_FRect d{hx, cy - is / 2, is, is};
                    SDL_RenderCopyF(G.r, t, nullptr, &d);
                }
                hx += is + u;
            }
            T.draw(H[i + 1], hx + 2 * u, cy, 9.5f, false, 0xFF5E6269);
            hx += 2 * u + T.width(H[i + 1], 9.5f, false) + 13 * u;
        }
    } else if (profStage == 2) {   // escolher o avatar do perfil novo
        T.draw("Escolha o avatar de " + profNomeNovo, G.W / 2.0f, G.H * 0.18f, 16, true, 0xFFFFFFFF, 1);
        int nav = totalAvatares(), rows = (nav + 5) / 6;
        float gap = 10 * u, s = std::min(58 * u, (G.H * 0.44f - (rows - 1) * gap) / rows);
        float tw = 6 * s + 5 * gap, x0 = (G.W - tw) / 2, y0 = G.H * 0.30f;
        for (int i = 0; i < nav; i++) {
            float x = x0 + (i % 6) * (s + gap), y = y0 + (i / 6) * (s + gap);
            drawAvatar(nullptr, i, x, y, s);
            if (i == profAvatar) G.roundRectStroke(x - 3 * u, y - 3 * u, s + 6 * u, s + 6 * u, 4 * u, 2.5f * u, 0xFF32E6A8);
        }
        T.draw("Avatares seus: imagens na pasta themes/avatares. Foto do perfil: avatar.png na pasta Perfis.",
               G.W / 2.0f, y0 + rows * (s + gap) + 6 * u, 8.5f, false, 0xCCFFFFFF, 1);
        T.draw("A: escolher este  •  depois você pode pôr uma senha (opcional)", G.W / 2.0f,
               y0 + rows * (s + gap) + 22 * u, 9, false, 0xFFFFFFFF, 1);
    }
}

// ------------------------------------------------------------------ acoes do gerenciador de arquivos

static void arqColar() {
    if (navClip.empty() || navDir.empty()) return;
    std::string origem = navClip, nome = navClip.substr(navClip.rfind('/') + 1), pasta = navDir;
    bool mover = navClipMover;
    if (pasta == origem || pasta.rfind(origem + "/", 0) == 0) { navAviso("Não dá para colar uma pasta dentro dela mesma."); return; }
    navAviso(std::string(mover ? "Movendo " : "Copiando ") + nome + "...");
    if (mover) navClip.clear();
    std::thread([origem, nome, pasta, mover] {
        std::string dst = Arquivos::freeName(pasta, nome);
        bool ok = mover && rename(origem.c_str(), dst.c_str()) == 0;   // mesma particao: so troca o nome
        if (!ok) {
            ok = Arquivos::copy(origem, dst);
            if (ok && mover) Arquivos::del(origem);
            if (!ok) Arquivos::del(dst);
        }
        sync();
        navAviso(ok ? nome + (mover ? " movido." : " copiado.") : "Não deu para colar " + nome + " (falta espaço?).");
        navRecarregar = true;
        redraw();
    }).detach();
}

// Clonar (como no iChroma): uma copia na mesma pasta, com outro nome
static void arqClonar() {
    if (navSel >= int(navItens.size()) || navItens[navSel].caminho.empty()) return;
    std::string origem = navItens[navSel].caminho, nome = navItens[navSel].nome, pasta = navDir;
    navAviso("Clonando " + nome + "...");
    std::thread([origem, nome, pasta] {
        std::string dst = Arquivos::freeName(pasta, nome);
        bool ok = Arquivos::copy(origem, dst);
        if (!ok) Arquivos::del(dst);
        sync();
        navAviso(ok ? nome + " clonado: " + dst.substr(dst.rfind('/') + 1) : "Não deu para clonar " + nome + " (falta espaço?).");
        navRecarregar = true;
        redraw();
    }).detach();
}

static void arqRenomear() {
    if (navSel >= int(navItens.size())) return;
    std::string velho = navItens[navSel].caminho, nome = navItens[navSel].nome;
    abrirTeclado("Novo nome para " + nome, false, [velho](bool ok) {
        while (!tkTexto.empty() && tkTexto.back() == ' ') tkTexto.pop_back();
        if (!ok || tkTexto.empty()) return;
        if (Arquivos::badName(tkTexto)) { navAviso("Nome inválido (sem / no nome)."); return; }
        std::string novo = velho.substr(0, velho.rfind('/') + 1) + tkTexto;
        if (isFile(novo) || Arquivos::isDirP(novo)) { navAviso("Já existe um item com esse nome."); return; }
        navAviso(rename(velho.c_str(), novo.c_str()) == 0 ? "Renomeado para " + tkTexto + "." : "Não deu para renomear.");
        sync();
        navRecarregar = true;
    });
    tkTexto = nome;
}

static void arqNovaPasta() {
    std::string pasta = navDir;
    abrirTeclado("Nome da pasta nova", false, [pasta](bool ok) {
        while (!tkTexto.empty() && tkTexto.back() == ' ') tkTexto.pop_back();
        if (!ok || tkTexto.empty()) return;
        if (Arquivos::badName(tkTexto)) { navAviso("Nome inválido (sem / no nome)."); return; }
        navAviso(mkdir((pasta + "/" + tkTexto).c_str(), 0755) == 0 ? "Pasta " + tkTexto + " criada." : "Não deu para criar a pasta.");
        navRecarregar = true;
    });
}

// Apagar manda para a Lixeira (SCRAP-DADOS/Lixeira), de onde da para trazer de volta; dentro dela, apaga de vez
static void arqApagar() {
    if (navSel >= int(navItens.size()) || navItens[navSel].caminho.empty()) return;
    std::string alvo = navItens[navSel].caminho, nome = navItens[navSel].nome;
    if (navClip == alvo) navClip.clear();
    bool ok;
    if (navDir == LIXEIRA) {
        ok = Arquivos::del(alvo);
        navAviso(ok ? nome + " apagado de vez." : "Não deu para apagar " + nome + ".");
    } else {
        mkdirs(LIXEIRA);
        ok = rename(alvo.c_str(), Arquivos::freeName(LIXEIRA, nome).c_str()) == 0;
        navAviso(ok ? nome + " foi para a Lixeira (pasta Lixeira, na SCRAP-DADOS)." : "Não deu para apagar " + nome + ".");
    }
    sync();
    navListar();
}

static bool tecladoInput(int b) {
    if (!tkOn) return false;
    int cols = tkR < 5 ? 10 : 5;
    if (b == B_UP) tkR = (tkR + 5) % 6;
    if (b == B_DOWN) tkR = (tkR + 1) % 6;
    if (b == B_UP || b == B_DOWN) tkC = std::min(tkC, (tkR < 5 ? 10 : 5) - 1);
    if (b == B_LEFT) tkC = (tkC + cols - 1) % cols;
    if (b == B_RIGHT) tkC = (tkC + 1) % cols;
    if (b == B_B) { if (!tkTexto.empty()) tkTexto.pop_back(); else fecharTeclado(false); }
    if (b == B_START) fecharTeclado(true);
    if (b == B_SELECT) fecharTeclado(false);
    if (b == B_A) {
        if (tkR < 5) tkTexto += tkUp ? char(toupper((unsigned char)OSK[tkR][tkC])) : OSK[tkR][tkC];
        else if (tkC == 0) tkUp = !tkUp;
        else if (tkC == 1) tkTexto += ' ';
        else if (tkC == 2) { if (!tkTexto.empty()) tkTexto.pop_back(); }
        else if (tkC == 3) tkVer = !tkVer;
        else fecharTeclado(true);
    }
    return true;
}

static bool perfisInput(int b) {
    if (!profStage) return false;
    int n = int(perfis.size()) + 1;
    if (profStage == 1) {
        if (b == B_LEFT) profSel = (profSel + n - 1) % n;
        if (b == B_RIGHT) profSel = (profSel + 1) % n;
        if (b == B_A) { if (profSel < int(perfis.size())) entrar(profSel); else criarPerfil(); }
        if (b == B_Y && profSel < int(perfis.size())) profStage = 3;
        if ((b == B_B || b == B_SELECT) && perfilAtual >= 0) profStage = 0;   // voltar so se ja tem alguem jogando
    } else if (profStage == 2) {
        int nav = totalAvatares();
        if (b == B_LEFT) profAvatar = (profAvatar + nav - 1) % nav;
        if (b == B_RIGHT) profAvatar = (profAvatar + 1) % nav;
        if (b == B_DOWN) profAvatar = (profAvatar + 6) % nav;
        if (b == B_UP) profAvatar = (profAvatar + nav * 6 - 6) % nav;
        if (b == B_A)
            abrirTeclado("Senha de " + profNomeNovo + " (opcional: deixe vazio e aperte Start)", true,
                         [](bool ok) { if (ok) salvarPerfilNovo(tkTexto); });
        if (b == B_B) profStage = 1;
    } else if (profStage == 3) {
        if (b == B_A) {
            bool eraAtual = profSel == perfilAtual;
            perfis.erase(perfis.begin() + profSel);
            savePerfis();
            if (eraAtual) { perfilAtual = -1; remove(PERFIL_AGORA.c_str()); }
            else if (perfilAtual > profSel) perfilAtual--;
            profSel = 0;
            profStage = 1;
        }
        if (b == B_B) profStage = 1;
    }
    return true;
}

// ------------------------------------------------------------------ desenho da tela

static void render() {
    if (profStage) {   // tela de perfis no lugar do carrossel
        drawPerfis();
        drawTeclado();
        drawToast();
        SDL_RenderPresent(G.r);
        return;
    }
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
    drawInstall();
    drawRede();
    drawBluetooth();
    drawAtualizar();
    drawNavegador();
    drawGratis();
    drawConfirmacao();
    drawPerguntaHD();
    drawControle();
    drawTeclado();
    drawEnergia();
    drawToast();
    drawProtecao();
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

// Nucleo libretro de cada console (o tos-jogar usa a mesma tabela): o primeiro da lista que estiver instalado
static std::string coreOf(const std::string &id) {
    static const std::map<std::string, std::vector<std::string>> m = {
        {"nes", {"mesen", "fceumm"}}, {"snes", {"snes9x", "snes9x2010"}}, {"megadrive", {"genesis_plus_gx", "picodrive"}},
        {"gb", {"gambatte"}}, {"gbc", {"gambatte"}}, {"gba", {"mgba", "gpsp"}}, {"psx", {"pcsx_rearmed"}},
        {"n64", {"mupen64plus_next"}}, {"arcade", {"fbneo"}}, {"neogeo", {"fbneo"}}, {"psp", {"ppsspp"}},
        {"nds", {"melonds"}}, {"saturn", {"mednafen_saturn"}}, {"dreamcast", {"flycast"}}, {"doom", {"prboom"}}};
    auto it = m.find(id);
    if (it == m.end()) return "";
    for (auto &c : it->second)
        if (isFile("/usr/lib/libretro/" + c + "_libretro.so")) return c;
    return it->second[0];
}

static std::string launchSys, launchRom, launchCore;   // jogo que abre quando a tela fecha

static std::string coreEscolhido(const std::string &id) {
    std::string pref = prefs["core|" + id];
    for (auto &n : nucleos(id))
        if (pref == n.so && isFile("/usr/lib/libretro/" + pref + "_libretro.so")) return pref;
    return coreOf(id);
}
static std::string emuNome(const Sys *s) {
    std::string esc = coreEscolhido(s->id);
    for (auto &n : nucleos(s->id))
        if (esc == n.so) { std::string t = n.nome; return t.substr(0, t.find(" (")); }
    return s->emu;
}
static void trocarNucleo(const Game *g) {
    auto v = nucleos(g->sys->id);
    std::vector<Nucleo> ok;
    for (auto &n : v) if (isFile(std::string("/usr/lib/libretro/") + n.so + "_libretro.so")) ok.push_back(n);
    if (ok.size() < 2) { toast("Este console tem um emulador só.", 2500); return; }
    std::string atual = coreEscolhido(g->sys->id);
    size_t i = 0;
    while (i < ok.size() && atual != ok[i].so) i++;
    const Nucleo &n = ok[(i + 1) % ok.size()];
    prefs["core|" + g->sys->id] = n.so;
    savePrefs();
    toast(std::string("Emulador do ") + g->sys->name + ": " + n.nome, 3000);
}

// A: abre o jogo. Antes, uma tela com os atalhos (a duvida mais comum: como sair do jogo).
static void play() {
    const Game *g = current();
    if (!g) return;
    std::string core = coreEscolhido(g->sys->id);
    // Ports (PortMaster: o jogo e um script) e Half-Life (Xash3D): sem nucleo do RetroArch
    bool port = g->sys->id == "ports" || g->sys->id == "halflife";
    if (!port && (core.empty() || !isFile("/usr/lib/libretro/" + core + "_libretro.so"))) {
        toast("O " + g->sys->emu + " entra numa próxima versão do ScrapCade", 3500);
        return;
    }
    prefs["plays|" + g->key()] = std::to_string(plays(*g) + 1);
    prefs["quando|" + g->key()] = std::to_string((long)time(nullptr));
    prefs["ultimo"] = g->key();
    savePrefs();
    float u = G.u;
    G.fillRect(0, 0, float(G.W), float(G.H), 0xE6101418);
    T.draw("Abrindo " + g->title, G.W / 2.0f, G.H * 0.40f, 16, true, 0xFFFFFFFF, 1, G.W - 40 * u);
    T.draw(g->sys->name + "  •  " + g->sys->emu, G.W / 2.0f, G.H * 0.40f + 22 * u, 10, false, 0xFFB8BEC8, 1);
    T.draw("Select + Start: sair do jogo (o progresso fica salvo)", G.W / 2.0f, G.H * 0.62f, 11, false,
           accent(g->sys->color), 1);
    T.draw("Select + X: menu     Select + RB: salvar     Select + LB: carregar", G.W / 2.0f, G.H * 0.62f + 18 * u, 9.5f,
           false, 0xFFDDE1E8, 1);
    SDL_RenderPresent(G.r);
    SDL_Delay(1800);
    pararMusica();
    launchSys = g->sys->id;
    launchRom = g->dir + "/" + g->file;
    launchCore = core;
    restartMenu = true;   // o laco principal solta a tela e o tos-jogar abre o emulador
}

// ------------------------------------------------------------------ principal

// Troca rapida (como o Game Switcher dos portateis): vai direto para "Continuar jogando"; de novo, volta para Todos
static void trocaRapida() {
    details = false;
    if (filter == F_REC) { filter = F_ALL; applyFilter(); clearCards(); toast("Todos os jogos", 1500); return; }
    if (std::find(filters.begin(), filters.end(), F_REC) == filters.end()) { toast("Nenhum jogo para continuar ainda: abra um jogo primeiro.", 3500); return; }
    filter = F_REC;
    applyFilter();
    clearCards();
    toast("Continuar jogando: A volta de onde você parou", 2500);
}

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
        std::string usb = Usb::import([](int n) {
            { std::lock_guard<std::mutex> l(workMu); workMsg = "Pen drive: copiando " + std::to_string(n) + (n == 1 ? " arquivo..." : " arquivos..."); }
            post(EV_TOAST);
        });
        if (!usb.empty()) {
            { std::lock_guard<std::mutex> l(workMu); workMsg = usb; }
            post(EV_TOAST);
        }
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

static void redraw() { post(EV_WEB); }

static std::string signature(const std::vector<Game> &games) {
    std::string s;
    for (auto &g : games) s += g.key() + "\n";
    return s;
}

int main() {
    if (getenv("ROMS_DIR")) ROMS = getenv("ROMS_DIR");
    if (getenv("SCRAPCADE_DATA")) DATA = getenv("SCRAPCADE_DATA");
    if (getenv("SCRAPCADE_CONF")) CONF_FILE = getenv("SCRAPCADE_CONF");
    if (getenv("INBOX_DIR")) INBOX = getenv("INBOX_DIR");
    signal(SIGTERM, onSignal);
    signal(SIGINT, onSignal);
    signal(SIGUSR1, onBotaoEnergia);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    {   // voltando de um jogo ou do desktop: as preferencias sao as do perfil que estava jogando
        std::ifstream pa("/tmp/tos-perfil");
        std::string l;
        if (std::getline(pa, l) && !l.empty()) {
            std::string pc = "/userdata/system/perfis/" + l.substr(0, l.find('|')) + "/scrapcade.conf";
            if (isFile(pc)) CONF_FILE = pc;
        }
    }
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
    gravarJogoCfg();
    montarPainel();
    prepararSons();
    tocarMusica();
    scanTemas();
    aplicarTemaPrefs();
    T.escala = escalaFonte();
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
    for (size_t i = 0; i < view.size(); i++)   // voltando de um jogo: fica nele
        if (view[i].key() == prefs["ultimo"]) { target = int(i); pos = float(target); break; }
    loadPerfis();
    {
        std::ifstream pa("/tmp/tos-perfil");
        std::string l, id;
        if (std::getline(pa, l)) id = l.substr(0, l.find('|'));
        for (size_t i = 0; i < perfis.size(); i++)
            if (perfis[i].id == id) perfilAtual = int(i);   // mesmo perfil, sem pedir de novo
        if (perfilAtual < 0) abrirPerfis();                 // ligou agora: quem vai jogar?
    }
    {   // o jogo nao abriu (o emulador fechou logo, ou nao ha emulador para o console): avisa em vez de voltar calado
        std::ifstream na("/tmp/tos-nao-abriu");
        std::string l;
        if (std::getline(na, l) && !l.empty()) toast(l, 15000);
        remove("/tmp/tos-nao-abriu");
    }
    std::string sig = signature(scanGames());
    lastMove = SDL_GetTicks();
    EV_TOAST = SDL_RegisterEvents(4);
    EV_META = EV_TOAST + 1;
    EV_ART = EV_TOAST + 2;
    EV_WEB = EV_TOAST + 3;
    std::thread work(worker);
    SDL_StartTextInput();
    if (energiaDisponivel()) {   // modo de energia escolhido no painel (vale para todos os perfis)
        std::ifstream ef(ENERGIA_FILE);
        int m = 1;
        if (ef >> m) aplicarEnergia(m);
    }
    Envio::changed = [] { post(EV_WEB); };
    Envio::start();

    Uint32 holdDir = 0, holdNext = 0, lastScan = SDL_GetTicks();
    int axisDir = 0;
    while (!quit && !restartMenu) {
        SDL_Event e;
        bool any = false;
        while (SDL_PollEvent(&e)) {
            any = true;
            bool botao = e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_JOYBUTTONDOWN ||
                         (e.type == SDL_CONTROLLERAXISMOTION && abs(e.caxis.value) > 20000);
            if (botao) {
                ultimaTecla = SDL_GetTicks();
                if (protecaoOn) { protecaoOn = false; continue; }   // acorda: o botao nao faz mais nada
                // sons do menu: andar, escolher, voltar
                int k = e.type == SDL_KEYDOWN ? e.key.keysym.sym : -1, cb = e.type == SDL_CONTROLLERBUTTONDOWN ? e.cbutton.button : -1;
                if (k == SDLK_RETURN || k == SDLK_SPACE || cb == SDL_CONTROLLER_BUTTON_A) tocarSom(somOk);
                else if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE || cb == SDL_CONTROLLER_BUTTON_B) tocarSom(somVoltar);
                else if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN || cb == SDL_CONTROLLER_BUTTON_DPAD_LEFT ||
                         cb == SDL_CONTROLLER_BUTTON_DPAD_RIGHT || cb == SDL_CONTROLLER_BUTTON_DPAD_UP || cb == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
                    tocarSom(somMover);
            }
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
            case SDL_JOYDEVICEADDED:
                // controle que o banco do SDL nao conhece: abre a configuracao na hora
                if (!SDL_IsGameController(e.jdevice.which)) {
                    if (SDL_Joystick *j = SDL_JoystickOpen(e.jdevice.which)) {
                        details = false; sysOpen = false; textTitle.clear();
                        ctlStart(j);
                    }
                }
                break;
            case SDL_JOYBUTTONDOWN: case SDL_JOYHATMOTION: case SDL_JOYAXISMOTION:
                ctlInput(e);
                break;
            case SDL_TEXTINPUT:
                if (tkOn) tkTexto += e.text.text;
                else if (netStage == 2) senha += e.text.text;
                break;
            case SDL_KEYDOWN: {
                SDL_Keycode kc = e.key.keysym.sym;
                if (ctlStep >= 0) {   // configurando controle pelo teclado: so pular e cancelar
                    if (kc == SDLK_SPACE) ctlSkip();
                    if (kc == SDLK_ESCAPE) { ctlStep = -1; toast("Configuração cancelada.", 2500); }
                    break;
                }
                if (tkOn) {   // nome e senha do perfil pelo teclado do PC
                    if (kc == SDLK_BACKSPACE) { if (!tkTexto.empty()) tkTexto.pop_back(); break; }
                    if (kc == SDLK_RETURN || kc == SDLK_KP_ENTER) { fecharTeclado(true); break; }
                    if (kc == SDLK_ESCAPE) { fecharTeclado(false); break; }
                    if (kc != SDLK_UP && kc != SDLK_DOWN && kc != SDLK_LEFT && kc != SDLK_RIGHT) break;
                }
                if (netStage == 2) {   // senha pelo teclado do notebook
                    if (kc == SDLK_BACKSPACE) { if (!senha.empty()) senha.pop_back(); break; }
                    if (kc == SDLK_RETURN || kc == SDLK_KP_ENTER) { netConnect(); break; }
                    if (kc == SDLK_ESCAPE) { netStage = 1; break; }
                    if (kc != SDLK_UP && kc != SDLK_DOWN && kc != SDLK_LEFT && kc != SDLK_RIGHT) break;
                }
                Btn pb = kc == SDLK_UP ? B_UP : kc == SDLK_DOWN ? B_DOWN : kc == SDLK_LEFT ? B_LEFT :
                         kc == SDLK_RIGHT ? B_RIGHT : (kc == SDLK_RETURN || kc == SDLK_SPACE) ? B_A :
                         (kc == SDLK_ESCAPE || kc == SDLK_BACKSPACE) ? B_B : kc == SDLK_TAB ? B_SELECT :
                         kc == SDLK_x ? B_X : kc == SDLK_DELETE ? B_Y : B_OTHER;
                if (panelInput(pb)) break;
                switch (kc) {
                case SDLK_LEFT: move(-1); break;
                case SDLK_RIGHT: move(1); break;
                case SDLK_RETURN: case SDLK_SPACE: play(); break;
                case SDLK_f: case SDLK_x: toggleFav(); break;   // X como no controle
                case SDLK_d: details = !details; break;
                case SDLK_ESCAPE: case SDLK_BACKSPACE: details = false; break;
                case SDLK_q: case SDLK_PAGEUP: nextFilter(-1); break;
                case SDLK_e: case SDLK_PAGEDOWN: nextFilter(1); break;
                case SDLK_TAB: details = false; openSystem(); break;
                case SDLK_c: trocaRapida(); break;
                }
                break;
            }
            case SDL_CONTROLLERBUTTONDOWN: {
                Uint8 cb = e.cbutton.button;
                Btn pb = cb == SDL_CONTROLLER_BUTTON_DPAD_UP ? B_UP : cb == SDL_CONTROLLER_BUTTON_DPAD_DOWN ? B_DOWN :
                         cb == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? B_LEFT : cb == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ? B_RIGHT :
                         cb == SDL_CONTROLLER_BUTTON_A ? B_A : cb == SDL_CONTROLLER_BUTTON_B ? B_B :
                         cb == SDL_CONTROLLER_BUTTON_BACK ? B_SELECT : cb == SDL_CONTROLLER_BUTTON_X ? B_X :
                         cb == SDL_CONTROLLER_BUTTON_START ? B_START : cb == SDL_CONTROLLER_BUTTON_Y ? B_Y : B_OTHER;
                if (panelInput(pb)) break;
                switch (cb) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: move(-1); holdDir = Uint32(-1); holdNext = SDL_GetTicks() + 380; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: move(1); holdDir = 1; holdNext = SDL_GetTicks() + 380; break;
                case SDL_CONTROLLER_BUTTON_A: play(); break;
                case SDL_CONTROLLER_BUTTON_X: if (details && current()) trocarNucleo(current()); else toggleFav(); break;
                case SDL_CONTROLLER_BUTTON_Y: details = !details; break;
                case SDL_CONTROLLER_BUTTON_B: details = false; break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: nextFilter(-1); break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: nextFilter(1); break;
                case SDL_CONTROLLER_BUTTON_BACK: details = false; openSystem(); break;
                case SDL_CONTROLLER_BUTTON_START: trocaRapida(); break;
                }
                break;
            }
            case SDL_CONTROLLERBUTTONUP:
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT || e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
                    holdDir = 0;
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (sysOpen || !textTitle.empty() || instStage || netStage || btStage || updStage || navModo || gratisOn || hdStage || profStage || tkOn) {
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
        static bool confirmado = false;
        if (!confirmado && now > 15000) {   // a tela abriu: a versao nova serve (abrir um jogo troca o processo)
            confirmado = true;
            std::thread([] { if (system("/usr/bin/tos-atualizar confirmar") != 0) {} }).detach();
        }
        static bool hdVisto = false;
        if (!hdVisto) { hdVisto = true; verSePergunta(); }
        if (hdPode == 1 && now > 20000 && perfilAtual >= 0 && !profStage && !sysOpen && !instStage && !netStage &&
            !btStage && !updStage && !navModo && !tkOn && ctlStep < 0 && textTitle.empty()) {
            hdPode = 2;
            hdStage = 1;
            redraw();
        }
        // procura versao nova sozinho (uma vez por boot, com a rede ja de pe) e avisa na tela
        // 0 nao procurou, 1 procurando, 2 tem versao nova, 3 nada, 4 baixou sozinho (falta reiniciar)
        static std::atomic<int> novaVersao{0};
        if (novaVersao == 0 && now > 40000) {
            novaVersao = 1;
            bool sozinho = prefs["autoupd"] != "0";
            std::thread([sozinho] {
                std::string r = Rede::run("/usr/bin/tos-atualizar verificar");
                size_t a = r.find('|'), b = a == std::string::npos ? a : r.find('|', a + 1);
                std::string n = b == std::string::npos ? "" : r.substr(a + 1, b - a - 1);
                bool tem = !n.empty() && isdigit((unsigned char)n[0]);   // numero de versao nova (nao "pronta"/"baixando")
                if (n == "pronta") { novaVersao = 4; return; }               // ja baixada: so lembrar de reiniciar
                if (tem && sozinho) {   // baixa em segundo plano; a versao de agora fica guardada (A/B)
                    if (system("/usr/bin/tos-atualizar instalar >> /userdata/system/logs/atualizar.log 2>&1") != 0) {}
                    std::ifstream in("/tmp/tos-atualizar.txt");
                    int pc = 0;
                    novaVersao = (in >> pc) && pc >= 100 ? 4 : 2;
                } else {
                    novaVersao = tem ? 2 : 3;
                }
            }).detach();
        }
        if (novaVersao == 4 && !updStage) {
            novaVersao = 3;
            toast("Versão nova do ScrapCade OS baixada: reinicie para usar (Sistema > Reiniciar)", 15000);
            redraw();
        }
        if (novaVersao == 2 && !updStage) {
            novaVersao = 3;
            toast("Versão nova do ScrapCade disponível: Sistema > Atualizar o ScrapCade OS", 9000);
            redraw();
        }
        if (botaoEnergia) {   // botao de energia apertado com o menu na tela
            botaoEnergia = 0;
            protecaoOn = false;
            ultimaTecla = now;
            energiaMenu = energiaMenu ? 0 : 1;
            any = true;
        }
        if (musicaAcabou.exchange(false)) tocarMusica();
        if (energiaMenu) any = true;
        // protecao de tela e desligar sozinho: so com o menu parado (jogando, o ScrapCade nem esta aberto)
        {
            if (!ultimaTecla) ultimaTecla = now;
            Uint32 parado = now - ultimaTecla;
            int pm = PROTECAO_MIN[protecaoIdx()], dm = DESLIGAR_MIN[desligarIdx()];
            if (!protecaoOn && pm && parado > Uint32(pm) * 60000u && ctlStep < 0 && !tkOn && instStage != 3) { protecaoOn = true; any = true; }
            static Uint32 ultimoDesenho = 0;
            if (protecaoOn && now - ultimoDesenho > 10000) { ultimoDesenho = now; any = true; }
            if (dm && instStage != 3 && updStage == 0) {
                Uint32 limite = Uint32(dm) * 60000u;
                static bool avisou = false;
                if (parado + 60000u > limite && !avisou) {
                    avisou = true;
                    protecaoOn = false;
                    toast("Parado faz tempo: o computador desliga em 1 minuto. Aperte qualquer botão para continuar.", 60000);
                    any = true;
                }
                if (parado + 60000u <= limite) avisou = false;
                if (parado > limite) powerOff(false);
            }
        }
        // segurando o direcional: anda sozinho
        if (holdDir && (sysOpen || !textTitle.empty() || instStage || netStage || btStage || updStage || navModo || gratisOn || hdStage || ctlStep >= 0 || profStage || tkOn)) holdDir = 0;
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
        bool barsDue = (sysOpen || instStage == 3 || netStage == 1 || netStage == 3 || btStage || updStage) && now - lastBars > 1000;
        if (ctlStep >= 0 || netStage == 2 || tkOn || profStage) any = true;
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
        if (!launchRom.empty()) {
            fprintf(stderr, "scrapcade: abrindo %s (%s)\n", launchRom.c_str(), launchSys.c_str());
            execl("/usr/bin/tos-jogar", "tos-jogar", launchSys.c_str(), launchRom.c_str(), launchCore.c_str(), (char *)nullptr);
        }
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
