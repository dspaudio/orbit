#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify the product core against the existing native runner and 729 strict oracle records.

python3 assets/op1-cluster/verify.py EVIDENCE_ROOT
Original evidence is read-only; all results go to build/host/cluster-product.
"""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from pathlib import Path
from typing import Final, TypedDict

ROOT: Final = Path(__file__).resolve().parents[2]
BUILD: Final = ROOT / "build/host/cluster-product"


class Case(TypedDict):
    name: str
    equal: bool
    exit_codes: list[int]
    mismatch: list[str]
    reference_sha256: str
    native_sha256: str


class Matrix(TypedDict):
    failed: int
    render_blocks: int
    pcm_samples: int
    return_words: int
    voice_bytes: int
    global_bytes: int
    table_values: int
    cases: list[Case]


def run(command: list[str], data: bytes | None = None) -> subprocess.CompletedProcess[bytes]:
    """Verify the exact child exit code and output with a bounded runtime."""
    result = subprocess.run(command, input=data, capture_output=True, check=True, timeout=120)
    print(f"exit={result.returncode} {' '.join(command)}")
    return result


def main(evidence: Path) -> None:
    """Compare every snapshot from the oracle-aligned and flash-constant runners."""
    native = evidence / "recovery/native-cluster"
    matrix: Matrix = json.loads((native / "comparison.json").read_bytes())
    assert matrix["failed"] == 0 and len(matrix["cases"]) == 729
    assert matrix["render_blocks"] == matrix["return_words"] == 5103
    BUILD.mkdir(parents=True, exist_ok=True)
    generated = BUILD / "felucca_op1_cluster.h"
    run([sys.executable, str(ROOT / "tools/gen_op1_cluster.py"), str(generated)])
    repeated = BUILD / "repeated.h"
    run([sys.executable, str(ROOT / "tools/gen_op1_cluster.py"), str(repeated)])
    assert generated.read_bytes() == repeated.read_bytes()

    runner = (native / "native_runner.c").read_text(encoding="utf-8")
    common = [
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-DNATIVE_CLUSTER_H", "-include", str(ROOT / "firmware/src/op1_cluster_core.h"),
        "-I", str(native), "-I", str(BUILD), "-I", str(ROOT / "firmware/src"),
        str(ROOT / "firmware/src/op1_cluster_core.c"),
    ]
    # Build the original runner unchanged; remove host table initialization only in the const version.
    constant_runner = (
        '#include "felucca_op1_cluster.h"\n'
        'static const char *const factory_name = OP1_CLUSTER_NAMES[0];\n'
        '#define t OP1_CLUSTER_TABLES\n'
        + runner.replace("    cls_tables t;", "")
        .replace("cls_tables_init(&t,w,c,f);", "(void)w; (void)c; (void)f;")
        .replace("int main(void) {", "int main(void) {\n    (void)factory_name;\n    (void)OP1_CLUSTER_KNOBS;")
    )
    executables = [BUILD / "native-product", BUILD / "constant-product", BUILD / "constant-ubsan"]
    run([*common, "-O2", str(native / "native_runner.c"), "-o", str(executables[0])])
    run([*common, "-O2", "-x", "c", "-", "-o", str(executables[1])], constant_runner.encode())
    run([
        *common, "-O1", "-g", "-fsanitize=undefined", "-fno-sanitize-recover=all",
        "-x", "c", "-", "-o", str(executables[2]),
    ], constant_runner.encode())
    records = []
    header_hash = ""
    for case in matrix["cases"]:
        assert case["equal"] and not case["mismatch"]
        assert len(case["exit_codes"]) == 5 and all(code == 0 for code in case["exit_codes"])
        assert case["reference_sha256"] == case["native_sha256"]
        prefix = native / "results" / case["name"]
        fixture = prefix.with_suffix(".input").read_bytes()
        expected = prefix.with_suffix(".native.bin").read_bytes()
        assert hashlib.sha256(expected).hexdigest() == case["reference_sha256"]
        # Independently reconstruct the serialization from original oracle text.
        oracle = prefix.with_suffix(".oracle.txt").read_text(encoding="utf-8")
        assert "oracle_mode=strict" in oracle and "frac_special_events=0" in oracle
        dumps: dict[tuple[int, int], bytes] = {}
        returns: dict[int, int] = {}
        for line in oracle.splitlines():
            fields = line.split()
            if len(fields) == 4 and fields[1] == "mem":
                dumps[(int(fields[0][5:]), int(fields[2], 16))] = bytes.fromhex(fields[3].split(":")[1])
            if len(fields) == 5 and fields[2] == "status=returned":
                returns[int(fields[0][5:])] = int(fields[4][3:], 16)
        original = bytearray()
        for step, address in ((0, 0x0101E5E8), (2, 0x01BAA91C), (1, 0x0106A954), (2, 0x01BAA918)):
            original.extend(dumps[(step, address)])
        ops = [1, 2, 3, 3, 3, 4, 3, 3, 5, 3, 2, 3]
        for index, op in enumerate(ops):
            for address in (0xFFB00000, 0x01BAA92C, 0xFF802BC8, 0x30001000):
                original.extend(dumps[(index + 3, address)])
            original.extend((returns[index + 3] if op == 3 else 0).to_bytes(4, "little"))
        assert original == expected, case["name"]
        codes = []
        for executable in executables:
            result = subprocess.run(
                [str(executable)], input=fixture, capture_output=True, check=True, timeout=120,
            )
            assert result.stdout == original, (case["name"], executable.name)
            assert b"runtime error" not in result.stderr
            (BUILD / f"{case['name']}.{executable.name}.stderr").write_bytes(result.stderr)
            codes.append(result.returncode)
        header_hash = hashlib.sha256(original[:16500]).hexdigest()
        records.append({"name": case["name"], "exit_codes": codes, "equal": True})
    summary = {
        "cases": len(records), "failed": 0, "render_blocks": matrix["render_blocks"],
        "samples": matrix["pcm_samples"], "return_words": matrix["return_words"],
        "voice_bytes": matrix["voice_bytes"], "global_bytes": matrix["global_bytes"],
        "table_values": matrix["table_values"], "table_serialization_sha256": header_hash,
        "generated_sha256": hashlib.sha256(generated.read_bytes()).hexdigest(),
        "generator_reproducible": True, "runs": records,
    }
    (BUILD / "verification.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in summary.items() if key != "runs"}))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: python3 assets/op1-cluster/verify.py EVIDENCE_ROOT")
    main(Path(sys.argv[1]).resolve())
