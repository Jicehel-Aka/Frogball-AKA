#!/usr/bin/env python3
"""gen_sprites.py — decoupe assets/sprites_sheet.png (grille 6x6, fond magenta) et genere
main/ui/sprites.cpp (RGB565, cle de transparence 0xF81F) + assets/sprites_preview.png.
Usage : python3 tools/gen_sprites.py"""
import numpy as np
from PIL import Image
import os
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sheet = Image.open(os.path.join(root, 'assets/sprites_sheet.png')).convert('RGB')
W, H = sheet.size
COLS = ROWS = 6
# nom, (ligne, colonne), hauteur cible en pixels (largeur proportionnelle)
SPR = [
 ("frog_stand",0,0,48), ("frog_sit",0,1,28), ("frog_tongue",0,2,40), ("ball",0,4,11), ("fly",0,5,14),
 ("bumper_stack",1,3,30), ("bumper_mushroom",1,4,30), ("bumper_mouth",2,2,30),
 ("cattail_post",2,1,30), ("fly_post",2,3,28), ("bell",2,4,18), ("star",2,5,18),
 ("lilypad",4,0,20), ("cattails",4,1,32), ("splash_a",4,2,28), ("rock",4,3,22), ("lotus",4,4,24), ("log",4,5,26),
 ("boom",5,0,28), ("frog_heart",5,4,14),
]
from scipy import ndimage as ndi
cw, ch = W / COLS, H / ROWS
_a = np.array(sheet).astype(np.float32)
_m = np.sqrt((_a[...,0]-255)**2 + _a[...,1]**2 + (_a[...,2]-255)**2) > 110
_lab, _n = ndi.label(ndi.binary_dilation(_m, iterations=10))   # 1 zone connexe = 1 sprite (une pointe qui deborde ne gene plus)
_box = {}
for k, o in enumerate(ndi.find_objects(_lab)):
    _box[(int(((o[0].start+o[0].stop)/2)//ch), int(((o[1].start+o[1].stop)/2)//cw))] = (o, k+1)
def crop(r, c):
    o, k = _box[(r, c)]
    sub = sheet.crop((o[1].start, o[0].start, o[1].stop, o[0].stop))
    a = np.array(sub)
    keep = (_lab[o[0], o[1]] == k)   # ne garde que les pixels de cette zone
    a[~keep] = (255, 0, 255)
    return Image.fromarray(a)
def to_sprite(img, th):
    a = np.array(img).astype(np.float32)
    d = np.sqrt((a[...,0]-255)**2 + a[...,1]**2 + (a[...,2]-255)**2)
    mask = d > 110
    ys, xs = np.where(mask)
    y0, y1, x0, x1 = ys.min(), ys.max()+1, xs.min(), xs.max()+1
    a = a[y0:y1, x0:x1]; m = mask[y0:y1, x0:x1].astype(np.float32)
    h, w = m.shape
    tw = max(1, round(w * th / h))
    prem = np.dstack([a * m[...,None], m])
    # premultiplie -> BOX (moyenne) pour une reduction propre
    chans = [Image.fromarray(prem[...,i]).resize((tw, th), Image.BOX) for i in range(4)]
    r = np.dstack([np.array(c) for c in chans])
    al = r[...,3]
    rgb = np.where(al[...,None] > 1e-3, r[...,:3] / np.maximum(al[...,None], 1e-3), 0)
    return np.clip(rgb, 0, 255).astype(np.uint8), al > 0.5
def c565(rgb):
    r, g, b = [int(v) for v in rgb]
    v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    return 0xF81E if v == 0xF81F else v
out = ['// GENERE par tools/gen_sprites.py — ne pas editer.', '#include "sprites.h"', 'namespace spr {', '']
prev = Image.new('RGB', (760, 150), (60, 120, 80)); px = 4
for name, r, c, th in SPR:
    rgb, op = to_sprite(crop(r, c), th)
    h, w = op.shape
    vals = [(c565(rgb[j, i]) if op[j, i] else 0xF81F) for j in range(h) for i in range(w)]
    out.append(f'static const uint16_t d_{name}[{w*h}] = {{')
    for k in range(0, len(vals), 16):
        out.append('  ' + ','.join('0x%04X' % v for v in vals[k:k+16]) + ',')
    out.append('};')
    tile = Image.fromarray(rgb).convert('RGBA'); alpha = Image.fromarray((op*255).astype(np.uint8))
    if px + w < 756:
        prev.paste(tile.convert('RGB'), (px, 4 + (0 if px < 380 else 0)), alpha); px += w + 4
    out.append(f'const Sprite {name} = {{{w}, {h}, d_{name}}};')
    out.append('')
out.append('} // namespace spr')
open(os.path.join(root, 'main/ui/sprites.cpp'), 'w').write('\n'.join(out) + '\n')
hdr = ['// GENERE par tools/gen_sprites.py — ne pas editer.', '#pragma once', '#include <stdint.h>', 'namespace spr {',
 '// RGB565 standard (R poids fort), cle de transparence 0xF81F. Converti au format materiel par render::init().',
 'struct Sprite { uint16_t w, h; const uint16_t* px; };']
hdr += [f'extern const Sprite {n};' for n,_,_,_ in SPR]
hdr += [f'constexpr int COUNT = {len(SPR)};', 'extern const Sprite* const all[COUNT];', '} // namespace spr']
open(os.path.join(root, 'main/ui/sprites.h'), 'w').write('\n'.join(hdr) + '\n')
# table all[] ajoutee a la fin du .cpp
with open(os.path.join(root, 'main/ui/sprites.cpp'), 'a') as f:
    f.write('namespace spr { const Sprite* const all[COUNT] = {' + ','.join('&'+n for n,_,_,_ in SPR) + '}; }\n')
prev.resize((prev.width*2, prev.height*2), Image.NEAREST).save(os.path.join(root, 'assets/sprites_preview.png'))
print('ok', len(SPR), 'sprites')
