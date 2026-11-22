#!/usr/bin/env python3
"""fontbin.py src/font.s out.bin : the normal font (font_width + font_bits) as a binary image,
so that it can be restored from a cartridge bank after the small font has been loaded."""
import re, sys
t = open(sys.argv[1]).read()
w = [int(x) for l in re.findall(r'^\t\.byte ([0-9,]+)$', t.split('_font_bits:')[0].split('_font_width:')[1], re.M) for x in l.split(',')]
b = [int(x, 16) for l in re.findall(r'^\t\.byte (\$[0-9A-F,$]+)', t.split('_font_bits:')[1], re.M) for x in l.replace('$', '').split(',')]
assert len(w) == 158 and len(b) == 1264, (len(w), len(b))
open(sys.argv[2], 'wb').write(bytes(w) + bytes(b))
