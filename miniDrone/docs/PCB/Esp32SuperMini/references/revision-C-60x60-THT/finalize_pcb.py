"""Add carrier socket models and drawing metadata without changing routing."""
from pathlib import Path
import csv
import pcbnew as pcb

root=Path(__file__).resolve().parent/'Esp32SuperMini'
board=pcb.LoadBoard(str(root/'Esp32SuperMini.kicad_pcb'))
fps={fp.GetReference():fp for fp in board.GetFootprints()}
for row in csv.DictReader((root/'BOM.csv').open(encoding='utf-8-sig')):
    fp=fps[row['Reference']]
    if row['Design note']:
        fp.SetField('DesignNote',row['Design note']); fp.GetField('DesignNote').SetVisible(False)
for ref,(x,y) in {'R11':(113.8,71.5),'R3':(132,107.2),'R4':(78,107.2),
                 'D4':(83.58,115),'D3':(126.42,115),'C10':(86,118.7),
                 'C9':(124,118.7),'JP1':(123,78.4)}.items():
    fps[ref].Reference().SetPosition(pcb.VECTOR2I(pcb.FromMM(x),pcb.FromMM(y)))
for drawing in board.GetDrawings():
    if isinstance(drawing,pcb.PCB_TEXT) and drawing.GetText()=='FRONT':
        drawing.SetPosition(pcb.VECTOR2I(pcb.FromMM(105),pcb.FromMM(71.3)))
fp=next(fp for fp in board.GetFootprints() if fp.GetReference()=='U1')
fp.Models().clear()
model_path='${KICAD10_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x08_P2.54mm_Vertical.step'
for x in [-7.62,7.62]:
    model=pcb.FP_3DMODEL()
    model.m_Filename=model_path
    model.m_Offset=pcb.VECTOR3D(x,8.89,0)
    model.m_Scale=pcb.VECTOR3D(1,1,1)
    model.m_Rotation=pcb.VECTOR3D(0,0,0)
    fp.Add3DModel(model)
title=board.GetTitleBlock()
title.SetTitle('MiniDrone carrier - 60 x 60 mm')
title.SetCompany('AlSayadProjects')
title.SetRevision('C - THROUGH HOLE PROTOTYPE')
title.SetDate('2026-10-31')
pcb.SaveBoard(str(root/'Esp32SuperMini.kicad_pcb'),board)
print('Added the two U1 carrier socket models and board title metadata.')
