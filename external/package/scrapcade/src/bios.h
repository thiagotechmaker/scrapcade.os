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
          {"bios_CD_J.bin", "278a9397d192149e84e820ac621a8edd", "Japão"},
          {"bios_CD_J.bin", "bdeb4c47da613946d422d97d98b21cda", "Japão (Rev H)"}}, {}},
        {"Sega Saturn", true, "os jogos de Saturn não abrem",
         {{"mpr-17933.bin", "3240872c70984b6cbfda1586cab68dbe", "EUA e Europa"},
          {"sega_101.bin", "85ec9ca47d8f6807718151cbcca8b964", "Japão"}}, {}},
        {"PlayStation 2", true, "os jogos de PS2 não abrem",
         {{"pcsx2/bios/", "", "qualquer BIOS do seu PS2"}}, {}},
        {"PC Engine CD", true, "os jogos de CD do PC Engine não abrem (os cartuchos abrem)",
         {{"syscard3.pce", "38179df8f4ac870017db21ebcbf53114", "System Card 3.0"}}, {}},
        {"Atari Lynx", true, "os jogos de Lynx não abrem",
         {{"lynxboot.img", "fcd403db69f54290b51035d82f835e7b", "Atari"}}, {}},
        {"Atari 7800", false, "abre direto, sem a tela da Atari",
         {{"7800 BIOS (U).rom", "0763f1ffb006ddbe32e52d497ee848ae", "Atari"}}, {}},
        {"Atari 5200", true, "os jogos de 5200 não abrem",
         {{"5200.rom", "281f20ea4320404ec820fb7ec0693b38", "Atari"}}, {}},
        {"ColecoVision", true, "os jogos de ColecoVision não abrem",
         {{"colecovision.rom", "2c66f5911e5b42b8ebe113403548eee7", "Coleco"}}, {}},
        {"Intellivision (Executive)", true, "os jogos de Intellivision não abrem",
         {{"exec.bin", "62e761035cb657903761800f4437b8af", "Mattel"}}, {}},
        {"Intellivision (GROM)", true, "os jogos de Intellivision não abrem",
         {{"grom.bin", "0cd5946c6473e42e8e4c2137785e427f", "Mattel"}}, {}},
        {"Odyssey²", true, "os jogos de Odyssey² não abrem",
         {{"o2rom.bin", "562d5ebf9e030a40d6fabfc2f33139fd", "Magnavox"}}, {}},
        {"3DO", true, "os jogos de 3DO não abrem",
         {{"panafz10.bin", "51f2f43ae2f3508a14d9f56597e2d3ce", "Panasonic FZ-10"},
          {"panafz1.bin", "f47264dd47fe30f73ab3c010015c155b", "Panasonic FZ-1"},
          {"goldstar.bin", "8639fd5e549bd6238cfee79e3e749114", "GoldStar"}}, {}},
        {"Neo Geo CD", true, "os jogos de Neo Geo CD não abrem",
         {{"neocd/neocd_f.rom", "", "Front loader"}, {"neocd/neocd_t.rom", "", "Top loader"},
          {"neocd/neocd_z.rom", "", "CDZ"}, {"neocd/neocd_sf.rom", "", "Front loader (SMKDAN)"},
          {"neocd/neocd_st.rom", "", "Top loader (SMKDAN)"}, {"neocd/neocd_sz.rom", "", "CDZ (SMKDAN)"}}, {}},
        {"Neo Geo CD (LO)", true, "os jogos de Neo Geo CD não abrem",
         {{"neocd/000-lo.lo", "fc7599f3f871578fe9a0453662d1c966", "SNK"}, {"neocd/ng-lo.rom", "", "SNK"}}, {}},
        {"Amiga", false, "usa o AROS livre embutido (a maioria dos jogos roda; alguns pedem o Kickstart original)",
         {{"kick34005.A500", "82a21c1890cae844b3df741f2762d48d", "Kickstart 1.3 (A500)"},
          {"kick40068.A1200", "646773759326fbac3b2311fd8c8793ee", "Kickstart 3.1 (A1200)"}}, {}},
        {"Atari ST", false, "usa o EmuTOS livre que vem no sistema",
         {{"tos.img", "", "TOS da Atari"}}, {}},
        {"Xbox (BIOS)", true, "os jogos de Xbox não abrem",
         {{"Complex_4627.bin", "", "BIOS do Xbox"}}, {}},
        {"Xbox (MCPX)", true, "os jogos de Xbox não abrem",
         {{"mcpx_1.0.bin", "d49c52a4102f6df7bcf8d0617ac475ed", "Microsoft"}}, {}},
        {"PlayStation 3", true, "os jogos de PS3 não abrem (o firmware oficial é baixado no site da Sony)",
         {{"PS3UPDAT.PUP", "", "firmware oficial do PS3"}}, {}},
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
                // nome terminado em "/": basta a pasta ter algum arquivo (as BIOS do PS2 tem nomes variados)
                if (!p.empty() && p.back() == '/') {
                    bool tem = false;
                    if (DIR *dd = opendir(p.c_str())) {
                        while (dirent *e = readdir(dd)) if (e->d_name[0] != '.' && isFile(p + e->d_name)) { tem = true; break; }
                        closedir(dd);
                    }
                    if (tem) { found = std::string(f.label); break; }
                    continue;
                }
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
