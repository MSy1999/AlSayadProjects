"""Rasterize the KiCad SVG export (requires resvg-py)."""
from pathlib import Path
import os
import sys
import xml.etree.ElementTree as ET

# The optional renderer is installed into a task-local temporary directory.
sys.path.insert(0, str(Path(os.environ['TEMP']) / 'miniDrone-pcb-tools'))
import resvg_py

path=Path(__file__).resolve().parent/'Esp32SuperMini'/'preview'/'Esp32SuperMini.svg'
root=ET.fromstring(path.read_text(encoding='utf-8'))
root.set('width','3400')
root.set('height','2404')
path.with_suffix('.png').write_bytes(resvg_py.svg_to_bytes(svg_string=ET.tostring(root,encoding='unicode'),width=3400,height=2404,background='white'))
print(path.with_suffix('.png'))
