"""Build offline hardware reference and vector wiring drawings from local assets."""
from pathlib import Path
from base64 import b64encode
from html import escape

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT.parent / 'Docs' / 'MeasurementESP'
ASSETS = OUT / 'assets'


def svg_start(title, subtitle, height):
    return [f'''<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="{height}" viewBox="0 0 1280 {height}" role="img" aria-label="{escape(title)}">
<style>text{{font-family:Arial,sans-serif;fill:#E6EDF3}}.title{{font-size:30px;font-weight:bold}}.sub{{font-size:18px;fill:#A0AEC0}}.label{{font-size:21px}}.small{{font-size:17px}}.box{{fill:#16232D;stroke:#24333E;stroke-width:2}}.wire{{fill:none;stroke-width:4;stroke-linejoin:round}}</style>
<rect width="1280" height="{height}" fill="#0B1118"/>
<text x="44" y="55" class="title">{title}</text><text x="44" y="90" class="sub">{subtitle}</text>''']


def line(parts, points, color):
    parts.append(f'<polyline points="{points}" class="wire" stroke="{color}"/>')


def label(parts, x, y, text, cls='label'):
    parts.append(f'<text x="{x}" y="{y}" class="{cls}">{escape(text)}</text>')


def box(parts, x, y, w, h):
    parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="12" class="box"/>')


def build_overview():
    p = svg_start('MeasurementESP / wiring overview', 'Logical connections by pin NAME; connector positions are not physical pin order.', 900)
    box(p, 70, 220, 340, 430)
    label(p, 95, 260, 'ESP32-DevKitC V4')
    label(p, 95, 292, 'ESP32-WROOM-32E', 'sub')
    for y, text in [(350, '3V3'), (410, 'GND'), (480, 'IO27 / DHT data'), (570, 'IO34 / ADC1_CH6')]:
        label(p, 95, y, text)
    label(p, 95, 620, 'USB power to this board', 'small')
    box(p, 840, 145, 365, 235)
    label(p, 862, 185, 'KY-015 / DHT11')
    for y, text in [(235, '+ / VCC (3.3 V)'), (290, 'S / DATA'), (345, '- / GND')]:
        label(p, 880, y, text)
    box(p, 840, 475, 365, 235)
    label(p, 862, 515, 'KY-018 / light module')
    for y, text in [(565, '+ / VCC (3.3 V)'), (620, '- / GND'), (675, 'S / analog output')]:
        label(p, 880, y, text)
    line(p, '410,344 490,344 490,230 840,230', '#FF716D')
    line(p, '490,344 490,560 840,560', '#FF716D')
    line(p, '410,404 540,404 540,340 840,340', '#A0AEC0')
    line(p, '540,404 540,615 840,615', '#A0AEC0')
    line(p, '410,474 650,474 650,285 840,285', '#00C2FF')
    # R3 is explicit in the overview: this user's DHT module has no built-in pull-up.
    line(p, '720,230 720,242', '#FF716D')
    p.append('<rect x="705" y="242" width="30" height="28" fill="#111B24" stroke="#00C2FF" stroke-width="3"/>')
    line(p, '720,270 720,285', '#00C2FF')
    for x, y, color in [(720, 230, '#FF716D'), (720, 285, '#00C2FF')]:
        p.append(f'<circle cx="{x}" cy="{y}" r="6" fill="{color}"/>')
    label(p, 580, 212, 'R3 10k REQUIRED', 'small')
    line(p, '840,670 730,670 730,785 410,785 410,564', '#F4B740')
    box(p, 445, 747, 245, 64)
    label(p, 462, 773, '10k / 10k divider', 'small')
    label(p, 462, 797, 'C1 optional (detail 02)', 'small')
    label(p, 70, 148, '01 / Supply both modules from 3V3; all grounds join at the sender.', 'small')
    label(p, 70, 184, 'Your DHT module has no pull-up: fit R3 between 3V3 and DATA as drawn.', 'small')
    for x, y, color in [(490, 344, '#FF716D'), (540, 404, '#A0AEC0')]:
        p.append(f'<circle cx="{x}" cy="{y}" r="6" fill="{color}"/>')
    label(p, 70, 827, 'Dots join wires; crossings without a dot are not connected.', 'small')
    label(p, 70, 855, 'Wi-Fi only to SmartDisplay; no signal/power wire between the two ESP boards.', 'small')
    p.append('</svg>')
    (ASSETS / 'wiring-overview.svg').write_text('\n'.join(p), encoding='utf-8')


