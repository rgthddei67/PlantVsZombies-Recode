"""Derive exact-canvas Aurora Torchwood sprites from classic parts and an ImageGen concept."""

from pathlib import Path
import colorsys
import hashlib
import re
import sys

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / "build/clang-release/resources"

EXPECTED_INPUT_HASHES = {
    "torchwood concept": "82add9505e2504f007149287609b3bd324920be90fb00042e35d5559fcfd3d6a",
    "projectile concept": "0c9c708fefa0547f7e16daa0794a614f09a355b022b44b686a0ae1926833e388",
    "Torchwood_body.png": "b050591d953bbcb8ea82c37feb382230016a00d072a56346e20b20d498a4b3c2",
    "Torchwood_mouth.png": "c0016dbe1f4fca2cd75e96b8aa854091ab2fd2ea1b98f695a281f862ad3f63e5",
    "TorchWood.png": "9afb440fa840559d4b6bf0701d1045d12c7fe8ab683d97f6112988295ec940e4",
    "Torchwood.reanim": "15190c350ad348d053817a500d12cae78ca30ab0cea8c4a3b6d85fcd78114985",
}

EXPECTED_OUTPUT_HASHES = {
    "image/PlantImage/AuroraTorchwood.png": "9d3af08d98d780ec4d59ff46ef6cba3a97627cd44fe41c9e775c99b08937fcc6",
    "image/ProjectileAuroraPea.png": "d2b44a142af2ede72712e4d6d15fa4494446541b1f4c38605f0d23130a7ff612",
    "image/ProjectileAuroraSnowPea.png": "e1549d6fc0833ab58e2cadf57a409c351ec0986049ec6c7b41a691ca4b04e0ce",
    "image/ProjectileAuroraToxicPea.png": "6a2e7a790ab13ab6eb83810a6ea4e6939df00374a14e805e8a6a6315b6eedfdb",
    "image/reanim/AuroraTorchwood_body.png": "24a11c7c4b339c1f822380e11b2f9ed45bd14770c9be2cc65e62def8109a7b3a",
    "image/reanim/AuroraTorchwood_mouth.png": "0065d970b0fc20bd56b791e366933f320a643dd7cefcf3625f23f155d4b95902",
    "image/reanim/AuroraTorchwood_prism1.png": "be036605f0a280df2cd78da72ab1f43fffac18eb43ad070def044196131ac8b9",
    "image/reanim/AuroraTorchwood_prism2.png": "f4f3582fd5fdab47b96dab361a578c54546911023d3008cce9cb43aee63a06d8",
    "image/reanim/AuroraTorchwood_prism3.png": "d9133ae769519dc0b46a33139cdb30e61dcbf6876b50acdaf89f488d18a79d78",
    "image/reanim/AuroraTorchwood_spark.png": "97cd379689c75d7e23ba74a647c8e88e68260200303a40c2cc82c5b4eabf8214",
    "reanim/AuroraTorchwood.reanim": "9b263e072ad8b8e1cf83345d0b590861eb01c40bb03ecba73e153b2a7e5929a5",
}


def assert_sha256(path: Path, expected: str, label: str) -> None:
    """Reject unreviewed source or generated-output drift."""
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise RuntimeError(f"{label} SHA-256 mismatch: expected {expected}, got {actual}")


