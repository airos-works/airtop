#!/usr/bin/env python3
"""Verify frame interpolation and suspended animation on the running native app."""
import json
from pathlib import Path
import re
import subprocess
import time

root = Path(__file__).resolve().parents[1]
def remote(command):
    return subprocess.check_output([str(root / 'tools/ws.sh'), command], text=True, timeout=20)
def state():
    result = {}
    for key, kind, value in re.findall(r'"([^\"]+)" \(B_(BOOL|INT32|INT64|DOUBLE)_TYPE\) : ([^\n]*)',
                                     remote('hey AirTop get MonitorState of Window 0')):
        result[key] = value == 'TRUE' if kind == 'BOOL' else float(value) if kind == 'DOUBLE' else int(value.split()[0])
    return result

observations = []
for _ in range(24):
    observations.append(state())
    time.sleep(.055)
pairs = list(zip(observations, observations[1:]))
assert all(b['graph_time'] >= a['graph_time'] for a, b in pairs)
assert any(a['sequence'] == b['sequence'] and b['graph_time'] > a['graph_time'] for a, b in pairs)
assert any(a['sequence'] == b['sequence'] and abs(b['cpu_display'] - a['cpu_display']) > .001 for a, b in pairs)
assert observations[-1]['buffered'] and observations[-1]['interval'] == 250000
try:
    remote('hey AirTop set Minimize of Window 0 to true')
    time.sleep(.4)
    before = state(); time.sleep(1.2); after = state()
    assert before['frames'] == after['frames'] and before['graph_time'] == after['graph_time']
finally:
    remote('hey AirTop set Minimize of Window 0 to false')
    remote('hey AirTop set Active of Window 0 to true')
time.sleep(.4)
assert state()['graph_time'] > after['graph_time']
(root / 'evidence/smoothing/animation-validation.json').write_text(json.dumps({
    'result': 'PASS', 'checks': ['monotonic graph cursor', 'graph moves between samples',
    'CPU display interpolates between samples', 'buffered 250ms sampling',
    'minimized window stops animation', 'restored window resumes animation'],
    'observations': observations}, indent=2) + '\n')
print('Animation: interpolation, buffering, minimize, and restore passed')
