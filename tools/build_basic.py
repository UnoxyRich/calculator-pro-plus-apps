"""Build the self-contained TI-BASIC PHYS1 program for TI-84 Plus CE."""

from pathlib import Path

from tivars.models import TI_84PCE
from tivars.types import TIProgram


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "PHYS1.tib"
OUTPUT = ROOT / "bin" / "PHYS1.8xp"


def main() -> None:
    program = TIProgram(name="PHYS1", archived=False)
    source = SOURCE.read_text(encoding="utf-8").strip() + "\n"
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
