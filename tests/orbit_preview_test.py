#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the HTTP bridge and actual native firmware audio in one process tree."""
import base64
import io
import json
from pathlib import Path
import signal
import socket
import subprocess
import sys
import urllib.error
import urllib.request
import wave
ROOT = Path(__file__).resolve().parents[1]
with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0))
    port = sock.getsockname()[1]
server = subprocess.Popen([sys.executable, 'tools/orbit_preview.py', '--no-build', '--port', str(port)], cwd=ROOT,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
try:
    assert 'ORBIT firmware preview:' in server.stdout.readline()
    base = f'http://127.0.0.1:{port}'
    assert 'ORBIT' in urllib.request.urlopen(base).read().decode()
    def api(data):
        request = urllib.request.Request(base+'/api',json.dumps(data).encode(),headers={'Content-Type':'application/json'})
        return json.load(urllib.request.urlopen(request))
    api({'op':'button','button':'PLAY'})
    value=api({'op':'render','notes':0})
    wav=wave.open(io.BytesIO(base64.b64decode(value['audio'])))
    assert wav.getnchannels()==2 and wav.getframerate()==44100
    assert any(wav.readframes(wav.getnframes())), 'sequencer is silent'
    (ROOT/'build/host/preview-audio.wav').write_bytes(base64.b64decode(value['audio']))
    (ROOT/'build/host/preview-screen.png').write_bytes(base64.b64decode(value['screen']))
    api({'op':'button','button':'EDIT'})
    api({'op':'knob','role':3,'delta':1})
    api({'op':'render','notes':1}); api({'op':'render','notes':0})
    try:
        api({'op':'knob','role':99,'delta':1})
    except urllib.error.HTTPError as error:
        assert error.code==400
    else:
        raise AssertionError('bad knob accepted')
    script=(ROOT/'preview/index.html').read_text().split('<script>')[1].split('</script>')[0]
    (ROOT/'build/host/preview-script.js').write_text(script)
    subprocess.run(['node','--check','build/host/preview-script.js'],cwd=ROOT,check=True)
    print('HTTP/native DSP preview: PASS (44100 Hz stereo, non-silent sequence, panel events, invalid input)')
finally:
    server.send_signal(signal.SIGINT)
    server.communicate(timeout=10)
