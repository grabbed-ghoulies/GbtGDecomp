"""Makes the xbox_one_buttons example plugin's textures: the game's Black button labelled LB and RB.

    python make_buttons.py --game-dir <folder holding bundles/common.bnl>

The host maps the Xbox controller's White button to LB and Black to RB (src/engine/runtime/platform.cpp), so the
game's White and Black button pictures (aid_texture_joypad_buttons_white and _black in common.bnl, 64x64 DXT3)
become the Black button with "LB" and "RB" on its top. This reads the Black button from the disc, draws the label
on its dome, and writes each as a loose texture resource named by its AID (docs/FORMATS.md) into files/aid/, where
the plugin's aid/ folder puts it in place of the bundle entry. Needs Pillow and numpy.
"""
import argparse
import struct
import zlib
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).resolve().parent
SIZE = 64
SUPER = 4  # The label is drawn at 4x and averaged down.

LABELS = {
    'aid_texture_joypad_buttons_white': 'LB',
    'aid_texture_joypad_buttons_black': 'RB',
}
# Where the label sits on the Black button's dome, in texels: its centre, width, and how much the view squashes
# the dome's top (it is seen from the front and above, so the label is shorter than it is wide).
CENTRE = (31.0, 25.5)
WIDTH = 22.0
SQUASH = 0.62
TURN = 8.0  # Degrees anticlockwise, as the game's letters on its A, B, X and Y are turned.
INK = (226, 226, 230)

FONTS = ['segoeuib.ttf', 'arialbd.ttf', 'DejaVuSans-Bold.ttf', 'Arial Bold.ttf']


def font(size):
    for name in FONTS:
        for folder in ['', 'C:/Windows/Fonts/', '/usr/share/fonts/truetype/dejavu/', '/Library/Fonts/']:
            try:
                return ImageFont.truetype(folder + name, size)
            except OSError:
                continue
    return ImageFont.load_default(size)


def bundle_entry(path, wanted):
    """The CPU and GPU data of entry `wanted` of a BNL bundle (docs/FORMATS.md)."""
    data = path.read_bytes()
    count = struct.unpack_from('<H', data, 0)[0]
    chunks = [struct.unpack_from('<II', data, 8 + i * 8) for i in range(4)]
    blob = zlib.decompress(data[40:])
    base = [offset - 40 for offset, _ in chunks]
    for i in range(count):
        record = blob[base[0] + i * 160: base[0] + (i + 1) * 160]
        if record[:128].split(b'\0')[0].decode() != wanted:
            continue
        file_offset, file_size, resource_offset, _ = struct.unpack_from('<IIII', record, 128 + 16)
        cpu = blob[base[3] + file_offset: base[3] + file_offset + file_size]
        # The GPU table: u32 size, u32 count, then {offset, size} pairs into chunk 2.
        gpu_offset, gpu_size = struct.unpack_from('<II', blob, base[1] + resource_offset + 8)
        return cpu, blob[base[2] + gpu_offset: base[2] + gpu_offset + gpu_size]
    raise SystemExit(f'{wanted} is not in {path}')


# DXT3: 4-bit alpha, then a 4-colour 565 block.
def to565(c):
    r, g, b = (np.clip(np.round(c * [31, 63, 31]), 0, [31, 63, 31])).astype(int)
    return (r << 11) | (g << 5) | b


def from565(v):
    return np.array([(v >> 11) & 31, (v >> 5) & 63, v & 31]) / [31, 63, 31]


def decode_dxt3(data):
    out = np.zeros((SIZE, SIZE, 4))
    at = 0
    for by in range(0, SIZE, 4):
        for bx in range(0, SIZE, 4):
            alpha = int.from_bytes(data[at:at + 8], 'little')
            c0, c1, bits = struct.unpack_from('<HHI', data, at + 8)
            at += 16
            p0, p1 = from565(c0), from565(c1)
            palette = [p0, p1, (2 * p0 + p1) / 3, (p0 + 2 * p1) / 3]
            for i in range(16):
                y, x = divmod(i, 4)
                out[by + y, bx + x, :3] = palette[(bits >> (2 * i)) & 3]
                out[by + y, bx + x, 3] = ((alpha >> (4 * i)) & 15) / 15
    return Image.fromarray(np.round(out * 255).astype(np.uint8), 'RGBA')


