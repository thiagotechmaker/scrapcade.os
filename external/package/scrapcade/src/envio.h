// ScrapCade - pagina para enviar jogos pelo navegador do PC ou celular: http://<ip>:8080
// Conversao do Envio.java do iChroma. Licenca: GPLv3.
//
//   GET  /             lista de jogos, area para arrastar arquivos
//   PUT  /envio/<nome> grava em "Jogos novos" (o organizador leva para o console certo)
//   PUT  /capa/<chave> troca a capa do jogo (images/<jogo>.png)
//   POST /apaga/<chave> apaga o jogo (so o que esta na lista)
//   GET  /arquivos     gerenciador de arquivos (arquivos.h): /fs/lista, /fs/baixar, /fs/enviar, /fs/<operacao>
// So atende a rede local (enderecos privados), como um aparelho de casa.
#pragma once
#include "catalogo.h"
#include "meta.h"
#include "arquivos.h"

#include <arpa/inet.h>
#include <functional>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <thread>
#include <unistd.h>

namespace Envio {

static const int PORT = 8080;
static const std::string PARTIAL = ".envio";
static std::mutex mu;
static std::vector<Game> games;          // copia da lista do menu
static std::function<void()> changed;    // avisa o menu (chamado fora da thread da tela)

static void setGames(const std::vector<Game> &g) {
    std::lock_guard<std::mutex> l(mu);
    games = g;
}

static std::string html(const std::string &s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else if (c == '"') o += "&quot;";
        else o += c;
    }
    return o;
}

static std::string urldecode(const std::string &s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
            o += char(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            o += s[i] == '+' ? ' ' : s[i];
        }
    }
    return o;
}

static bool sendAll(int fd, const void *p, size_t n) {
    const char *c = (const char *)p;
    while (n > 0) {
        ssize_t w = ::send(fd, c, n, MSG_NOSIGNAL);
        if (w <= 0) return false;
        c += w;
        n -= size_t(w);
    }
    return true;
}

static void reply(int fd, const std::string &status) {
    std::string h = "HTTP/1.0 " + status + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    sendAll(fd, h.data(), h.size());
}

static void sendBody(int fd, const std::string &type, const std::string &body, const char *cache = "no-store") {
    std::string h = "HTTP/1.0 200 OK\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size()) +
                    "\r\nCache-Control: " + cache + "\r\nConnection: close\r\n\r\n";
    sendAll(fd, h.data(), h.size());
    sendAll(fd, body.data(), body.size());
}

// Recebe 'length' bytes (parte ja lida em 'pre') em dest, pelo arquivo temporario. "" = deu certo.
static std::string receiveFile(int fd, std::string pre, long long length, const std::string &dest) {
    std::string dir = dest.substr(0, dest.rfind('/'));
    mkdirs(dir);
    struct statvfs fs;
    if (statvfs(dir.c_str(), &fs) == 0 && (unsigned long long)length > (unsigned long long)fs.f_bavail * fs.f_frsize)
        return "507 Disco cheio";
    std::string tmp = dest + PARTIAL;
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) return "500 Nao gravou";
    long long left = length;
    if (!pre.empty()) {
        size_t n = std::min<long long>(left, (long long)pre.size());
        fwrite(pre.data(), 1, n, f);
        left -= (long long)n;
    }
    std::vector<char> b(1 << 16);
    while (left > 0) {
        ssize_t n = recv(fd, b.data(), size_t(std::min<long long>(left, (long long)b.size())), 0);
        if (n <= 0) break;
        if (fwrite(b.data(), 1, size_t(n), f) != size_t(n)) break;
        left -= n;
    }
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    if (left > 0) { remove(tmp.c_str()); return "500 Envio incompleto"; }
    remove(dest.c_str());
    if (rename(tmp.c_str(), dest.c_str()) != 0) { remove(tmp.c_str()); return "500 Nao gravou"; }
    return "";
}

static bool findGame(const std::string &key, Game &out) {
    std::lock_guard<std::mutex> l(mu);
    for (auto &g : games)
        if (g.key() == key) { out = g; return true; }
    return false;
}

