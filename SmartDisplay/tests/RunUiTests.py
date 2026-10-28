"""Build real LVGL and render/test the actual Room Monitor UI without hardware."""
import argparse
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--zig', type=Path, required=True)
parser.add_argument('--cmake', default='cmake')
parser.add_argument('--ninja', default='ninja')
parser.add_argument('--library-root', type=Path,
                    default=Path('D:/GitHub/AlSayad/AlSayadLib/ESP32'))
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
build = project / 'build/ui-tests'
captures = project / 'build/ui-preview'
captures.mkdir(exist_ok=True)
subprocess.run([args.cmake, '-S', str(project / 'tests'), '-B', str(build), '-G', 'Ninja',
                f'-DCMAKE_MAKE_PROGRAM={args.ninja}',
                f'-DCMAKE_C_COMPILER={args.zig.resolve()}', '-DCMAKE_C_COMPILER_ARG1=cc',
                f'-DAS_LIBRARY_ROOT={args.library_root.resolve()}'], check=True)
subprocess.run([args.cmake, '--build', str(build), '-j', '8'], check=True)
subprocess.run([str(build / 'AsUiTests.exe'), str(captures)], check=True)
