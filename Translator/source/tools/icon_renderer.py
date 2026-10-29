#!/usr/bin/env python3
"""Create restrained, text-free 92x64 fx-CG50 icons in two neutral states.

The supplied reference is a 92x64 menu icon, with an ~80x38 rounded graphite
panel near the top. These vector pictograms and gradients are original; no
reference pixels or Casio imagery are copied. Requires Pillow.
"""
from pathlib import Path
from PIL import Image, ImageDraw
import math

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Icons'
W, H, S = 92, 64, 6
INK = (245, 245, 245)
MUTED = (195, 195, 195)


def xy(x, y): return (round(x*S), round(y*S))
def pts(points): return [xy(x, y) for x, y in points]
def line(draw, points, color=INK, width=1.5):
    draw.line(pts(points), fill=color, width=max(1, round(width*S)), joint='curve')
    # Round line endings at icon scale.
    r = width / 2
    for x, y in (points[0], points[-1]):
        draw.ellipse((round((x-r)*S), round((y-r)*S), round((x+r)*S), round((y+r)*S)), fill=color)
def dot(draw, x, y, r, color=INK):
    draw.ellipse((round((x-r)*S), round((y-r)*S), round((x+r)*S), round((y+r)*S)), fill=color)
def rect(draw, bounds, color=INK, width=1.4, radius=0, fill=None):
    x1,y1,x2,y2 = bounds
    box = (round(x1*S), round(y1*S), round(x2*S), round(y2*S))
    if radius:
        draw.rounded_rectangle(box, radius=round(radius*S), outline=color, width=max(1,round(width*S)), fill=fill)
    else:
        draw.rectangle(box, outline=color, width=max(1,round(width*S)), fill=fill)

def bg(selected):
    # Actual states are neutral R=G=B; no blue anywhere.
    base = Image.new('RGB', (W*S, H*S), (255,255,255))
    if selected:
        pix = base.load()
        for y in range(H*S):
            shade = round(226 - 20*y/(H*S-1))
            for x in range(W*S): pix[x,y] = (shade,shade,shade)
    d = ImageDraw.Draw(base)
    shadow = (181,181,181) if selected else (216,216,216)
    d.rounded_rectangle((5*S, 6*S, 86*S, 44*S), radius=3*S, fill=shadow)
    mask = Image.new('L', base.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((5*S, 5*S, 86*S, 42*S), radius=3*S, fill=255)
    tile = Image.new('RGB',base.size)
    px = tile.load()
    for y in range(5*S,43*S):
        for x in range(5*S,87*S):
            # Graphite panel: slightly lighter top-left, matching the reference's restraint.
            t=(x/S-5)/81; u=(y/S-5)/37
            value=round((88 if not selected else 74) - 16*t - 8*u)
            px[x,y]=(value,value,value)
    base.paste(tile, (0,0), mask)
    return base

def cubic(a,b,c,d,n=32):
    result=[]
    for i in range(n+1):
        t=i/n; u=1-t
        result.append((u*u*u*a[0]+3*u*u*t*b[0]+3*u*t*t*c[0]+t*t*t*d[0],
                       u*u*u*a[1]+3*u*u*t*b[1]+3*u*t*t*c[1]+t*t*t*d[1]))
    return result

def graphlab(d):
    # Cartesian axes + a smooth plotted curve, no X/Y letters.
    line(d, [(30,34),(30,14)], MUTED, 1.25)
    line(d, [(30,34),(63,34)], MUTED, 1.25)
    for xx in (38,46,54,62): line(d,[(xx,33.2),(xx,35.1)],MUTED,0.8)
    for yy in (20,27): line(d,[(28.9,yy),(30.9,yy)],MUTED,0.8)
    curve=cubic((31,30),(35,28),(36,18),(41,21))[:-1]+cubic((41,21),(47,25),(47,31),(52,26))[:-1]+cubic((52,26),(57,21),(57,15),(63,14))
    line(d,curve,INK,2.0)

def wireworld(d):
    # Small circuit network/pulse: paths and terminals, not text.
    line(d, [(30,24),(62,24)], MUTED,1.5)
    line(d, [(38,24),(38,15),(51,15)], MUTED,1.5)
    line(d, [(47,24),(47,33),(58,33)], MUTED,1.5)
    line(d, [(57,24),(57,19),(62,19)], MUTED,1.3)
    for x,y in [(30,24),(38,15),(51,15),(47,33),(58,33),(62,19),(62,24)]:
        rect(d,(x-2.2,y-2.2,x+2.2,y+2.2),INK,1.1,0.6,fill=(78,78,78))
    # An active cell against the calmer connecting wire.
    rect(d,(41,21.5,45.8,26.3),INK,0.9,0.5,fill=INK)

def pycedit(d):
    # A page with a turned corner and pencil, without characters.
    line(d,[(34,12),(50,12),(57,19),(57,36),(34,36),(34,12)],INK,1.6)
    line(d,[(50,12),(50,19),(57,19)],INK,1.4)
    line(d,[(38,22),(50,22)],MUTED,1.3)
    line(d,[(38,26),(48,26)],MUTED,1.3)
    line(d,[(38,30),(46,30)],MUTED,1.3)
    line(d,[(51,33),(62,22)],INK,2.7)
    line(d,[(49.8,35),(51,32.2)],INK,1.25)
    dot(d,62.4,21.5,1.0,INK)

def translator(d):
    # Two overlapping conversation bubbles, with symbolic dots rather than letters.
    rect(d,(29,12,50.5,26.5),INK,1.5,3.6)
    line(d,[(34,26.5),(32.6,29),(38,26.5)],INK,1.4)
    rect(d,(43,22,64,35),INK,1.5,3.3,fill=(76,76,76))
    line(d,[(58,35),(60.5,38),(55,35)],INK,1.4)
    for x in (34.5,39.7,44.9): dot(d,x,19,1.05,INK)
    for x in (48.2,53.3,58.4): dot(d,x,28.3,1.05,INK)

def objview(d):
    # Isometric wireframe model plus a small orbit, no lettering.
    box=(29*S,15*S,63*S,36*S)
    d.arc(box, start=10, end=165, fill=MUTED, width=round(1.2*S))
    d.arc(box, start=190, end=345, fill=MUTED, width=round(1.2*S))
    top=(46,11); l=(34,17); mid=(46,23); r=(58,17)
    lb=(34,30); bot=(46,36); rb=(58,30)
    for path in [(top,l,mid,r,top),(l,lb,bot,rb,r),(mid,bot)]: line(d,path,INK,1.65)
    dot(d,62,25.1,1.6,INK)

DRAWERS = {'GraphLab':graphlab,'Wireworld':wireworld,'PyCEdit':pycedit,'Translator':translator,'ObjView':objview}

def render_icon(app, selected=False):
    """Return one 92x64 neutral RGB icon without writing any files."""
    im=bg(selected)
    DRAWERS[app](ImageDraw.Draw(im))
    im=im.resize((W,H),Image.Resampling.LANCZOS)
    assert im.size==(92,64)
    assert im.getchannel("R").tobytes()==im.getchannel("G").tobytes()==im.getchannel("B").tobytes()
    return im

def main():
    for app in DRAWERS:
        out=OUT/app; out.mkdir(parents=True,exist_ok=True)
        for selected in (False,True):
            im=render_icon(app,selected)
            name='selected' if selected else 'unselected'
            im.save(out/(name+'.png'),optimize=True)
            im.save(out/(name+'.bmp'))
            print(out/(name+'.png'))

if __name__ == '__main__':
    main()