static const char *CSS =
    "*{box-sizing:border-box}"
    "body{font:16px system-ui,-apple-system,Segoe UI,Roboto,sans-serif;margin:0;background:#0b0520;color:#f2f0ff}"
    "main{max-width:860px;margin:auto;padding:16px}a{color:#19e6ff}"
    ".logo{display:block;width:100%;max-width:520px;margin:0 auto 10px;border-radius:12px}"
    "nav{display:flex;gap:8px;justify-content:center;margin:0 0 14px}"
    "nav a{padding:8px 18px;border-radius:999px;background:#1d1447;text-decoration:none;color:#f2f0ff}"
    "nav a.on{background:#19e6ff;color:#0b0520;font-weight:700}h2{font-size:18px;margin:18px 0 8px}"
    "button{border:0;border-radius:8px;padding:8px 12px;min-height:36px;cursor:pointer;background:#2c2260;color:#fff;font:inherit;font-size:14px}"
    "button.v,.x{background:#e0356b}button.a{background:#7dff3a;color:#0b0520;font-weight:700}"
    "table{width:100%;border-collapse:collapse;background:#17103a;border-radius:12px;overflow:hidden}"
    "td{padding:9px 12px;border-bottom:1px solid #2c2260;vertical-align:middle}tr:last-child td{border-bottom:0}"
    ".c,td.s{color:#a59fcf;font-size:13px}td.s{white-space:nowrap}.b,td.o{text-align:right}.b{white-space:nowrap}"
    ".b button,td.o button{margin:2px 0 2px 4px}.n{cursor:pointer;color:#19e6ff;font-weight:600;word-break:break-word}"
    "#d{display:block;border:2px dashed #19e6ff;border-radius:14px;padding:30px 16px;text-align:center;margin:8px 0 10px;cursor:pointer;background:#120a30}"
    "#d.o{background:#1d1447}input[type=file]{display:none}#s div,#msg{margin:4px 0;font-size:14px;color:#ffd34e;min-height:20px}"
    "#cam{margin:8px 0;font-size:15px;word-break:break-word}#cam a{cursor:pointer}.bar{display:flex;flex-wrap:wrap;gap:8px;margin:8px 0}"
    "#clip{background:#1d1447;border:1px solid #19e6ff;padding:10px 12px;border-radius:10px;display:none;margin:8px 0}"
    "@media(max-width:640px){main{padding:10px}td{display:block;border:0;padding:3px 12px}"
    "tr{display:block;border-bottom:1px solid #2c2260;padding:8px 0}.b,td.o{text-align:left;white-space:normal}"
    ".b button,td.o button{margin:4px 6px 0 0}}";

// Inicio comum das paginas: estilo que se ajusta ao celular e ao PC, logo e abas
static std::string head(const std::string &title, bool files) {
    return std::string("<!DOCTYPE html><html lang=pt-br><meta charset=utf-8>"
                       "<meta name=viewport content='width=device-width,initial-scale=1'><title>") + title +
           "</title><style>" + CSS + "</style><main><img class=logo src=/logo.jpg alt=ScrapCade>"
           "<nav><a href=/" + (files ? "" : " class=on") + ">Jogos</a><a href=/arquivos" + (files ? " class=on" : "") +
           ">Arquivos</a></nav>";
}

