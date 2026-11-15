"""Blocos da barra animada do boot (a mesma da animacao de boot do iChroma), em pixels crus para o initrd desenhar
direto na tela (/dev/fb0) embaixo da logo: barra32.raw (XRGB8888: bytes B G R 0) e barra16.raw (RGB565).
Cada arquivo tem 5 blocos de BW x BH, um embaixo do outro: 0 = apagado, 1 = a cabeca acesa, 2 a 4 = o rastro.
Uso: python3 gera_barra.py   (grava na pasta deste script)"""
import os
import struct

BW, BH = 34, 10                 # 12 blocos de 34 px com 6 px entre eles: 474 px, a largura da logo (480)
FUNDO = (30, 36, 66)
MENTA = (50, 230, 168)


def mistura(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


cabeca = mistura(MENTA, (255, 255, 255), 0.35)
cores = [FUNDO] + [mistura(FUNDO, cabeca if k == 0 else MENTA, t) for k, t in enumerate((1.0, 0.6, 0.35, 0.18))]
px = []
for c in cores:
    for y in range(BH):
        cor = mistura((0, 0, 0), c, 0.72) if y >= BH - 3 else c      # as 3 linhas de baixo mais escuras, como no iChroma
        px += [cor] * BW
aqui = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(aqui, "barra32.raw"), "wb") as f:
    f.write(b"".join(bytes((b, g, r, 0)) for r, g, b in px))
with open(os.path.join(aqui, "barra16.raw"), "wb") as f:
    f.write(b"".join(struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)) for r, g, b in px))
print(len(cores), "blocos de", BW, "x", BH)
