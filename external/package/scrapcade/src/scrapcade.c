/* ScrapCade para o T-OS - versao 0.1
 *
 * Menu de jogos do T-OS. Desenha direto no framebuffer (/dev/fb0) e le teclado e controle pelo
 * evdev (/dev/input/event*). Leve de proposito: roda em PC sem aceleracao 3D.
 *
 * Telas: carrossel de consoles (estilo Aurora) e lista de jogos do console escolhido.
 * Jogos: ROMS_DIR/<console>/ (padrao /userdata/roms). Os emuladores entram na proxima versao.
 *
 * Licenca: GPLv3. stb_truetype.h: dominio publico (Sean Barrett).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kd.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#define VERSAO "T-OS 2026.11"
#define FONTE "/usr/share/fonts/dejavu/DejaVuSans.ttf"
#define FONTE_NEGRITO "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"

/* ------------------------------------------------------------------ tela */

static int W, H;                 /* tamanho da tela em pixels */
static uint32_t *tela;           /* quadro desenhado na memoria, 0xRRGGBB */
static uint8_t *fbmem;
static int fb_stride, fb_bpp;
static size_t fb_size;
static struct fb_var_screeninfo vinfo;

static int fb_abrir(void)
{
    struct fb_fix_screeninfo finfo;
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) { perror("scrapcade: /dev/fb0"); return -1; }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) || ioctl(fd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("scrapcade: fbinfo");
        return -1;
    }
    W = (int)vinfo.xres;
    H = (int)vinfo.yres;
    fb_bpp = (int)vinfo.bits_per_pixel;
    fb_stride = (int)finfo.line_length;
    fb_size = (size_t)fb_stride * vinfo.yres_virtual;
    fbmem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fbmem == MAP_FAILED) { perror("scrapcade: mmap"); return -1; }
    tela = malloc((size_t)W * H * 4);
    return tela ? 0 : -1;
}

/* Copia o quadro para a tela, convertendo para o formato do framebuffer. */
static void fb_mostrar(void)
{
    for (int y = 0; y < H; y++) {
        uint8_t *linha = fbmem + (size_t)y * fb_stride;
        const uint32_t *src = tela + (size_t)y * W;
        if (fb_bpp == 32 && vinfo.red.offset == 16 && vinfo.blue.offset == 0) {
            memcpy(linha, src, (size_t)W * 4);
        } else if (fb_bpp == 32) {
            uint32_t *d = (uint32_t *)linha;
            for (int x = 0; x < W; x++) {
                uint32_t c = src[x];
                d[x] = ((c >> 16 & 255) << vinfo.red.offset) | ((c >> 8 & 255) << vinfo.green.offset) |
                       ((c & 255) << vinfo.blue.offset);
            }
        } else if (fb_bpp == 16) {
            uint16_t *d = (uint16_t *)linha;
            for (int x = 0; x < W; x++) {
                uint32_t c = src[x];
                d[x] = (uint16_t)((c >> 19 & 31) << 11 | (c >> 10 & 63) << 5 | (c >> 3 & 31));
            }
        } else if (fb_bpp == 24) {
            for (int x = 0; x < W; x++) {
                uint32_t c = src[x];
                linha[x * 3] = c & 255;
                linha[x * 3 + 1] = c >> 8 & 255;
                linha[x * 3 + 2] = c >> 16 & 255;
            }
        }
    }
}

/* b sobre a, com opacidade t (0..255) */
static inline uint32_t mistura(uint32_t a, uint32_t b, int t)
{
    int r = ((int)(a >> 16 & 255) * (255 - t) + (int)(b >> 16 & 255) * t) / 255;
    int g = ((int)(a >> 8 & 255) * (255 - t) + (int)(b >> 8 & 255) * t) / 255;
    int bl = ((int)(a & 255) * (255 - t) + (int)(b & 255) * t) / 255;
    return (uint32_t)(r << 16 | g << 8 | bl);
}

static void retangulo(int x, int y, int w, int h, uint32_t c, int alfa)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (int j = y0; j < y1; j++) {
        uint32_t *p = tela + (size_t)j * W;
        for (int i = x0; i < x1; i++) p[i] = alfa >= 255 ? c : mistura(p[i], c, alfa);
    }
}

