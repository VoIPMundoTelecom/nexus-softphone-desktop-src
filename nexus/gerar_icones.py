#!/usr/bin/env python3
"""Gera os icones/logos do Nexus desktop a partir do hexagrama do Grupo IVM
(givm_Frontend/apps/gestao/src/LogoIvmNexus.tsx), no mesmo desenho do icone Android
(nexus-softphone-android: hexagrama branco sobre azul IVM, no laranja, centro laranja).

Saidas (relativas a raiz do repo):
  Linphone/data/image/logo.svg, logo_margins.svg, splashscreen-logo.svg, linphone.svg
  Linphone/data/icon/hicolor/<N>x<N>/apps/icon.png  (16..1024)
  Linphone/data/icon.ico                             (16..256)

Uso: python3 nexus/gerar_icones.py   (requer Pillow)
"""
from __future__ import annotations

import os
from PIL import Image, ImageDraw

AZUL = "#1E2C5C"
LARANJA = "#E8672A"
CREME = "#F4F1EB"
BRANCO = "#FFFFFF"

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(RAIZ, "Linphone", "data", "image")
ICON = os.path.join(RAIZ, "Linphone", "data", "icon")

# Vertices do hexagono (coordenadas do LogoIvmNexus, raio 76)
VERT = [(0, -76), (66, -38), (66, 38), (0, 76), (-66, 38), (-66, -38)]
NO_LARANJA = 1  # (66,-38)


def hexagrama_svg(cx: float, cy: float, s: float, linha: str, no: str, fundo_centro: str,
                  traco_centro: str, borda_hex_opac: float = 0.32) -> str:
    """Desenho do hexagrama em SVG, centrado em (cx,cy), escala s (1 = raio 76)."""
    pts = [(cx + x * s, cy + y * s) for x, y in VERT]
    hexpath = "M " + " L ".join("%.2f %.2f" % p for p in pts) + " Z"
    out = ['<path d="%s" fill="none" stroke="%s" stroke-width="%.2f" stroke-linejoin="round" opacity="%.2f"/>'
           % (hexpath, linha, 3 * s, borda_hex_opac)]
    for (x, y) in pts:
        out.append('<line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f" stroke="%s" stroke-width="%.2f" stroke-linecap="round"/>'
                   % (cx, cy, x, y, linha, 5 * s))
    for i, (x, y) in enumerate(pts):
        out.append('<circle cx="%.2f" cy="%.2f" r="%.2f" fill="%s"/>' % (x, y, 10 * s, LARANJA if i == NO_LARANJA else no))
    out.append('<circle cx="%.2f" cy="%.2f" r="%.2f" fill="%s" stroke="%s" stroke-width="%.2f"/>'
               % (cx, cy, 20 * s, fundo_centro, traco_centro, 5 * s))
    out.append('<circle cx="%.2f" cy="%.2f" r="%.2f" fill="%s"/>' % (cx, cy, 10 * s, LARANJA))
    return "\n  ".join(out)


def svg(w: int, h: int, corpo: str) -> str:
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<!-- Nexus (Grupo IVM): hexagrama de LogoIvmNexus.tsx. Gerado por nexus/gerar_icones.py -->\n'
            '<svg width="%d" height="%d" viewBox="0 0 %d %d" fill="none" xmlns="http://www.w3.org/2000/svg">\n  %s\n</svg>\n'
            % (w, h, w, h, corpo))


def icone_svg(tam: int, margem: float) -> str:
    lado = tam - 2 * margem
    raio = lado * 0.1875
    fundo = '<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" rx="%.2f" fill="%s"/>' % (margem, margem, lado, lado, raio, AZUL)
    s = lado / 512 * 2.35
    return svg(tam, tam, fundo + "\n  " + hexagrama_svg(tam / 2, tam / 2, s, BRANCO, BRANCO, AZUL, BRANCO, 0.45))


def png_icone(tam: int, margem_frac: float) -> Image.Image:
    """Rasteriza o mesmo desenho com Pillow (supersampling 8x)."""
    k = 8
    T = tam * k
    im = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    m = T * margem_frac
    lado = T - 2 * m
    d.rounded_rectangle([m, m, T - m, T - m], radius=lado * 0.1875, fill=AZUL)
    s = lado / 512 * 2.35
    c = T / 2
    pts = [(c + x * s, c + y * s) for x, y in VERT]
    # contorno do hexagono (branco translucido)
    camada = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    ImageDraw.Draw(camada).line(pts + [pts[0]], fill=(255, 255, 255, 115), width=max(1, int(3 * s)), joint="curve")
    im = Image.alpha_composite(im, camada)
    d = ImageDraw.Draw(im)
    w = max(1, int(5 * s))
    for p in pts:
        d.line([(c, c), p], fill=BRANCO, width=w)
        d.ellipse([p[0] - w / 2, p[1] - w / 2, p[0] + w / 2, p[1] + w / 2], fill=BRANCO)
    for i, (x, y) in enumerate(pts):
        r = 10 * s
        d.ellipse([x - r, y - r, x + r, y + r], fill=LARANJA if i == NO_LARANJA else BRANCO)
    r = 20 * s + w / 2
    d.ellipse([c - r, c - r, c + r, c + r], fill=BRANCO)
    r = 20 * s - w / 2
    d.ellipse([c - r, c - r, c + r, c + r], fill=AZUL)
    r = 10 * s
    d.ellipse([c - r, c - r, c + r, c + r], fill=LARANJA)
    return im.resize((tam, tam), Image.LANCZOS)


def main() -> None:
    with open(os.path.join(IMG, "logo.svg"), "w") as f:
        f.write(icone_svg(512, 0))
    with open(os.path.join(IMG, "logo_margins.svg"), "w") as f:
        f.write(icone_svg(512, 32))
    # Splash: circulo azul com o hexagrama (substitui o circulo laranja do Linphone)
    corpo = '<circle cx="197.5" cy="197.5" r="197.5" fill="%s"/>\n  ' % AZUL + \
        hexagrama_svg(197.5, 197.5, 1.75, BRANCO, BRANCO, AZUL, BRANCO, 0.45)
    with open(os.path.join(IMG, "splashscreen-logo.svg"), "w") as f:
        f.write(svg(395, 395, corpo))
    # Boas-vindas (carrossel): hexagrama azul/laranja sobre fundo transparente, como no givm
    corpo = hexagrama_svg(50, 51, 0.6, AZUL, AZUL, CREME, AZUL, 0.32)
    with open(os.path.join(IMG, "linphone.svg"), "w") as f:
        f.write(svg(100, 102, corpo))

    for n in (16, 22, 24, 32, 64, 128, 256, 512, 1024):
        pasta = os.path.join(ICON, "hicolor", "%dx%d" % (n, n), "apps")
        os.makedirs(pasta, exist_ok=True)
        png_icone(n, 32 / 512).save(os.path.join(pasta, "icon.png"))
    grande = png_icone(256, 0)
    grande.save(os.path.join(RAIZ, "Linphone", "data", "icon.ico"),
                sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("ok")


if __name__ == "__main__":
    main()
