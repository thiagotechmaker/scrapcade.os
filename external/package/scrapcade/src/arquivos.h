// ScrapCade (T-OS) - gerenciador de arquivos: listar, renomear, mover, copiar, clonar, criar pasta e apagar.
// Conversao do Arquivos.java do iChroma, usado pela pagina http://<ip>:8080/arquivos. Licenca: GPLv3.
//
// So enxerga as pastas de jogos e o pen drive. Todo caminho e resolvido (realpath) e precisa cair dentro de
// uma das raizes: nada mais do sistema aparece ou pode ser mexido por aqui.
#pragma once
#include "catalogo.h"

#include <climits>
#include <cstdio>
#include <map>
#include <sys/stat.h>
#include <unistd.h>

namespace Arquivos {

static const std::string USB = "Pen drive";
static std::string USB_DIR = "/media/usb";   // o pen drive sera montado aqui (importacao: proxima etapa)

struct Root { std::string name, path; };
static std::vector<Root> roots() {
    return {{"roms", ROMS}, {"Jogos novos", INBOX}, {"bios", "/userdata/bios"}, {"saves", "/userdata/saves"},
            {"Registros", "/userdata/system/logs"}, {USB, USB_DIR}};
}

static std::string real(const std::string &p) {
    char buf[PATH_MAX];
    return realpath(p.c_str(), buf) ? std::string(buf) : "";
}

static bool inside(const std::string &p, const std::string &b) {
    return p == b || (p.size() > b.size() && p.compare(0, b.size(), b) == 0 && p[b.size()] == '/');
}

// "roms/psx/jogo.cue" -> caminho de verdade, ou "" se nao existe ou sai das raizes
static std::string resolve(std::string rel) {
    std::replace(rel.begin(), rel.end(), '\\', '/');
    while (!rel.empty() && rel[0] == '/') rel.erase(0, 1);
    size_t cut = rel.find('/');
    std::string root = rel.substr(0, cut), rest = cut == std::string::npos ? "" : rel.substr(cut + 1);
    for (auto &r : roots()) {
        if (r.name != root) continue;
        std::string b = real(r.path);
        if (b.empty()) return "";
        std::string f = real(b + "/" + rest);
        if (!f.empty() && inside(f, b)) return f;
    }
    return "";
}

static bool isRoot(const std::string &p) {
    for (auto &r : roots()) if (real(r.path) == p) return true;
    return false;
}

// O inverso de resolve()
static std::string rel(const std::string &p) {
    for (auto &r : roots()) {
        std::string b = real(r.path);
        if (!b.empty() && inside(p, b)) return r.name + p.substr(b.size());
    }
    return p;
}

static bool isDirP(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool exists(const std::string &p) {
    struct stat st;
    return lstat(p.c_str(), &st) == 0;
}

static std::map<std::string, std::string> query(const std::string &path, std::string (*dec)(const std::string &)) {
    std::map<std::string, std::string> m;
    size_t q = path.find('?');
    if (q == std::string::npos) return m;
    std::stringstream ss(path.substr(q + 1));
    std::string kv;
    while (std::getline(ss, kv, '&')) {
        size_t e = kv.find('=');
        if (e != std::string::npos && e > 0) m[kv.substr(0, e)] = dec(kv.substr(e + 1));
    }
    return m;
}

static std::string js(const std::string &s) {
    std::string b = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { b += '\\'; b += char(c); }
        else if (c < 32) { char u[8]; snprintf(u, sizeof u, "\\u%04x", c); b += u; }
        else b += char(c);
    }
    return b + "\"";
}

// Conteudo de uma pasta em JSON, pastas primeiro. "" = as raizes que existem. "" de volta = pasta invalida.
static std::string list(const std::string &relp) {
    struct E { std::string n, p; bool d; long long s; };
    std::vector<E> l;
    if (relp.empty()) {
        for (auto &r : roots()) {
            if (!isDirP(r.path)) continue;
            if (r.name == USB) {   // pen drive so aparece se tiver algo montado
                DIR *d = opendir(r.path.c_str());
                bool any = false;
                while (d && !any) { dirent *e = readdir(d); if (!e) break; any = e->d_name[0] != '.'; }
                if (d) closedir(d);
                if (!any) continue;
            }
            l.push_back({r.name, r.name, true, 0});
        }
    } else {
        std::string d = resolve(relp);
        if (d.empty() || !isDirP(d)) return "";
        std::string at = rel(d);
        DIR *dir = opendir(d.c_str());
        if (!dir) return "";
        while (dirent *e = readdir(dir)) {
            std::string n = e->d_name;
            if (n.empty() || n[0] == '.' || endsWith(n, ".envio")) continue;   // internos do menu e envios pela metade
            struct stat st;
            if (stat((d + "/" + n).c_str(), &st) != 0) continue;
            l.push_back({n, at + "/" + n, S_ISDIR(st.st_mode), S_ISDIR(st.st_mode) ? 0 : (long long)st.st_size});
        }
        closedir(dir);
        std::sort(l.begin(), l.end(), [](const E &a, const E &b) {
            if (a.d != b.d) return a.d;
            return strcasecmp(a.n.c_str(), b.n.c_str()) < 0;
        });
    }
    std::string b = "[";
    for (auto &e : l) {
        if (b.size() > 1) b += ',';
        b += "{\"n\":" + js(e.n) + ",\"p\":" + js(e.p) + ",\"d\":" + (e.d ? "true" : "false") + ",\"s\":" + std::to_string(e.s) + "}";
    }
    return b + "]";
}

static bool copy(const std::string &a, const std::string &b) {
    if (isDirP(a)) {
        if (mkdir(b.c_str(), 0755) != 0 && !isDirP(b)) return false;
        DIR *d = opendir(a.c_str());
        if (!d) return false;
        bool ok = true;
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            ok = copy(a + "/" + n, b + "/" + n) && ok;
        }
        closedir(d);
        return ok;
    }
    FILE *in = fopen(a.c_str(), "rb");
    if (!in) return false;
    FILE *out = fopen(b.c_str(), "wb");
    if (!out) { fclose(in); return false; }
    std::vector<char> buf(1 << 16);
    size_t n;
    bool ok = true;
    while ((n = fread(buf.data(), 1, buf.size(), in)) > 0)
        if (fwrite(buf.data(), 1, n, out) != n) { ok = false; break; }
    fclose(in);
    fflush(out);
    fsync(fileno(out));
    if (fclose(out) != 0) ok = false;
    if (!ok) remove(b.c_str());
    return ok;
}

