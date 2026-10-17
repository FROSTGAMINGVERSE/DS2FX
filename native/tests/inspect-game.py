"""Read-only on-disk check of the reference signature; does not launch DS2."""
import argparse, hashlib, json, re, struct
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('executable', type=Path)
p.add_argument('report', type=Path)
args = p.parse_args()
data = args.executable.read_bytes()
digest = hashlib.sha256(data).hexdigest()
assert digest == '0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131'
pe = struct.unpack_from('<I', data, 0x3c)[0]
assert data[pe:pe+4] == b'PE\0\0'
machine, count = struct.unpack_from('<HH', data, pe+4)
optional_size = struct.unpack_from('<H', data, pe+20)[0]
sections = pe+24+optional_size
matches = []
pattern = re.compile(rb'\x48\x8b\x05....\x48\x8b\x58\x38\x48\x85\xdb\x74.\xf6', re.DOTALL)
for i in range(count):
    offset = sections+i*40
    virtual_size, rva, raw_size, raw = struct.unpack_from('<IIII', data, offset+8)
    flags = struct.unpack_from('<I', data, offset+36)[0]
    if not flags & 0x20000000:
        continue
    for match in pattern.finditer(data[raw:raw+raw_size]):
        instruction = rva+match.start()
        relative = struct.unpack_from('<i', data, raw+match.start()+3)[0]
        matches.append({'instruction_rva':hex(instruction),'base_slot_rva':hex(instruction+7+relative)})
assert machine == 0x8664 and len(matches) == 1, matches
report = {'executable':str(args.executable),'sha256':digest,'signature_matches':matches,
          'status':'static reference resolved; runtime pointer meaning and gameplay not yet verified'}
args.report.parent.mkdir(parents=True,exist_ok=True)
args.report.write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