static const char *FILES_SCRIPT =
    "var cur='',clip=null;function $(i){return document.getElementById(i)}"
    "function tam(s){return s>1048576?(s/1048576).toFixed(1)+' MB':Math.ceil(s/1024)+' KB'}"
    "function req(m,u,body,ok){var x=new XMLHttpRequest;x.open(m,u);x.onload=function(){if(x.status==200)ok(x);else $('msg').textContent='Erro: '+x.statusText};x.send(body||null)}"
    "function op(o,a,b,n){$('msg').innerHTML='<img src=/carregando.gif alt=Aguarde style=height:32px>';var u='/fs/'+o+'?a='+encodeURIComponent(a)+(b!=null?'&b='+encodeURIComponent(b):'')"
    "+(n!=null?'&n='+encodeURIComponent(n):'');req('POST',u,null,function(){$('msg').textContent='Feito.';abre(cur)})}"
    "function abre(p){cur=p;req('GET','/fs/lista?p='+encodeURIComponent(p),null,function(x){var l=JSON.parse(x.responseText),h='';"
    "var c='<a onclick=\"abre(\\'\\')\">Início</a>',acc='';if(p)p.split('/').forEach(function(s){acc+=(acc?'/':'')+s;"
    "c+=' / <a onclick=\"abre(\\''+acc.replace(/'/g,\"\\\\'\")+'\\')\">'+esc(s)+'</a>'});$('cam').innerHTML=c;"
    "if(!l.length)h='<tr><td>Pasta vazia</td></tr>';l.forEach(function(f,i){h+='<tr><td><span class=n data-i='+i+'>'+(f.d?'📁 ':'')+esc(f.n)+'</span></td>'"
    "+'<td class=s>'+(f.d?'':tam(f.s))+'</td><td class=o>'+(p?(f.d?'':'<button data-o=baixar data-i='+i+'>baixar</button>')"
    "+'<button data-o=renomear data-i='+i+'>renomear</button><button data-o=recortar data-i='+i+'>mover</button>'"
    "+'<button data-o=copiarclip data-i='+i+'>copiar</button><button data-o=clonar data-i='+i+'>clonar</button>'"
    "+'<button class=v data-o=apagar data-i='+i+'>apagar</button>':'')+'</td></tr>'});$('t').innerHTML=h;"
    "Array.prototype.forEach.call(document.querySelectorAll('[data-i]'),function(e){e.onclick=function(){acao(e.dataset.o,l[e.dataset.i])}});mostraClip()})}"
    "function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;')}"
    "function acao(o,f){if(!o){if(f.d)abre(f.p);return}"
    "if(o=='baixar')location.href='/fs/baixar?p='+encodeURIComponent(f.p);"
    "else if(o=='renomear'){var n=prompt('Novo nome:',f.n);if(n&&n!=f.n)op('renomear',f.p,null,n)}"
    "else if(o=='recortar'||o=='copiarclip'){clip={o:o=='recortar'?'mover':'copiar',f:f};mostraClip()}"
    "else if(o=='clonar')op('clonar',f.p);"
    "else if(o=='apagar'){if(confirm('Apagar '+f.n+(f.d?' e tudo dentro dela':'')+'? Não dá para desfazer.'))op('apagar',f.p)}}"
    "function mostraClip(){var c=$('clip');if(!clip){c.style.display='none';return}c.style.display='block';"
    "c.innerHTML=(clip.o=='mover'?'Mover ':'Copiar ')+'<b>'+esc(clip.f.n)+'</b>: abra a pasta de destino e '"
    "+(cur?'<button class=a onclick=\"cola()\">colar aqui</button> ':'escolha uma pasta. ')+'<button onclick=\"clip=null;mostraClip()\">cancelar</button>'}"
    "function cola(){var c=clip;clip=null;op(c.o,c.f.p,cur)}"
    "function nova(){if(!cur){alert('Abra uma pasta primeiro.');return}var n=prompt('Nome da pasta nova:');if(n)op('pasta',cur,null,n)}"
    "$('up').onchange=function(e){if(!cur){alert('Abra uma pasta primeiro.');return}var fs=Array.prototype.slice.call(e.target.files);"
    "(function vai(){var f=fs.shift();if(!f){abre(cur);return}var x=new XMLHttpRequest;x.open('PUT','/fs/enviar?a='+encodeURIComponent(cur)+'&n='+encodeURIComponent(f.name));"
    "x.upload.onprogress=function(ev){$('msg').textContent=f.name+': '+Math.round(ev.loaded*100/f.size)+'%'};x.onload=vai;x.send(f)})()};"
    "abre('roms')";

static std::string filesPage() {
    return head("ScrapCade - arquivos", true) +
           "<div id=cam></div>"
           "<div class=bar><button onclick='nova()'>Nova pasta</button><button onclick=\"document.getElementById('up').click()\">"
           "Enviar arquivos para esta pasta</button><input id=up type=file multiple></div>"
           "<div id=clip></div><div id=msg></div><table id=t></table></main><script>" + FILES_SCRIPT + "</script></html>";
}

static const char *SCRIPT =
    "var d=document.getElementById('d'),s=document.getElementById('s'),q=[],on=0;"
    "var cg=document.getElementById('cg');"
    "function go(){if(on||!q.length){if(!on){cg.style.display='none';if(s.dataset.fez){s.dataset.fez='';setTimeout(function(){location.reload()},15000)}}return}on=1;cg.style.display='block';"
    "var f=q.shift(),l=document.createElement('div');s.appendChild(l);var x=new XMLHttpRequest;"
    "x.open('PUT','/envio/'+encodeURIComponent(f.name));"
    "x.upload.onprogress=function(e){l.textContent=f.name+': '+Math.round(e.loaded*100/f.size)+'%'};"
    "x.onload=function(){l.textContent=f.name+(x.status==200?': enviado':': erro, '+x.statusText);s.dataset.fez=1;on=0;go()};"
    "x.onerror=function(){l.textContent=f.name+': erro de rede';on=0;go()};x.send(f)}"
    "function add(fs){for(var i=0;i<fs.length;i++)q.push(fs[i]);go()}"
    "document.getElementById('f').onchange=function(e){add(e.target.files)};"
    "d.ondragover=function(e){e.preventDefault();d.className='o'};d.ondragleave=function(){d.className=''};"
    "d.ondrop=function(e){e.preventDefault();d.className='';add(e.dataTransfer.files)};"
    "document.querySelectorAll('.x').forEach(function(b){b.onclick=function(){"
    "if(!confirm('Apagar '+b.dataset.t+'? Não dá para desfazer.'))return;"
    "var x=new XMLHttpRequest;x.open('POST','/apaga/'+encodeURIComponent(b.dataset.k));"
    "x.onload=function(){location.reload()};x.send()}});"
    "document.querySelectorAll('.p').forEach(function(b){b.onclick=function(){"
    "var i=document.createElement('input');i.type='file';i.accept='image/png,image/jpeg';i.onchange=function(){"
    "var x=new XMLHttpRequest;x.open('PUT','/capa/'+encodeURIComponent(b.dataset.k));"
    "x.onload=function(){b.textContent=x.status==200?'capa trocada':'erro'};x.send(i.files[0])};i.click()}});";

