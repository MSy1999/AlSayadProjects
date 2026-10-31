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
title.SetTitle('MiniDrone carrier - 70 x 70 mm - hand soldering')
title.SetCompany('AlSayadProjects')
title.SetRevision('D - THROUGH HOLE PROTOTYPE')
title.SetDate('2026-10-31')
pcb.SaveBoard(str(root/'Esp32SuperMini.kicad_pcb'),board)
print('Added the two U1 carrier socket models and board title metadata.')
