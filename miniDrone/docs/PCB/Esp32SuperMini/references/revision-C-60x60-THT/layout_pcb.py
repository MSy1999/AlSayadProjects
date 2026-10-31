"""Build the 60 x 60 mm MiniDrone prototype layout with KiCad 10 Python.

Run with KiCad's bundled python.exe. 'prepare' writes placement + fixed power
trunks and exports DSN. 'finish' imports a locally routed SES and pours ground.
The original imported board is preserved in references/before-layout.kicad_pcb.
"""
from pathlib import Path
import json
import csv
import sys
import pcbnew as pcb

ROOT=Path(__file__).resolve().parent/'Esp32SuperMini'
ROUTE=ROOT/'routing'
ROUTE.mkdir(exist_ok=True)
MM=pcb.FromMM
FOOTPRINT_IO=pcb.PCB_IO_MGR.FindPlugin(pcb.PCB_IO_MGR.KICAD_SEXP)

def xy(x,y): return pcb.VECTOR2I(MM(x),MM(y))

def shape(board,a,b,layer,width=0.15):
    item=pcb.PCB_SHAPE(board)
    item.SetShape(pcb.SHAPE_T_SEGMENT)
    item.SetStart(xy(*a)); item.SetEnd(xy(*b))
    item.SetLayer(layer); item.SetWidth(MM(width))
    board.Add(item)
    return item

def rect(board,x1,y1,x2,y2,layer,width=0.15):
    for a,b in [((x1,y1),(x2,y1)),((x2,y1),(x2,y2)),((x2,y2),(x1,y2)),((x1,y2),(x1,y1))]:
        shape(board,a,b,layer,width)

def text(board,value,x,y,size=0.8,layer=pcb.F_SilkS,angle=0):
    size=max(size,0.8)
    item=pcb.PCB_TEXT(board)
    item.SetText(value); item.SetPosition(xy(x,y))
    item.SetTextSize(xy(size,size)); item.SetTextThickness(MM(0.12))
    item.SetLayer(layer); item.SetTextAngle(pcb.EDA_ANGLE(angle,pcb.DEGREES_T))
    board.Add(item)
    return item

def configure_project():
    path=ROOT/'Esp32SuperMini.kicad_pro'
    pro=json.loads(path.read_text(encoding='utf-8'))
    template=pro['net_settings']['classes'][0].copy()
    classes=[]
    for priority,(name,width,via,drill) in enumerate([
        ('Motor',0.8,1.0,0.5),('Battery',1.2,1.2,0.6),
        ('Ground',0.7,1.0,0.5),('LogicPower',0.5,0.8,0.4),('Default',0.25,0.7,0.35)]):
        cl=template.copy()
        cl.update(name=name,track_width=width,clearance=0.25,via_diameter=via,
                  via_drill=drill,priority=priority if name!='Default' else 2147483647)
        classes.append(cl)
    pro['net_settings']['classes']=classes
    patterns=[{'netclass':'Battery','pattern':'/VBAT_*'},
              {'netclass':'Motor','pattern':'/M?_NEG'},
              {'netclass':'Ground','pattern':'/GND'}]
    patterns += [{'netclass':'LogicPower','pattern':net} for net in ['/3V3','/SUPPLY_5V','/BOOST_5V']]
    pro['net_settings']['netclass_patterns']=patterns
    settings=pro['board']['design_settings']
    settings['rules'].update(min_clearance=0.2,min_track_width=0.2,
                             min_copper_edge_clearance=0.5,min_via_diameter=0.6,
                             min_through_hole_diameter=0.3)
    path.write_text(json.dumps(pro,indent=2)+'\n',encoding='utf-8')

