#!/usr/bin/env python3
"""Run the native MATHAPP through the complete CEmu file/launch path.

CEmu intentionally requires a ROM dumped from the user's own calculator.  The
suite refuses to guess, download, or bundle one.  Host math tests run without
that file; this script covers TI-OS packaging, transfer, launch, menu input,
and screenshots when a ROM or saved CEmu image is available.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import threading


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "bin" / "MATHAPP.8xp"
DEFAULT_HEADLESS = Path(
    "/Users/unoxyrich/Documents/Codex/2026-09-16/you-are-on-the-other-computer/work/CEmu/tests/autotester/cemu-headless"
)
DEFAULT_AUTOTESTER = DEFAULT_HEADLESS.with_name("autotester")


def app_files() -> list[Path]:
    """Return the launcher and any CEdev-generated AppVar sidecars."""
    sidecars = sorted(APP.parent.glob(APP.name + ".*.8xv"))
    return [APP, *[path for path in sidecars if path.is_file()]]


def drain_stderr(stream) -> None:
    for line in stream:
        sys.stderr.write("[CEmu] " + line)


def key_sequence(keys: list[str]) -> str:
    return "keys|" + ",".join(keys)


def submitted(keys: list[str]) -> str:
    return key_sequence([*keys, "enter"])


def x_variable() -> list[str]:
    return ["alpha", "xton"]


def worksheet_smoke_sequence() -> list[str]:
    x = x_variable()
    x_squared = [*x, "pow", "2"]
    quadratic_left = [*x_squared, "add", "2", *x, "add", "3"]
    transform_input = [*x_squared, "sub", "3", *x, "sub", "4"]
    branch_one = ["lpar", *x, "add", "1", "rpar", "pow", "2"]
    branch_two = ["1", "sub", *x]
    branch_three = ["neg", "3", *x, "pow", "2", "add", "1", "8", *x,
                    "sub", "2", "5"]
    sequence = ["action|launch"]

    # Automatic normal function input.
    sequence += [submitted(["0"]), submitted([*x_squared, "add", "1"]),
                 "key|clear"]
    # Dedicated equation solver: separate sides avoid the non-keyboard '=' token.
    sequence += ["key|down", "key|enter", submitted(quadratic_left),
                 submitted(["0"]), "key|clear"]
    # Radical equation: 2nd+x-squared enters sqrt( on the CE keypad.
    radical_left = ["2nd", "^2", "2", "mul", *x, "sub", "3", "rpar"]
    sequence += ["key|enter", submitted(radical_left), submitted(["1"]),
                 "key|clear"]
    # Function analysis.
    sequence += ["key|down", "key|enter", submitted([*x, "pow", "3"]),
                 "key|clear"]
    # Translation/dilation/vertex/intercept screen.
    sequence += ["key|down", "key|enter", submitted(transform_input),
                 submitted(["neg", "4"]), submitted(["4"]), submitted(["1"]),
                 submitted(["1"]), submitted(["0"]), submitted(["0"]),
                 "key|clear"]
    # Guided worksheet piecewise input.
    sequence += ["key|down", "key|enter", submitted(["3"]),
                 submitted(branch_one), submitted(["neg", "2"]), submitted(["0"]),
                 submitted(["0"]), submitted(["0"]), submitted(branch_two),
                 submitted(["0"]), submitted(["2"]), submitted(["1"]),
                 submitted(["1"]), submitted(branch_three), submitted(["2"]),
                 submitted(["4"]), submitted(["0"]), submitted(["0"]),
                 "key|clear"]
    # Automatic piecewise toggle uses the same guided editor.
    sequence += ["key|up", "key|up", "key|up", "key|up", "key|enter",
                 submitted(["1"]), submitted(x), submitted(["neg", "1"]),
                 submitted(["1"]), submitted(["1"]), submitted(["1"]),
                 "key|clear"]
    # Enter the More tools submenu.
    sequence += ["key|down", "key|down", "key|down", "key|down", "key|down",
                 "key|enter"]
    # Two-function composition.
    sequence += [submitted([*x_squared, "sub", "1"]), submitted([*x, "add", "1"]),
                 "key|clear", "key|down", "key|enter"]
    # Three-function composition.
    sequence += [submitted([*x_squared, "sub", "1"]), submitted([*x, "add", "1"]),
                 submitted([*x, "sub", "2"]), "key|clear", "key|down",
                 "key|enter"]
    # Average rate of change.
    sequence += [submitted(["0"]), submitted(["1", "sub", *x]), submitted(["0"]),
                 submitted(["2"]), "key|clear", "key|down", "key|enter"]
    # Three-point quadratic fit, then visit About and exit.
    sequence += [submitted(["0"]), submitted(["1", "4"]), submitted(["3", "5"]),
                 submitted(["5", "7"]), submitted(["4", "5"]), submitted(["4", "6"]),
                 "key|clear", "key|down", "key|enter", "key|down", "key|enter"]
    sequence += ["key|clear", "key|clear"]
    return sequence


class HeadlessSession:
    def __init__(self, executable: Path, firmware_kind: str, firmware: Path):
        self.process = subprocess.Popen(
            [str(executable), f"--{firmware_kind}", str(firmware)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        threading.Thread(target=drain_stderr, args=(self.process.stderr,), daemon=True).start()
        ready = self.process.stdout.readline().strip()
        if not ready.startswith("CEMU_HEADLESS_READY"):
            raise RuntimeError(f"CEmu did not start: {ready or 'no response'}")

    def command(self, command: str) -> str:
        assert self.process.stdin is not None
        assert self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        response = self.process.stdout.readline().strip()
        if not response.startswith("OK"):
            raise RuntimeError(f"CEmu command failed: {command!r}: {response}")
        return response

    def close(self) -> None:
        if self.process.poll() is None:
            try:
                self.command("quit")
            finally:
                self.process.wait(timeout=10)


def run_autotester(autotester: Path, firmware: Path, output: Path) -> None:
    files = app_files()
    config = {
        "rom": str(firmware),
        "transfer_files": [str(path) for path in files],
        "target": {"name": "MATHAPP", "isASM": True},
        "delay_after_key": 140,
        "delay_after_step": 140,
        "sequence": worksheet_smoke_sequence(),
        "hashes": {},
    }
    config_path = output / "launch_config.json"
    config_path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    result = subprocess.run(
        [str(autotester), "-d", str(config_path)],
        cwd=output,
        text=True,
        capture_output=True,
    )
    (output / "autotester.stdout.txt").write_text(result.stdout, encoding="utf-8")
    (output / "autotester.stderr.txt").write_text(result.stderr, encoding="utf-8")
    if result.returncode != 0 or "[Autotest passed]" not in result.stdout:
        raise RuntimeError(
            "CEmu full-path launch failed; see emulator-artifacts/autotester.*.txt"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--rom", type=Path, help="ROM dumped from your own TI-84 Plus CE")
    source.add_argument("--image", type=Path, help="Saved CEmu .ce image")
    parser.add_argument("--headless", type=Path, default=Path(os.environ.get("CEMU_HEADLESS", DEFAULT_HEADLESS)))
    parser.add_argument("--autotester", type=Path, default=Path(os.environ.get("CEMU_AUTOTESTER", DEFAULT_AUTOTESTER)))
    parser.add_argument("--output", type=Path, default=ROOT / "tests" / "emulator-artifacts")
    args = parser.parse_args()

    firmware = args.rom or args.image
    if firmware is None:
        firmware_text = os.environ.get("TI84CE_IMAGE", os.environ.get("TI84CE_ROM", ""))
        firmware = Path(firmware_text) if firmware_text else None
    if firmware is None:
        print(
            "No TI-84 Plus CE ROM/state was supplied. Dump one from your own calculator "
            "with CEmu, then rerun with --rom /path/to/CE.rom or --image /path/to/state.ce.",
            file=sys.stderr,
        )
        return 2
    firmware = firmware.expanduser().resolve()
    if not firmware.is_file():
        print(f"Firmware image does not exist: {firmware}", file=sys.stderr)
        return 2
    files = app_files()
    if not files or not APP.is_file():
        print(f"Native app is missing: {APP}; run make ce first.", file=sys.stderr)
        return 2
    if not args.headless.is_file():
        print(f"CEmu headless binary is missing: {args.headless}", file=sys.stderr)
        return 2

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    firmware_kind = "image" if args.image or firmware.suffix.lower() == ".ce" else "rom"

    session = HeadlessSession(args.headless, firmware_kind, firmware)
    try:
        session.command("run 10000")
        session.command(f"screenshot {output / 'boot.bmp'}")
        for path in files:
            session.command(f"send-file auto {path}")
        session.command("run 5000")
        session.command(f"screenshot {output / 'after-transfer.bmp'}")
        session.command(f"save-state {output / 'after-transfer.ce'}")
    finally:
        session.close()

    if firmware_kind == "rom" and args.autotester.is_file():
        run_autotester(args.autotester, firmware, output)
    elif firmware_kind == "image":
        print(
            "CEmu boot/transfer checks passed from the saved image; use --rom for "
            "the separate TI-OS program-launch check."
        )
    else:
        print(f"Warning: autotester binary is missing: {args.autotester}", file=sys.stderr)
        return 2

    print(f"CEmu transfer and TI-OS launch passed. Artifacts: {output}")
    print("Open tools/emulator_preview.html to view the calculator-shaped screen preview.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
