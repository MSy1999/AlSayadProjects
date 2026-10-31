"""Validate the native PCB's components, nets, geometry, routing and DRC report."""
from pathlib import Path
from collections import defaultdict
import json
import xml.etree.ElementTree as ET
import pcbnew as pcb

root=Path(__file__).resolve().parent/'Esp32SuperMini'
board=pcb.LoadBoard(str(root/'Esp32SuperMini.kicad_pcb'))
fps={fp.GetReference():fp for fp in board.GetFootprints()}
netlist=ET.parse(root/'MiniDrone.net.xml').getroot()
expected_refs={c.attrib['ref'] for c in netlist.findall('./components/comp')}
expected_refs={r for r in expected_refs if not r.startswith('#')}
assert set(fps)==expected_refs|{'H1','H2','H3','H4'}
actual=defaultdict(set)
plated_pads=0
for ref,fp in fps.items():
    for pad in fp.Pads():
        if ref in expected_refs:
            assert pad.GetAttribute()==pcb.PAD_ATTRIB_PTH,(ref,pad.GetNumber(),'Not through-hole')
            assert pcb.ToMM(pad.GetDrillSize().x)>=0.75,(ref,'Drill too small for leaded parts')
            plated_pads+=1
        name=str(pad.GetNetname())
        if name:
            actual[name].add((ref,pad.GetNumber()))
    pos=fp.GetPosition()
    assert 75<pcb.ToMM(pos.x)<145 and 70<pcb.ToMM(pos.y)<140, ref
for comp in netlist.findall('./components/comp'):
    ref=comp.attrib['ref']
    if ref not in expected_refs: continue
    assert fps[ref].GetFPIDAsString()==comp.findtext('footprint'),ref
    assert fps[ref].GetValue()==comp.findtext('value'),ref
for ref in ['U1','U2']:
    pads={p.GetNumber():p for p in fps[ref].Pads()}
    assert len(pads)==(16 if ref=='U1' else 8)
    a,b=pads['1'].GetPosition(),pads['2'].GetPosition()
    assert abs(pcb.ToMM((a-b).EuclideanNorm())-2.54)<0.001
    assert all(abs(pcb.ToMM(p.GetDrillSize().x)-1.0)<0.001 for p in pads.values())
    assert all(p.GetLocalZoneConnection()!=pcb.ZONE_CONNECTION_FULL
               for p in pads.values() if str(p.GetNetname())=='/GND')
    if ref=='U1':
        assert abs(pcb.ToMM((pads['1'].GetPosition()-pads['16'].GetPosition()).EuclideanNorm())-15.24)<0.001
holes=[fps[f'H{i}'] for i in range(1,5)]
assert sorted({round(pcb.ToMM(h.GetPosition().x),3) for h in holes})==[78,142]
assert sorted({round(pcb.ToMM(h.GetPosition().y),3) for h in holes})==[73,137]
for hole in holes:
    pad=next(iter(hole.Pads()))
    assert pad.GetAttribute()==pcb.PAD_ATTRIB_NPTH
    assert abs(pcb.ToMM(pad.GetDrillSize().x)-2.2)<0.001
for net in netlist.findall('./nets/net'):
    expected={(p.attrib['ref'],p.attrib['pin']) for p in net.findall('node')}
    assert actual[net.attrib['name']]==expected,(net.attrib['name'],actual[net.attrib['name']],expected)
assert board.GetCopperLayerCount()==2
assert abs(pcb.ToMM(board.GetDesignSettings().GetBoardThickness())-1.0)<0.001
box=board.GetBoardEdgesBoundingBox()
assert abs(pcb.ToMM(box.GetWidth())-70)<0.1 and abs(pcb.ToMM(box.GetHeight())-70)<0.1
assert len(list(board.GetTracks()))>100
keepouts=[z for z in board.Zones() if z.GetIsRuleArea()]
assert len(keepouts)==1
assert keepouts[0].GetDoNotAllowTracks() and keepouts[0].GetDoNotAllowVias() and keepouts[0].GetDoNotAllowZoneFills()
assert keepouts[0].GetLayerSet().Contains(pcb.F_Cu) and keepouts[0].GetLayerSet().Contains(pcb.B_Cu)
pours=[z for z in board.Zones() if not z.GetIsRuleArea()]
assert len(pours)==2 and all(str(z.GetNetname())=='/GND' for z in pours)
assert all(z.GetPadConnection()==pcb.ZONE_CONNECTION_THERMAL for z in pours)
report=json.loads((root/'drc-report.json').read_text(encoding='utf-8'))
assert not report['violations'], report['violations']
assert not report['unconnected_items'], report['unconnected_items']
assert not report['schematic_parity'], report['schematic_parity']
tracks=list(board.GetTracks())
summary={
    'board_size_mm':[70,70], 'layers':2, 'thickness_mm':1.0,
    'schematic_footprints':len(expected_refs), 'mounting_holes':4,
    'mounting_hole_pattern_mm':[64,64],'module_ground_pads':'thermal relief',
    'assembly':'All carrier components through-hole; existing plug-in modules retained',
    'plated_component_holes':plated_pads,
    'module_header_pitch_mm':2.54,'module_header_drill_mm':1.0,
    'track_segments':sum(isinstance(t,pcb.PCB_TRACK) and not isinstance(t,pcb.PCB_VIA) for t in tracks),
    'vias':sum(isinstance(t,pcb.PCB_VIA) for t in tracks),
    'ground_pours':2, 'antenna_keepout_layers':['F.Cu','B.Cu'],
    'drc_violations':0, 'unconnected_items':0, 'schematic_parity_issues':0,
    'status':'Prototype; physical module fit and motor-current ratings require validation.'
}
(root/'pcb-validation.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print(json.dumps(summary,indent=2))
