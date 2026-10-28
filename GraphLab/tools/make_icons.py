#!/usr/bin/env python3
"""Original GraphLab menu icons. Requires Pillow only to regenerate PNGs."""
from math import sin, cos
from pathlib import Path
from PIL import Image, ImageDraw

# Original 5x7 pixel lettering, not copied from a font.
LETTERS = {
    'G': ('01110','10001','10000','10111','10001','10001','01110'),
    'R': ('11110','10001','10001','11110','10100','10010','10001'),
    'A': ('01110','10001','10001','11111','10001','10001','10001'),
    'P': ('11110','10001','10001','11110','10000','10000','10000'),
    'H': ('10001','10001','10001','11111','10001','10001','10001'),
    'L': ('10000','10000','10000','10000','10000','10000','11111'),
    'B': ('11110','10001','10001','11110','10001','10001','11110'),
    ' ': ('000','000','000','000','000','000','000'),
}

def make(selected):
    scale=3; im=Image.new('RGB',(92*scale,64*scale)); d=ImageDraw.Draw(im)
    def line(points, fill, width=1):
        d.line([(int(x*scale),int(y*scale)) for x,y in points],fill=fill,width=width*scale)
    bg=(8,27,45) if not selected else (4,54,65)
    d.rectangle((0,0,92*scale-1,64*scale-1),fill=bg)
    border=(50,183,196) if selected else (87,115,132)
    d.rectangle((1*scale,1*scale,90*scale,62*scale),outline=border,width=scale)
    # A wireframe saddle surface, explicitly drawn from our own coordinates.
    for fixed in [-1,-.6,-.2,.2,.6,1]:
        for mode in (0,1):
            pts=[]
            for t in range(25):
                q=-1+2*t/24
                x,y=(q,fixed) if mode==0 else (fixed,q)
                z=.5*sin(x*2.2)*cos(y*2.1)
                pts.append((46+20*x+12*y, 26+8*y-12*z))
            line(pts,(28,194,200) if mode else (92,147,205))
    # Own pixel lettering, GRAPH LAB (5x7).
    word='GRAPH LAB';width=sum(len(LETTERS[c][0])+1 for c in word)-1;x=(92-width)//2
    for c in word:
        rows=LETTERS[c]
        for yy,row in enumerate(rows):
            for xx,v in enumerate(row):
                if v=='1':d.rectangle(((x+xx)*scale,(53+yy)*scale,(x+xx+1)*scale-1,(54+yy)*scale-1),fill=(239,249,250))
        x+=len(rows[0])+1
    return im.resize((92,64),Image.Resampling.LANCZOS)

out=Path(__file__).resolve().parent.parent/'assets-cg';out.mkdir(exist_ok=True)
make(False).save(out/'icon-uns.png');make(True).save(out/'icon-sel.png')
