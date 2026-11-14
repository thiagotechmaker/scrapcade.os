// ScrapCade - rede: cabo e Wi-Fi pelo ConnMan. A senha do Wi-Fi vai num arquivo de configuracao do
// ConnMan (/var/lib/connman, guardado em TOS-DADOS/system/connman), e ele conecta sozinho nas proximas vezes.
// Licenca: GPLv3.
#pragma once
#include "catalogo.h"

#include <cstdio>
#include <fstream>
#include <unistd.h>

namespace Rede {

struct Wifi { std::string name, id; bool saved, connected; };

static std::string run(const std::string &cmd) {
    std::string out;
    FILE *f = popen((cmd + " 2>/dev/null").c_str(), "r");
    if (!f) return out;
    char b[512];
    while (fgets(b, sizeof b, f)) out += b;
    pclose(f);
    return out;
}

static std::string readLine(const std::string &p) {
    std::ifstream in(p);
    std::string s;
    std::getline(in, s);
    return s;
}

// Placas de rede com cabo: nome, se o cabo esta ligado
struct Cabo { std::string dev; bool link; };
static std::vector<Cabo> cabos() {
    std::vector<Cabo> v;
    DIR *d = opendir("/sys/class/net");
    if (!d) return v;
    while (dirent *e = readdir(d)) {
        std::string n = e->d_name;
        if (n[0] == '.' || n == "lo") continue;
        struct stat st;
        if (stat(("/sys/class/net/" + n + "/wireless").c_str(), &st) == 0) continue;   // e Wi-Fi
        if (stat(("/sys/class/net/" + n + "/device").c_str(), &st) != 0) continue;     // virtual
        v.push_back({n, readLine("/sys/class/net/" + n + "/carrier") == "1"});
    }
    closedir(d);
    return v;
}

static bool temWifi() {
    DIR *d = opendir("/sys/class/net");
    if (!d) return false;
    bool w = false;
    while (dirent *e = readdir(d)) {
        struct stat st;
        if (stat(("/sys/class/net/" + std::string(e->d_name) + "/wireless").c_str(), &st) == 0) w = true;
    }
    closedir(d);
    return w;
}

// Procura as redes Wi-Fi (demora alguns segundos: chamar fora da thread da tela)
static std::vector<Wifi> scan() {
    run("connmanctl enable wifi");
    run("connmanctl scan wifi");
    std::vector<Wifi> v;
    std::stringstream ss(run("connmanctl services"));
    std::string l;
    while (std::getline(ss, l)) {
        // "*AO Casa                 wifi_..._managed_psk"  (A = salva, O/R = conectada)
        size_t id = l.rfind(' ');
        if (id == std::string::npos || l.find("wifi_") == std::string::npos) continue;
        std::string flags = l.substr(0, 4), name = l.substr(4, id - 4);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        while (!name.empty() && name[0] == ' ') name.erase(0, 1);
        if (name.empty()) continue;   // rede escondida
        v.push_back({name, l.substr(id + 1), flags.find('A') != std::string::npos,
                     flags.find('O') != std::string::npos || flags.find('R') != std::string::npos});
    }
    return v;
}

// Guarda a senha e manda conectar. true se conectou em ate ~20 s.
static bool connect(const Wifi &w, const std::string &senha) {
    std::string safe;
    for (char c : w.name) safe += isalnum((unsigned char)c) ? c : '_';
    std::string p = "/var/lib/connman/tos-" + safe + ".config";
    std::ofstream f(p);
    f << "[service_tos_" << safe << "]\nType = wifi\nName = " << w.name << "\n";
    if (!senha.empty()) f << "Passphrase = " << senha << "\n";
    f.close();
    sync();
    run("connmanctl connect " + w.id);
    for (int i = 0; i < 20; i++) {
        SDL_Delay(1000);
        for (auto &x : scan())
            if (x.id == w.id && x.connected) return true;
    }
    return false;
}

}  // namespace Rede
