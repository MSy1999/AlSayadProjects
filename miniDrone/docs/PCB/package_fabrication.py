"""Validate exported fabrication files and build the bare-PCB upload ZIP.

The archive is a prototype/quotation package; physical module fit and motor
current ratings still need confirmation before an order is placed.
"""
from pathlib import Path
import hashlib
import json
import re
import zipfile

root=Path(__file__).resolve().parent/'Esp32SuperMini'
fab=root/'fabrication'
gerbers=fab/'gerbers'
prefix='Esp32SuperMini'
files=[f'{prefix}-{suffix}' for suffix in [
    'F_Cu.gtl','B_Cu.gbl','F_Mask.gts','B_Mask.gbs',
    'F_Silkscreen.gto','B_Silkscreen.gbo','Edge_Cuts.gm1','PTH.drl','NPTH.drl']]
report=json.loads((root/'drc-report.json').read_text(encoding='utf-8'))
validation=json.loads((root/'pcb-validation.json').read_text(encoding='utf-8'))
assert not report['violations'] and not report['unconnected_items'] and not report['schematic_parity']
for name in files:
    content=(gerbers/name).read_text(encoding='utf-8')
    if name.endswith('.drl'):
        assert 'METRIC' in content and content.rstrip().endswith('M30'),name
    else:
        assert '%MOMM*%' in content and '%FSLAX46Y46*%' in content and content.rstrip().endswith('M02*'),name
outline=(gerbers/f'{prefix}-Edge_Cuts.gm1').read_text(encoding='utf-8')
coords=[(int(x)/1e6,int(y)/1e6) for x,y in re.findall(r'X(-?\d+)Y(-?\d+)D0[12]\*',outline)]
width=max(x for x,y in coords)-min(x for x,y in coords)
height=max(y for x,y in coords)-min(y for x,y in coords)
assert width==70 and height==70,(width,height)
counts={}
for kind,count in [('PTH',validation['plated_component_holes']+validation['vias']),('NPTH',4)]:
    content=(gerbers/f'{prefix}-{kind}.drl').read_text(encoding='utf-8')
    counts[kind]=len(re.findall(r'^X-?[\d.]+Y-?[\d.]+$',content,re.M))
    assert counts[kind]==count,(kind,counts[kind])
archive=fab/'MiniDrone_RevD_THT_70x70_Prototype_Gerbers.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for name in files:
        z.write(gerbers/name,name)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert sorted(z.namelist())==sorted(files)
manifest={
    'archive':archive.name,'status':'Prototype quotation package; physical fit and motor current unverified.',
    'source_board_sha256':hashlib.sha256((root/'Esp32SuperMini.kicad_pcb').read_bytes()).hexdigest(),
    'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
    'outline_mm':[width,height],'layers':2,'thickness_mm':1.0,
    'plated_holes':counts['PTH'],'nonplated_holes':counts['NPTH'],
    'files':files,'format':'RS-274X Gerber + metric Excellon; matching absolute origins',
    'assembly_files_included':False,
}
(fab/'package-validation.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(json.dumps(manifest,indent=2))
