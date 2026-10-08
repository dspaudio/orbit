#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Host checks for ORBIT; vendor compiler/SDK is not required."""
import concurrent.futures
import os
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/host'
CASES = ['orbit_engines_test', 'irq_init_test', 'recovery_test', 'orbit_test', 'seq2_test', 'project_test', 'storage_test', 'drumkit_test', 'studio_drums_test',
         'punch_test', 'song_audio_test', 'song_ui_test']


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, '-c', 'import sys;sys.path.insert(0,"tools");import build;build.generate()'], cwd=ROOT, check=True)
    def run(name):
        binary = OUT / name
        compile_result = subprocess.run([os.environ.get('CC', 'cc'), '-O2', '-w', '-Ibuild/gen', '-Ifirmware/src', '-Ifirmware/hal',
                                        f'tests/{name}.c', '-o', str(binary), '-lm'], cwd=ROOT, capture_output=True, text=True)
        if compile_result.returncode:
            return name, compile_result.returncode, compile_result.stdout + compile_result.stderr
        extra = [str(OUT)] if name == 'orbit_test' else []
        result = subprocess.run([str(binary), *extra], cwd=ROOT, capture_output=True, text=True, timeout=120)
        return name, result.returncode, result.stdout + result.stderr
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        for name, code, log in pool.map(run, CASES):
            (OUT / f'{name}.log').write_text(log)
            print(f'{name}: {"PASS" if code == 0 else "FAIL"}', flush=True)
            if code:
                print(log)
            results.append((name, code))
    summary = '\n'.join(f'{name}: {"PASS" if code == 0 else "FAIL"}' for name, code in results) + '\n'
    (OUT / 'orbit-checks.txt').write_text(summary)
    return int(any(code for _, code in results))


if __name__ == '__main__':
    raise SystemExit(main())
