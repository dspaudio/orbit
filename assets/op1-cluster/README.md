# Immutable data for the standalone OP-1 #246 cluster core

`firmware/src/op1_cluster_core.c/.h` is the verified native cluster C translation,
split out as a product unit. It keeps the public `cls_*` names, types, the 128-sample cadence,
integer saturation, wrap and ties-to-even, and the full R0 return value.
Only private helpers get the `cls_` prefix, to avoid unity-build collisions.
The ORBIT engine adapter, registry and pitch/ADSR/FX aren't part of this unit.

## Generation contract

```sh
python3 tools/gen_op1_cluster.py build/gen/felucca_op1_cluster.h
```

The caller creates the output directory. `main(path: str) -> None` in
`tools/gen_op1_cluster.py` can be registered in the existing build generation step or run
through the CLI above. Generation reads only `tables.json` and `factory.json` in this directory.
The official package, DB, `.omo/evidence` and the network aren't build dependencies.
The same input produces the same header regardless of output path or run time.
The SHA256 of both inputs is pinned in the generator to detect changes to the originals.

The generated header includes `op1_cluster_core.h` and declares:

- `static const cls_tables OP1_CLUSTER_TABLES`
- `static const int16_t OP1_CLUSTER_KNOBS[16][8]`
- `static const char OP1_CLUSTER_NAMES[16][13]`

Factory rows follow original SQLite IDs 65..80. The 8 signed16 values are
emitted as is, and names keep their case, spacing and spelling. For example, `sqeek bend`
and `streengs` aren't corrected. The address of each row of the 2D name array can be used in a C static
initializer.

The product passes `&OP1_CLUSTER_TABLES` directly. `cls_tables_init()` remains
for verifying the original matrix, but the product doesn't need to copy the 16,500 B table into RAM.
`firmware/app.ld` places `.rodata*` in XIP `.text`.
The generated header is included once in the same unity translation unit to avoid duplicating constants.

## Original source and extraction

The official URL is `https://teenage.engineering/_software/op-1/op1_246.op1`.
Addresses and SHA256 values for the package, TAR, analysis LDR, DB, original C translation and data are
recorded in `provenance.json`. The package SHA256 is
`c5315218f825f143b415ca554516541898abee843d3a236df0b54c04e1fb13a9`,
and the analysis LDR SHA256 is
`82a51d76ad19a23da485b1e4f007335a95d815731b096d21ef0642e8c8c0577d`.
These identify the analysis inputs; they aren't a claim that a manufacturer signature was verified.

The LDR's 801 headers were checked for magic/XOR/bounds, and IGNORE entries were skipped.
FILL repeats the argument's little-endian 4 bytes, and overlaps were applied in loader
order. The original tables are:

| Item | LDR source | runtime | Conversion |
|---|---|---|---|
| wave, 8193 entries | `0x018b6688`, 32772 B | `0x0101e5e8`, 16386 B | low16 of each u32 read as signed16, guard included |
| count, 8 entries | `0xff900524`, 32 B | `0x01baa91c`, 16 B | integer Q15 conversion of the binary32 bit pattern |
| curve, 24 entries | `0x018befb0`, 96 B | `0x0106a954`, 96 B | integer Q31 conversion of the binary32 bit pattern |
| master | initialized at `0x019608e0` | `0x01baa918`, 2 B | `0x4000` |

`tables.json` interprets the first 16,500 B of the native result without ABI padding:
8193 wave int16 → 8 count int16 → 24 curve int32 → master int16.
That serialization is identical across all 729 results and was cross-checked against the real LDR low16
and binary32 conversions. Its SHA256 is
`9ef611a57b95dcc8cdd732255bd804ae29da34f924a77b95fed0786908e9443f`.
No values are approximated with host floats or newly generated sines.