def prepare():
    configure_project()
    board=pcb.LoadBoard(str(ROOT/'references'/'before-layout.kicad_pcb'))
    board.SetFileName(str(ROOT/'Esp32SuperMini.kicad_pcb'))
    board.GetDesignSettings().SetBoardThickness(MM(1.0))
    for item in list(board.GetTracks())+list(board.GetDrawings())+list(board.Zones()):
        board.Remove(item)
    fps={f.GetReference():f for f in board.GetFootprints()}
    # Rebuild from the schematic BOM while preserving schematic identities and
    # pad-number/net assignments. No SMD footprints remain on the carrier.
    replacements=[]
    for row in csv.DictReader((ROOT/'BOM.csv').open(encoding='utf-8-sig')):
        ref=row['Reference']; old=fps[ref]
        lib,name=row['Footprint'].split(':')
        directory=ROOT/'MiniDrone.pretty' if lib=='MiniDrone' else Path(r'C:\Program Files\KiCad\10.0\share\kicad\footprints')/(lib+'.pretty')
        fp=FOOTPRINT_IO.FootprintLoad(str(directory),name)
        assert fp, row['Footprint']
        fp.SetReference(ref); fp.SetValue(row['Value'])
        if row['Design note']:
            fp.SetField('DesignNote',row['Design note']); fp.GetField('DesignNote').SetVisible(False)
        fp.SetFPIDAsString(row['Footprint']); fp.SetPath(old.GetPath())
        fp.SetUuid(old.m_Uuid); fp.SetDNP(old.IsDNP())
        nets={pad.GetNumber():pad.GetNetCode() for pad in old.Pads()}
        replacements.append((ref,old,fp,nets))
    for ref,old,fp,nets in replacements:
        board.Remove(old); board.Add(fp); fps[ref]=fp
        for pad in fp.Pads(): pad.SetNetCode(nets.get(pad.GetNumber(),0))
    positions={
        'U1':(100,118.5,0),'U2':(92,87,90),
        'J1':(97.5,73,90),'SW1':(97.5,79,90),'J2':(112.5,74,0),'JP1':(118,76,90),
        'J3':(79,80,90),'J4':(131,80,270),'J5':(131,122,270),'J6':(79,122,90),
        'Q1':(85.5,91.5,180),'Q2':(124.5,91.5,0),
        'Q3':(124.5,110.5,0),'Q4':(85.5,110.5,180),
        'D1':(78.5,85,0),'D2':(131.5,85,180),'D3':(131.5,115,180),'D4':(78.5,115,0),
        'C7':(81,76.5,0),'C8':(129,76.5,180),'C9':(126.5,118.7,180),'C10':(83.5,118.7,0),
        'R1':(79.5,96.5,0),'R2':(130.5,96.5,180),'R3':(130.5,105.5,180),'R4':(79.5,105.5,0),
        'R5':(79.5,101.5,0),'R6':(130.5,101.5,180),'R7':(128.5,126,180),'R8':(81.5,126,0),
        'C1':(90.8,79,0),'C2':(104,81,0),'C3':(112.4,111.5,0),
        'C4':(117.7,112,270),'C5':(112.4,118.5,0),
        'R9':(116,98.5,270),'R10':(116,88,270),
        'R11':(116,72,0),'R12':(116,81,0),'C6':(113,127,0),
    }
    assert set(positions)==set(fps),(set(fps)-set(positions))
    for ref,(x,y,a) in positions.items():
        fp=fps[ref]
        fp.SetOrientationDegrees(a); fp.SetPosition(xy(x,y))
        fp.Value().SetVisible(False)
        reference=fp.Reference()
        reference.SetTextSize(xy(0.8,0.8)); reference.SetTextThickness(MM(0.12))
        reference.SetTextAngle(pcb.EDA_ANGLE(0,pcb.DEGREES_T))
        reference.SetKeepUpright(True); reference.SetVisible(True)
        reference.SetPosition(xy(x,y-1.8))
    refs={
        'U1':(100,105.6),'U2':(92,84.7),
        'J1':(97.5,71.2),'SW1':(97.5,76.8),'J2':(115.3,74),'JP1':(119,73.8),
        'J3':(80,77.9),'J4':(130,77.9),'J5':(130,122.2),'J6':(80,122.2),
        'Q1':(83,86.8),'Q2':(122,89.5),'Q3':(127,113.2),'Q4':(87.9,110.5),
        'D1':(87,84.5),'D2':(123,84.5),'D3':(123,115.5),'D4':(87,115.5),
        'C7':(85.5,75.4),'C8':(124.5,75.4),'C9':(124.5,124.6),'C10':(85.5,124.6),
        'C1':(90.5,96.5),'C2':(80.5,98.2),'C3':(112.5,104.3),
        'C4':(114,114.7),'C5':(114,118),
        'R1':(87.2,96.2),'R2':(122.8,96.2),'R3':(122.8,103.8),'R4':(89.8,105.5),
        'R5':(82,96.2),'R6':(128,96.2),'R7':(128,103.8),'R8':(82,103.8),
        'R9':(116,99),'R10':(116,95),'R11':(112.5,123.3),'R12':(112.5,126.5),'C6':(116.5,126.5),
    }
    # Through-hole bodies span from pad 1 to pad 2; label their body centres.
    for ref,fp in fps.items():
        if ref.startswith(('R','D','C')):
            pads=list(fp.Pads()); centre=(pads[0].GetPosition()+pads[1].GetPosition())/2
            refs[ref]=(pcb.ToMM(centre.x),pcb.ToMM(centre.y)-2.2)
    refs.update({'C1':(92.05,74.7),'C3':(113.4,108),'C5':(113.4,122),
                 'C4':(117.7,109.5),'R9':(118.7,102.31),'R10':(118.7,91.81),
                 'R11':(113.8,71.5),'R12':(119.81,78.5),
                 'R1':(83.31,99),'R2':(126.69,99),'R5':(83.31,103.5),'R6':(126.69,103.5),
                 'R3':(132,107.2),'R4':(78,107.2),
                 'D4':(83.58,115),'D3':(126.42,115),
                 'C10':(86,118.7),'C9':(124,118.7),
                 'Q1':(87.5,89.7),'Q2':(122.5,89.7),'Q3':(132.3,110.5),'Q4':(77.7,110.5),
                 'JP1':(123,78.4),'J5':(130,124.2),'J6':(80,124.2)})
    for ref,point in refs.items(): fps[ref].Reference().SetPosition(xy(*point))
    for fp in fps.values():
        for pad in fp.Pads():
            if pad.GetNetname()=='/GND' and fp.GetReference().startswith(('Q','J','SW','C1','U')):
                pad.SetLocalZoneConnection(pcb.ZONE_CONNECTION_FULL)

    # Nominal plug-in envelopes are drawn for mechanical review. Keep other
    # components out of these areas; the actual breakout boards must be measured.
    rect(board,91,87,112,103,pcb.Dwgs_User,0.2)
    text(board,'GY-521 BODY ENVELOPE - VERIFY',101.5,101,0.7,pcb.Dwgs_User)
    text(board,'MPU6050',101.5,94.3,1.0)
    text(board,'3V G SCL SDA XDA XCL AD0 INT',100.9,89.5,0.65)

    # Actual all-copper-layer antenna keepout, between the two header rows.
    keepout=pcb.ZONE(board)
    keepout.SetIsRuleArea(True)
    layers=pcb.LSET(); layers.AddLayer(pcb.F_Cu); layers.AddLayer(pcb.B_Cu)
    keepout.SetLayerSet(layers)
    keepout.SetDoNotAllowTracks(True); keepout.SetDoNotAllowVias(True)
    keepout.SetDoNotAllowZoneFills(True)
    keepout.SetDoNotAllowPads(False); keepout.SetDoNotAllowFootprints(False)
    polygon=keepout.Outline(); polygon.NewOutline()
    for point in [(94,125.5),(106,125.5),(106,130.1),(94,130.1)]:
        polygon.Append(int(MM(point[0])),int(MM(point[1])))
    keepout.SetZoneName('SUPER_MINI_ANTENNA_NO_COPPER_VERIFY')
    board.Add(keepout)
    text(board,'ANTENNA',100,128.3,0.7)

    # 60 x 60 mm outline with 2 mm corner chamfers.
    outline=[(77,70),(133,70),(135,72),(135,128),(133,130),(77,130),(75,128),(75,72)]
    for a,b in zip(outline,outline[1:]+outline[:1]): shape(board,a,b,pcb.Edge_Cuts,0.05)
    for i,point in enumerate([(78,73),(132,73),(132,127),(78,127)],1):
        fp=FOOTPRINT_IO.FootprintLoad(r'C:\Program Files\KiCad\10.0\share\kicad\footprints\MountingHole.pretty',
                             'MountingHole_2.2mm_M2')
        fp.SetReference(f'H{i}'); fp.SetPosition(xy(*point))
        fp.SetAttributes(fp.GetAttributes()|pcb.FP_BOARD_ONLY|pcb.FP_EXCLUDE_FROM_BOM|pcb.FP_EXCLUDE_FROM_POS_FILES)
        fp.Reference().SetVisible(False); fp.Value().SetVisible(False)
        board.Add(fp)

    text(board,'MINIDRONE',100.5,97,1.1)
    text(board,'60x60  REV C THT',101,99,0.65)
    text(board,'FRONT',105,71.3,0.7)
    for value,x,y in [('M1 FL',84,72),('M2 FR',128,72),
                       ('M3 RR',125,129),('M4 RL',85,129),
                       ('BAT',103.3,73),('SW',103.4,77.5),('5V BOOST',108,84),
                       ('REMOVE JP1 FOR USB',101.5,91.5),('PROTOTYPE',100.5,101)]:
        text(board,value,x,y,0.65)
    for ref in ['J3','J4','J5','J6']:
        for pad in fps[ref].Pads():
            pos=pad.GetPosition()
            text(board,'+' if pad.GetNumber()=='1' else '-',pcb.ToMM(pos.x),
                 pcb.ToMM(pos.y)+(2.1 if ref in ['J3','J4'] else -2.1),0.65)

    # Fixed wide battery distribution. PTH connector pads join the two layers.
    def track(net,points,width,layer):
        for a,b in zip(points,points[1:]):
            tr=pcb.PCB_TRACK(board); tr.SetStart(xy(*a)); tr.SetEnd(xy(*b))
            tr.SetWidth(MM(width)); tr.SetLayer(layer)
            tr.SetNetCode(board.GetNetsByName()[net].GetNetCode())
            tr.SetLocked(True); board.Add(tr)
    # Side trunks feed the motor pairs. The router connects them to SW1 and C1.
    track('/VBAT_SW',[(79,80),(77,82),(77,119),(79,122)],1.8,pcb.F_Cu)
    track('/VBAT_SW',[(131,80),(133,82),(133,119),(131,122)],1.8,pcb.F_Cu)
    # Connect the bulk-capacitor branch to an explicit point on the left trunk.
    # KiCad/Specctra connectivity recognizes contacts along track interiors.

    # Assign routing widths in memory as well as in the project file.
    ns=board.GetDesignSettings().m_NetSettings
    ns.GetDefaultNetclass().SetTrackWidth(MM(0.25))
    ns.GetDefaultNetclass().SetClearance(MM(0.25))
    classes={}
    for name,width,via,drill in [('Motor',0.8,1.0,0.5),('Battery',1.2,1.2,0.6),
                                ('Ground',0.7,1.0,0.5),('LogicPower',0.5,0.8,0.4)]:
        cl=pcb.NETCLASS(name)
        cl.SetTrackWidth(MM(width)); cl.SetClearance(MM(0.25))
        cl.SetViaDiameter(MM(via)); cl.SetViaDrill(MM(drill))
        ns.SetNetclass(name,cl); classes[name]=cl
    for name,net in board.GetNetsByName().items():
        name=str(name)
        cl='Battery' if name.startswith('/VBAT_') else 'Motor' if name.startswith('/M') and name.endswith('_NEG') else 'Ground' if name=='/GND' else 'LogicPower' if name in ['/3V3','/SUPPLY_5V','/BOOST_5V'] else None
        if cl: net.SetNetClass(classes[cl])
    board.BuildConnectivity()
    pcb.SaveBoard(str(ROOT/'Esp32SuperMini.kicad_pcb'),board)
    pcb.SaveBoard(str(ROUTE/'placed.kicad_pcb'),board)
    assert pcb.ExportSpecctraDSN(board,str(ROUTE/'MiniDrone.dsn'))
    configure_dsn(board)
    placement={ref:{'x_mm':x,'y_mm':y,'rotation_deg':a} for ref,(x,y,a) in positions.items()}
    (ROOT/'placement.json').write_text(json.dumps(placement,indent=2)+'\n',encoding='utf-8')
    print('Prepared 60x60 mm placement; 4 mounting holes; antenna keepout; fixed battery trunks.')

