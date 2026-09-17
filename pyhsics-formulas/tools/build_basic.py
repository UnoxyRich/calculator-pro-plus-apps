"""Build the self-contained TI-BASIC PHYS1 program for TI-84 Plus CE."""

from pathlib import Path
import re

from tivars.models import TI_84PCE
from tivars.types import TIProgram


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "PHYS1.tib"
OUTPUT = ROOT / "bin" / "PHYS1.8xp"


def validate_draw_calls(source: str) -> None:
    """Reject literal drawing arguments outside TI-84 Plus CE Full-mode limits."""
    for line_number, line in enumerate(source.splitlines(), start=1):
        if line.startswith("Text("):
            match = re.fullmatch(r'Text\((\d+),(\d+),"[^"]*"\)', line)
            if match is None:
                raise ValueError(f"Unsupported Text call on line {line_number}")
            row, column = map(int, match.groups())
            if not (0 <= row <= 148 and 0 <= column <= 256):
                raise ValueError(f"Text outside Full-mode bounds on line {line_number}")
        elif line.startswith("TextColor("):
            match = re.fullmatch(r"TextColor\((\d+)\)", line)
            if match is None or not 10 <= int(match.group(1)) <= 24:
                raise ValueError(f"Invalid text color on line {line_number}")
        elif line.startswith("Pxl-On("):
            match = re.fullmatch(r"Pxl-On\((\d+),X,(\d+)\)", line)
            if match is None:
                raise ValueError(f"Unsupported pixel call on line {line_number}")
            row, color = map(int, match.groups())
            if not (0 <= row <= 164 and 10 <= color <= 24):
                raise ValueError(f"Invalid pixel argument on line {line_number}")
        elif line.startswith("For(X,"):
            match = re.fullmatch(r"For\(X,(\d+),(\d+)\)", line)
            if match is None:
                raise ValueError(f"Unsupported fraction bar on line {line_number}")
            left, right = map(int, match.groups())
            if not (0 <= left <= right <= 264):
                raise ValueError(f"Fraction bar outside Full-mode bounds on line {line_number}")


def main() -> None:
    program = TIProgram(name="PHYS1", archived=False)
    source = SOURCE.read_text(encoding="utf-8").strip() + "\n"
    validate_draw_calls(source)
    program.load_string(source, model=TI_84PCE)
    decoded = program.string(model=TI_84PCE)
    if decoded.strip() != source.strip():
        raise ValueError("Token round trip changed the TI-BASIC source")
    if "Asm(" in decoded or "prgmPHYS1L" in decoded:
        raise ValueError("Assembly launcher token found in BASIC build")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    program.export().save(str(OUTPUT))
    rebuilt = TIProgram.open(str(OUTPUT))
    if rebuilt.string(model=TI_84PCE).strip() != source.strip():
        raise ValueError("Saved program failed to round trip")
    print(f"Built {OUTPUT} ({OUTPUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
