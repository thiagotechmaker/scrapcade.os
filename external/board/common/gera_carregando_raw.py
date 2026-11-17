"""Tela de carregamento entre o menu e os jogos, em pixels crus para desenhar direto em /dev/fb0 (sem terminal):
a logo (a mesma do initrd) e o "CARREGANDO..." (os 8 quadros do carregando.png do menu, um embaixo do outro).
Grava em rootfs-overlay/usr/share/tos/carregando/: carregando32.raw e carregando16.raw (445 x 72 x 8 quadros).
Uso: python3 gera_carregando_raw.py"""
import os
import struct

from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(AQUI, "..", "..", "package", "scrapcade", "data", "carregando.png")
OUT = os.path.join(AQUI, "rootfs-overlay", "usr", "share", "tos", "carregando")
os.makedirs(OUT, exist_ok=True)
tira = Image.open(SRC).convert("RGBA")
qw, qh = tira.width // 8, tira.height
pilha = Image.new("RGB", (qw, qh * 8), (0, 0, 0))            # fundo preto, igual ao da tela de carregamento
for i in range(8):
    q = Image.new("RGBA", (qw, qh), (0, 0, 0, 255))
    q.alpha_composite(tira.crop((i * qw, 0, i * qw + qw, qh)))
    pilha.paste(q.convert("RGB"), (0, i * qh))
px = list(pilha.getdata())
with open(os.path.join(OUT, "carregando32.raw"), "wb") as f:
    f.write(b"".join(bytes((b, g, r, 0)) for r, g, b in px))
with open(os.path.join(OUT, "carregando16.raw"), "wb") as f:
    f.write(b"".join(struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)) for r, g, b in px))
print(qw, "x", qh, "x 8 quadros")
