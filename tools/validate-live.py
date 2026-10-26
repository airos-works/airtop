#!/usr/bin/env python3
"""Run the bounded native workload against the real Haiku sampler."""
import json
from pathlib import Path
import subprocess
import time
root = Path(__file__).resolve().parents[1]
ws = [str(root / 'tools/ws.sh')]
evidence = root / 'evidence'
evidence.mkdir(exist_ok=True)
with (evidence / 'load-samples.jsonl').open('w') as output:
    collector = subprocess.Popen(ws + ['/boot/home/Develop/AirTop/build/AirTop --sample 15'], stdout=output)
    time.sleep(2)
    load = subprocess.Popen(ws + ['/boot/home/Develop/AirTop/build/AirTopWorkload'], stdout=subprocess.PIPE, text=True)
    print(load.communicate(timeout=20)[0].strip())
    assert load.returncode == 0
    assert collector.wait(timeout=20) == 0
samples = [json.loads(line) for line in (evidence / 'load-samples.jsonl').read_text().splitlines()]
work = [p for s in samples for p in s['processes'] if p['name'] == 'AirTopWorkload']
assert len(samples) == 15 and all(s['valid'] and s['cpu_valid'] for s in samples)
assert all(len(s['cores']) == 32 and all(0 <= c <= 100 for c in s['cores']) for s in samples)
assert max(p['cpu'] for p in work) > 70
assert max(p['resident'] for p in work) >= 64 * 1024 * 1024
assert all(0 <= s['memory_available'] <= s['memory_total'] for s in samples)
assert all(p['cpu'] < 150 for s in samples[2:] for p in s['processes'] if p['name'] == 'kernel_team')
assert not any(p['name'] == 'AirTopWorkload' for p in samples[-1]['processes'])
assert all(len(s['disks']) >= 4 and len(s['networks']) >= 2 for s in samples)
assert all(s['temperature_c'] is not None and 0 < s['temperature_c'] < 115 for s in samples)
assert all(s['sensor'] == 'AMD 1950X Tdie / SMN' for s in samples)
summary = dict(samples=len(samples), workload_peak_cpu=max(p['cpu'] for p in work),
               workload_resident=max(p['resident'] for p in work),
               cpu_peak=max(s['cpu_percent'] for s in samples),
               sample_ms_max=max(s['sample_ms'] for s in samples),
               kernel_cpu_peak=max(p['cpu'] for s in samples for p in s['processes'] if p['name']=='kernel_team'),
               temperature_min=min(s['temperature_c'] for s in samples),
               temperature_max=max(s['temperature_c'] for s in samples), result='PASS')
(evidence / 'load-validation.json').write_text(json.dumps(summary, indent=2) + '\n')
print(summary)
