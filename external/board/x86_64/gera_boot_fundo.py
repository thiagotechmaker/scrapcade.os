"""Fundo do menu de boot do BIOS antigo (syslinux vesamenu, 640x480): degrade, faixas de luz tipo aurora, a logo do
ScrapCade OS e o endereco do projeto. A versao NAO vai na imagem: ela entra no titulo do menu (boot-menu.cfg, @VERSAO@,
preenchido pelo post-image-efi.sh a partir do arquivo VERSION).
Uso: python3 gera_boot_fundo.py   (grava boot-fundo.png na pasta deste script)"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

AQUI = os.path.dirname(os.path.abspath(__file__))
LOGO = os.path.join(AQUI, "..", "..", "package", "scrapcade", "data", "web", "logo.png")
W, H = 640, 480
img = Image.new("RGB", (W, H), (11, 5, 32))
d = ImageDraw.Draw(img)
for y in range(H):   # degrade de cima (azul escuro) para baixo (roxo escuro)
    t = y / H
    d.line([(0, y), (W, y)], fill=(int(11 + 20 * t), int(10 + 4 * t), int(40 + 30 * t)))

# faixas de luz tipo aurora
luz = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ld = ImageDraw.Draw(luz)
for cor, amp, fase, y0 in [((50, 230, 168, 90), 26, 0.0, 120), ((25, 230, 255, 70), 30, 1.7, 150), ((120, 90, 255, 60), 22, 3.1, 175)]:
    pts = [(x, y0 + amp * math.sin(x / 95 + fase) + 10 * math.sin(x / 31 + fase * 2)) for x in range(0, W + 8, 8)]
    for k in range(18):
        ld.line([(x, y + k * 2) for x, y in pts], fill=cor[:3] + (max(0, cor[3] - k * 5),), width=3)
luz = luz.filter(ImageFilter.GaussianBlur(9))
img = Image.alpha_composite(img.convert("RGBA"), luz).convert("RGB")


def fonte(tam):
    for p in (r"C:\Windows\Fonts\segoeui.ttf", r"C:\Windows\Fonts\arial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(p, tam)
        except OSError:
            pass
    return ImageFont.load_default()


logo = Image.open(LOGO).convert("RGBA")
lw = 440
lh = int(logo.height * lw / logo.width)
logo = logo.resize((lw, lh), Image.LANCZOS)
img = img.convert("RGBA")
img.alpha_composite(logo, ((W - lw) // 2, 34))
img = img.convert("RGB")
d = ImageDraw.Draw(img)
s, sub = "Jogos em qualquer computador", fonte(16)
d.text(((W - d.textlength(s, font=sub)) / 2, 34 + lh + 16), s, font=sub, fill=(200, 205, 230))
r, pq = "github.com/thiagotechmaker/scrapcade.os", fonte(12)
d.text(((W - d.textlength(r, font=pq)) / 2, H - 26), r, font=pq, fill=(150, 150, 190))
img.save(os.path.join(AQUI, "boot-fundo.png"), optimize=True)
print("ok")
