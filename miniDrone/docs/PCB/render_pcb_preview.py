"""Rasterize a KiCad PCB SVG plot for visual review."""
from pathlib import Path
import os
import sys
import xml.etree.ElementTree as ET
sys.path.insert(0,str(Path(os.environ['TEMP'])/'miniDrone-pcb-tools'))
import resvg_py

path=Path(sys.argv[1])
root=ET.fromstring(path.read_text(encoding='utf-8'))
root.set('width','1800'); root.set('height','1800')
path.with_suffix('.png').write_bytes(resvg_py.svg_to_bytes(
    svg_string=ET.tostring(root,encoding='unicode'),width=1800,height=1800,background='white'))
print(path.with_suffix('.png'))