static std::string page() {
    std::vector<Game> gs;
    {
        std::lock_guard<std::mutex> l(mu);
        gs = games;
    }
    std::string rows;
    for (auto &g : gs) {
        std::string meta = Meta::line(g);
        rows += "<tr><td>" + html(g.title) + "<div class=c>" + html(g.sys->name) + (meta.empty() ? "" : "  •  " + html(meta)) +
                "</div></td><td class=b><button class=p data-k=\"" + html(g.key()) + "\">trocar capa</button>"
                "<button class=x data-k=\"" + html(g.key()) + "\" data-t=\"" + html(g.title) + "\">apagar</button></td></tr>";
    }
    return head("ScrapCade", false) +
           "<label id=d>Arraste os jogos para cá, ou toque para escolher<input id=f type=file multiple></label><div id=s></div>"
           "<img id=cg src=/carregando.gif alt=Carregando style='display:none;height:36px;margin:8px 0'>"
                 "<div class=c>Cada jogo vai sozinho para o console certo, e a capa é baixada sozinha.</div>"
                 "<h2>Jogos (" + std::to_string(gs.size()) + ")</h2><table>" + rows + "</table></main><script>" + SCRIPT +
           "</script></html>";
}

// So a rede de casa: 10.x, 172.16-31.x, 192.168.x, 127.x e link-local
static bool local(const sockaddr_in &a) {
    uint32_t ip = ntohl(a.sin_addr.s_addr);
    return (ip >> 24) == 10 || (ip >> 24) == 127 || (ip >> 20) == 0xAC1 || (ip >> 16) == 0xC0A8 || (ip >> 16) == 0xA9FE;
}

