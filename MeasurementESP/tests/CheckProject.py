"""Check resolved component registration and C11 flags after an IDF configure/build."""
import argparse
import json
import re
from pathlib import Path
parser = argparse.ArgumentParser()
parser.add_argument('--library-root', type=Path, default=Path('D:/GitHub/AlSayad/AlSayadLib/ESP32'))
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
root = args.library_root.resolve()
modules = ['AsMeasurements', 'AsMeasurementJson', 'AsMeasurementSensors',
           'AsMeasurementSender', 'AsWifiStation', 'AsMeasurementHttp', 'DHT11', 'Light', 'TimeManager']
description = json.loads((project / 'build/project_description.json').read_text())
components = description['build_components']
for module in modules:
    assert components.count(module) == 1, module
for module in ['as_display', 'AsHistoryStorage', 'AsSdHistory', 'AsWifiTransport',
               'WifiManager', 'MQTTManager', 'SDCardController', 'Device', 'StateMachine']:
    assert module not in components, module
commands = json.loads((project / 'build/compile_commands.json').read_text())
sources = list((project / 'main').glob('*.c'))
for module in modules:
    directory = {'DHT11': 'sensors/DHT11', 'Light': 'sensors/Light'}.get(module, module)
    sources += list((root / directory).glob('*.c'))
for source in sources:
    matched = [entry for entry in commands if Path(entry['file']).resolve() == source.resolve()]
    assert len(matched) == 1, source
    standards = re.findall(r'-std=([^ ]+)', matched[0]['command'])
    assert standards[-1] in ('c11', 'gnu11'), (source, standards)
    text = source.read_text()
    assert not re.search(r'\bgoto\s+[A-Za-z_]', text), source
    assert '@file ' + source.name in text, source
assert not list((project / 'libs').rglob('*.c'))
print('PASS: single component registrations, sender-only selection, C11 flags, source headers/no goto')