static bool del(const std::string &p) {
    struct stat st;
    if (lstat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        DIR *d = opendir(p.c_str());
        while (d) {
            dirent *e = readdir(d);
            if (!e) break;
            std::string n = e->d_name;
            if (n != "." && n != "..") del(p + "/" + n);
        }
        if (d) closedir(d);
        return rmdir(p.c_str()) == 0;
    }
    return remove(p.c_str()) == 0;
}

// "Jogo.zip" -> "Jogo (cópia).zip", "Jogo (cópia 2).zip"... sem sobrescrever nada
static std::string freeName(const std::string &dir, const std::string &name) {
    std::string f = dir + "/" + name;
    if (!exists(f)) return f;
    size_t dot = name.rfind('.');
    std::string base = dot != std::string::npos && dot > 0 ? name.substr(0, dot) : name;
    std::string ext = dot != std::string::npos && dot > 0 ? name.substr(dot) : "";
    for (int i = 1;; i++) {
        f = dir + "/" + base + (i == 1 ? " (cópia)" : " (cópia " + std::to_string(i) + ")") + ext;
        if (!exists(f)) return f;
    }
}

static bool badName(const std::string &n) {
    return n.empty() || n.find('/') != std::string::npos || n.find('\\') != std::string::npos || n == "." || n == "..";
}

static std::string parent(const std::string &p) { return p.substr(0, p.rfind('/')); }
static std::string nameOf(const std::string &p) { return p.substr(p.rfind('/') + 1); }

// Executa uma operacao (a = origem, b = pasta de destino, n = nome). Devolve o status HTTP.
static std::string run(const std::string &op, std::map<std::string, std::string> &q) {
    std::string a = resolve(q["a"]);
    if (a.empty()) return "404 Nao encontrado";
    if (op == "pasta") {
        if (badName(q["n"]) || !isDirP(a)) return "400 Nome invalido";
        return mkdir((a + "/" + q["n"]).c_str(), 0755) == 0 ? "200 OK" : "409 Ja existe";
    }
    if (isRoot(a)) return "403 Pasta principal nao pode ser mexida";
    if (op == "apagar") return del(a) ? "200 OK" : "500 Nao apagou";
    if (op == "renomear") {
        if (badName(q["n"])) return "400 Nome invalido";
        std::string to = parent(a) + "/" + q["n"];
        if (exists(to)) return "409 Ja existe";
        return rename(a.c_str(), to.c_str()) == 0 ? "200 OK" : "500 Nao renomeou";
    }
    if (op == "clonar") return copy(a, freeName(parent(a), nameOf(a))) ? "200 OK" : "500 Nao copiou";
    std::string dest = resolve(q["b"]);
    if (dest.empty() || !isDirP(dest)) return "400 Destino invalido";
    if (inside(dest, a)) return "400 Destino dentro da origem";
    std::string to = freeName(dest, nameOf(a));
    if (op == "copiar") return copy(a, to) ? "200 OK" : "500 Nao copiou";
    if (op == "mover") {
        if (rename(a.c_str(), to.c_str()) == 0) return "200 OK";
        if (!copy(a, to)) return "500 Nao copiou";   // outro disco (pen drive): copia e apaga
        return del(a) ? "200 OK" : "500 Copiou, mas nao apagou a origem";
    }
    return "404 Operacao desconhecida";
}

}  // namespace Arquivos
