#!/usr/bin/env python3
"""Generate AirTop's original HVIF/SVG monitor and pulse icon (no dependencies)."""
from pathlib import Path
root = Path(__file__).resolve().parents[1] / 'resources'
colors = ['25384c', '8199b0', '101c2b', '41d5e0', 'b89df4', '85db82']
shapes = [
 (0, [(5,7),(54,7),(60,13),(60,48),(36,48),(36,54),(46,54),(46,59),(19,59),(19,54),(29,54),(29,48),(5,48)]),
 (1, [(7,9),(53,9),(57,13),(57,44),(7,44)]),
 (2, [(10,12),(54,12),(54,40),(10,40)]),
 (0, [(10,42),(54,42),(54,45),(10,45)]),
 (4, [(13,33),(18,33),(18,38),(13,38)]),
 (4, [(21,29),(26,29),(26,38),(21,38)]),
 (4, [(29,24),(34,24),(34,38),(29,38)]),
 (3, [(10,23),(20,23),(24,15),(29,28),(35,18),(40,24),(54,24),(54,27),(39,27),(35,23),(28,35),(24,24),(22,26),(10,26)]),
 (5, [(50,42),(53,42),(53,44),(50,44)]),
]
data = bytearray(b'ncif')
data.append(len(colors))
for color in colors: data.extend(bytes([3]) + bytes.fromhex(color))
data.append(len(shapes))
for _, points in shapes:
 data.extend([10, len(points)]) # closed, straight paths
 for x, y in points: data.extend([x + 32, y + 32])
data.append(len(shapes))
for i, (style, _) in enumerate(shapes): data.extend([10, style, 1, i, 0])
(root / 'AirTop.hvif').write_bytes(data)
svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">\n'
for style, points in shapes:
 svg += '<polygon fill="#' + colors[style] + '" points="' + ' '.join(f'{x},{y}' for x,y in points) + '"/>\n'
(root / 'AirTop.svg').write_text(svg + '</svg>\n')
print(f'Wrote {len(data)}-byte native icon and SVG source.')
