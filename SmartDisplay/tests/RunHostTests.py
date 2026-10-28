"""Compile and run the real portable C modules with Zig's native C compiler.

Usage: python tests/RunHostTests.py --zig build/host-tools/ziglang/zig.exe
Run idf.py reconfigure once to resolve the pinned cJSON dependency first.
"""
import argparse
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--zig', type=Path, required=True)
parser.add_argument('--library-root', type=Path,
                    default=Path('D:/GitHub/AlSayad/AlSayadLib/ESP32'))
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
library = args.library_root.resolve()
output = project / 'build/tests'
output.mkdir(exist_ok=True)
executable = output / 'AsCoreTests.exe'
includes = [project / 'tests/stubs', library / 'AsMeasurements',
            library / 'AsMeasurementJson', library / 'AsHistoryStorage',
            library / 'AsTransport', library / 'AsWifiTransport',
            project / 'managed_components/espressif__cjson/cJSON']
sources = [project / 'tests/AsCoreTests.c', library / 'AsMeasurements/AsMeasurements.c',
           project / 'tests/AsTransportTests.c', library / 'AsWifiTransport/AsWifiTransport.c',
           library / 'AsMeasurementJson/AsMeasurementJson.c',
           library / 'AsHistoryStorage/AsHistoryFiles.c',
           project / 'managed_components/espressif__cjson/cJSON/cJSON.c']
command = [str(args.zig.resolve()), 'cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
           '-DCJSON_NESTING_LIMIT=16', '-D_POSIX_C_SOURCE=200809L',
           '-include', str(project / 'tests/stubs/AsHostPlatform.h')]
command += ['-I' + str(path) for path in includes]
command += [str(path) for path in sources] + ['-o', str(executable)]
subprocess.run(command, check=True)
with tempfile.TemporaryDirectory(prefix='as-room-tests-') as directory:
    subprocess.run([str(executable), directory], check=True)
