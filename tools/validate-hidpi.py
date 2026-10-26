#!/usr/bin/env python3
"""Check native buffer density, resize, frame rate, and logical input on X399."""
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import time

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('vnc', os.environ.get('AIRTOP_VNC_CLIENT', '/mnt/HaikuWork/x399/tools/vnc.py'))
vnc = importlib.util.module_from_spec(spec); spec.loader.exec_module(vnc)
def remote(command):
    return subprocess.check_output([str(root / 'tools/ws.sh'), command], text=True, timeout=20)
def state():
    result = {}
    for key, kind, value in re.findall(r'"([^\"]+)" \(B_(BOOL|INT32|INT64|DOUBLE|STRING)_TYPE\) : ([^\n]*)',
                                     remote('hey AirTop get MonitorState of Window 0')):
        result[key] = value == 'TRUE' if kind == 'BOOL' else json.loads(value) if kind == 'STRING' else float(value) if kind == 'DOUBLE' else int(value.split()[0])
    return result
def input_event(click=None, text=None, key=None):
    client = vnc.VNC(password=os.environ.get('AIRTOP_VNC_PASSWORD', vnc.PASSWORD))
    try:
        if click: client.click(*click)
        if text: client.type_text(text)
        if key: client.key(vnc.keysym(key))
        time.sleep(.3)
    finally:
        client.close()
    time.sleep(.3)
def check_density(s):
    assert s['render_scale'] == 2 and s['buffered']
    assert s['buffer_width'] == s['view_width'] * 2
    assert s['buffer_height'] == s['view_height'] * 2

original = re.search(r'BRect\(([^)]+)\)', remote('hey AirTop get Frame of Window 0')).group(1)
left, top, right, bottom = map(float, original.split(','))
remote('hey AirTop set Active of Window 0 to true'); time.sleep(.3)
before = state(); check_density(before)
assert not before['paused'] and before['filter'] == ''
try:
    remote(f'hey AirTop set Frame of Window 0 to "BRect({left},{top},{right + 73},{bottom + 41})"')
    time.sleep(.4); resized = state(); check_density(resized)
    assert resized['buffer_width'] == before['buffer_width'] + 146
    assert resized['buffer_height'] == before['buffer_height'] + 82
finally:
    remote(f'hey AirTop set Frame of Window 0 to "BRect({original})"')
time.sleep(.4); check_density(state())
start = state(); started = time.monotonic(); time.sleep(2.2); end = state()
fps = (end['frames'] - start['frames']) / (time.monotonic() - started)
assert 24 <= fps <= 34, fps
try:
    # Mouse coordinates stay logical when the backing bitmap doubles.
    input_event(click=(round(right - 166), round(top + 58)))
    paused = state(); assert paused['paused']
    time.sleep(.6)
    assert state()['frames'] == paused['frames']
finally:
    if state()['paused']: input_event(key='Space')
try:
    input_event(text='/AirTop')
    assert state()['filter'] == 'AirTop' and state()['rows'] >= 1
finally:
    input_event(key='Escape')
assert not state()['paused'] and state()['filter'] == ''
result = dict(result='PASS', fps=fps, before=before, resized=resized, after=state(),
    checks=['2x native buffer', 'inclusive pixel dimensions', 'buffer recreated on resize',
            '30fps animation', 'logical mouse coordinates', 'pause freezes drawing', 'filter input'])
(root / 'evidence/hidpi/ui-validation.json').write_text(json.dumps(result, indent=2) + '\n')
print(f'HiDPI: 2x buffer, resize, input, pause, filter passed; {fps:.1f} frames/s')