def build_details():
    p = svg_start('MeasurementESP / circuit details', '02 / Added components are outside the sensor modules. All grounds are common.', 870)
    label(p, 60, 150, 'A. KY-018 output conditioning')
    label(p, 60, 203, 'KY-018 S', 'small')
    line(p, '65,240 250,240', '#F4B740')
    p.append('<rect x="250" y="225" width="115" height="30" fill="#111B24" stroke="#F4B740" stroke-width="3"/>')
    label(p, 265, 209, 'R1 10k', 'small')
    line(p, '365,240 1040,240', '#F4B740')
    label(p, 925, 211, 'GPIO34', 'label')
    for x in [530, 750]:
        p.append(f'<circle cx="{x}" cy="240" r="6" fill="#F4B740"/>')
    line(p, '530,240 530,305', '#F4B740')
    p.append('<rect x="515" y="305" width="30" height="100" fill="#111B24" stroke="#F4B740" stroke-width="3"/>')
    label(p, 563, 360, 'R2 10k', 'small')
    line(p, '530,405 530,465', '#A0AEC0')
    line(p, '750,240 750,333', '#F4B740')
    line(p, '718,333 782,333', '#00C2FF')
    line(p, '718,351 782,351', '#00C2FF')
    line(p, '750,351 750,465', '#A0AEC0')
    label(p, 800, 352, 'C1 100 nF (optional)', 'small')
    line(p, '425,465 880,465', '#A0AEC0')
    label(p, 890, 471, 'GND', 'small')
    label(p, 60, 510, 'V(GPIO34) = V(S) / 2, at most 1.65 V with a 3.3 V module supply.', 'small')
    label(p, 60, 540, 'The divider loads the module. Calibrate dark/bright with this complete circuit attached.', 'small')
    label(p, 60, 605, 'B. KY-015 DATA pull-up (required for your module)')
    label(p, 60, 680, '3V3', 'label')
    line(p, '135,675 270,675', '#FF716D')
    p.append('<rect x="270" y="660" width="115" height="30" fill="#111B24" stroke="#00C2FF" stroke-width="3"/>')
    label(p, 281, 644, 'R3 10k', 'small')
    line(p, '385,675 680,675 680,755', '#00C2FF')
    line(p, '270,755 1040,755', '#00C2FF')
    p.append('<circle cx="680" cy="755" r="6" fill="#00C2FF"/>')
    label(p, 60, 762, 'GPIO27', 'label')
    label(p, 1055, 762, 'DHT S', 'label')
    label(p, 60, 824, 'Use module markings and manufacturer pinout images; do not infer pin order from photographs.', 'small')
    p.append('</svg>')
    (ASSETS / 'wiring-details.svg').write_text('\n'.join(p), encoding='utf-8')


def data_url(name):
    ext = Path(name).suffix
    mime = 'image/svg+xml' if ext == '.svg' else 'image/png'
    return f'data:{mime};base64,' + b64encode((ASSETS / name).read_bytes()).decode()


def build_html():
    cards = ''
    for code, name, detail in [
        ('KY-015', 'Temperature + humidity', 'DHT11 timed digital interface • DATA → GPIO27 • supply 3.3 V'),
        ('KY-018', 'Relative light', 'Analog LDR module • S → divider → GPIO34 • supply 3.3 V')]:
        cards += f'''<article><img class="photo" alt="Joy-IT {code} sensor reference photograph" src="{data_url(code + '.png')}"><div class="body"><span class="tag">{code}</span><h2>{name}</h2><p>{detail}</p><p class="muted">Reference photograph, not a photograph of your individual module.</p><a href="https://sensorkit.joy-it.net/en/sensors/{code.lower()}">Joy-IT photo, specifications &amp; pinout ↗</a></div></article>'''
    diagrams = ''.join(f'<figure><img alt="{title}" src="{data_url(name)}"><figcaption>{title}</figcaption></figure>' for name, title in [
        ('wiring-overview.svg', '01 — Wiring overview (logical connection drawing)'),
        ('wiring-details.svg', '02 — Light divider, optional filter, and required DHT pull-up')])
    pinouts = ''.join(f'<article><div class="body"><h2>{code} manufacturer pinout</h2><img class="pinout" alt="Joy-IT {code} manufacturer pinout" src="{data_url(code + "-pinout.svg")}"></div></article>' for code in ['KY-015', 'KY-018'])
    content = f'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MeasurementESP — Hardware &amp; wiring</title>
