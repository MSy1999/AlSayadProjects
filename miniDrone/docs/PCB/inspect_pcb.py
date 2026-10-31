"""Inspect the native PCB and its component geometry using KiCad's Python."""
from pathlib import Path
import pcbnew as pcb

root=Path(__file__).resolve().parent/'Esp32SuperMini'
board=pcb.LoadBoard(str(root/'references'/'before-layout.kicad_pcb'))
print('FOOTPRINTS',len(list(board.GetFootprints())))
for fp in sorted(board.GetFootprints(),key=lambda f:f.GetReference()):
    print(fp.GetReference(),fp.GetValue(),fp.GetFPID().GetLibItemName())
    pos=fp.GetPosition()
    for pad in fp.Pads():
        p=pad.GetPosition()
        print(' ',pad.GetNumber(),pad.GetNetname(),
              round(pcb.ToMM(p.x-pos.x),3),round(pcb.ToMM(p.y-pos.y),3),
              'size',round(pcb.ToMM(pad.GetSizeX()),3),round(pcb.ToMM(pad.GetSizeY()),3))
print('DSN',pcb.ExportSpecctraDSN.__doc__)
print('ZONE',pcb.ZONE.__doc__)
