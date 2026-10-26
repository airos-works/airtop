#!/usr/bin/env python3
"""Exercise the running X399 app through VNC, check its read-only scripting state."""
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import time
root = Path(__file__).resolve().parents[1]
client_path = os.environ.get('AIRTOP_VNC_CLIENT', '/mnt/HaikuWork/x399/tools/vnc.py')
spec = importlib.util.spec_from_file_location('vnc', client_path)
vnc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vnc)
ws = [str(root / 'tools/ws.sh')]
checks = []
def remote(command):
    return subprocess.check_output(ws + [command], text=True, timeout=20)
def state():
    text = remote('hey AirTop get MonitorState of Window 0')
    result = {}
    for key, kind, value in re.findall(r'"([^\"]+)" \(B_(BOOL|INT32|INT64|STRING)_TYPE\) : ([^\n]*)', text):
        result[key] = value == 'TRUE' if kind == 'BOOL' else json.loads(value) if kind == 'STRING' else int(value.split()[0])
    return result

def press(*keys, text=None, click=None):
    v = vnc.VNC(password=os.environ.get('AIRTOP_VNC_PASSWORD', vnc.PASSWORD))
    try:
        if click: v.click(*click)
        if text is not None: v.type_text(text)
        for key in keys: v.key(vnc.keysym(key))
        time.sleep(.3)
    finally: v.close()
    time.sleep(.3)

def check(name, condition):
    assert condition, name
    checks.append(name)
    print('PASS', name, flush=True)

def shot(name):
    remote(f'screenshot -s -w -b /tmp/AirTop-{name}.png; test -s /tmp/AirTop-{name}.png')
    subprocess.run(['scp', '-q', '-F', os.environ.get('AIRTOP_SSH_CONFIG', '/mnt/HaikuWork/x399/ssh/config'),
                    os.environ.get('AIRTOP_HOST', 'ws-haiku') + f':/tmp/AirTop-{name}.png',
                    str(root / 'evidence' / (name + '.png'))], check=True)
    from PIL import Image
    path = root / 'evidence' / (name + '.png')
    image = Image.open(path)
    if image.width > 2400:
        image.crop((75, 35, 1306, 1020)).save(path)

remote('hey AirTop set Active of Window 0 to true')
time.sleep(.5)
if state()['paused']: press('Space')
press('Escape')
press(text='/AirTop')
s = state()
check('filter matches AirTop', s['filter'] == 'AirTop' and s['rows'] >= 1 and s['rows'] < 5)
shot('filter')
press('Escape')
check('escape clears filter', state()['filter'] == '')
press('m')
s = state()
check('memory sort descending', s['sort'] == 1 and s['descending'])
shot('sort-memory')
press('m')
check('repeat sort reverses order', not state()['descending'])
press('c')
press('Space')
s = state();time.sleep(1.6)
check('pause holds sample sequence', s['paused'] and state()['sequence'] == s['sequence'])
check('pause freezes animation', state()['graph_time'] == s['graph_time'] and state()['frames'] == s['frames'])
shot('paused')
press('Space');time.sleep(1.2)
check('resume advances sample sequence', not state()['paused'] and state()['sequence'] > s['sequence'])
press('End')
s = state()
check('end selects and scrolls processes', s['selected'] >= 1 and s['offset'] > 0)
press('Home', 'Return')
# An alert is a second app window, with a native close button.
check('enter opens process details', '"Process details"' in remote('hey AirTop get Title of Window 1'))
shot('details')
press('Escape')
check('escape closes process details', '1 (0x00000001)' in remote('hey AirTop count Window'))
press('F1')
check('F1 opens help', '"AirTop"' in remote('hey AirTop get Title of Window 1'))
shot('help')
press('Escape')
check('escape closes help', '1 (0x00000001)' in remote('hey AirTop count Window'))
before = state()['interval'];press('+')
check('sampling interval changes', state()['interval'] != before)
while state()['interval'] != 250000: press('+')
time.sleep(1)
a = state(); started = time.monotonic(); time.sleep(2.2); b = state(); elapsed = time.monotonic() - started
fps = (b['frames'] - a['frames']) / elapsed
check('buffered drawing near 30 frames per second', b['buffered'] and 24 <= fps <= 34)
check('quarter-second live measurements', 3 <= (b['sequence'] - a['sequence']) / elapsed <= 5)
check('graph scrolls continuously', b['graph_time'] > a['graph_time'])
print(f'Animation: {fps:.1f} frames/s', flush=True)
# Resize through Haiku's standard window scripting, then capture actual drawing.
remote('hey AirTop set Frame of Window 0 to "BRect(80,60,1080,850)"')
time.sleep(.5);shot('minimum')
check('minimum resize matches requested bounds', 'BRect(80.0, 60.0, 1080.0, 850.0)' in remote('hey AirTop get Frame of Window 0'))
check('minimum resize keeps app alive', state()['rows'] > 0)
remote('hey AirTop set Frame of Window 0 to "BRect(80,60,1300,1010)"')
time.sleep(.5)
press(click=(1100,120))
check('pause button works', state()['paused'])
press(click=(1100,120))
check('resume button works', not state()['paused'])
# Network panel is at (16,342)..(787,506), in a view offset by its menu bar.
press(click=(400,530))
check('network click cycles interface', state()['network_index'] == 1)
press(click=(400,530))
press(click=(1100,760,5))
check('wheel scrolls to remaining volumes', state()['disk_offset'] > 0)
shot('volumes-scrolled')
press(click=(1100,760,4))
check('wheel returns to boot volume', state()['disk_offset'] == 0)
press(click=(735,695))
check('column click sorts memory', state()['sort'] == 1)
press('c')
shot('dashboard')
(root / 'evidence' / 'ui-validation.json').write_text(json.dumps(dict(checks=checks, result='PASS'), indent=2) + '\n')
