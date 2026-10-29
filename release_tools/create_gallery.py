#!/usr/bin/env python3
"""Generate self-contained HTML and PNG preview of the 5x2 actual icons."""
from pathlib import Path
import base64, html
from PIL import Image, ImageDraw, ImageFont

root=Path(__file__).resolve().parents[1]
apps=['GraphLab','Wireworld','PyCEdit','Translator','ObjView']
cards=[]
for app in apps:
    images=[]
    for state, caption in [('unselected','Обычная'),('selected','Выделена')]:
        data=base64.b64encode((root/'Icons'/app/(state+'.png')).read_bytes()).decode()
        images.append(f'<figure><img src="data:image/png;base64,{data}" width="184" height="128" alt="{html.escape(app)} {caption}"><figcaption>{caption}</figcaption></figure>')
    cards.append(f'<section><h2>{html.escape(app)}</h2><div class="pair">{ "".join(images) }</div></section>')
page='''<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Иконки для Casio fx-CG50</title><style>body{font:16px system-ui,sans-serif;background:#f4f4f4;color:#222;max-width:1200px;margin:auto;padding:28px}h1{font-size:25px;margin:0 0 8px}p{color:#555;line-height:1.5}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(410px,1fr));gap:16px}section{background:#fff;border:1px solid #ddd;border-radius:12px;padding:14px 18px}h2{font-size:18px;margin:2px 0 10px}.pair{display:flex;gap:14px;flex-wrap:wrap}figure{margin:0}img{display:block;width:184px;height:128px;image-rendering:pixelated}figcaption{color:#555;font-size:13px;margin-top:4px}.note{margin-top:24px}</style>
<h1>Новые иконки для Casio fx-CG50</h1><p>Одинаковый спокойный стиль, без букв внутри значка. Показаны увеличенные исходные изображения 92×64; выделенное состояние — серое, не синее.</p><main>'''+''.join(cards)+'''</main><p class="note">Названия в этой галерее расположены снаружи изображений. На калькуляторе название отображает его меню.</p></html>'''
(root/'docs/menu_icons.html').write_text(page,encoding='utf-8')

font='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
bold='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
canvas=Image.new('RGB',(1040,810),'#f4f4f4');d=ImageDraw.Draw(canvas)
f=ImageFont.truetype(font,18);b=ImageFont.truetype(bold,26);bf=ImageFont.truetype(bold,20);sm=ImageFont.truetype(font,15)
d.text((32,20),'Иконки fx-CG50: без букв, выделение серое',font=b,fill='#202020')
d.text((32,58),'Оригинальные размеры 92×64; здесь показаны увеличенными в 2 раза',font=f,fill='#555555')
for i,app in enumerate(apps):
    x=32+(i%2)*500; y=105+(i//2)*230
    d.rounded_rectangle((x,y,x+476,y+211),radius=12,fill='white',outline='#d7d7d7',width=1)
    d.text((x+16,y+12),app,font=bf,fill='#202020')
    for j,(state,caption) in enumerate([('unselected','Обычная'),('selected','Выделена')]):
        im=Image.open(root/'Icons'/app/(state+'.png')).convert('RGB').resize((184,128),Image.Resampling.NEAREST)
        xx=x+16+j*215; canvas.paste(im,(xx,y+44));d.text((xx,y+177),caption,font=sm,fill='#555555')
canvas.save(root/'docs/menu_icons.png',optimize=True)
print(root/'docs/menu_icons.html')
print(root/'docs/menu_icons.png')
