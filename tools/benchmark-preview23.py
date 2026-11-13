"""Measure isolated renderer processes. Does NOT measure launcher-to-usable AO.

Never changes driver caches or launches a game. Output is confined to --output.
"""
from pathlib import Path
import argparse
import ctypes
from ctypes import wintypes as w
import hashlib
import json
import os
import re
import shutil
import subprocess
import time

class Memory(ctypes.Structure):
    _fields_=[('cb',w.DWORD),('faults',w.DWORD),('peak',ctypes.c_size_t),('working',ctypes.c_size_t),('peakPool',ctypes.c_size_t),('pool',ctypes.c_size_t),('peakNonPaged',ctypes.c_size_t),('nonPaged',ctypes.c_size_t),('pagefile',ctypes.c_size_t),('peakPagefile',ctypes.c_size_t)]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--skip-recovery',action='store_true',help='Reuse earlier corruption/mismatch receipts when only shaders changed')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    exe=(a.build/'ao-vk-cache-warm.exe').resolve();rows=[]
    cache=out/'final.cache'
    if cache.exists():raise RuntimeError('Choose a fresh output directory; previous evidence is preserved')
    def run(name,cache):
        start=time.perf_counter();env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(cache))
        with (out/(name+'.log')).open('w') as log:
            proc=subprocess.Popen([str(exe),'--cache',str(cache)],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT)
            proc.wait(timeout=600)
            mem=Memory();mem.cb=ctypes.sizeof(mem)
            peak=mem.peak if ctypes.windll.psapi.GetProcessMemoryInfo(w.HANDLE(int(proc._handle)),ctypes.byref(mem),mem.cb) else None
            created=w.FILETIME();exited=w.FILETIME();kernel=w.FILETIME();user=w.FILETIME()
            cpu=None
            if ctypes.windll.kernel32.GetProcessTimes(w.HANDLE(int(proc._handle)),ctypes.byref(created),ctypes.byref(exited),ctypes.byref(kernel),ctypes.byref(user)):
                cpu=((kernel.dwHighDateTime<<32|kernel.dwLowDateTime)+(user.dwHighDateTime<<32|user.dwLowDateTime))/1e7
        elapsed=time.perf_counter()-start;text=(out/(name+'.log')).read_text(errors='replace')
        assert proc.returncode==0,text[-2000:]
        m=re.search(r'startup:.*total (\d+) ms',text);first=re.search(r'startup-event: \+(\d+) ms first successful',text)
        rows.append(dict(name=name,process_elapsed_seconds=elapsed,renderer_init_ms=int(m[1]) if m else None,
            first_submitted_frame_ms=int(first[1]) if first else None,cpu_seconds=cpu,peak_working_set_bytes=peak,cache_bytes=cache.stat().st_size))
        (out/'RESULTS.json').write_text(json.dumps(dict(scope='Offline 320x240 renderer and eight HDR frames; not AO Play-to-window. GPU driver cache was NOT purged. Other desktop/game activity may interfere.',measurements=rows),indent=2)+'\n')
        print(rows[-1],flush=True)
    for name in ['cold-aovk-cache','warm-1','warm-2','warm-3']:run(name,cache)
    data=cache.read_bytes();assert data[:8]==b'AOVKPC23','Missing corruption envelope'
    if not a.skip_recovery:
        corrupted=out/'corrupted.cache';bad=bytearray(data);bad[-1]^=1;corrupted.write_bytes(bad);run('corrupt-recovery',corrupted)
        assert 'corrupt/incompatible envelope' in (out/'corrupt-recovery.log').read_text()
        mismatch=out/'driver-mismatch.cache';bad=bytearray(data);bad[24]^=1;mismatch.write_bytes(bad);run('driver-mismatch-recovery',mismatch)
        assert 'corrupt/incompatible envelope' in (out/'driver-mismatch-recovery.log').read_text()
    assert not list(out.glob('*.tmp')),'Atomic write debris'
    result=json.loads((out/'RESULTS.json').read_text());result.update(result='passed',executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),actual_full_client_launch_measured=False,actual_driver_cache_invalidation_tested=False,recovery_cases_tested=not a.skip_recovery)
    (out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')

if __name__=='__main__':main()
