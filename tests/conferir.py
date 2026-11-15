#!/usr/bin/env python3
# Confere uma rodada do tests/testar.sh: compara cada tela (teste/z_*.ppm) com tests/referencias/*.png e
# confere os resultados de teste/resumo.txt. Imprime so o resumo; as telas diferentes vao para teste/falhas.png.
#   conferir.py <pasta do teste> <pasta das referencias> [--referencias]
import glob, os, re, sys
from PIL import Image, ImageChops, ImageDraw, ImageStat

T, REF = sys.argv[1], sys.argv[2]
GERAR = len(sys.argv) > 3 and sys.argv[3] == "--referencias"
LIMITE = 9.0   # % de diferenca media aceita (relogio, CPU e animacoes mudam um pouco)

# resultados esperados no resumo (o que o teste mediu por fora: rede, portas, envio, som)
ESPERADO = [
    (r"iso arrumado: 2", ".iso arrumado em SCRAPCADE + SCRAP-DADOS"),
    (r"iso particoes: SCRAPCADE SCRAP-DADOS", ".iso: particoes com os nomes certos"),
    (r"disco particoes: SCRAPCADE SCRAP-DADOS", "pen drive: particao de jogos criada no 1o boot"),
    (r"pagina web: 200", "pagina web"),
    (r"samba antes: aberta", "Samba ligado de fabrica"), (r"ssh antes: fechada", "SSH desligado de fabrica"),
    (r"samba desligado: fechada", "desligar o Samba"), (r"samba religado: aberta", "religar o Samba"),
    (r"ssh ligado: aberta", "ligar o SSH"), (r"ssh desligado de novo: fechada", "desligar o SSH"),
    (r"som: tocou", "som pelo PipeWire"),
    (r"copia dos saves no pen drive: [1-9]", "copia dos saves no pen drive"),
    (r"ota baixou: 1", "atualizacao: baixou a versao nova"),
    (r"ota anterior guardada: sim", "atualizacao: guardou a versao anterior"),
    (r"ota confirmada: sim", "atualizacao: reiniciou na nova e confirmou"),
]


def mini(p):
    return Image.open(p).convert("L").resize((160, 100))


telas = sorted(glob.glob(os.path.join(T, "z_*.ppm")), key=os.path.getmtime)
os.makedirs(REF, exist_ok=True)
if GERAR:
    for p in telas:
        mini(p).save(os.path.join(REF, os.path.basename(p)[2:-4] + ".png"))
    print(f"referencias gravadas: {len(telas)} telas em {REF}")

falhas, ok = [], 0
feitas = {os.path.basename(p)[2:-4]: p for p in telas}
for ref in sorted(glob.glob(os.path.join(REF, "*.png"))):
    nome = os.path.basename(ref)[:-4]
    if nome not in feitas:
        falhas.append((nome, "nao foi capturada", None))
        continue
    a, b = mini(feitas[nome]), Image.open(ref).convert("L")
    dif = ImageStat.Stat(ImageChops.difference(a, b)).mean[0] / 2.55
    if dif > LIMITE:
        falhas.append((nome, f"mudou {dif:.0f}%", feitas[nome]))
    else:
        ok += 1
novas = [n for n in feitas if not os.path.exists(os.path.join(REF, n + ".png"))]

resumo = open(os.path.join(T, "resumo.txt"), encoding="utf-8", errors="replace").read()
erros = [d for r, d in ESPERADO if not re.search(r, resumo)]
envios = [l for l in resumo.splitlines() if re.match(r"(envio|capa) ", l)]
erros += [l.strip() for l in envios if not l.rstrip().endswith(": 200")]
total = len(ESPERADO) + len(envios)

print(f"Telas: {ok} iguais, {len(falhas)} com problema" + (f", {len(novas)} novas sem referencia" if novas else ""))
for n, m, _ in falhas:
    print(f"  TELA {n}: {m}")
print(f"Verificacoes: {total - len(erros)} ok, {len(erros)} falharam")
for e in erros:
    print(f"  FALHOU: {e}")

# folha so com as telas que mudaram (lado a lado com a referencia), para olhar sem abrir todas
ruins = [(n, p) for n, _, p in falhas if p]
if ruins:
    folha = Image.new("RGB", (660, 210 * len(ruins)), (40, 40, 40))
    d = ImageDraw.Draw(folha)
    for i, (n, p) in enumerate(ruins):
        folha.paste(Image.open(p).convert("RGB").resize((320, 200)), (0, i * 210))
        folha.paste(Image.open(os.path.join(REF, n + ".png")).convert("RGB").resize((320, 200)), (330, i * 210))
        d.text((4, i * 210 + 2), n + " (agora | referencia)", fill=(255, 255, 0))
    folha.save(os.path.join(T, "falhas.png"))
    print("telas que mudaram: teste/falhas.png")
sys.exit(1 if falhas or erros else 0)
