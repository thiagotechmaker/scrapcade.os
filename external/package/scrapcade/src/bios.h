// ScrapCade - tela "BIOS dos consoles": quais BIOS cada emulador usa, se estao na pasta e se
// o arquivo e o certo (MD5). O ScrapCade so traz BIOS livres; as originais sao do proprio console do usuario.
// Licenca: GPLv3.
#pragma once
#include "catalogo.h"

#include <cstdio>
#include <openssl/evp.h>

namespace Bios {

static std::string BIOS_DIR = "/userdata/bios";

struct File {
    const char *name;      // nome que o emulador procura
    const char *md5;       // md5 da versao aceita ("" = qualquer)
    const char *label;     // de onde e
};

struct Need {
    const char *console;
    bool required;                 // sem ela o console nao roda
    const char *without;           // o que acontece sem ela
    std::vector<File> files;       // basta uma delas
    std::vector<std::string> alsoIn;   // outras pastas onde o emulador tambem procura
};

static const std::vector<Need> &list() {
    static const std::vector<Need> L = {
        {"PlayStation", false, "usa a BIOS livre embutida (PCSX ReARMed)",
         {{"scph5501.bin", "490f666e1afb15b7362b406ed1cea246", "EUA"},
          {"scph5500.bin", "8dd7d5296a650fac7319bce665a6a53c", "Japão"},
          {"scph5502.bin", "32736f17079d0b2b7024407c39bd3050", "Europa"},
          {"scph1001.bin", "924e392ed05558ffdb115408c263dccf", "EUA"}}, {}},
        {"Neo Geo", true, "os jogos de Neo Geo não abrem",
         {{"neogeo.zip", "", "BIOS do Neo Geo"}}, {"/userdata/roms/neogeo"}},
        {"Famicom Disk System (NES)", true, "os jogos .fds não abrem",
         {{"disksys.rom", "ca30b50f880eb660a320674ed365ef7a", "Nintendo"}}, {}},
        {"Game Boy Advance", false, "usa a BIOS livre embutida (mGBA)",
         {{"gba_bios.bin", "a860e8c0b6d573d191e4ec7db1b1e4f6", "Nintendo"}}, {}},
        {"Game Boy", false, "abre direto, sem a tela da Nintendo",
         {{"gb_bios.bin", "32fbbd84168d3482956eb3c5051637f5", "Nintendo"}}, {}},
        {"Game Boy Color", false, "abre direto, sem a tela da Nintendo",
         {{"gbc_bios.bin", "dbfce9db9deaa2567f6a84fde55f9680", "Nintendo"}}, {}},
        {"Sega CD", true, "os jogos de Sega CD não abrem",
         {{"bios_CD_U.bin", "2efd74e3232ff260e371b99f84024f7f", "EUA"},
          {"bios_CD_E.bin", "e66fa1dc5820d254611fdcdba0662372", "Europa"},
          {"bios_CD_J.bin", "278a9397d192149e84e820ac621a8edd", "Japão"}}, {}},
        {"PC Engine CD", true, "os jogos de CD do PC Engine não abrem (os cartuchos abrem)",
         {{"syscard3.pce", "38179df8f4ac870017db21ebcbf53114", "System Card 3.0"}}, {}},
        {"Atari Lynx", true, "os jogos de Lynx não abrem",
         {{"lynxboot.img", "fcd403db69f54290b51035d82f835e7b", "Atari"}}, {}},
        {"Atari 7800", false, "abre direto, sem a tela da Atari",
         {{"7800 BIOS (U).rom", "0763f1ffb006ddbe32e52d497ee848ae", "Atari"}}, {}},
    };
    return L;
}

static std::string md5Of(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return "";
    EVP_MD_CTX *c = EVP_MD_CTX_new();
    EVP_DigestInit_ex(c, EVP_md5(), nullptr);
    std::vector<unsigned char> b(1 << 16);
    size_t n;
    while ((n = fread(b.data(), 1, b.size(), f)) > 0) EVP_DigestUpdate(c, b.data(), n);
    fclose(f);
    unsigned char d[EVP_MAX_MD_SIZE];
    unsigned int dl = 0;
    EVP_DigestFinal_ex(c, d, &dl);
    EVP_MD_CTX_free(c);
    std::string hex;
    char h[3];
    for (unsigned i = 0; i < dl; i++) { snprintf(h, sizeof h, "%02x", d[i]); hex += h; }
    return hex;
}

// Linhas da tela. Comecam com "✓" (tudo certo), "✗" (falta ou arquivo errado) ou "•" (opcional que falta).
static std::vector<std::string> report() {
    std::vector<std::string> out;
    int missing = 0;
    for (auto &n : list()) {
        std::string found, wrong;
        for (auto &f : n.files) {
            std::vector<std::string> where = {BIOS_DIR};
            where.insert(where.end(), n.alsoIn.begin(), n.alsoIn.end());
            for (auto &d : where) {
                std::string p = d + "/" + f.name;
                if (!isFile(p)) continue;
                if (!*f.md5 || md5Of(p) == f.md5) { found = std::string(f.name) + " (" + f.label + ")"; break; }
                wrong = f.name;
            }
            if (!found.empty()) break;
        }
        std::string names;
        for (auto &f : n.files) names += (names.empty() ? "" : ", ") + std::string(f.name);
        if (!found.empty()) {
            out.push_back("✓  " + std::string(n.console) + ": " + found);
        } else if (!wrong.empty()) {
            out.push_back("✗  " + std::string(n.console) + ": " + wrong + " está na pasta, mas não é o arquivo certo");
            missing++;
        } else if (n.required) {
            out.push_back("✗  " + std::string(n.console) + ": falta " + names + " (sem ela, " + n.without + ")");
            missing++;
        } else {
            out.push_back("•  " + std::string(n.console) + ": opcional, " + n.without);
        }
    }
    out.push_back("");
    out.push_back(missing ? "Coloque as BIOS do seu console na pasta \"bios\" pela página de Arquivos:"
                          : "Tudo pronto. Outras BIOS vão na pasta \"bios\", pela página de Arquivos:");
    out.push_back("http://" + std::string("%IP%") + ":8080/arquivos  →  bios");
    out.push_back("NES, SNES, Mega Drive, N64 e quase todo o fliperama não precisam de BIOS.");
    return out;
}

}  // namespace Bios
