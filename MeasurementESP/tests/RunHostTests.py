"""Build the real shared C modules with a native Zig compiler; run after idf.py build."""
import argparse
import subprocess
from pathlib import Path
parser = argparse.ArgumentParser()
parser.add_argument('--zig', type=Path, required=True)
parser.add_argument('--library-root', type=Path, default=Path('D:/GitHub/AlSayad/AlSayadLib/ESP32'))
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
root = args.library_root.resolve()
modules = ['AsMeasurements', 'AsMeasurementJson', 'AsMeasurementSensors', 'AsMeasurementSender', 'AsMeasurementHttp']
cjson = project / 'managed_components/espressif__cjson/cJSON'
output = project / 'build/tests/AsSenderTests.exe'
output.parent.mkdir(parents=True, exist_ok=True)
command = [str(args.zig.resolve()), 'cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
           '-DCJSON_NESTING_LIMIT=16', '-I' + str(project / 'tests/stubs'), '-I' + str(cjson)]
command += ['-I' + str(root / module) for module in modules]
command += [str(project / 'tests/AsSenderTests.c'), str(project / 'tests/AsHttpTests.c')]
command += [str(root / module / (module + '.c')) for module in modules]
command += ['-I' + str(root / 'sensors/DHT11'), str(root / 'sensors/DHT11/AsDht11Decode.c')]
command += [str(project / 'tests/AsDht11Tests.c'), str(project / 'tests/AsHardwareTests.c')]
command += ['-I' + str(root / 'sensors/Light'), '-I' + str(root / 'TimeManager'),
            '-I' + str(project / 'main'), str(root / 'sensors/DHT11/AsDht11.c'),
            str(root / 'sensors/Light/AsAnalogLight.c'),
            str(project / 'main/AsMeasurementBoardConfig.c')]
command += [str(cjson / 'cJSON.c'), '-o', str(output)]
for gpio_capture in (False, True):
    for calibrated in (False, True):
        flags = (['-DAS_TEST_CALIBRATED'] if calibrated else [])
        flags += (['-DAS_TEST_GPIO_CAPTURE'] if gpio_capture else [])
        print(f'Checking GPIO capture={gpio_capture}, light calibrated={calibrated}', flush=True)
        subprocess.run(command + flags, check=True)
        subprocess.run([str(output)], check=True)
