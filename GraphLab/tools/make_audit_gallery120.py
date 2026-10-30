#!/usr/bin/env python3
"""Assemble existing host screenshots into a readable Russian PDF.
Usage: python3 tools/make_audit_gallery120.py EVIDENCE_DIR OUTPUT.pdf
Screenshots themselves are not altered, other than nearest-neighbor scaling.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import sys

root, out = Path(sys.argv[1]), Path(sys.argv[2])
after = root
pages = [
 ('Единственный движок Ray', [
  ('Настройки 3D', 'Остались независимые начальные шаги Ray и Preview и качество Q. Выбора режима больше нет.', after/'settings3d.png'),
  ('Область внутри сферы', 'Неравенство x²+y²+z²≤4: конечный кадр Ray Q12. Численные пробы могут пропустить мелкие области.', after/'3d_volume.png')]),
 ('Ray: Preview и уточнение', [
  ('Preview после движения', 'Сфера x²+y²+z²=4 и плоскость z=x−y; крупный шаг P32.', after/'3d_preview.png'),
  ('Идёт уточнение 2→1', 'Отдельный запуск Q8: реально опубликованный частичный кадр с процентом выполненных строк.', after/'3d_progress.png')]),
 ('Ray и карта пересечения', [
  ('Финальный кадр Ray', 'Сфера и плоскость, Q12 после автоматического уточнения. Скорость на приборе неизвестна.', after/'3d_final.png'),
  ('XY-проекция', 'Сфера и горизонтальная плоскость; численная карта не гарантирует все корни.', after/'3d_map.png')]),
 ('Карта и график 2D', [
  ('Поиск точки в пикселе', 'Сфера и x=0: найден представитель возле z=−2; для других представителей доступен NEXT.', after/'3d_map_found.png'),
  ('Двумерный график', 'Построены y=sin(x) и x²+y²=4; показаны оси и сетка.', after/'2d.png')]),
 ('Редактор суммы и произведения', [
  ('Сумма с дробью', 'Редактируемая сумма с пределами и дробным телом (x/n).', after/'sum.png'),
  ('Произведение с дробью', 'Поля и пределы Π остаются редактируемыми.', after/'product.png')]),
 ('Удаление и функции', [
  ('Сумма удалена, введено 7', 'Пустое тело шаблона Σ не блокирует удаление.', after/'sum_erased.png'),
  ('Удалена функция sin', 'Имя функции удалено, а аргумент и скобки сохранены.', after/'unary_erased.png')]),
 ('Дроби и бесконечный ряд', [
  ('Дробь в 2D', 'Редактирование y=sin(x)/(1+x²).', after/'fraction_edit.png'),
  ('Бесконечный ряд', 'Сумма с индексом, степенью, факториалом и редактируемым бесконечным пределом.', after/'series_edit.png')]),
 ('Калькулятор', [
  ('Выход из дроби', 'Выражение sin(1/2)+1 и результат около 1.4794255.', after/'calc.png'),
  ('Сумма в калькуляторе', 'Выражение summ(0,3,n,1/(2))+1 даёт 3.', after/'calc_sum.png')]),
 ('3D-редактор и фрактал', [
  ('Формула 3D', 'Вертикальная дробь при редактировании z=sin(x/(1+y²)).', after/'3d_edit.png'),
  ('Фрактал z²+c', 'Построен стандартный фрактал в host-сценарии.', after/'fractal.png')]),
]
regular='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
bold='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
F=ImageFont.truetype(regular,25);S=ImageFont.truetype(regular,22)
H=ImageFont.truetype(bold,39);T=ImageFont.truetype(bold,28)
W,HGT=1240,1754

def wrapped(draw,text,font,width):
    lines=[];line=''
    for word in text.split():
        t=(line+' '+word).strip()
        if draw.textlength(t,font=font)>width and line:lines.append(line);line=word
        else:line=t
    if line:lines.append(line)
    return lines

images=[]
for n,(title,sections) in enumerate(pages,1):
    page=Image.new('RGB',(W,HGT),'white');d=ImageDraw.Draw(page)
    d.rectangle((0,0,W,18),fill='#111827')
    d.text((66,43),'GRAPHLAB 1.20  /  ПРОВЕРКА',font=S,fill='#596579')
    d.text((66,82),title,font=H,fill='#111827')
    d.text((66,143),'HOST-СНИМКИ • НЕ ФОТО ПРИБОРА',font=S,fill='#a84a32')
    y=196
    for label,desc,path in sections:
        d.text((75,y),label,font=T,fill='#111827');y+=43
        for line in wrapped(d,desc,F,W-150):d.text((75,y),line,font=F,fill='#384557');y+=34
        y+=15
        im=Image.open(path).convert('RGB')
        assert im.size==(396,224), (path,im.size)
        im=im.resize((990,560),resample=Image.Resampling.NEAREST)
        x=(W-im.width)//2
        d.rectangle((x-2,y-2,x+im.width+1,y+im.height+1),outline='#bdc5cf',width=2)
        page.paste(im,(x,y));y+=im.height+56
    d.line((66,HGT-80,W-66,HGT-80),fill='#d1d5db',width=2)
    d.text((66,HGT-57),'Синтетические клавиши и VRAM gint • подробности и ограничения — в отчёте',font=S,fill='#596579')
    d.text((W-110,HGT-57),str(n),font=S,fill='#596579')
    images.append(page)
images[0].save(out,'PDF',save_all=True,append_images=images[1:],resolution=150.0,title='GraphLab 1.20 — снимки проверок')
print(f'{len(images)} pages -> {out}')
