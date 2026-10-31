"""Remove router leftovers identified by KiCad DRC and refill the ground planes."""
from pathlib import Path
import json
import re
import pcbnew as pcb

root=Path(__file__).resolve().parent/'Esp32SuperMini'
report=json.loads((root/'drc-report.json').read_text(encoding='utf-8'))
ids={item['uuid'] for v in report['violations'] if v['type']=='track_dangling' for item in v['items']}
removed=0
path=root/'Esp32SuperMini.kicad_pcb'
source=path.read_text(encoding='utf-8')
# Remove exact DRC-identified segment records before loading the board. This
# avoids stale SWIG connectivity references when deleting live native tracks.
for match in reversed(list(re.finditer(r'\(segment\s',source))):
    start=match.start(); depth=0
    for i in range(start,len(source)):
        if source[i]=='(': depth+=1
        elif source[i]==')':
            depth-=1
            if depth==0:
                block=source[start:i+1]
                if any(uid in block for uid in ids):
                    source=source[:start]+source[i+1:]; removed+=1
                break
path.write_text(source,encoding='utf-8')
board=pcb.LoadBoard(str(path))
for fp in board.GetFootprints():
    for pad in fp.Pads():
        if pad.GetNetname()=='/GND' and fp.GetReference().startswith(('Q','J','SW','C1')):
            pad.SetLocalZoneConnection(pcb.ZONE_CONNECTION_FULL)
board.BuildConnectivity()
filler=pcb.ZONE_FILLER(board)
filler.Fill(board.Zones())
pcb.SaveBoard(str(root/'Esp32SuperMini.kicad_pcb'),board)
print(f'Removed {removed} unused dangling segments; refreshed ground-pad connections and pours.')
