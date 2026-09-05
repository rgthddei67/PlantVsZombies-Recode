"""Derive bounded equipment and a single head-with-hat particle from reviewed source art."""
from pathlib import Path
import hashlib
import json
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / 'build/clang-release/resources'
SOURCE = ROOT / 'scripts/assets/excavator_equipment_source.png'


def generate():
    """Crop three separated atlas cells; runtime dimensions are independent of mother image size."""
    source = Image.open(SOURCE).convert('RGBA')
    paths = [SOURCE, RES / 'reanim/NormalZombie.reanim', RES / 'particles/ZombieHead.png']
    for name, box, size in [('hat',(0,0,790,490),(63,44)),
                            ('drill',(0,495,797,1024),(83,48)),
                            ('broken',(797,495,1536,1024),(83,55))]:
        part = source.crop(box)
        part = part.crop(part.getchannel('A').point(lambda a:255 if a>=160 else 0).getbbox())
        part.thumbnail(size,Image.Resampling.LANCZOS)
        path = RES / f'image/reanim/Excavator_{name}.png'
        part.save(path)
        paths.append(path)
    # Whole head plus its fixed hat is one particle so spin and launch never separate them.
    head = Image.open(RES / 'particles/ZombieHead.png').convert('RGBA')
    hat = Image.open(RES / 'image/reanim/Excavator_hat.png').convert('RGBA')
    composite = Image.new('RGBA',(90,100))
    composite.alpha_composite(head,(15,28))
    composite.alpha_composite(hat,(10,7))
    path = RES / 'particles/ExcavatorHead.png'
    composite.save(path)
    paths.append(path)
    hashes = {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    lock = Path(__file__).with_suffix('.sha256.json')
    if lock.exists() and json.loads(lock.read_text()) != hashes:
        raise RuntimeError('Excavator asset hash drift: inspect visuals before updating the lock')
    print(json.dumps(hashes,indent=2))


if __name__ == '__main__':
    generate()
