// ScrapCade - pen drive com jogos: o ScrapCade monta somente leitura em /media/usb e copia para
// "Jogos novos" o que ainda nao veio dele (o organizador leva cada jogo ao console certo).
// Conversao do Organizer.importUsb do iChroma. Licenca: GPLv3.
#pragma once
#include "catalogo.h"
#include "central.h"
#include "organizar.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <sys/mount.h>

namespace Usb {

static std::string MOUNT = "/media/usb";
static std::string mounted;   // particao montada agora ("" = nenhuma)
static std::string ejetado;   // disco ejetado pelo painel: nao monta de novo ate sair e voltar

static std::string readFile(const std::string &p) {
    std::ifstream in(p);
    std::string s;
    std::getline(in, s);
    return s;
}

// Disco onde o ScrapCade esta rodando (nunca e tratado como pen drive de jogos)
static std::string bootDisk() {
    std::string b = readFile("/run-tos-boot");   // /dev/sda1
    if (b.empty()) return "";
    std::string part = b.substr(b.rfind('/') + 1);
    char buf[512];
    std::string link = "/sys/class/block/" + part;
    ssize_t n = readlink(link.c_str(), buf, sizeof buf - 1);
    if (n <= 0) return part;
    buf[n] = 0;
    std::string full = buf;   // ../../devices/.../block/sda/sda1
    size_t e = full.rfind('/');
    std::string parent = full.substr(0, e);
    return parent.substr(parent.rfind('/') + 1);
}

// Particoes de discos USB ligados agora (o disco do proprio ScrapCade fica de fora)
static std::vector<std::string> candidates() {
    std::vector<std::string> out;
    std::string boot = bootDisk();
    DIR *d = opendir("/sys/block");
    if (!d) return out;
    while (dirent *e = readdir(d)) {
        std::string n = e->d_name;
        if (n.compare(0, 2, "sd") != 0 || n == boot) continue;
        char buf[512];
        ssize_t l = readlink(("/sys/block/" + n).c_str(), buf, sizeof buf - 1);
        if (l <= 0) continue;
        buf[l] = 0;
        if (std::string(buf).find("/usb") == std::string::npos) continue;   // so USB
        bool parts = false;
        DIR *p = opendir(("/sys/block/" + n).c_str());
        while (p) {
            dirent *q = readdir(p);
            if (!q) break;
            std::string pn = q->d_name;
            if (pn.compare(0, n.size(), n) == 0 && pn.size() > n.size()) { out.push_back("/dev/" + pn); parts = true; }
        }
        if (p) closedir(p);
        if (!parts) out.push_back("/dev/" + n);   // pen drive sem tabela de particoes
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

static bool exists(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

// Monta o primeiro pen drive que tiver sistema de arquivos; desmonta se ele saiu
static bool attach() {
    if (!mounted.empty()) {
        if (exists(mounted)) return true;
        umount2(MOUNT.c_str(), MNT_DETACH);   // tiraram sem ejetar: so solta
        mounted.clear();
    }
    mkdirs(MOUNT);
    if (!ejetado.empty() && !exists(ejetado)) ejetado.clear();   // saiu: na volta pode montar
    for (auto &dev : candidates()) {
        if (!ejetado.empty() && dev.compare(0, ejetado.size(), ejetado) == 0) continue;
        for (const char *fs : {"vfat", "exfat", "ntfs3", "ext4", "iso9660"}) {
            if (mount(dev.c_str(), MOUNT.c_str(), fs, MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC, nullptr) == 0) {
                // outro pen drive do ScrapCade OS (SCRAPCADE): nao e de jogos
                if (exists(MOUNT + "/boot/tos.squashfs")) { umount2(MOUNT.c_str(), MNT_DETACH); break; }
                mounted = dev;
                fprintf(stderr, "scrapcade: pen drive %s montado em %s (%s)\n", dev.c_str(), MOUNT.c_str(), fs);
                return true;
            }
        }
    }
    return false;
}

// Ejetar: grava o que falta e solta o pen drive para poder tirar
static bool eject() {
    if (mounted.empty() && !attach()) return false;
    sync();
    if (umount2(MOUNT.c_str(), 0) != 0) umount2(MOUNT.c_str(), MNT_DETACH);
    std::string d = mounted;
    while (!d.empty() && isdigit((unsigned char)d.back())) d.pop_back();   // /dev/sdb1 -> /dev/sdb
    ejetado = d;
    mounted.clear();
    return true;
}

// Pastas que nunca tem jogo (instalador do Windows, lixeira, fotos...)
static bool skipDir(std::string n) {
    n = lower(n);
    static const std::set<std::string> S = {"lost.dir", "system volume information", "android", "$recycle.bin",
                                            "recycler", "boot", "efi", "sources", "support", "windows",
                                            "program files", "program files (x86)", "dcim", "bios", "saves"};
    return n.empty() || n[0] == '.' || S.count(n) > 0;
}

static bool isGame(const std::string &name) {
    std::string n = lower(name);
    if (n.empty() || n[0] == '.') return false;
    if (!Org::folder(n).empty()) return true;
    return endsWith(n, ".zip") || endsWith(n, ".bin") || endsWith(n, ".7z");
}

static void scan(const std::string &d, int depth, std::vector<std::string> &out) {
    DIR *dir = opendir(d.c_str());
    if (!dir) return;
    while (dirent *e = readdir(dir)) {
        std::string n = e->d_name, p = d + "/" + n;
        if (n == "." || n == "..") continue;
        if (Org::isDir(p)) {
            if (depth < 3 && !skipDir(n)) scan(p, depth + 1, out);
        } else if (isGame(n)) {
            out.push_back(p);
        }
    }
    closedir(dir);
}

static std::string memoFile() { return ROMS + "/.importados"; }

// Pen drive arrumado como o ScrapCade (roms/<console>/..., bios/...): cada arquivo vai direto para a mesma pasta
// aqui dentro, com as subpastas (CD de varias faixas, Half-Life, ScummVM), sem passar pelo organizador, que
// adivinha pela extensao. As pastas que nao sao de console caem no jeito antigo ("Jogos novos").
struct Copia { std::string de, para, nome; };
static bool ehConsole(const std::string &id) {
    for (auto &s : SYSTEMS) if (s.id == id) return true;
    return false;
}
static void scanArrumado(const std::string &d, const std::string &rel, const std::string &destBase, int depth,
                         std::vector<Copia> &out) {
    DIR *dir = opendir(d.c_str());
    if (!dir) return;
    while (dirent *e = readdir(dir)) {
        std::string n = e->d_name, p = d + "/" + n;
        if (n.empty() || n[0] == '.') continue;
        if (Org::isDir(p)) {
            if (depth < 6 && lower(n) != "system volume information") scanArrumado(p, rel + n + "/", destBase, depth + 1, out);
        } else {
            out.push_back({p, destBase + "/" + rel + n, n});
        }
    }
    closedir(dir);
}

// Copia um arquivo em pedacos, avisando a central (para a barra andar mesmo num ISO de 5 GB)
static bool copiaArquivo(const std::string &de, const std::string &para, long long &feitoTotal, long long total, int i, int n) {
    std::string tmp = para + ".baixando";
    mkdirs(para.substr(0, para.rfind('/')));
    FILE *a = fopen(de.c_str(), "rb");
    FILE *b = a ? fopen(tmp.c_str(), "wb") : nullptr;
    if (!a || !b) { if (a) fclose(a); if (b) fclose(b); remove(tmp.c_str()); return false; }
    std::vector<char> buf(4 << 20);
    size_t r;
    bool ok = true;
    std::string det = std::to_string(i) + " de " + std::to_string(n) + ": " + Org::base(de);
    while ((r = fread(buf.data(), 1, buf.size(), a)) > 0) {
        if (fwrite(buf.data(), 1, r, b) != r) { ok = false; break; }
        feitoTotal += (long long)r;
        Central::avanca("usb", feitoTotal, det, total);
    }
    ok = ok && !ferror(a);
    fclose(a);
    ok = (fclose(b) == 0) && ok && Org::sizeOf(tmp) == Org::sizeOf(de);
    if (!ok) { remove(tmp.c_str()); return false; }
    if (isFile(para)) remove(para.c_str());
    return rename(tmp.c_str(), para.c_str()) == 0;
}

// Copia o que ainda nao veio deste pen drive. Devolve o aviso, ou "". 'start' e chamado antes de copiar, com
// quantos arquivos vao vir. Tudo aparece na central de downloads.
template <typename F> static std::string import(F start) {
    if (!attach()) return "";
    std::vector<Copia> todo, all;
    // 1) arrumado como o ScrapCade: roms/<console>/... e bios/...
    if (Org::isDir(MOUNT + "/roms")) {
        if (DIR *r = opendir((MOUNT + "/roms").c_str())) {
            while (dirent *e = readdir(r)) {
                std::string c = e->d_name;
                if (c.empty() || c[0] == '.' || !Org::isDir(MOUNT + "/roms/" + c)) continue;
                if (ehConsole(c)) scanArrumado(MOUNT + "/roms/" + c, "", ROMS + "/" + c, 0, all);
            }
            closedir(r);
        }
    }
    if (Org::isDir(MOUNT + "/bios")) scanArrumado(MOUNT + "/bios", "", "/userdata/bios", 0, all);
    // 2) o resto (jogos soltos no pen drive): para "Jogos novos", e o organizador leva ao console
    std::vector<std::string> soltos;
    scan(MOUNT, 0, soltos);
    for (auto &f : soltos) {
        if (f.compare(0, MOUNT.size() + 6, MOUNT + "/roms/") == 0) continue;   // ja entrou pelo jeito arrumado
        all.push_back({f, INBOX + "/" + Org::base(f), Org::base(f)});
    }
    std::set<std::string> memo;
    {
        std::ifstream in(memoFile());
        std::string l;
        while (std::getline(in, l)) memo.insert(l);
    }
    long long total = 0;
    for (auto &c : all) {
        std::string chave = c.para + "|" + std::to_string(Org::sizeOf(c.de));
        std::string antiga = c.nome + "|" + std::to_string(Org::sizeOf(c.de));   // memoria das versoes antigas
        if (memo.count(chave) || memo.count(antiga)) continue;
        if (isFile(c.para) && Org::sizeOf(c.para) == Org::sizeOf(c.de)) continue;   // ja esta aqui igualzinho
        todo.push_back(c);
        total += Org::sizeOf(c.de);
    }
    if (todo.empty()) return "";
    start(int(todo.size()), total);
    mkdirs(INBOX);
    Central::inicia("usb", "Pen drive para o disco", total, "0 de " + std::to_string(todo.size()));
    int done = 0;
    long long feito = 0;
    for (auto &c : todo) {
        if (!copiaArquivo(c.de, c.para, feito, total, done + 1, int(todo.size()))) {
            // pen drive saiu no meio ou disco cheio: o que faltou vem na proxima vez
            std::string msg = std::to_string(done) + " de " + std::to_string(todo.size()) +
                              " copiados. Confira o espaço livre e o pen drive.";
            Central::termina("usb", false, msg);
            return msg;
        }
        FILE *m = fopen(memoFile().c_str(), "a");
        if (m) { fprintf(m, "%s|%lld\n", c.para.c_str(), Org::sizeOf(c.de)); fclose(m); }
        done++;
    }
    sync();
    std::string msg = (done == 1 ? std::string("1 arquivo copiado") : std::to_string(done) + " arquivos copiados") +
                      " do pen drive (" + Central::tamanho(total) + "). Pode tirar.";
    Central::termina("usb", true, msg);
    return msg;
}

}  // namespace Usb