static void retangulo_redondo(int x, int y, int w, int h, int r, uint32_t c, int alfa)
{
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    for (int j = 0; j < h; j++) {
        int dx = 0;
        if (j < r) {
            int dy = r - j;
            dx = r - (int)sqrt((double)(r * r - dy * dy));
        } else if (j >= h - r) {
            int dy = j - (h - r) + 1;
            dx = r - (int)sqrt((double)(r * r - dy * dy));
        }
        retangulo(x + dx, y + j, w - 2 * dx, 1, c, alfa);
    }
}

/* Fundo em degrade, estilo Aurora: azul escuro em cima, mais claro no meio. */
static void fundo(uint32_t tom)
{
    for (int y = 0; y < H; y++) {
        int t = y < H / 2 ? y * 255 / (H / 2) : (H - y) * 255 / (H / 2);
        uint32_t c = mistura(0x07101e, tom, t * 3 / 4);
        uint32_t *p = tela + (size_t)y * W;
        for (int x = 0; x < W; x++) p[x] = c;
    }
}

/* ------------------------------------------------------------------ texto */

enum { TXT_P, TXT_M, TXT_G, TXT_T, NTAM };
static int tam[NTAM];
static stbtt_fontinfo fonte, fonte_negrito;
static int tem_fonte, tem_negrito;

typedef struct { unsigned char *bmp; int w, h, xo, yo, avanco; } Glifo;
static Glifo cache[2][NTAM][256];

static unsigned char *ler_arquivo(const char *caminho)
{
    FILE *f = fopen(caminho, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)n);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    return b;
}

static Glifo *glifo(int neg, int ti, int cp)
{
    if (cp < 32 || cp > 255) cp = '?';
    Glifo *g = &cache[neg][ti][cp];
    if (g->avanco) return g;
    stbtt_fontinfo *f = neg && tem_negrito ? &fonte_negrito : &fonte;
    float s = stbtt_ScaleForPixelHeight(f, (float)tam[ti]);
    int adv, lsb;
    stbtt_GetCodepointHMetrics(f, cp, &adv, &lsb);
    g->avanco = (int)(adv * s + 0.5f);
    if (!g->avanco) g->avanco = 1;
    g->bmp = stbtt_GetCodepointBitmap(f, 0, s, cp, &g->w, &g->h, &g->xo, &g->yo);
    return g;
}

/* Le um caractere UTF-8 (so ate U+00FF, o que cobre o portugues). */
static int utf8(const char **s)
{
    unsigned char c = (unsigned char)**s;
    if (!c) return 0;
    (*s)++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && **s) {
        int r = (c & 31) << 6 | ((unsigned char)**s & 63);
        (*s)++;
        return r;
    }
    while (((unsigned char)**s & 0xC0) == 0x80) (*s)++;
    return '?';
}

static int largura_texto(const char *t, int neg, int ti)
{
    if (!tem_fonte) return 0;
    int w = 0, cp;
    while ((cp = utf8(&t))) w += glifo(neg, ti, cp)->avanco;
    return w;
}

/* y e a linha de base do texto */
static void texto(int x, int y, const char *t, uint32_t cor, int neg, int ti)
{
    if (!tem_fonte) return;
    int cp;
    while ((cp = utf8(&t))) {
        Glifo *g = glifo(neg, ti, cp);
        for (int j = 0; j < g->h; j++) {
            int py = y + g->yo + j;
            if (py < 0 || py >= H) continue;
            uint32_t *p = tela + (size_t)py * W;
            for (int i = 0; i < g->w; i++) {
                int px = x + g->xo + i;
                int a = g->bmp[j * g->w + i];
                if (px >= 0 && px < W && a) p[px] = mistura(p[px], cor, a);
            }
        }
        x += g->avanco;
    }
}

static void texto_centro(int cx, int y, const char *t, uint32_t cor, int neg, int ti)
{
    texto(cx - largura_texto(t, neg, ti) / 2, y, t, cor, neg, ti);
}

/* ------------------------------------------------------------------ consoles e jogos */

typedef struct {
    const char *id, *nome;
    uint32_t cor;
    int njogos;
    char **jogos;
} Console;

