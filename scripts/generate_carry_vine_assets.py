"""Derive CarryVine's leaf basket, original jointed kelp rig and matching card."""
from pathlib import Path
import hashlib
import json
import math
import xml.etree.ElementTree as ET
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / 'build/clang-release/resources'
SOURCE = ROOT / 'scripts/assets/carry_vine_basket_source.png'


def generate():
    """Keep the classic per-part transforms; append a leaf basket on the face anchor."""
    basket = Image.open(SOURCE).convert('RGBA')
    # Ignore near-invisible generator speckles when finding content bounds; preserve alpha inside.
    basket = basket.crop(basket.getchannel('A').point(lambda a: 255 if a >= 16 else 0).getbbox())
    basket.thumbnail((64, 40), Image.Resampling.LANCZOS)
    basket.save(RES / 'image/reanim/CarryVine_basket.png')
    root = ET.fromstring('<root>' + (RES / 'reanim/Tanglekelp.reanim').read_text() + '</root>')
    # Waterline is presentation only. Keep every arm's original attachment transform.
    for track in list(root.findall('track')):
        if track.findtext('name') == 'anim_waterline':
            root.remove(track)
        elif track.findtext('name') in ('Layer 29', 'Layer 32'):
            # Classic grab artwork wraps a zombie to the right of the original root.
            # Rebase both halves together around the carrying basket, retaining swaps.
            for frame in track.findall('t'):
                if frame.find('x') is not None:
                    frame.find('x').text = str(float(frame.findtext('x')) - 65.0)
    face = next(t for t in root.findall('track') if t.findtext('name') == 'anim_face')
    attachment = ET.SubElement(root, 'track')
    ET.SubElement(attachment, 'name').text = 'CarryVine_basket'
    state = dict(x=0., y=0., sx=1., sy=1., kx=0., ky=0., a=1.)
    for frame in face.findall('t'):
        for key in state:
            if frame.find(key) is not None:
                state[key] = float(frame.findtext(key))
        out = ET.SubElement(attachment, 't')
        for key, value in state.items():
            ET.SubElement(out, key).text = str(value + (7 if key == 'x' else 23 if key == 'y' else 0))
        ET.SubElement(out, 'i').text = 'IMAGE_REANIM_CARRYVINE_BASKET'
    reanim = RES / 'reanim/CarryVine.reanim'
    reanim.write_text('\n'.join(ET.tostring(c, encoding='unicode') for c in root), encoding='utf-8')
    # Render the same first pose at 4x for a card, including true affine arm transforms.
    canvas = Image.new('RGBA', (640, 480))
    for track in root.findall('track'):
        frame = track.find('t')
        key = frame.findtext('i') if frame is not None else None
        if not key or frame.findtext('f') == '-1':
            continue
        stem = key.removeprefix('IMAGE_REANIM_').lower()
        path = next((p for p in (RES / 'image/reanim').iterdir()
                     if p.stem.lower() == stem and p.suffix.lower() in ('.png', '.jpg')), None)
        if path is None:
            raise RuntimeError('Missing rig part ' + key)
        sprite = Image.open(path).convert('RGBA')
        if path.suffix.lower() == '.jpg':
            mask = path.with_name(path.stem + '_.png')
            if mask.exists(): sprite.putalpha(Image.open(mask).convert('L'))
        x, y, sx, sy, kx, ky = [float(frame.findtext(k, str(v))) for k,v in
                              [('x',0),('y',0),('sx',1),('sy',1),('kx',0),('ky',0)]]
        a,b = sx*math.cos(math.radians(kx))*4, -sy*math.sin(math.radians(ky))*4
        c,d = sx*math.sin(math.radians(kx))*4, sy*math.cos(math.radians(ky))*4
        det = a*d-b*c
        tx,ty = x*4+120,y*4+60
        inv = (d/det,-b/det,(b*ty-d*tx)/det,-c/det,a/det,(c*tx-a*ty)/det)
        layer = sprite.transform(canvas.size, Image.Transform.AFFINE, inv, Image.Resampling.BICUBIC)
        canvas.alpha_composite(layer)
    canvas = canvas.crop(canvas.getbbox())
    canvas.thumbnail((90, 90), Image.Resampling.LANCZOS)
    canvas.save(RES / 'image/PlantImage/CarryVine.png')
    paths = [SOURCE, RES / 'reanim/Tanglekelp.reanim', reanim,
             RES / 'image/reanim/CarryVine_basket.png', RES / 'image/PlantImage/CarryVine.png']
    hashes = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    lock = Path(__file__).with_suffix('.sha256.json')
    if lock.exists() and json.loads(lock.read_text()) != hashes:
        raise RuntimeError('CarryVine asset hash drift; review outputs before updating lock')
    print(json.dumps(hashes, indent=2))


if __name__ == '__main__':
    generate()
