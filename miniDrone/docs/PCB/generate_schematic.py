"""Generate the MiniDrone carrier schematic and local Super Mini library.

Uses the installed KiCad 10 standard libraries. Run from any directory with Python.
UUIDs are deterministic, so regeneration produces reviewable diffs.
"""
from pathlib import Path
import csv
import json
import math
import re
import uuid

from inspect_symbols import BASE, extract

ROOT = Path(__file__).resolve().parent / 'Esp32SuperMini'
ROOT.mkdir(exist_ok=True)
G = 1.27
NAMESPACE = uuid.UUID('18ae3b42-e35c-4097-aed5-a0ba1f93fe01')
counter = 0

def uid(key=None):
    global counter
    counter += 1
    return str(uuid.uuid5(NAMESPACE, key or str(counter)))

def q(s):
    return json.dumps(str(s), ensure_ascii=False)

def n(v):
    return f'{v:.5f}'.rstrip('0').rstrip('.') if v else '0'

def effect(size=1.0, justify=''):
    return f'(effects (font (size {n(size)} {n(size)})) {justify})'

def prop(key, value, x=0, y=0, hide=False, justify='', angle=0):
    return f'(property {q(key)} {q(value)} (at {n(x)} {n(y)} {angle}) (effects (font (size 1 1)) {justify} {"(hide yes)" if hide else ""}))'

def module(name, pins, width=12, height=10):
    s = [f'(symbol {q(name)} (pin_names (offset 1.016)) (in_bom yes) (on_board yes)',
         prop('Reference', 'U', 0, (height+4)*G), prop('Value', name, 0, (height+2)*G),
         prop('Footprint', '', hide=True), prop('Datasheet', '', hide=True),
         f'(symbol {q(name+"_0_1")} (rectangle (start {n(-width*G)} {n(height*G)}) (end {n(width*G)} {n(-height*G)}) (stroke (width 0.254) (type default)) (fill (type background))))',
         f'(symbol {q(name+"_1_1")}']
    for number, label, x, y, angle, kind in pins:
        s.append(f'(pin {kind} line (at {n(x*G)} {n(y*G)} {angle}) (length {n(4*G)}) (name {q(label)} {effect()}) (number {q(number)} {effect()}))')
    s.append('))')
    return '\n'.join(s)

super_pins = [
    ('16','5V',0,20,270,'power_in'),
    ('14','3V3',-16,14,0,'power_out'),
    ('15','GND',-16,10,0,'passive'),
    ('13','GPIO4 / SDA',-16,6,0,'bidirectional'),
    ('1','GPIO5 / SCL',-16,2,0,'bidirectional'),
    ('9','GPIO0 / ADC',-16,-2,0,'bidirectional'),
    ('12','GPIO3',-16,-6,0,'bidirectional'),
    ('11','GPIO2',-16,-10,0,'bidirectional'),
    ('10','GPIO1',-16,-14,0,'bidirectional'),
    ('2','GPIO6 / M1',16,12,180,'bidirectional'),
    ('3','GPIO7 / M2',16,8,180,'bidirectional'),
    ('6','GPIO10 / M3',16,4,180,'bidirectional'),
    ('7','GPIO20 / M4',16,0,180,'bidirectional'),
    ('4','GPIO8',16,-4,180,'bidirectional'),
    ('5','GPIO9',16,-8,180,'bidirectional'),
    ('8','GPIO21',16,-12,180,'bidirectional'),
]
imu_pins = [(str(i+1), label, 14, 7-2*i, 180, kind) for i, (label, kind) in enumerate([
    ('VCC','power_in'),('GND','passive'),('SCL','input'),('SDA','bidirectional'),
    ('XDA','bidirectional'),('XCL','output'),('AD0','input'),('INT','output')])]
custom = {
    'ESP32_C3_SUPER_MINI': module('ESP32_C3_SUPER_MINI', super_pins, height=16),
    'GY521_MODULE': module('GY521_MODULE', imu_pins, width=10),
}
library_text = '(kicad_symbol_lib (version 20251024) (generator "kicad_symbol_editor")\n' + '\n'.join(custom.values()) + '\n)\n'
(ROOT / 'MiniDrone.kicad_sym').write_text(library_text, encoding='utf-8')
(ROOT / 'sym-lib-table').write_text('(sym_lib_table (version 7) (lib (name "MiniDrone")(type "KiCad")(uri "${KIPRJMOD}/MiniDrone.kicad_sym")(options "")(descr "MiniDrone plug-in modules")))\n', encoding='utf-8')
(ROOT / 'fp-lib-table').write_text('(fp_lib_table (version 7) (lib (name "MiniDrone")(type "KiCad")(uri "${KIPRJMOD}/MiniDrone.pretty")(options "")(descr "Carrier footprints - verify module dimensions")))\n', encoding='utf-8')

