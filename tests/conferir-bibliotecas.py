#!/usr/bin/env python3
# Confere o sistema compilado sem ligar nada: cada programa e biblioteca acha as bibliotecas que pede (NEEDED),
# e cada nucleo do RetroArch acha os simbolos que precisa ja ao abrir. Pega o nucleo que compila mas nao abre
# (v0.22: ScummVM sem a vorbisfile, PlayStation sem duas funcoes do leitor de CD; Citra, Flycast e PPSSPP com
# bibliotecas internas que nao foram instaladas).
#   python3 tests/conferir-bibliotecas.py output/x86_64/target     sai com 1 se achar problema
import os, re, subprocess, sys

T = os.path.abspath(sys.argv[1])
# vem de fora do sistema: o vgui.so e da copia do Half-Life da Steam do usuario
DE_FORA = {"vgui.so"}
# relocacoes resolvidas ja no dlopen (as JUMP_SLOT ficam para a primeira chamada)
IMEDIATAS = ("R_X86_64_GLOB_DAT", "R_X86_64_64", "R_386_GLOB_DAT", "R_386_32")

def real(p):
    """Caminho dentro do sistema compilado, seguindo links absolutos como se a raiz fosse T."""
    for _ in range(40):
        if not os.path.islink(p): return p
        alvo = os.readlink(p)
        p = T + alvo if alvo.startswith("/") else os.path.join(os.path.dirname(p), alvo)
    return p

cache = {}
def le(p):
    if p in cache: return cache[p]
    out = subprocess.run(["readelf", "-W", "-d", "--dyn-syms", "-r", p], capture_output=True, text=True).stdout
    need = re.findall(r"\(NEEDED\).*\[(.+?)\]", out)
    runpath = [x for rp in re.findall(r"\((?:RUNPATH|RPATH)\).*\[(.+?)\]", out) for x in rp.split(":") if x]
    defs, und = set(), set()
    for l in out.splitlines():
        c = l.split()
        if len(c) >= 8 and c[0].endswith(":") and c[0][:-1].isdigit():
            nome = c[7].split("@")[0]
            if c[6] == "UND":
                if c[4] != "WEAK": und.add(nome)
            elif c[4] != "LOCAL": defs.add(nome)
    imed = set()
    for l in out.splitlines():
        c = l.split()
        if len(c) >= 5 and c[2] in IMEDIATAS: imed.add(c[4].split("@")[0])
    # o sistema liga tudo com "-z now": nesse caso todo simbolo e resolvido ja ao abrir, nao so os IMEDIATAS
    if re.search(r"\(FLAGS_1\).*\bNOW\b|\(BIND_NOW\)|\(FLAGS\).*\bBIND_NOW\b", out): imed = set(und)
    cache[p] = (need, runpath, defs, und, imed)
    return cache[p]

def pastas(p):
    _, runpath, _, _, _ = le(p)
    d = os.path.dirname(p)
    ps = [T + x.replace("$ORIGIN", d[len(T):]) if x.startswith(("/", "$ORIGIN")) else T + "/" + x for x in runpath]
    return ps + [T + "/lib", T + "/usr/lib", d]

def acha(nome, p):
    for d in pastas(p):
        c = os.path.join(d, nome)
        if os.path.lexists(c) and os.path.exists(real(c)): return real(c)

def fecho(p, vistos):
    for n in le(p)[0]:
        c = acha(n, p)
        if c and c not in vistos:
            vistos.add(c); fecho(c, vistos)
    return vistos

def eh_elf(p):
    try:
        with open(p, "rb") as h: return h.read(4) == b"\x7fELF"
    except OSError: return False

problemas = 0
# 1) bibliotecas que faltam, em todo o sistema
for raiz, _, arquivos in os.walk(T):
    for a in arquivos:
        p = os.path.join(raiz, a)
        if os.path.islink(p) or not os.path.isfile(p) or not eh_elf(p): continue
        faltam = [n for n in le(p)[0] if n not in DE_FORA and not acha(n, p)]
        if faltam:
            problemas += 1
            print(f"falta biblioteca: {p[len(T):]} pede {', '.join(faltam)}")
# 2) simbolos dos nucleos: o RetroArch abre o nucleo com o que ele ja carregou (libc, libm...) valendo para todos
ra = T + "/usr/bin/retroarch"
globais = set(le(ra)[2])
for l in fecho(ra, set()): globais |= le(l)[2]
pasta = T + "/usr/lib/libretro"
nucleos = sorted(f for f in os.listdir(pasta) if f.endswith("_libretro.so")) if os.path.isdir(pasta) else []
for f in nucleos:
    p = os.path.join(pasta, f)
    _, _, defs, und, imed = le(p)
    tem = set(defs) | globais
    for l in fecho(p, set()): tem |= le(l)[2]
    falta = sorted(und - tem)
    graves = [s for s in falta if s in imed]
    if graves:
        problemas += 1
        print(f"nucleo nao abre: {f} sem {', '.join(graves[:6])}{' ...' if len(graves) > 6 else ''}")
    elif falta:
        print(f"aviso: {f} chama {', '.join(falta[:4])} que nao existe (so quebra se o nucleo chegar a usar)")
print(f"bibliotecas: {len(nucleos)} nucleos conferidos, {problemas} problema(s)")
sys.exit(1 if problemas else 0)