def configure_dsn(board):
    """Retain intended net widths even when KiCad exports a single default class."""
    def end_of_block(s,start):
        depth=0; quoted=False; escaped=False
        for i in range(start,len(s)):
            c=s[i]
            if quoted:
                if escaped: escaped=False
                elif c=='\\': escaped=True
                elif c=='"': quoted=False
            elif c=='"': quoted=True
            elif c=='(': depth+=1
            elif c==')':
                depth-=1
                if depth==0: return i+1
        raise ValueError('Unclosed DSN block')
    path=ROUTE/'MiniDrone.dsn'
    s=path.read_text(encoding='utf-8')
    classes={name:[] for name in ['Battery','Motor','Ground','LogicPower','Default']}
    for raw in board.GetNetsByName():
        name=str(raw)
        if not name: continue
        cl='Battery' if name.startswith('/VBAT_') else 'Motor' if name.startswith('/M') and name.endswith('_NEG') else 'Ground' if name=='/GND' else 'LogicPower' if name in ['/3V3','/SUPPLY_5V','/BOOST_5V'] else 'Default'
        classes[cl].append(name)
    new=[]
    for name,width,via in [('Battery',1200,'1200:600'),('Motor',800,'1000:500'),
                            ('Ground',700,'1000:500'),('LogicPower',500,'800:400'),('Default',250,'700:350')]:
        new.append('(class '+name+' '+' '.join(json.dumps(net) for net in classes[name])+
                   f' (circuit (use_via "Via[0-1]_{via}_um")) (rule (width {width}) (clearance 250)))')
    start=s.index('(class kicad_default ')
    s=s[:start]+'\n'.join(new)+s[end_of_block(s,start):]
    vias=['1200:600','1000:500','800:400','700:350']
    s=s.replace('(via "Via[0-1]_600:300_um")','(via '+' '.join(f'"Via[0-1]_{v}_um"' for v in vias)+')')
    start=s.index('(library')
    end=end_of_block(s,start)-1
    stacks=[]
    for v in vias:
        diameter=int(v.split(':')[0])
        stacks.append(f'(padstack "Via[0-1]_{v}_um" (shape (circle F.Cu {diameter})) (shape (circle B.Cu {diameter})) (attach off))')
    s=s[:end]+'\n'+'\n'.join(stacks)+'\n'+s[end:]
    path.write_text(s,encoding='utf-8')