defs = {}
def get_def(lib_id):
    if lib_id in defs:
        return defs[lib_id]
    lib, name = lib_id.split(':')
    if lib == 'MiniDrone':
        block = custom[name]
    else:
        text = (BASE / (lib+'.kicad_sym')).read_text(encoding='utf-8')
        block = extract(text, name)
        parent = re.search(r'\(extends "([^"]+)"\)', block)
        if parent:
            parent_name = parent.group(1)
            block = extract(text, parent_name).replace('"'+parent_name+'"', '"'+name+'"').replace('"'+parent_name+'_', '"'+name+'_')
    defs[lib_id] = block.replace('(symbol '+q(name), '(symbol '+q(lib_id), 1)
    return defs[lib_id]

parts, items, bom = {}, [], []
sheet_uuid = '66b3a1ab-4d4d-49c1-984f-561caa976869'

def add(ref, lib_id, value, x, y, angle=0, footprint='', note='', fields=None):
    block = get_def(lib_id)
    x, y = x*G, y*G
    pins = {}
    for match in re.finditer(r'\(pin (\w+) \w+\s*\(at ([\d.\-]+) ([\d.\-]+) (\d+)\)[\s\S]*?\(number "([^"]+)"', block):
        _, px, py, _, number = match.groups()
        a = math.radians(angle)
        dx, dy = float(px), float(py)
        pins[number] = (round(x + dx*math.cos(a)-dy*math.sin(a), 5), round(y-dx*math.sin(a)-dy*math.cos(a), 5))
    parts[ref] = pins
    symbol_uuid = uid(ref)
    # Keep fields horizontal; rotate only the electrical symbol.
    if lib_id.startswith('MiniDrone:'):
        rx, ry, vx, vy = x, y-18*G, x, y-16*G
    elif ref.startswith(('R','C','D')) and angle in (90,270):
        rx, ry, vx, vy = x, y-6*G, x, y-4*G
    elif ref.startswith('Q'):
        rx, ry, vx, vy = x+6*G, y-3*G, x+6*G, y-G
    else:
        rx, ry, vx, vy = x+6*G, y-2*G, x+6*G, y
    if fields:
        rx, ry, vx, vy = [t*G for t in fields]
    field_angle = 90 if angle in (90,270) else 0
    props = [prop('Reference',ref,rx,ry,hide=ref.startswith('#'),angle=field_angle),prop('Value',value,vx,vy,hide=ref.startswith('#'),angle=field_angle),prop('Footprint',footprint,x,y,True),prop('Datasheet','',x,y,True)]
    if note:
        props.append(prop('DesignNote',note,x,y,True))
    items.append(f'(symbol (lib_id {q(lib_id)}) (at {n(x)} {n(y)} {angle}) (unit 1) (in_bom yes) (on_board yes) (dnp {"yes" if "DNP" in value else "no"}) (uuid {symbol_uuid})\n'+ '\n'.join(props) + '\n' + '\n'.join(f'(pin {q(p)} (uuid {uid(ref+":"+p)}))' for p in pins) + f'\n(instances (project "Esp32SuperMini" (path "/{sheet_uuid}" (reference {q(ref)}) (unit 1)))))')
    bom.append({'Reference':ref,'Value':value,'Footprint':footprint,'Design note':note})
    return pins

def wire(a,b):
    if a == b: return
    items.append(f'(wire (pts (xy {n(a[0])} {n(a[1])}) (xy {n(b[0])} {n(b[1])})) (stroke (width 0) (type default)) (uuid {uid()}))')

def label(net, point, angle=0):
    items.append(f'(label {q(net)} (at {n(point[0])} {n(point[1])} {angle}) {effect(1.0, "(justify left bottom)")} (uuid {uid()}))')