<style>:root{{color-scheme:dark}}*{{box-sizing:border-box}}body{{margin:0;background:#0B1118;color:#E6EDF3;font:16px/1.6 Arial,sans-serif}}main{{max-width:1264px;margin:auto;padding:48px 28px}}h1{{font-size:44px;line-height:1.15;margin:12px 0}}h2{{font-size:24px;line-height:1.3}}a{{color:#00C2FF}}.tag{{color:#F4B740;font-weight:bold;letter-spacing:1px}}.lead{{max-width:850px;font-size:20px}}.muted,figcaption{{color:#A0AEC0}}.grid{{display:grid;grid-template-columns:1fr 1fr;gap:24px;margin:28px 0}}article,aside{{background:#16232D;border:1px solid #24333E;border-radius:8px;overflow:hidden}}.body,aside{{padding:24px}}.photo{{width:100%;height:280px;object-fit:contain;background:#fff;padding:18px}}.pinout{{width:100%;height:320px;object-fit:contain;background:#0B1118}}figure{{margin:32px 0}}figure img{{width:100%;height:auto;border:1px solid #24333E;border-radius:8px}}table{{width:100%;border-collapse:collapse}}td,th{{text-align:left;padding:12px;border-bottom:1px solid #24333E}}footer{{margin-top:40px;border-top:1px solid #24333E;padding-top:24px}}@media(max-width:760px){{.grid{{grid-template-columns:1fr}}h1{{font-size:34px}}main{{padding:24px 18px}}}}</style>
<main><span class="tag">AL SAYAD / MEASUREMENTESP</span><h1>Know the parts.<br>Connect by pin name.</h1><p class="lead">Reference hardware: ESP32-DevKitC V4 with ESP32-WROOM-32E, KY-015 / DHT11, and KY-018 / LDR.</p><aside><strong>Fit R3: your DHT11 module has no built-in DATA pull-up.</strong> Drivers are implemented; physical sensor acceptance is still pending. R3 is 10k between DATA/GPIO27 and 3V3. The light-filter capacitor C1 is optional and may be omitted; keep both divider resistors.</aside>
<div class="grid">{cards}</div><h2>Connection schedule</h2><table><thead><tr><th>Sender</th><th>Destination</th></tr></thead><tbody><tr><td>3V3</td><td>Both modules + / VCC</td></tr><tr><td>GND</td><td>Both modules − / GND; divider and capacitor ground</td></tr><tr><td>GPIO27</td><td>KY-015 S / DATA; required external R3 10k pull-up to 3V3</td></tr><tr><td>GPIO34 / ADC1_CH6</td><td>KY-018 S through R1/R2 divider; optional C1 to GND</td></tr></tbody></table>
{diagrams}<h2>Check the actual module markings</h2><p>These manufacturer drawings identify the reference modules. Header orientation and board revisions vary. S / + / − labels are authoritative, not the apparent left-to-right order of the overview.</p><div class="grid">{pinouts}</div>
<aside>Power the sender by USB. Use its 3V3 output for both modules; keep signal pull-ups at 3.3 V. GPIO27 is digital DHT data; GPIO34 is an ADC1 input. SmartDisplay communicates over Wi-Fi and has its own power supply.</aside>
<footer><p>Based on local <code>35_sensor_photos_inline.html</code>, cards 26 and 27. Real sensor photos and pinout illustrations: Joy-IT / SensorKit, linked above. Original logical drawings: AlSayad project documentation. All images are embedded for offline viewing.</p><p><a href="https://documentation.espressif.com/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html">Espressif board and header reference</a></p></footer></main></html>'''
    (OUT / 'MeasurementESP_Hardware.html').write_text(content, encoding='utf-8')


if __name__ == '__main__':
    build_overview()
    build_details()
    build_html()
    print(OUT / 'MeasurementESP_Hardware.html')
