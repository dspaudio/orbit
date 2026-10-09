#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the native firmware preview: python tools/orbit_preview.py [--port 8080]."""
import argparse
import base64
import io
import json
import os
from pathlib import Path
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/host/preview'
BUTTONS = {'FX': 0, 'SCL': 1, 'ENV': 2, 'LFO': 3, 'EDIT': 4, 'GLO': 5,
           'HOME': 6, 'SAVE': 7, 'ARP': 8, 'SEQ': 9, 'PLAY': 10, 'REC': 11,
           'OCT-': 12, 'OCT+': 13}


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, '-c', 'import sys;sys.path.insert(0,"tools");import build;build.generate()'], cwd=ROOT, check=True)
    subprocess.run([os.environ.get('CC', 'cc'), '-O2', '-w', '-Ibuild/gen', '-Ifirmware/src', '-Ifirmware/hal',
                    'tools/orbit_host.c', '-o', str(OUT / 'orbit_host'), '-lm'], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8080)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--no-build', action='store_true')
    args = parser.parse_args()
    if not args.no_build:
        build()
    process = subprocess.Popen([str(OUT / 'orbit_host'), str(OUT)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path in ('/', '/index.html'):
                body = (ROOT / 'preview/index.html').read_bytes()
                self.send_response(200); self.send_header('Content-Type', 'text/html; charset=utf-8')
                self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body)
            else:
                self.send_error(404)

        def do_POST(self):
            if self.path != '/api':
                self.send_error(404); return
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 0 < length <= 4096:
                    raise ValueError('invalid body size')
                data = json.loads(self.rfile.read(length))
                if not isinstance(data, dict):
                    raise ValueError('expected an operation object')
                op = data.get('op', 'render')
                if op == 'button':
                    command = f"button {BUTTONS[data['button']]}"
                elif op == 'knob':
                    role = int(data['role']); delta = int(data['delta'])
                    if not 0 <= role < 7 or not -64 <= delta <= 64:
                        raise ValueError('invalid knob')
                    command = f'knob {role} {delta}'
                elif op == 'render':
                    notes = int(data.get('notes', 0))
                    if not 0 <= notes <= 0xFFFFFF:
                        raise ValueError('invalid notes')
                    command = f'render 11008 {notes}'
                else:
                    raise ValueError('invalid operation')
                process.stdin.write(command + '\n'); process.stdin.flush()
                if process.stdout.readline().strip() != 'OK':
                    raise RuntimeError('native preview stopped')
                image = Image.open(OUT / 'screen.ppm')
                png = io.BytesIO(); image.save(png, format='PNG')
                result = {'screen': base64.b64encode(png.getvalue()).decode()}
                if op == 'render':
                    result['audio'] = base64.b64encode((OUT / 'chunk.wav').read_bytes()).decode()
                body = json.dumps(result).encode()
                self.send_response(200); self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(body))); self.send_header('Cache-Control', 'no-store')
                self.end_headers(); self.wfile.write(body)
            except (ValueError, KeyError, TypeError) as error:
                self.send_error(400, str(error))
            except (RuntimeError, BrokenPipeError) as error:
                self.send_error(503, str(error))

    server = HTTPServer((args.host, args.port), Handler)
    print(f'ORBIT firmware preview: http://{args.host}:{args.port}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close(); process.terminate(); process.wait(timeout=5)


if __name__ == '__main__':
    main()