static Console consoles[] = {
    {"nes", "NES", 0xc0392b, 0, NULL},
    {"snes", "Super Nintendo", 0x6c5ce7, 0, NULL},
    {"megadrive", "Mega Drive", 0x2d6cdf, 0, NULL},
    {"gb", "Game Boy", 0x7f8c55, 0, NULL},
    {"gbc", "Game Boy Color", 0x27ae60, 0, NULL},
    {"gba", "Game Boy Advance", 0x8e44ad, 0, NULL},
    {"n64", "Nintendo 64", 0x16a085, 0, NULL},
    {"psx", "PlayStation", 0x7f8c8d, 0, NULL},
    {"ps2", "PlayStation 2", 0x2c3e9a, 0, NULL},
    {"gamecube", "GameCube", 0x5b3fa8, 0, NULL},
    {"wii", "Wii", 0x8a97a3, 0, NULL},
    {"psp", "PSP", 0x34495e, 0, NULL},
    {"nds", "Nintendo DS", 0x9aa3aa, 0, NULL},
    {"3ds", "Nintendo 3DS", 0xd35400, 0, NULL},
    {"arcade", "Arcade", 0xe67e22, 0, NULL},
    {"neogeo", "Neo Geo", 0x1e6fd9, 0, NULL},
    {"dos", "PC (DOS)", 0x8c2f2f, 0, NULL},
};
#define NCON ((int)(sizeof consoles / sizeof consoles[0]))

static const char *roms_dir = "/userdata/roms";

static int compara(const void *a, const void *b)
{
    return strcasecmp(*(char *const *)a, *(char *const *)b);
}

static void criar_pasta(const char *caminho)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", caminho);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

static void ler_jogos(Console *c)
{
    char pasta[512];
    for (int i = 0; i < c->njogos; i++) free(c->jogos[i]);
    free(c->jogos);
    c->jogos = NULL;
    c->njogos = 0;
    snprintf(pasta, sizeof pasta, "%s/%s", roms_dir, c->id);
    criar_pasta(pasta);
    DIR *d = opendir(pasta);
    if (!d) return;
    struct dirent *e;
    int cap = 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (e->d_type == DT_DIR) continue;
        const char *ext = strrchr(e->d_name, '.');
        if (ext && (!strcasecmp(ext, ".txt") || !strcasecmp(ext, ".png") || !strcasecmp(ext, ".jpg"))) continue;
        if (c->njogos == cap) {
            cap = cap ? cap * 2 : 16;
            c->jogos = realloc(c->jogos, sizeof(char *) * (size_t)cap);
        }
        c->jogos[c->njogos++] = strdup(e->d_name);
    }
    closedir(d);
    if (c->njogos) qsort(c->jogos, (size_t)c->njogos, sizeof(char *), compara);
}

/* Nome do jogo sem a extensao, para mostrar */
static void nome_jogo(const char *arquivo, char *saida, size_t n)
{
    snprintf(saida, n, "%s", arquivo);
    char *p = strrchr(saida, '.');
    if (p && p != saida) *p = 0;
}

/* ------------------------------------------------------------------ entrada (teclado e controle) */

enum { NADA, ESQ, DIREITA, CIMA, BAIXO, OK, VOLTAR };

#define MAXDEV 32
static int devs[MAXDEV], ndev;
static struct input_absinfo abs_x[MAXDEV], abs_y[MAXDEV];
static int tem_x[MAXDEV], tem_y[MAXDEV], est_x[MAXDEV], est_y[MAXDEV];
static int fila[32], nfila;

static void empilhar(int a)
{
    if (a != NADA && nfila < 32) fila[nfila++] = a;
}

static void abrir_entradas(void)
{
    for (int i = 0; i < ndev; i++) close(devs[i]);
    ndev = 0;
    DIR *d = opendir("/dev/input");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && ndev < MAXDEV) {
        if (strncmp(e->d_name, "event", 5)) continue;
        char caminho[300];
        snprintf(caminho, sizeof caminho, "/dev/input/%s", e->d_name);
        int fd = open(caminho, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        int i = ndev++;
        devs[i] = fd;
        tem_x[i] = ioctl(fd, EVIOCGABS(ABS_X), &abs_x[i]) == 0 && abs_x[i].maximum > abs_x[i].minimum;
        tem_y[i] = ioctl(fd, EVIOCGABS(ABS_Y), &abs_y[i]) == 0 && abs_y[i].maximum > abs_y[i].minimum;
        est_x[i] = est_y[i] = 0;
    }
    closedir(d);
}

