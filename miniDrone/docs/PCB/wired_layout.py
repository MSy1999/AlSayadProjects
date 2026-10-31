"""Fully wired single-sheet layout. Coordinates use the generator's 1.27 mm grid."""
from collections import defaultdict


def draw(add, parts, wire, label, nc, text, junction, G):
    resistor = 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal'
    capacitor = 'Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm'
    electrolytic = 'Capacitor_THT:CP_Radial_D5.0mm_P2.00mm'
    header = 'Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical'
    segments = []

    def p(ref, pin):
        return tuple(round(v/G, 5) for v in parts[ref][str(pin)])

    def path(net, *points):
        for a, b in zip(points, points[1:]):
            assert a[0] == b[0] or a[1] == b[1], (net, a, b)
            if a != b:
                segments.append((net, a, b))

    text('MINIDRONE | all components connected with visible wires',14,14,2.3)
    text('BC337 prototype / 1S 3.7 V 400 mAh / existing Super Mini + GY-521 plug-in boards',14,21,1.3)
    text('Dots = joined wires. Crossing wires without a dot are separate. Labels only name already-connected wires.',14,27,1.15)
    text('01  BATTERY, MAIN SWITCH AND EXTERNAL BOOST CONNECTION',14,32,1.3)

    add('J1','Connector_Generic:Conn_01x02','BATTERY_1S',30,45,180,footprint=header,
        note='3.7 V nominal, 4.2 V full; 400 mAh. Connector/discharge rating TBD.',fields=(27,36,27,38))
    add('SW1','Switch:SW_SPST','MAIN_SWITCH',52,45,footprint=header,
        note='External switch via header; total motor stall current + logic input current.',fields=(52,36,52,38))
    add('J2','Connector_Generic:Conn_01x04','EXTERNAL_5V_BOOST',95,47,180,
        footprint='Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
        note='Wired module interface: 1 IN+, 2 IN-/GND, 3 OUT+ 5V, 4 OUT-/GND.',fields=(95,35,95,37))
    add('JP1','Jumper:Jumper_2_Open','RUN_POWER_LINK',138,45,footprint=header,
        note='REMOVABLE shunt. REMOVE and switch battery OFF before USB.',fields=(138,36,138,38))
    path('VBAT_RAW',p('J1',1),p('SW1',1))
    path('VBAT_SW',p('SW1',2),(64,45),(64,100),(408,100))
    path('VBAT_SW',p('J2',1),(106,49),(106,100))
    path('BOOST_5V',p('J2',3),p('JP1',1))
    path('SUPPLY_5V',p('JP1',2),(150,45),(150,125),(110,125),(110,170))
    path('GND',p('J1',2),(16,p('J1',2)[1]),(16,260),(422,260))
    path('GND',p('J2',4),(104,43),(104,120),(20,120),(20,260))
    path('GND',p('J2',2),(104,47))
    text('J2: external regulated 5.0 V boost module, >=0.5 A output across battery range.',200,45)
    text('RUN: USB unplugged, JP1 fitted. PROGRAM: battery OFF, JP1 removed, then USB.',200,52)
    text('Motor positives use the battery rail; logic uses the boost + onboard regulator.',200,59)

    add('C1','Device:C_Polarized','470u / 10V',165,115,
        footprint='Capacitor_THT:CP_Radial_D6.3mm_P2.50mm')
    add('C2','Device:C','100n / 16V',185,115,footprint=capacitor)
    for ref in ['C1','C2']:
        x=p(ref,1)[0]
        path('VBAT_SW',p(ref,1),(x,100))
        path('GND',p(ref,2),(x,135),(165,135),(165,260))
    add('C3','Device:C_Polarized','22u / 16V',145,150,footprint=electrolytic)
    path('SUPPLY_5V',p('C3',1),(145,125))
    path('GND',p('C3',2),(160,p('C3',2)[1]),(160,260))
    for ref,x,value in [('C4',33,'100n / 16V'),('C5',53,'10u / 10V')]:
        add(ref,'Device:C_Polarized' if ref=='C5' else 'Device:C',value,x,145,
            footprint=electrolytic if ref=='C5' else capacitor)
        path('3V3',p(ref,1),(x,130))
        path('GND',p(ref,2),(x,156),(23,156),(23,260))

    text('02  SENSOR AND CONTROLLER',22,112,1.3)
    text('U1 uses functional pin grouping; pad numbers still match the physical headers.',22,119,1.05)
    add('U1','MiniDrone:ESP32_C3_SUPER_MINI','ESP32-C3 Super Mini',110,190,
        footprint='MiniDrone:ESP32_C3_SuperMini_THT',
        note='Plug-in board; verify physical dimensions before layout.',fields=(110,211,110,214))
    add('U2','MiniDrone:GY521_MODULE','GY-521 / MPU6050',45,185,
        footprint='Connector_PinSocket_2.54mm:PinSocket_1x08_P2.54mm_Vertical',
        note='Verify actual header order VCC,GND,SCL,SDA,XDA,XCL,AD0,INT.')
    for pin in [4,5,8,10,11,12]:
        nc('U1',pin)
    for pin in [5,6,8]:
        nc('U2',pin)
    path('3V3',(33,130),(83,130))
    path('3V3',p('U1',14),(79,176),(79,130))
    path('3V3',(79,165),(65,165),(65,178),p('U2',1))
    path('GND',p('U1',15),p('U2',2))
    path('GND',(67,180),(67,199),(23,199))
    path('GND',p('U2',7),(63,190),(63,199),(67,199))
    path('IMU_SDA',p('U1',13),p('U2',4))
    path('IMU_SCL',p('U1',1),(75,188),(75,182),p('U2',3))
    label('IMU_SDA',(85*G,184*G))
    label('IMU_SCL',(85*G,188*G))
    for ref,x,net,bottom in [('R9',83,'IMU_SDA',184),('R10',73,'IMU_SCL',182)]:
        add(ref,'Device:R','4k7 DNP',x,155,footprint=resistor,
            note='Optional: do not populate if suitable 3V3 pull-ups are already on GY-521.',
            fields=(x+4,150,x+4,152))
        path('3V3',p(ref,1),(x,130))
        path(net,p(ref,2),(x,bottom))

    add('R11','Device:R','100k 1%',115,233,footprint=resistor)
    add('R12','Device:R','100k 1%',115,251,footprint=resistor)
    add('C6','Device:C','100n / 16V',78,251,footprint=capacitor)
    path('VBAT_SW',p('R11',1),(170,p('R11',1)[1]),(170,100))
    path('BAT_ADC',p('R11',2),(115,242),p('R12',1))
    path('BAT_ADC',(115,242),(78,242),p('C6',1))
    path('BAT_ADC',p('U1',9),(90,192),(90,242),(115,242))
    for ref in ['R12','C6']:
        path('GND',p(ref,2),(p(ref,2)[0],260))

    for i,(ox,position) in enumerate([(200,'FRONT LEFT / CW'),(264,'FRONT RIGHT / CCW'),
                                       (328,'REAR RIGHT / CW'),(392,'REAR LEFT / CCW')],1):
        text(f'M{i}  {position}',ox+20,118,1.25)
        qr,rb,pd,di,ca,jo=f'Q{i}',f'R{i}',f'R{i+4}',f'D{i}',f'C{i+6}',f'J{i+2}'
        add(qr,'Transistor_BJT:Q_NPN_CBE','BC337',ox+28,200,
            footprint='Package_TO_SOT_THT:TO-92_Inline_Wide',
            note='Assumed onsemi pinout 1 C, 2 B, 3 E. Verify your transistor supplier.')
        add(rb,'Device:R','330R*',ox+8,200,90,footprint=resistor,
            note='PROVISIONAL base resistor; verify motor startup current and transistor saturation.')
        add(pd,'Device:R','10k',ox+20,224,footprint=resistor)
        path(f'M{i}_BASE',p(rb,2),(ox+20,200),p(qr,2))
        path(f'M{i}_BASE',(ox+20,200),p(pd,1))
        path('GND',p(pd,2),(ox+20,260))
        path('GND',p(qr,3),(p(qr,3)[0],260))
        add(jo,'Connector_Generic:Conn_01x02',f'M{i}_MOTOR',ox+24,155,footprint=header,
            note='Motor + pin1, motor - pin2. Connector current rating TBD.',
            fields=(ox+24,147,ox+24,149))
        add(di,'Device:D_Schottky','1N5819*',ox+44,160,270,
            footprint='Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal',
            note='Leaded DO-41 Schottky; verify motor current/thermal ratings. Stripe to VBAT_SW.',fields=(ox+44,150,ox+44,152))
        add(ca,'Device:C','100n',ox+56,160,footprint=capacitor,
            note='16V ceramic, fit directly at motor terminals.',fields=(ox+56,150,ox+56,152))
        path('VBAT_SW',(ox+16,100),(ox+16,140),(ox+56,140))
        path('VBAT_SW',p(jo,1),(ox+16,155),(ox+16,140))
        for ref in [di,ca]:
            path('VBAT_SW',p(ref,1),(p(ref,1)[0],140))
        path(f'M{i}_NEG',p(jo,2),(ox+18,157),(ox+18,178),(ox+56,178))
        for ref in [di,ca]:
            path(f'M{i}_NEG',p(ref,2),(p(ref,2)[0],178))
        path(f'M{i}_NEG',p(qr,1),(p(qr,1)[0],178))
        label(f'M{i}_NEG',((ox+33)*G,178*G))
        # Dedicated visible signal lanes below the common ground rail.
        upin=[2,3,6,7][i-1]
        xfan=130+3*i
        lane=262+6*i
        path(f'M{i}_PWM',p('U1',upin),(xfan,p('U1',upin)[1]),(xfan,lane),
             (ox-6,lane),(ox-6,200),p(rb,1))
        label(f'M{i}_PWM',( (150+10*i)*G,lane*G ))

    # Power markers sit on the actual rails, rather than on detached label stubs.
    for ref,net,x,y in [('#FLG01','SUPPLY_5V',110,125),
                         ('#FLG02','VBAT_SW',180,100),('#FLG03','GND',30,260)]:
        add(ref,'power:PWR_FLAG','PWR_FLAG',x,y)
    for name,x,y in [('VBAT_RAW',38,45),('BOOST_5V',116,45),('SUPPLY_5V',115,125),
                     ('VBAT_SW',200,100),('3V3',37,130),('GND',40,260),('BAT_ADC',94,242)]:
        label(name,(x*G,y*G))

    text('DRAFT: 330R base resistors and 1N5819 diodes require motor startup/stall-current verification.',14,297,1.1)
    text('Diode stripe / K goes to battery positive. Put each 100 nF capacitor at the motor terminals.',14,303,1.1)
    text('AD0 is grounded (0x68). R9/R10 are optional pull-ups. Divider: 4.20 V battery -> 2.10 V at GPIO0.',14,309,1.1)
    text('Module dimensions / connector ratings are provisional. No charger, reverse protection or automatic cutoff.',14,315,1.05)

    # Split wires at same-net branch points and add visible junction dots. A plain
    # crossing of different nets remains unjoined, as in a conventional schematic.
    points=defaultdict(set)
    for net,a,b in segments:
        points[net].update((a,b))
    for ref in ['#FLG01','#FLG02','#FLG03']:
        net={'#FLG01':'SUPPLY_5V','#FLG02':'VBAT_SW','#FLG03':'GND'}[ref]
        points[net].add(p(ref,1))
    split=set()
    for net,a,b in segments:
        on=[]
        for point in points[net]:
            if a[0]==b[0]==point[0] and min(a[1],b[1])<=point[1]<=max(a[1],b[1]):
                on.append(point)
            elif a[1]==b[1]==point[1] and min(a[0],b[0])<=point[0]<=max(a[0],b[0]):
                on.append(point)
        on.sort()
        split.update((net,c,d) for c,d in zip(on,on[1:]) if c!=d)
    degree=defaultdict(int)
    for net,a,b in sorted(split):
        wire(tuple(v*G for v in a),tuple(v*G for v in b))
        degree[(net,a)]+=1
        degree[(net,b)]+=1
    for (net,point),count in degree.items():
        if count>=3:
            junction(tuple(v*G for v in point))