def connect(ref, pin, net, dx=-6, dy=0):
    a = parts[ref][str(pin)]
    b = (a[0]+dx*G,a[1]+dy*G)
    wire(a,b)
    label(net,b)

def nc(ref,pin):
    x,y=parts[ref][str(pin)]
    items.append(f'(no_connect (at {n(x)} {n(y)}) (uuid {uid()}))')

def text(value,x,y,size=1.15):
    items.append(f'(text {q(value)} (at {n(x*G)} {n(y*G)} 0) {effect(size,"(justify left)")} (uuid {uid()}))')

def junction(p):
    items.append(f'(junction (at {n(p[0])} {n(p[1])}) (diameter 0) (color 0 0 0 0) (uuid {uid()}))')

from wired_layout import draw
draw(add, parts, wire, label, nc, text, junction, G)

schema = f'(kicad_sch (version 20260306) (generator "eeschema") (generator_version "10.0") (uuid {sheet_uuid}) (paper "A2")\n'
schema += '(title_block (title "MiniDrone - through-hole plug-in Super Mini / GY-521 carrier") (date "2026-10-31") (rev "D") (company "AlSayadProjects") (comment 1 "70 x 70 mm hand-soldered THT prototype / 1S 400mAh / BC337 - fit and current validation pending"))\n'
schema += '(lib_symbols\n'+'\n'.join(defs.values())+'\n)\n'+'\n'.join(items)
schema += '\n(sheet_instances (path "/" (page "1"))) (embedded_fonts no))\n'
sch = ROOT / 'Esp32SuperMini.kicad_sch'
backup=ROOT / 'references' / 'original-empty.kicad_sch'
if not backup.exists():
    backup.parent.mkdir(exist_ok=True)
    backup.write_bytes(sch.read_bytes())
sch.write_text(schema, encoding='utf-8')
with (ROOT / 'BOM.csv').open('w',newline='',encoding='utf-8-sig') as f:
    w=csv.DictWriter(f,fieldnames=['Reference','Value','Footprint','Design note'])
    w.writeheader(); w.writerows(row for row in bom if not row['Reference'].startswith('#'))

# Board footprint uses a clearly documented, provisional common module geometry.
pretty=ROOT/'MiniDrone.pretty'; pretty.mkdir(exist_ok=True)
fp=['(footprint "ESP32_C3_SuperMini_THT" (version 20260206) (generator "pcbnew") (layer "F.Cu")',
    '(descr "Plug-in Super Mini; 2.54mm pin pitch, 15.24mm row spacing. VERIFY ACTUAL BOARD.")',
    '(attr through_hole)',
    '(fp_text reference "REF**" (at 0 -13) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))',
    '(fp_text value "ESP32_C3_SuperMini_THT" (at 0 17) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))',
    '(fp_rect (start -9 -11.25) (end 9 11.25) (stroke (width 0.15) (type default)) (fill none) (layer "F.SilkS"))',
    '(fp_rect (start -9.5 -11.75) (end 9.5 11.75) (stroke (width 0.05) (type default)) (fill none) (layer "F.CrtYd"))',
    '(fp_text user "USB" (at 0 -8.5) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))',
    '(fp_rect (start -9 7) (end 9 15) (stroke (width 0.15) (type dash)) (fill none) (layer "Dwgs.User"))',
    '(fp_text user "ANTENNA - KEEP ALL COPPER CLEAR" (at 0 13) (layer "Dwgs.User") (effects (font (size 0.8 0.8) (thickness 0.12))))']
for number in range(1,17):
    x=-7.62 if number<=8 else 7.62
    y=-8.89+(number-1)*2.54 if number<=8 else 8.89-(number-9)*2.54
    shape='rect' if number==1 else 'oval'
    fp.append(f'(pad "{number}" thru_hole {shape} (at {n(x)} {n(y)}) (size 1.8 1.8) (drill 1) (layers "*.Cu" "*.Mask"))')
for x in [-7.62,7.62]:
    fp.append('(model "${KICAD10_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x08_P2.54mm_Vertical.step" '
              f'(offset (xyz {n(x)} 8.89 0)) (scale (xyz 1 1 1)) (rotate (xyz 0 0 0)))')
fp.append(')')
(pretty/'ESP32_C3_SuperMini_THT.kicad_mod').write_text('\n'.join(fp)+'\n',encoding='utf-8')
print(f'Generated {sch} with {len(bom)} symbols.')