`raw_json` in `factory.json` preserves the mapping's original SQLite TEXT as is.
The DB SHA256 is `be4572298fe9dc83b106d5c4eaf1cc972280d97f028f3eaebc0c76dc68ac9cf0`.
The original full ADSR/FX/LFO, octave, synth_version and default texts are also kept.
The generator uses only name and knobs. The original loader stores knobs at
`0xff8013c4 + slot*16 + index*2`, and Cls reads them as signed words.
The product preset row selector and the shared ORBIT envelope/FX adaptation belong to the separate
adapter's contract, and aren't claimed to be identical to original full-patch playback.

## Core contract and verification

`cls_voice` is 108 B and `cls_global` is 40 B in the host ABI.
The caller picks the seed and keeps the call order of the shared global across voices.
Like the original, `cls_global_init()` doesn't change the seed.
`cls_construct()` preserves the original padding.
Safe inputs are `1 <= arithmetic(knobs[0] >>> 11) <= 8` and
`0 <= arithmetic(knobs[2] >>> 10) < 24`.
The phase input is 129 u32 words, of which the core reads 0..127.
`cls_render()` returns the original R0 as a full `uint32_t`, not a bool.

Where the existing original evidence is available, reverify with:

```sh
python3 assets/op1-cluster/verify.py .omo/evidence/op1-structure-20261010
```

The verification script links the existing `native_runner.c` to the product core.
A separate version of the same runner, with only its table initialization removed, references the generated const tables
directly, and a UBSan build of it also runs. It reads the existing 729 fixtures and the strict oracle
text and compares table/master, PCM, voice/global state and every render R0
byte for byte. It also checks the existing hashes of the oracle records and the native binary.
New results and executables are written only to `build/host/cluster-product`, and the original records aren't
overwritten. The original evidence is needed only for verification, not for the product build.

This finite matrix covers 5,103 blocks, 653,184 samples and 5,103 R0 words.
The strict oracle rejects `RND_MOD=1` and GNU/PRM fractional-special conflicts, and
the existing matrix has 0 special events. It doesn't prove behavior for arbitrary corrupted state or full silicon
equivalence. The original shared pitch/ADSR/FX and hardware
audio timing are outside the scope of recovery and verification.

The 2026-10-10 product verification exited 0. The product core linked to the original runner,
the direct const table version and the const UBSan version each ran 729 times, all
exit 0 and exactly matching the oracle serialization. The comparison covered voice 944,784 B,
global 314,928 B and 5,996,754 table/master values. Both generator runs also
exited 0, with an identical generated header SHA256 of
`e9ec63b6c7bde6ba086c9ec4c75e43fe0c5e2da30c44bf39407f93dc36ca6992`.
Per-case exit codes are in `build/host/cluster-product/verification.json`.

For the 0.6.0 release, generated comments and diagnostics were translated into English.
The final header is reproducible across two runs with SHA256
`299bb466d856e220b978ac848d83ff49212fa2ebd45bc2f44d0854299278b429`.
Only prose changed; the rebuilt firmware package is byte-identical to the installed package.

The C serialization of the raw knobs and original names and the static initializer check are:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror \
  -Ifirmware/src -Ibuild/host/cluster-product \
  assets/op1-cluster/data_probe.c -o build/host/cluster-product/data-probe
build/host/cluster-product/data-probe
```

Compile and run exited 0. The first 256 B of stdout are the little-endian serialization of the 128 signed16
knobs, and the next 208 B are the NUL-padded `[16][13]`
original names; both matched the original mapping exactly.
The standalone core compile with pi32v2 `-Os -Wall -Wextra -Werror` and the
`-DCLS_DATA_ONLY` probe compile also exited 0. In objdump, the 16,500 B table,
256 B of knobs and 208 B of names are all in `.rodata`, and the probe has no `.data/.bss`
allocation. The core's code sections total 3,092 B. This is a standalone object measurement,
not a measurement of RAM, CPU or real-device safety for the full firmware with the adapter linked.

## Rights

The new C translation, generator and verification scripts keep the original translation's SPDX
`GPL-3.0-only`. The waveforms, tables and factory JSON extracted from the official firmware are
original Teenage Engineering assets and **aren't relicensed as CC0 or as new GPL
assets**. The original rights remain, and the fact of extraction alone doesn't establish permission for separate
distribution. This data is separate from the existing `samples-cc0`.