def finish():
    board=pcb.LoadBoard(str(ROUTE/'placed.kicad_pcb'))
    assert pcb.ImportSpecctraSES(board,str(ROUTE/'MiniDrone.ses'))
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            if pad.GetNetname()=='/GND' and fp.GetReference().startswith(('Q','J','SW','C1','U')):
                pad.SetLocalZoneConnection(pcb.ZONE_CONNECTION_FULL)
    for layer in [pcb.F_Cu,pcb.B_Cu]:
        zone=pcb.ZONE(board); zone.SetLayer(layer)
        zone.SetNetCode(board.GetNetsByName()['/GND'].GetNetCode())
        zone.SetZoneName('GND_FRONT' if layer==pcb.F_Cu else 'GND_BACK')
        zone.SetLocalClearance(MM(0.25)); zone.SetMinThickness(MM(0.25))
        zone.SetPadConnection(pcb.ZONE_CONNECTION_THERMAL)
        zone.SetThermalReliefGap(MM(0.25)); zone.SetThermalReliefSpokeWidth(MM(0.4))
        polygon=zone.Outline(); polygon.NewOutline()
        for point in [(75.5,70.5),(134.5,70.5),(134.5,129.5),(75.5,129.5)]:
            polygon.Append(int(MM(point[0])),int(MM(point[1])))
        board.Add(zone)
    board.BuildConnectivity()
    filler=pcb.ZONE_FILLER(board); filler.Fill(board.Zones())
    board.SetFileName(str(ROOT/'Esp32SuperMini.kicad_pcb'))
    pcb.SaveBoard(str(ROOT/'Esp32SuperMini.kicad_pcb'),board)
    print('Imported routed session and filled both ground planes.')

if __name__=='__main__':
    {'prepare':prepare,'finish':finish,'configure':configure_project}[sys.argv[1] if len(sys.argv)>1 else 'prepare']()