/* Analogico: -1, 0 ou 1, com histerese para nao repetir sozinho */
static int eixo(int valor, const struct input_absinfo *a, int *estado)
{
    double meio = (a->maximum + a->minimum) / 2.0, raio = (a->maximum - a->minimum) / 2.0;
    double v = (valor - meio) / raio;
    int novo = *estado;
    if (v > 0.6) novo = 1;
    else if (v < -0.6) novo = -1;
    else if (v > -0.3 && v < 0.3) novo = 0;
    int disparou = novo != 0 && novo != *estado;
    *estado = novo;
    return disparou ? novo : 0;
}

static void ler_entradas(void)
{
    struct input_event ev;
    for (int i = 0; i < ndev; i++) {
        while (read(devs[i], &ev, sizeof ev) == (ssize_t)sizeof ev) {
            if (ev.type == EV_KEY && (ev.value == 1 || (ev.value == 2 && ev.code < BTN_MISC))) {
                switch (ev.code) {
                case KEY_LEFT: empilhar(ESQ); break;
                case KEY_RIGHT: empilhar(DIREITA); break;
                case KEY_UP: empilhar(CIMA); break;
                case KEY_DOWN: empilhar(BAIXO); break;
                case KEY_ENTER: case KEY_KPENTER: case KEY_SPACE: case BTN_SOUTH: empilhar(OK); break;
                case KEY_ESC: case KEY_BACKSPACE: case BTN_EAST: empilhar(VOLTAR); break;
                case BTN_DPAD_LEFT: empilhar(ESQ); break;
                case BTN_DPAD_RIGHT: empilhar(DIREITA); break;
                case BTN_DPAD_UP: empilhar(CIMA); break;
                case BTN_DPAD_DOWN: empilhar(BAIXO); break;
                }
            } else if (ev.type == EV_ABS) {
                if (ev.code == ABS_HAT0X && ev.value) empilhar(ev.value < 0 ? ESQ : DIREITA);
                else if (ev.code == ABS_HAT0Y && ev.value) empilhar(ev.value < 0 ? CIMA : BAIXO);
                else if (ev.code == ABS_X && tem_x[i]) {
                    int r = eixo(ev.value, &abs_x[i], &est_x[i]);
                    if (r) empilhar(r < 0 ? ESQ : DIREITA);
                } else if (ev.code == ABS_Y && tem_y[i]) {
                    int r = eixo(ev.value, &abs_y[i], &est_y[i]);
                    if (r) empilhar(r < 0 ? CIMA : BAIXO);
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ desenho das telas */

enum { TELA_CONSOLES, TELA_JOGOS };
static int tela_atual = TELA_CONSOLES, sel, jogo_sel, jogo_topo;
static double pos;                     /* posicao animada do carrossel */
static char aviso[160];
static time_t aviso_ate;

static void barra_topo(const char *titulo)
{
    int h = H * 9 / 100;
    retangulo(0, 0, W, h, 0x000000, 110);
    retangulo(0, h - 2, W, 2, 0x4aa3ff, 160);
    texto(W * 3 / 100, h * 68 / 100, titulo, 0xffffff, 1, TXT_G);
    char hora[16];
    time_t t = time(NULL);
    strftime(hora, sizeof hora, "%H:%M", localtime(&t));
    int wh = largura_texto(hora, 1, TXT_M);
    texto(W - W * 3 / 100 - wh, h * 50 / 100, hora, 0xffffff, 1, TXT_M);
    int wv = largura_texto(VERSAO, 0, TXT_P);
    texto(W - W * 3 / 100 - wv, h * 85 / 100, VERSAO, 0xa9c4de, 0, TXT_P);
}

static void barra_dicas(const char *dicas)
{
    int h = H * 7 / 100;
    retangulo(0, H - h, W, h, 0x000000, 120);
    texto_centro(W / 2, H - h * 35 / 100, dicas, 0xdfe9f3, 0, TXT_P);
}

static void mostrar_aviso(void)
{
    if (!aviso[0] || time(NULL) > aviso_ate) return;
    int w = largura_texto(aviso, 0, TXT_M) + W * 4 / 100, h = H * 8 / 100;
    int x = (W - w) / 2, y = H * 80 / 100;
    retangulo_redondo(x, y, w, h, h / 4, 0x101820, 230);
    texto_centro(W / 2, y + h * 64 / 100, aviso, 0xffffff, 0, TXT_M);
}

static void cartao(int cx, int cy, double escala, const Console *c, int destaque, int brilho)
{
    int cw = (int)(W * 0.24 * escala), ch = (int)(cw * 0.72);
    int x = cx - cw / 2, y = cy - ch / 2, r = cw / 14;
    if (destaque) retangulo_redondo(x - 5, y - 5, cw + 10, ch + 10, r + 4, 0xffffff, 255);
    retangulo_redondo(x, y, cw, ch, r, mistura(0x000000, c->cor, brilho), 255);
    retangulo_redondo(x, y, cw, ch / 5, r, mistura(0x000000, c->cor, brilho * 3 / 5), 255);
    retangulo(x, y + ch / 5 - r, cw, r, mistura(0x000000, c->cor, brilho * 3 / 5), 255);
    int ti = escala > 0.9 ? TXT_G : (escala > 0.7 ? TXT_M : TXT_P);
    if (largura_texto(c->nome, 1, ti) > cw * 9 / 10 && ti > TXT_P) ti--;
    uint32_t claro = mistura(0x000000, 0xffffff, brilho);
    texto_centro(cx, y + ch * 58 / 100, c->nome, claro, 1, ti);
    char n[40];
    snprintf(n, sizeof n, c->njogos == 1 ? "1 jogo" : "%d jogos", c->njogos);
    texto_centro(cx, y + ch * 85 / 100, n, mistura(0x000000, 0xdfe9f3, brilho), 0, ti > TXT_P ? ti - 1 : TXT_P);
}

static void desenhar_consoles(void)
{
    fundo(mistura(0x1f4f7a, consoles[sel].cor, 70));
    barra_topo("ScrapCade");
    int cy = H * 46 / 100;
    double passo = W * 0.21;
    /* de tras para frente: as capas do meio ficam por cima */
    for (int nivel = 4; nivel >= 0; nivel--) {
        for (int i = 0; i < NCON; i++) {
            double off = i - pos, a = fabs(off);
            if ((int)(a + 0.5) != nivel) continue;
            if (a > 3.6) continue;
            double escala = 1.0 / (1.0 + 0.32 * a);
            int brilho = a < 0.5 ? 255 : (int)(255 - 60 * (a > 3 ? 3 : a));
            int cx = W / 2 + (int)(off * passo * (a > 1 ? 0.92 : 1.0));
            cartao(cx, cy + (int)(a * H * 0.01), escala, &consoles[i], a < 0.5, brilho);
        }
    }
    const Console *c = &consoles[sel];
    texto_centro(W / 2, H * 80 / 100, c->nome, 0xffffff, 1, TXT_T);
    char n[64];
    if (c->njogos) snprintf(n, sizeof n, c->njogos == 1 ? "1 jogo" : "%d jogos", c->njogos);
    else snprintf(n, sizeof n, "Sem jogos ainda");
    texto_centro(W / 2, H * 86 / 100, n, 0xa9c4de, 0, TXT_M);
    barra_dicas("Direcional: escolher      A / Enter: abrir      B / Esc: voltar");
}

static void desenhar_jogos(void)
{
    const Console *c = &consoles[sel];
    fundo(mistura(0x1f4f7a, c->cor, 90));
    barra_topo(c->nome);
    int topo = H * 13 / 100, lh = H * 6 / 100;
    int visiveis = (H * 78 / 100 - topo) / lh;
    if (!c->njogos) {
        char p[300];
        texto_centro(W / 2, H * 42 / 100, "Nenhum jogo ainda.", 0xffffff, 1, TXT_G);
        texto_centro(W / 2, H * 50 / 100, "Copie os jogos para a pasta:", 0xdfe9f3, 0, TXT_M);
        snprintf(p, sizeof p, "%s/%s", roms_dir, c->id);
        texto_centro(W / 2, H * 56 / 100, p, 0xffd479, 0, TXT_M);
    } else {
        if (jogo_sel < jogo_topo) jogo_topo = jogo_sel;
        if (jogo_sel >= jogo_topo + visiveis) jogo_topo = jogo_sel - visiveis + 1;
        for (int k = 0; k < visiveis && jogo_topo + k < c->njogos; k++) {
            int i = jogo_topo + k, y = topo + k * lh;
            if (i == jogo_sel) retangulo_redondo(W * 5 / 100, y, W * 90 / 100, lh - 6, lh / 5, c->cor, 220);
            char nome[256];
            nome_jogo(c->jogos[i], nome, sizeof nome);
            texto(W * 7 / 100, y + lh * 64 / 100, nome, 0xffffff, i == jogo_sel, TXT_M);
        }
        char cont[48];
        snprintf(cont, sizeof cont, "%d de %d", jogo_sel + 1, c->njogos);
        texto_centro(W / 2, H * 89 / 100, cont, 0xa9c4de, 0, TXT_P);
    }
    barra_dicas("Direcional: escolher      A / Enter: jogar      B / Esc: voltar");
}

/* ------------------------------------------------------------------ principal */

static volatile sig_atomic_t sair;
static void ao_sinal(int s) { (void)s; sair = 1; }

int main(void)
{
    if (getenv("ROMS_DIR")) roms_dir = getenv("ROMS_DIR");
    signal(SIGTERM, ao_sinal);
    signal(SIGINT, ao_sinal);

    /* o console de texto para de desenhar por cima do menu */
    int tty = open("/dev/tty1", O_RDWR);
    if (tty >= 0) ioctl(tty, KDSETMODE, KD_GRAPHICS);

    if (fb_abrir()) return 1;
    tam[TXT_P] = H * 20 / 800;
    tam[TXT_M] = H * 26 / 800;
    tam[TXT_G] = H * 34 / 800;
    tam[TXT_T] = H * 46 / 800;
    unsigned char *f1 = ler_arquivo(FONTE), *f2 = ler_arquivo(FONTE_NEGRITO);
    tem_fonte = f1 && stbtt_InitFont(&fonte, f1, 0);
    tem_negrito = f2 && stbtt_InitFont(&fonte_negrito, f2, 0);
    if (!tem_fonte) fprintf(stderr, "scrapcade: fonte %s nao encontrada\n", FONTE);

    criar_pasta(roms_dir);
    for (int i = 0; i < NCON; i++) ler_jogos(&consoles[i]);
    abrir_entradas();

    int sujo = 1, minuto = -1;
    time_t ultima_busca = time(NULL);
    while (!sair) {
        struct pollfd pf[MAXDEV];
        for (int i = 0; i < ndev; i++) { pf[i].fd = devs[i]; pf[i].events = POLLIN; }
        int animando = fabs(pos - sel) > 0.001;
        poll(pf, (nfds_t)ndev, animando ? 16 : 250);
        ler_entradas();

        /* controle ligado depois: procura de novo a cada 3 s */
        if (time(NULL) - ultima_busca >= 3) { abrir_entradas(); ultima_busca = time(NULL); }

        for (int k = 0; k < nfila; k++) {
            int a = fila[k];
            sujo = 1;
            if (tela_atual == TELA_CONSOLES) {
                if (a == ESQ && sel > 0) sel--;
                else if (a == DIREITA && sel < NCON - 1) sel++;
                else if (a == OK) {
                    ler_jogos(&consoles[sel]);
                    tela_atual = TELA_JOGOS;
                    jogo_sel = jogo_topo = 0;
                }
            } else {
                Console *c = &consoles[sel];
                if (a == VOLTAR) tela_atual = TELA_CONSOLES;
                else if (a == CIMA && jogo_sel > 0) jogo_sel--;
                else if (a == BAIXO && jogo_sel < c->njogos - 1) jogo_sel++;
                else if (a == OK && c->njogos) {
                    snprintf(aviso, sizeof aviso, "Os emuladores chegam na próxima versão do T-OS");
                    aviso_ate = time(NULL) + 3;
                }
            }
        }
        nfila = 0;

        if (animando) {
            pos += (sel - pos) * 0.22;
            if (fabs(pos - sel) < 0.002) pos = sel;
            sujo = 1;
        }
        time_t t = time(NULL);
        int m = localtime(&t)->tm_min;
        if (m != minuto) { minuto = m; sujo = 1; }
        if (aviso[0] && t <= aviso_ate + 1) sujo = 1;
        if (!sujo) continue;

        if (tela_atual == TELA_CONSOLES) desenhar_consoles();
        else desenhar_jogos();
        mostrar_aviso();
        fb_mostrar();
        sujo = 0;
    }
    if (tty >= 0) ioctl(tty, KDSETMODE, KD_TEXT);
    return 0;
}
