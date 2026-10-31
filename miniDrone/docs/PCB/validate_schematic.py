"""Check exported KiCad connectivity against the firmware and motor topology."""
from pathlib import Path
from collections import Counter
import json
import re
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parent/'Esp32SuperMini'
tree=ET.parse(ROOT/'MiniDrone.net.xml').getroot()
nets={net.attrib['name']: {(p.attrib['ref'],p.attrib['pin']) for p in net.findall('node')}
      for net in tree.findall('./nets/net')}
pin_net={pin:name for name,pins in nets.items() for pin in pins}

def expect(net,*pins):
    assert set(pins)<=nets[net], (net,set(pins)-nets[net])

board=(ROOT.parents[2]/'main'/'board.h').read_text(encoding='utf-8')
firmware_pins=[int(p) for p in re.search(r'AS_MOTOR_PINS\s+\{([^}]+)\}',board).group(1).split(',')]
gpio_pad={5:'1',6:'2',7:'3',8:'4',9:'5',10:'6',20:'7',21:'8',0:'9',1:'10',2:'11',3:'12',4:'13'}
for i,gpio in enumerate(firmware_pins,1):
    expect(f'/M{i}_PWM',('U1',gpio_pad[gpio]),(f'R{i}','1'))
    expect(f'/M{i}_NEG',(f'Q{i}','1'),(f'D{i}','2'),(f'J{i+2}','2'),(f'C{i+6}','2'))
    expect('/VBAT_SW',(f'D{i}','1'),(f'J{i+2}','1'),(f'C{i+6}','1'))
    expect('/GND',(f'Q{i}','3'),(f'R{i+4}','2'))
    bnet=pin_net[(f'Q{i}','2')]
    expect(bnet,(f'R{i}','2'),(f'R{i+4}','1'))
    assert bnet not in ['/GND','/VBAT_SW'], bnet

for define,net,imu_pin,pullup in [('AS_SDA','/IMU_SDA','4','R9'),('AS_SCL','/IMU_SCL','3','R10')]:
    gpio=int(re.search(rf'{define}\s+(\d+)',board).group(1))
    expect(net,('U1',gpio_pad[gpio]),('U2',imu_pin),(pullup,'2'))
expect('/3V3',('U1','14'),('U2','1'),('R9','1'),('R10','1'))
expect('/GND',('U1','15'),('U2','2'),('U2','7'),('J1','2'),('J2','2'),('J2','4'))
expect('/SUPPLY_5V',('U1','16'),('JP1','2'))
expect('/BOOST_5V',('J2','3'),('JP1','1'))
expect('/VBAT_RAW',('J1','1'),('SW1','1'))
expect('/VBAT_SW',('SW1','2'),('J2','1'),('R11','1'))
expect('/BAT_ADC',('U1','9'),('R11','2'),('R12','1'),('C6','1'))
expect('/GND',('R12','2'),('C6','2'))
assert len({pin_net[('U1',p)] for p in ['9','14','16']})==3

# A local label appearing only once cannot silently connect two detached wires.
# Combined with exact exported net equivalence, this checks the requested visible
# wiring without relying on the generator's route bookkeeping.
schematic=(ROOT/'Esp32SuperMini.kicad_sch').read_text(encoding='utf-8')
labels=Counter(re.findall(r'\(label "([^"]+)"',schematic))
assert not {name:count for name,count in labels.items() if count>1}, 'Repeated net labels'
assert '(global_label ' not in schematic and '(hierarchical_label ' not in schematic
baseline=json.loads((ROOT/'references'/'revision-A-connectivity.json').read_text(encoding='utf-8'))
original={frozenset(tuple(pin) for pin in pins) for pins in baseline.values()}
actual={frozenset(pins) for pins in nets.values()}
assert actual==original, {'missing':original-actual,'extra':actual-original}

erc=json.loads((ROOT/'erc-report.json').read_text(encoding='utf-8'))
assert not [v for s in erc['sheets'] for v in s['violations']], 'ERC violations'
footprint_root=Path(r'C:\Program Files\KiCad\10.0\share\kicad\footprints')
for comp in tree.findall('./components/comp'):
    fp=comp.findtext('footprint')
    if not fp:
        continue
    lib,name=fp.split(':')
    folder=ROOT/'MiniDrone.pretty' if lib=='MiniDrone' else footprint_root/(lib+'.pretty')
    assert (folder/(name+'.kicad_mod')).is_file(), (comp.attrib['ref'],fp)
print('PASS: all four motor stages, firmware pins, IMU, power isolation and ADC connectivity.')
print('PASS: ERC has zero violations; every assigned footprint exists.')
print('PASS: exact original net connectivity preserved, with no repeated or off-sheet labels.')
