// ScrapCade - central de downloads: tudo o que esta chegando (pen drive -> disco, envios pela pagina, jogos gratis,
// versao nova do sistema), com o progresso de cada um. Quem copia ou baixa avisa aqui; a tela so desenha a lista.
// Licenca: GPLv3.
#pragma once
#include <algorithm>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

namespace Central {

struct Item {
    std::string id, titulo, detalhe;   // id: "usb", "web:<arquivo>", "gratis:<nome>", "sistema"
    long long feito = 0, total = 0;    // bytes (total 0 = tamanho ainda desconhecido)
    int estado = 0;                    // 0 andando, 1 pronto, 2 deu erro
    time_t quando = 0;                 // inicio (andando) ou fim (pronto/erro)
};

static std::mutex mu;
static std::vector<Item> itens;
static void (*avisa)() = nullptr;      // o menu redesenha (vem de fora da thread da tela)

static Item *acha(const std::string &id) {
    for (auto &i : itens) if (i.id == id) return &i;
    return nullptr;
}

static void inicia(const std::string &id, const std::string &titulo, long long total = 0, const std::string &detalhe = "") {
    {
        std::lock_guard<std::mutex> l(mu);
        Item *i = acha(id);
        if (!i) { itens.push_back(Item{}); i = &itens.back(); }
        *i = Item{id, titulo, detalhe, 0, total, 0, time(nullptr)};
        // guarda so os 12 mais recentes (os andando nunca saem)
        while (itens.size() > 12) {
            auto velho = std::find_if(itens.begin(), itens.end(), [](const Item &x) { return x.estado != 0; });
            if (velho == itens.end()) break;
            itens.erase(velho);
        }
    }
    if (avisa) avisa();
}

static void avanca(const std::string &id, long long feito, const std::string &detalhe = "", long long total = -1) {
    static time_t ultimo = 0;
    {
        std::lock_guard<std::mutex> l(mu);
        Item *i = acha(id);
        if (!i) return;
        i->feito = feito;
        if (total >= 0) i->total = total;
        if (!detalhe.empty()) i->detalhe = detalhe;
    }
    time_t t = time(nullptr);
    if (avisa && t != ultimo) { ultimo = t; avisa(); }   // no maximo uma tela por segundo
}

static void termina(const std::string &id, bool ok, const std::string &detalhe = "") {
    {
        std::lock_guard<std::mutex> l(mu);
        Item *i = acha(id);
        if (!i) return;
        i->estado = ok ? 1 : 2;
        if (ok && i->total > 0) i->feito = i->total;
        if (!detalhe.empty()) i->detalhe = detalhe;
        i->quando = time(nullptr);
    }
    if (avisa) avisa();
}

static std::vector<Item> lista() {
    std::lock_guard<std::mutex> l(mu);
    std::vector<Item> v = itens;
    // andando primeiro; depois os que acabaram, do mais novo para o mais velho
    std::stable_sort(v.begin(), v.end(), [](const Item &a, const Item &b) {
        if ((a.estado == 0) != (b.estado == 0)) return a.estado == 0;
        return a.quando > b.quando;
    });
    return v;
}

static int andando() {
    std::lock_guard<std::mutex> l(mu);
    return int(std::count_if(itens.begin(), itens.end(), [](const Item &i) { return i.estado == 0; }));
}

static std::string tamanho(long long b) {
    char s[32];
    if (b >= 1073741824LL) snprintf(s, sizeof s, "%.1f GB", b / 1073741824.0);
    else if (b >= 1048576) snprintf(s, sizeof s, "%.0f MB", b / 1048576.0);
    else snprintf(s, sizeof s, "%lld KB", (b + 1023) / 1024);
    return s;
}

}  // namespace Central
