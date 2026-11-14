// ScrapCade - pen drive com jogos: o ScrapCade monta somente leitura em /media/usb e copia para
// "Jogos novos" o que ainda nao veio dele (o organizador leva cada jogo ao console certo).
// Conversao do Organizer.importUsb do iChroma. Licenca: GPLv3.
#pragma once
#include "catalogo.h"
#include "organizar.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <sys/mount.h>

namespace Usb {

static std::string MOUNT = "/media/usb";
static std::string mounted;   // particao montada agora ("" = nenhuma)

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
    for (auto &dev : candidates()) {
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

// Copia para "Jogos novos" o que ainda nao veio deste pen drive. Devolve o aviso, ou "".
// 'start' e chamado antes de copiar, com quantos arquivos vao vir.
template <typename F> static std::string import(F start) {
    if (!attach()) return "";
    std::vector<std::string> all, todo;
    scan(MOUNT, 0, all);
    std::set<std::string> memo;
    {
        std::ifstream in(memoFile());
        std::string l;
        while (std::getline(in, l)) memo.insert(l);
    }
    for (auto &f : all)
        if (!memo.count(Org::base(f) + "|" + std::to_string(Org::sizeOf(f)))) todo.push_back(f);
    if (todo.empty()) return "";
    start(int(todo.size()));
    mkdirs(INBOX);
    int done = 0;
    for (auto &f : todo) {
        std::string name = Org::base(f), tmp = INBOX + "/" + name + ".baixando";
        std::ifstream in(f, std::ios::binary);
        std::ofstream out(tmp, std::ios::binary);
        out << in.rdbuf();
        out.close();
        if (!in || !out || Org::sizeOf(tmp) != Org::sizeOf(f)) {
            remove(tmp.c_str());   // pen drive saiu no meio ou disco cheio: o que faltou vem na proxima vez
            return std::to_string(done) + " de " + std::to_string(todo.size()) +
                   " copiados. Confira o espaço livre e o pen drive.";
        }
        rename(tmp.c_str(), (INBOX + "/" + name).c_str());
        FILE *m = fopen(memoFile().c_str(), "a");
        if (m) { fprintf(m, "%s|%lld\n", name.c_str(), Org::sizeOf(f)); fclose(m); }
        done++;
    }
    sync();
    return (done == 1 ? std::string("1 arquivo copiado") : std::to_string(done) + " arquivos copiados") +
           " do pen drive. Pode tirar.";
}

}  // namespace Usb
