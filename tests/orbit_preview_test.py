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
    assert value['state'] == {'track': 0, 'view': 'tape', 'module': -1}
    wav=wave.open(io.BytesIO(base64.b64decode(value['audio'])))
    assert wav.getnchannels()==2 and wav.getframerate()==44100
    assert any(wav.readframes(wav.getnframes())), 'sequencer is silent'
    (ROOT/'build/host/preview-audio.wav').write_bytes(base64.b64decode(value['audio']))
    (ROOT/'build/host/preview-screen.png').write_bytes(base64.b64decode(value['screen']))
    state=api({'op':'button','button':'EDIT'})['state']
    assert state == {'track': 0, 'view': 'synth', 'module': 0}
    assert api({'op':'button','button':'ENV'})['state']['module'] == 1
    assert api({'op':'button','button':'HOME'})['state']['view'] == 'tape'
    assert api({'op':'button','button':'HOME'})['state']['view'] == 'visualizer'
    assert api({'op':'button','button':'HOME'})['state']['view'] == 'tape'
    assert api({'op':'button','button':'GLO'})['state']['view'] == 'mixer'
    assert api({'op':'knob','role':0,'delta':1})['state']['view'] == 'mixer'
    assert api({'op':'knob','role':1,'delta':1})['state']['track'] == 1
    assert api({'op':'knob','role':1,'delta':-1})['state']['track'] == 0
    api({'op':'button','button':'EDIT'})
    api({'op':'knob','role':3,'delta':1})
    api({'op':'render','notes':1}); api({'op':'render','notes':0})
    try:
        api({'op':'knob','role':99,'delta':1})
    except urllib.error.HTTPError as error:
        assert error.code==400
    else:
        raise AssertionError('bad knob accepted')
    for invalid in ([], None, 'render'):
        try:
            api(invalid)
        except urllib.error.HTTPError as error:
            assert error.code == 400
        else:
            raise AssertionError('non-object operation accepted')
    script=(ROOT/'preview/index.html').read_text().split('<script>')[1].split('</script>')[0]
    (ROOT/'build/host/preview-script.js').write_text(script)
    subprocess.run(['node','--check','build/host/preview-script.js'],cwd=ROOT,check=True)
    subprocess.run(['node', '--input-type=module', '-e', '''
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";
const nodes = new Map();
function element() {
  return { children: [], dataset: {}, attributes: new Map(), style: { setProperty() {} },
    setAttribute(name, value) { this.attributes.set(name, value); },
    getAttribute(name) { return this.attributes.get(name); },
    set innerHTML(value) { if (value.startsWith("<b>")) this.children = [element(), element()]; },
    get firstChild() { return this.children[0]; }, get lastChild() { return this.children.at(-1); },
    append(child) { this.children.push(child); },
    querySelectorAll() { return [element(), element()]; }, setPointerCapture() {} };
}
const document = {
  getElementById(id) { if (!nodes.has(id)) nodes.set(id, element()); return nodes.get(id); },
  createElement: element,
};
const requests = [], events = new Map();
const state = { track: 0, view: "tape", module: -1 };
let resolveRendered;
const rendered = new Promise(resolve => { resolveRendered = resolve; });
let context;
class AudioContext {
  currentTime = 0;
  async resume() {}
  async decodeAudioData() { return { duration: 0.25 }; }
  createBufferSource() {
    return { connect() {}, start() { vm.runInContext("running=false", context); resolveRendered(); } };
  }
}
context = vm.createContext({
  document, AudioContext, atob, Uint8Array, setTimeout,
  window: { addEventListener(name, handler) { events.set(name, handler); } },
  fetch: async (_, options) => {
    const request = JSON.parse(options.body);
    requests.push(request);
    if (request.op === "knob" && request.role === 1)
      state.track = Math.max(0, Math.min(3, state.track + Math.sign(request.delta)));
    if (request.op === "button") {
      if (request.button === "HOME") { state.view = state.view === "tape" ? "visualizer" : "tape"; state.module = -1; }
      else if (request.button === "GLO") { state.view = state.view === "tape" ? "mixer" : "other"; state.module = -1; }
      else if (["EDIT", "ENV", "FX", "LFO"].includes(request.button)) {
        state.view = state.track === 3 ? "drum" : "synth";
        state.module = ["EDIT", "ENV", "FX", "LFO"].indexOf(request.button);
      }
    }
    return { ok: true, json: async () => ({ screen: "", audio: "", state: { ...state } }) };
  },
});
vm.runInContext(readFileSync("build/host/preview-script.js", "utf8"), context);
assert(nodes.get("keys-more").children.some(button => button.textContent === "SCL"));
const mode = id => nodes.get("modes").children.find(button => button.dataset.id === id);
const homeBefore = requests.filter(request => request.op === "button" && request.button === "HOME").length;
await mode("tape").onclick();
await vm.runInContext("queue", context);
assert.equal(state.view, "tape");
assert.equal(requests.filter(request => request.op === "button" && request.button === "HOME").length, homeBefore);
const toDrum = mode("drum").onclick();
const toSynth = mode("synth").onclick();
await Promise.all([toDrum, toSynth]);
await vm.runInContext("queue", context);
assert.notEqual(state.track, 3);
assert.equal(state.view, "synth");
await mode("drum").onclick();
await vm.runInContext("queue", context);
assert.equal(state.track, 3);
assert.equal(state.view, "drum");
await mode("synth").onclick();
await vm.runInContext("queue", context);
assert.notEqual(state.track, 3);
assert.equal(state.view, "synth");
await mode("mixer").onclick();
await vm.runInContext("queue", context);
assert.equal(state.view, "mixer");
await mode("tape").onclick();
await vm.runInContext("queue", context);
assert.equal(state.view, "tape");
await mode("tape").onclick();
await vm.runInContext("queue", context);
assert.equal(state.view, "tape");
const key = nodes.get("keys").children[0];
key.onpointerdown({ preventDefault() {}, pointerId: 1 });
key.onpointerup();
assert.equal(vm.runInContext("notes", context), 0);
let timeout;
const bounded = Promise.race([rendered, new Promise((_, reject) => {
  timeout = setTimeout(() => reject(Error("render event missing")), 5000);
})]);
try {
  await nodes.get("audio").onclick();
  await bounded;
} finally {
  clearTimeout(timeout);
}
assert(requests.some(request => request.op === "render" && (request.notes & 1)));
assert.equal(vm.runInContext("pendingNotes", context), 0);
key.onpointerdown({ preventDefault() {}, pointerId: 1 });
events.get("blur")();
assert.equal(vm.runInContext("notes | pendingNotes", context), 0);
key.onpointerdown({ preventDefault() {}, pointerId: 1 });
key.onpointercancel();
assert.equal(vm.runInContext("notes | pendingNotes", context), 0);
console.log("Preview controls: PASS (SCL, short note tap, blur release, pointer cancel)");
'''], cwd=ROOT, check=True)
    scope_dir = ROOT / 'build/host/stereo-scope'
    scope_dir.mkdir(exist_ok=True)
    scope_source = ROOT / 'build/host/stereo-scope-test.c'
    scope_source.write_text(f'''#define main orbit_preview_main
#include "{ROOT / 'tools/orbit_host.c'}"
#undef main
int main(int argc, char **argv)
{{
    FILE *commands=tmpfile(), *saved=stdin;
    uint32_t k, nonzero=0, base;
    char *args[]={{"scope",argv[1],NULL}};
    assert(commands && argc==2);
    fprintf(commands,"button %u\\nrender 11024 1\\n",(unsigned)B_HOME);
    rewind(commands); stdin=commands;
    assert(orbit_preview_main(2,args)==0);
    stdin=saved; fclose(commands);
    base=scope_w-CTL/2u;
    for(k=0;k<CTL/2u;k++) {{
        uint32_t pos=(base+k)&(SCOPE_N-1u);
        uint32_t sample=k*2u+1u;
        assert(scope_buf[pos]==vis_tap[sample*2u]);
        assert(scope_bufr[pos]==vis_tap[sample*2u+1u]);
    }}
    for(k=0;k<SCOPE_N;k++) nonzero+=scope_bufr[k]!=0;
    assert(nonzero);
    puts("Native preview stereo scope: PASS (actual render command, left/right taps)");
    return 0;
}}
''')
    scope_binary = ROOT / 'build/host/stereo-scope-test'
    subprocess.run(['cc', '-O2', '-Ibuild/gen', '-Ifirmware/src', '-Ifirmware/hal',
                    str(scope_source), '-o', str(scope_binary), '-lm'], cwd=ROOT, check=True)
    subprocess.run([str(scope_binary), str(scope_dir)], cwd=ROOT, check=True)
    print('HTTP/native DSP preview: PASS (44100 Hz stereo, non-silent sequence, panel events, invalid input)')
finally:
    server.send_signal(signal.SIGINT)
    server.communicate(timeout=10)
