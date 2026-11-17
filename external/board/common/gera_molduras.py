#!/usr/bin/python3
# Molduras (bezels) do ScrapCade para os consoles de mesa: chapa de metal escura com rebites e um filete verde
# em volta da imagem 4:3, uma para cada formato de tela. Saida em rootfs-overlay/usr/share/tos/molduras.
# Uso: python3 external/board/common/gera_molduras.py
import os, random
from PIL import Image, ImageDraw, ImageFilter

AQUI = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(AQUI, "rootfs-overlay/usr/share/tos/molduras")
TINTA, ACO, MENTA = (22, 19, 46), (58, 53, 102), (50, 230, 168)

def chapa(w, h, seed):
    random.seed(seed)
    img = Image.new("RGB", (w, h), TINTA)
    d = ImageDraw.Draw(img)
    for y in range(0, h, 2):   # metal escovado: riscos horizontais bem fracos
        v = random.randint(-6, 6)
        d.line([(0, y), (w, y)], fill=(TINTA[0] + 4 + v, TINTA[1] + 3 + v, TINTA[2] + 8 + v))
    return img.filter(ImageFilter.GaussianBlur(0.6))

def rebite(d, x, y, r):
    d.ellipse([x - r, y - r, x + r, y + r], fill=(40, 36, 78), outline=(14, 12, 30), width=2)
    d.ellipse([x - r // 2, y - r // 2, x, y], fill=(88, 82, 140))

def moldura(nome, w, h):
    hw = min(w, h * 4 // 3); hh = hw * 3 // 4   # buraco 4:3 o maior possivel
    x0, y0 = (w - hw) // 2, (h - hh) // 2
    img = chapa(w, h, nome).convert("RGBA")
    d = ImageDraw.Draw(img)
    # chapas laterais: emenda e rebites nos cantos
    for (ax, ay, bx, by) in ([(0, 0, x0, h), (x0 + hw, 0, w, h)] if x0 > 0 else [(0, 0, w, y0), (0, y0 + hh, w, h)]):
        if bx - ax < 80 or by - ay < 80:   # faixa fina (tela 5:4): so a chapa
            continue
        m = 18
        d.rectangle([ax + m, ay + m, bx - m, by - m], outline=(12, 10, 28), width=3)
        d.rectangle([ax + m + 3, ay + m + 3, bx - m - 3, by - m - 3], outline=ACO, width=1)
        for (rx, ry) in ((ax + m + 16, ay + m + 16), (bx - m - 16, ay + m + 16), (ax + m + 16, by - m - 16), (bx - m - 16, by - m - 16)):
            rebite(d, rx, ry, 9)
    # filete verde e sombra em volta da imagem
    for i in range(10):
        a = int(110 * (1 - i / 10))
        d.rectangle([x0 - 4 - i, y0 - 4 - i, x0 + hw + 3 + i, y0 + hh + 3 + i], outline=(0, 0, 0, a))
    d.rectangle([x0 - 4, y0 - 4, x0 + hw + 3, y0 + hh + 3], outline=MENTA + (255,), width=2)
    d.rectangle([x0, y0, x0 + hw - 1, y0 + hh - 1], fill=(0, 0, 0, 0))   # o jogo aparece aqui
    img.save(os.path.join(OUT, "moldura-%s.png" % nome), optimize=True)
    with open(os.path.join(OUT, "moldura-%s.cfg" % nome), "w") as f:
        f.write("overlays = 1\noverlay0_overlay = moldura-%s.png\noverlay0_full_screen = true\noverlay0_descs = 0\n" % nome)

os.makedirs(OUT, exist_ok=True)
moldura("169", 1920, 1080)
moldura("1610", 1920, 1200)
moldura("54", 1280, 1024)
print("molduras em", OUT)
