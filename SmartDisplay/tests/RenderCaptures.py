"""Convert the headless LVGL RGB565 framebuffer captures to reviewable PNGs."""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
args = parser.parse_args()
for path in args.directory.glob('*.rgb565'):
    data = path.read_bytes()
    if len(data) != 320 * 240 * 2:
        raise ValueError(f'Unexpected framebuffer size: {path}')
    rgb = bytearray()
    for index in range(0, len(data), 2):
        value = int.from_bytes(data[index:index + 2], 'little')
        rgb.extend((((value >> 11) & 31) * 255 // 31,
                    ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31))
    Image.frombytes('RGB', (320, 240), bytes(rgb)).save(path.with_suffix('.png'))