def recolor_bark(image: Image.Image) -> Image.Image:
    """Map warm bark pixels to blue-violet while preserving original shading and alpha."""
    out = image.convert("RGBA")
    pixels = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            if (r > b * 1.15 and r > g * 0.75) or (r > 45 and g > 25 and b < 55):
                nh = 0.66 + 0.08 * (1.0 - v)
                nr, ng, nb = colorsys.hsv_to_rgb(nh % 1.0, min(0.85, s + 0.18), min(1.0, v * 1.12))
                pixels[x, y] = (int(nr * 255), int(ng * 255), int(nb * 255), a)

    veins = Image.new("RGBA", out.size)
    draw = ImageDraw.Draw(veins)
    width = max(1, out.width // 64)
    paths = [
        [(out.width * .18, out.height * .93), (out.width * .32, out.height * .62), (out.width * .29, out.height * .33)],
        [(out.width * .76, out.height * .94), (out.width * .66, out.height * .69), (out.width * .70, out.height * .42)],
        [(out.width * .43, out.height * .88), (out.width * .50, out.height * .70), (out.width * .48, out.height * .53)],
    ]
    for index, path in enumerate(paths):
        color = (55, 235, 255, 175) if index != 1 else (225, 70, 255, 155)
        draw.line([(int(x), int(y)) for x, y in path], fill=color, width=width)
    veins = veins.filter(ImageFilter.GaussianBlur(max(0.35, width * .35)))
    veins.putalpha(Image.composite(veins.getchannel("A"), Image.new("L", out.size), out.getchannel("A")))
    return Image.alpha_composite(out, veins)


def recolor_mouth(image: Image.Image) -> Image.Image:
    """Recolor every warm lip/bark pixel while preserving white teeth and the dark cavity."""
    out = image.convert("RGBA")
    pixels = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = pixels[x, y]
            is_tooth = r > 145 and g > 125 and b > 90
            if a == 0 or is_tooth or max(r, g, b) < 38:
                continue
            if r > 45 and g > 18 and b < 115:
                _, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
                nr, ng, nb = colorsys.hsv_to_rgb(.69, min(1.0, s + .22), min(1.0, v * 1.08))
                pixels[x, y] = (int(nr * 255), int(ng * 255), int(nb * 255), a)
    return out


def extract_crown(concept_path: Path) -> Image.Image:
    concept = Image.open(concept_path).convert("RGBA")
    # ImageGen sheet places the isolated crown on the right half.
    crop = concept.crop((int(concept.width * .53), int(concept.height * .25), concept.width, int(concept.height * .93)))
    data = crop.load()
    for y in range(crop.height):
        for x in range(crop.width):
            r, g, b, _ = data[x, y]
            brightness = max(r, g, b)
            alpha = max(0, min(255, (brightness - 10) * 4))
            data[x, y] = (r, g, b, alpha)
    bbox = crop.getbbox()
    if bbox:
        crop = crop.crop(bbox)
    return crop


def fit_crown(crown: Image.Image, size=(74, 52), squash=1.0, hue_overlay=None) -> Image.Image:
    canvas = Image.new("RGBA", size)
    target_h = max(1, int((size[1] - 2) * squash))
    scale = min((size[0] - 2) / crown.width, target_h / crown.height)
    resized = crown.resize((max(1, int(crown.width * scale)), max(1, int(crown.height * scale))), Image.Resampling.LANCZOS)
    if hue_overlay:
        tint = Image.new("RGBA", resized.size, hue_overlay)
        resized = Image.alpha_composite(resized, Image.composite(tint, Image.new("RGBA", resized.size), resized.getchannel("A")))
    x = (size[0] - resized.width) // 2
    y = size[1] - resized.height
    canvas.alpha_composite(resized, (x, y))
    return canvas


def extract_projectile(concept_path: Path) -> Image.Image:
    """Turn ImageGen's near-black presentation background into soft sprite alpha."""
    source = Image.open(concept_path).convert("RGBA")
    pixels = source.load()
    alpha_mask = Image.new("L", source.size)
    alpha_pixels = alpha_mask.load()
    for y in range(source.height):
        for x in range(source.width):
            r, g, b, _ = pixels[x, y]
            brightness = max(r, g, b)
            # Preserve the painted indigo outline and a restrained glow, but discard the backdrop.
            alpha_pixels[x, y] = max(0, min(255, (brightness - 7) * 4))

    bbox = alpha_mask.point(lambda value: 255 if value >= 16 else 0).getbbox()
    if bbox:
        source = source.crop(bbox)
        alpha_mask = alpha_mask.crop(bbox)
    source.putalpha(alpha_mask)
    return source


def fit_projectile(projectile: Image.Image, size=(48, 30)) -> Image.Image:
    """Downsample the painted master onto the exact runtime canvas."""
    canvas = Image.new("RGBA", size)
    scale = min((size[0] - 2) / projectile.width, (size[1] - 2) / projectile.height)
    resized = projectile.resize(
        (max(1, round(projectile.width * scale)), max(1, round(projectile.height * scale))),
        Image.Resampling.LANCZOS)
    canvas.alpha_composite(resized, ((size[0] - resized.width) // 2, (size[1] - resized.height) // 2))
    return canvas


def recolor_projectile(image: Image.Image, variant: str) -> Image.Image:
    """Derive snow and toxic lineage colors while preserving the painted facets."""
    out = image.copy().convert("RGBA")
    pixels = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            if variant == "snow":
                h = 0.53 + (h - 0.55) * 0.18
                s = min(0.88, s * 0.78)
                v = min(1.0, v * 1.10)
            elif variant == "toxic":
                h = 0.31 if h < 0.68 else 0.82
                s = min(1.0, s * 1.12 + 0.08)
                v = min(1.0, v * 1.03)
            nr, ng, nb = colorsys.hsv_to_rgb(h % 1.0, s, v)
            pixels[x, y] = (round(nr * 255), round(ng * 255), round(nb * 255), a)

    if variant == "snow":
        # A thin icy halo makes the lineage readable without covering the crystal painting.
        alpha = out.getchannel("A")
        expanded = alpha.filter(ImageFilter.MaxFilter(3))
        halo_alpha = expanded.point(lambda value: round(value * 0.38))
        halo = Image.new("RGBA", out.size, (105, 235, 255, 0))
        halo.putalpha(halo_alpha)
        out = Image.alpha_composite(halo, out)
    return out


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: generate_aurora_assets.py <torchwood-concept.png> <projectile-concept.png>")
    concept_path = Path(sys.argv[1])
    projectile_concept_path = Path(sys.argv[2])
    reanim_dir = RES / "image/reanim"
    card_dir = RES / "image/PlantImage"

    assert_sha256(concept_path, EXPECTED_INPUT_HASHES["torchwood concept"], "torchwood concept")
    assert_sha256(reanim_dir / "Torchwood_body.png",
                  EXPECTED_INPUT_HASHES["Torchwood_body.png"], "Torchwood_body.png")
    assert_sha256(reanim_dir / "Torchwood_mouth.png",
                  EXPECTED_INPUT_HASHES["Torchwood_mouth.png"], "Torchwood_mouth.png")
    assert_sha256(card_dir / "TorchWood.png", EXPECTED_INPUT_HASHES["TorchWood.png"], "TorchWood.png")
    assert_sha256(RES / "reanim/Torchwood.reanim",
                  EXPECTED_INPUT_HASHES["Torchwood.reanim"], "Torchwood.reanim")
    assert_sha256(projectile_concept_path,
                  EXPECTED_INPUT_HASHES["projectile concept"], "projectile concept")

    crown = extract_crown(concept_path)
    body = recolor_bark(Image.open(reanim_dir / "Torchwood_body.png"))
    body.save(reanim_dir / "AuroraTorchwood_body.png")
    recolor_mouth(Image.open(reanim_dir / "Torchwood_mouth.png")).save(
        reanim_dir / "AuroraTorchwood_mouth.png")
    fit_crown(crown, squash=.94).save(reanim_dir / "AuroraTorchwood_prism1.png")
    fit_crown(crown, squash=1.0).save(reanim_dir / "AuroraTorchwood_prism2.png")
    fit_crown(crown, squash=.90).save(reanim_dir / "AuroraTorchwood_prism3.png")
    fit_crown(crown, size=(28, 28), squash=.8).save(reanim_dir / "AuroraTorchwood_spark.png")

    card = recolor_bark(Image.open(card_dir / "TorchWood.png"))
    # Clear the original flame region, then fit the concept crown into the same card footprint.
    clear = Image.new("RGBA", card.size)
    clear.alpha_composite(card)
    pixels = clear.load()
    for y in range(0, 57):
        for x in range(20, 100):
            pixels[x, y] = (0, 0, 0, 0)
    card_crown = fit_crown(crown, size=(82, 58), squash=1.0)
    # 卡槽缩略图需要让棱晶底缘压进树桩开口，避免黑色描边造成悬空错觉。
    clear.alpha_composite(card_crown, (19, 6))
    clear.save(card_dir / "AuroraTorchwood.png")

    projectile = fit_projectile(extract_projectile(projectile_concept_path))
    projectile.save(RES / "image/ProjectileAuroraPea.png")
    recolor_projectile(projectile, "snow").save(RES / "image/ProjectileAuroraSnowPea.png")
    recolor_projectile(projectile, "toxic").save(RES / "image/ProjectileAuroraToxicPea.png")

    source_reanim = (RES / "reanim/Torchwood.reanim").read_text(encoding="utf-8")
    replacements = {
        "IMAGE_REANIM_TORCHWOOD_BODY": "IMAGE_REANIM_AURORATORCHWOOD_BODY",
        "IMAGE_REANIM_TORCHWOOD_MOUTH": "IMAGE_REANIM_AURORATORCHWOOD_MOUTH",
        "IMAGE_REANIM_TORCHWOOD_FIRE1A": "IMAGE_REANIM_AURORATORCHWOOD_PRISM1",
        "IMAGE_REANIM_TORCHWOOD_FIRE1B": "IMAGE_REANIM_AURORATORCHWOOD_PRISM2",
        "IMAGE_REANIM_TORCHWOOD_FIRE1C": "IMAGE_REANIM_AURORATORCHWOOD_PRISM3",
        "IMAGE_REANIM_TORCHWOOD_SPARK": "IMAGE_REANIM_AURORATORCHWOOD_SPARK",
    }
    for old, new in replacements.items():
        source_reanim = source_reanim.replace(old, new)
    track_start = source_reanim.index("<name>Torchwood_fire1</name>")
    track_end = source_reanim.index("</track>", track_start)
    prism_track = source_reanim[track_start:track_end]

    def scale_value(match: re.Match[str]) -> str:
        return f"<{match.group(1)}>{float(match.group(2)) * 1.25:.3f}</{match.group(1)}>"

    def shift_value(match: re.Match[str]) -> str:
        axis = match.group(1)
        shift = -6.0 if axis == "x" else -7.0
        return f"<{axis}>{float(match.group(2)) + shift:.3f}</{axis}>"

    prism_track = re.sub(r"<(sx|sy)>([-0-9.]+)</\1>", scale_value, prism_track)
    prism_track = re.sub(r"<(x|y)>([-0-9.]+)</\1>", shift_value, prism_track)
    source_reanim = source_reanim[:track_start] + prism_track + source_reanim[track_end:]
    (RES / "reanim/AuroraTorchwood.reanim").write_text(source_reanim, encoding="utf-8", newline="\n")

    for relative_path, expected in EXPECTED_OUTPUT_HASHES.items():
        assert_sha256(RES / relative_path, expected, relative_path)


if __name__ == "__main__":
    main()
