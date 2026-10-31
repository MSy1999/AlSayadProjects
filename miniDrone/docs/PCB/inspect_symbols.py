from pathlib import Path
import re

BASE = Path(r'C:\Program Files\KiCad\10.0\share\kicad\symbols')

def extract(text, name):
    start = text.index('(symbol "' + name + '"')
    depth = 0
    quoted = escaped = False
    for i in range(start, len(text)):
        c = text[i]
        if quoted:
            if escaped:
                escaped = False
            elif c == '\\':
                escaped = True
            elif c == '"':
                quoted = False
        elif c == '"':
            quoted = True
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise ValueError(name)

if __name__ == '__main__':
    for lib, name in [('Device', 'R'), ('Device', 'C'), ('Device', 'C_Polarized'),
                      ('Device', 'D_Schottky'), ('Transistor_BJT', 'BC337'),
                      ('Connector_Generic', 'Conn_01x02'), ('Jumper', 'Jumper_2_Open')]:
        text = (BASE / (lib + '.kicad_sym')).read_text(encoding='utf-8')
        block = extract(text, name)
        print(lib, name)
        print(block[:220])
        print('\n'.join(re.findall(r'\(pin [\s\S]*?\(number "[^"]+"', block)))