def encode_dxt3(image):
    pixels = np.asarray(image, dtype=np.float64) / 255
    out = bytearray()
    for by in range(0, SIZE, 4):
        for bx in range(0, SIZE, 4):
            block = pixels[by:by + 4, bx:bx + 4].reshape(16, 4)
            alpha = np.round(block[:, 3] * 15).astype(int)
            out += int(sum(int(a) << (4 * i) for i, a in enumerate(alpha))).to_bytes(8, 'little')
            colour = block[:, :3]
            weight = block[:, 3] + 0.05
            mean = (colour * weight[:, None]).sum(0) / weight.sum()
            centred = (colour - mean) * weight[:, None]
            axis = np.linalg.svd(centred, full_matrices=False)[2][0] if np.any(centred) else np.ones(3)
            proj = (colour - mean) @ axis
            ends = [to565(mean + axis * proj.min()), to565(mean + axis * proj.max())]
            best = None
            for c0, c1 in [(ends[1], ends[0]), (ends[0], ends[1])]:
                p0, p1 = from565(c0), from565(c1)
                palette = np.array([p0, p1, (2 * p0 + p1) / 3, (p0 + 2 * p1) / 3])
                error = ((colour[:, None, :] - palette[None]) ** 2).sum(-1) * weight[:, None]
                if best is None or error.min(1).sum() < best[0]:
                    best = (error.min(1).sum(), c0, c1, error.argmin(1))
            _, c0, c1, index = best
            bits = int(sum(int(k) << (2 * i) for i, k in enumerate(index)))
            out += struct.pack('<HHI', c0, c1, bits)
    return bytes(out)


def labelled(button, text):
    """The button with `text` on its dome: pale, with a dark edge below it so it reads as raised."""
    big = SIZE * SUPER
    face = font(200)
    probe = ImageDraw.Draw(Image.new('L', (1, 1)))
    box = probe.textbbox((0, 0), text, font=face)
    mask = Image.new('L', (box[2] - box[0], box[3] - box[1]), 0)
    ImageDraw.Draw(mask).text((-box[0], -box[1]), text, 255, font=face)
    w = round(WIDTH * SUPER)
    h = round(mask.height * w / mask.width * SQUASH)
    mask = mask.resize((w, h), Image.LANCZOS).rotate(TURN, Image.BICUBIC, expand=True)
    layer = Image.new('L', (big, big), 0)
    layer.paste(mask, (round(CENTRE[0] * SUPER - mask.width / 2), round(CENTRE[1] * SUPER - mask.height / 2)))
    shadow = Image.new('L', (big, big), 0)
    shadow.paste(layer, (SUPER // 2, SUPER))
    shadow = shadow.filter(ImageFilter.GaussianBlur(SUPER * 0.6))
    out = button.resize((big, big), Image.LANCZOS)
    out = Image.composite(Image.new('RGBA', (big, big), (0, 0, 0, 255)), out,
                          shadow.point(lambda v: v * 0.6))
    out = Image.composite(Image.new('RGBA', (big, big), INK + (255,)), out, layer)
    # Keep the button's own outline: the label never reaches past it.
    result = out.resize((SIZE, SIZE), Image.LANCZOS)
    rgba = np.array(result)
    rgba[..., 3] = np.asarray(button)[..., 3]
    return Image.fromarray(rgba, 'RGBA')


def loose_texture(image, cpu):
    """A loose texture resource (docs/FORMATS.md): the disc entry's texture header, with the new pixels."""
    gpu = encode_dxt3(image)
    table = struct.pack('<IIII', 0x10, 1, 0x10, len(gpu))
    resource = len(table) + len(gpu)
    coverage = float(np.asarray(image)[..., 3].mean() / 255)
    header = struct.pack('<IfIIIIII', 1, coverage, 0x48E58836, 2, 0x20 + resource, len(cpu), 0x20, resource)
    return header + table + gpu + cpu


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game-dir', type=Path, required=True, help='the game folder, holding bundles/common.bnl')
    args = parser.parse_args()
    cpu, gpu = bundle_entry(args.game_dir / 'bundles' / 'common.bnl', 'aid_texture_joypad_buttons_black')
    fmt, _, width, height = struct.unpack_from('<IIHH', cpu, 0)
    if (fmt, width, height) != (0x0E, SIZE, SIZE):
        raise SystemExit(f'unexpected Black button texture: format {fmt:#x}, {width}x{height}')
    button = decode_dxt3(gpu)
    (HERE / 'art').mkdir(exist_ok=True)
    (HERE / 'files' / 'aid').mkdir(parents=True, exist_ok=True)
    for name, text in LABELS.items():
        image = labelled(button, text)
        image.save(HERE / 'art' / f'{name}.png')
        (HERE / 'files' / 'aid' / name).write_bytes(loose_texture(image, cpu))
        print('wrote', name, text)


if __name__ == '__main__':
    main()
