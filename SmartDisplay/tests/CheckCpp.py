"""Check the actual C++ facade/public headers using the configured ESP32 compiler."""
import json
import re
import subprocess
from pathlib import Path

project = Path(__file__).resolve().parents[1]
entries = json.loads((project / 'build/compile_commands.json').read_text())
entry = next(item for item in entries if item['file'].replace('\\', '/').endswith('/main/main.c'))
includes = re.findall(r'-I([^\s]+)', entry['command'])
compiler = re.search(r'(C:[^\s"]*xtensa-esp32-elf-gcc\.exe)', entry['command'])
if compiler is None:
    raise RuntimeError('Cannot locate ESP32 GCC in compile_commands.json')
executable = compiler.group(1).replace('\\\\', '\\').replace('-gcc.exe', '-g++.exe')
command = [executable, '@' + str(project / 'build/toolchain/cxxflags'),
           '-std=c++17', '-fsyntax-only', '-Wall', '-Wextra', '-Werror',
           '-DESP_PLATFORM', '-D_GNU_SOURCE', '-DLV_CONF_INCLUDE_SIMPLE']
command += ['-I' + path for path in includes]
command += [str(project / 'tests/AsCppSmoke.cpp')]
subprocess.run(command, check=True)
print('PASS: ESP32 C++17 facade and public shared headers')