static void handle(int fd) {
    // cabecalho
    std::string buf;
    char c[4096];
    size_t end;
    while ((end = buf.find("\r\n\r\n")) == std::string::npos) {
        ssize_t n = recv(fd, c, sizeof c, 0);
        if (n <= 0 || buf.size() > 65536) return;
        buf.append(c, size_t(n));
    }
    std::string head = buf.substr(0, end), rest = buf.substr(end + 4);
    std::string first = head.substr(0, head.find("\r\n"));
    size_t sp1 = first.find(' '), sp2 = first.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return;
    std::string method = first.substr(0, sp1), path = first.substr(sp1 + 1, sp2 - sp1 - 1);
    long long length = 0;
    std::string low = lower(head);
    size_t cl = low.find("\r\ncontent-length:");
    if (cl != std::string::npos) length = atoll(head.c_str() + cl + 17);

    if (method == "GET" && path == "/") {
        sendBody(fd, "text/html; charset=utf-8", page());
    } else if (method == "GET" && (path == "/logo.jpg" || path == "/carregando.gif")) {
        std::ifstream in(DATA + "/web" + path, std::ios::binary);
        std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (body.empty()) reply(fd, "404 Nao encontrado");
        else sendBody(fd, path == "/logo.jpg" ? "image/jpeg" : "image/gif", body, "max-age=86400");
    } else if (method == "PUT" && path.compare(0, 7, "/envio/") == 0) {
        // so o nome do arquivo: nenhuma pasta vinda de fora
        std::string name = urldecode(path.substr(7));
        std::replace(name.begin(), name.end(), '\\', '/');
        name = name.substr(name.rfind('/') + 1);
        if (name.empty() || name[0] == '.' || endsWith(name, PARTIAL)) { reply(fd, "400 Nome invalido"); return; }
        std::string err = receiveFile(fd, rest, length, INBOX + "/" + name);
        reply(fd, err.empty() ? "200 OK" : err);
        if (err.empty() && changed) changed();
    } else if (method == "PUT" && path.compare(0, 6, "/capa/") == 0) {
        Game g;
        if (length <= 0 || length > 16LL * 1024 * 1024) { reply(fd, "400 Imagem invalida"); return; }
        if (!findGame(urldecode(path.substr(6)), g)) { reply(fd, "404 Nao encontrado"); return; }
        // o SDL_image le pelo conteudo: PNG ou JPG servem com o nome .png
        std::string err = receiveFile(fd, rest, length, g.dir + "/images/" + g.base + ".png");
        reply(fd, err.empty() ? "200 OK" : err);
        if (err.empty() && changed) changed();
    } else if (method == "POST" && path.compare(0, 7, "/apaga/") == 0) {
        Game g;
        if (!findGame(urldecode(path.substr(7)), g)) { reply(fd, "404 Nao encontrado"); return; }
        std::string p = g.dir + "/" + g.file;
        if (remove(p.c_str()) != 0) { reply(fd, "500 Nao apagou"); return; }
        if (endsWith(g.file, ".cue")) remove((g.dir + "/" + g.base + ".bin").c_str());   // .cue leva junto o .bin
        reply(fd, "200 OK");
        if (changed) changed();
    } else if (method == "GET" && path == "/arquivos") {
        sendBody(fd, "text/html; charset=utf-8", filesPage());
    } else if (method == "GET" && path.compare(0, 9, "/fs/lista") == 0) {
        auto q = Arquivos::query(path, urldecode);
        std::string j = Arquivos::list(q["p"]);
        if (j.empty()) reply(fd, "404 Nao encontrado");
        else sendBody(fd, "application/json; charset=utf-8", j);
    } else if (method == "GET" && path.compare(0, 10, "/fs/baixar") == 0) {
        auto q = Arquivos::query(path, urldecode);
        std::string f = Arquivos::resolve(q["p"]);
        FILE *in = f.empty() || Arquivos::isDirP(f) ? nullptr : fopen(f.c_str(), "rb");
        if (!in) { reply(fd, "404 Nao encontrado"); return; }
        std::string n = Arquivos::nameOf(f);
        n.erase(std::remove(n.begin(), n.end(), '"'), n.end());
        struct stat st;
        stat(f.c_str(), &st);
        std::string h = "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " +
                        std::to_string(st.st_size) + "\r\nContent-Disposition: attachment; filename=\"" + n +
                        "\"\r\nConnection: close\r\n\r\n";
        sendAll(fd, h.data(), h.size());
        std::vector<char> b(1 << 16);
        size_t r;
        while ((r = fread(b.data(), 1, b.size(), in)) > 0)
            if (!sendAll(fd, b.data(), r)) break;
        fclose(in);
    } else if (method == "PUT" && path.compare(0, 10, "/fs/enviar") == 0) {
        // envio direto para uma pasta escolhida no gerenciador (nao passa pela caixa de entrada)
        auto q = Arquivos::query(path, urldecode);
        std::string dir = Arquivos::resolve(q["a"]), n = q["n"];
        if (dir.empty() || !Arquivos::isDirP(dir)) { reply(fd, "404 Pasta nao encontrada"); return; }
        if (Arquivos::badName(n) || n[0] == '.') { reply(fd, "400 Nome invalido"); return; }
        std::string err = receiveFile(fd, rest, length, dir + "/" + n);
        reply(fd, err.empty() ? "200 OK" : err);
        if (err.empty() && changed) changed();
    } else if (method == "POST" && path.compare(0, 4, "/fs/") == 0) {
        size_t qm = path.find('?');
        std::string op = path.substr(4, qm == std::string::npos ? std::string::npos : qm - 4);
        auto q = Arquivos::query(path, urldecode);
        std::string r = Arquivos::run(op, q);
        reply(fd, r);
        if (r.compare(0, 3, "200") == 0 && changed) changed();
    } else {
        reply(fd, "404 Nao encontrado");
    }
}

static void serve() {
    int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);   // nao passa para o menu reaberto
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(PORT);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (sockaddr *)&a, sizeof a) != 0 || listen(s, 16) != 0) {
        fprintf(stderr, "scrapcade: envio: porta %d ocupada\n", PORT);
        close(s);
        return;
    }
    while (true) {
        sockaddr_in from{};
        socklen_t fl = sizeof from;
        int fd = accept4(s, (sockaddr *)&from, &fl, SOCK_CLOEXEC);
        if (fd < 0) continue;
        if (!local(from)) { close(fd); continue; }
        timeval tv{60, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        // cada pedido na sua thread: um envio longo nao trava a pagina
        std::thread([fd] {
            handle(fd);
            close(fd);
        }).detach();
    }
}

static void start() { std::thread(serve).detach(); }

}  // namespace Envio
