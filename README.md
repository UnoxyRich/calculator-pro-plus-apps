# PHYS1 — Physics Formulas for TI-84 Plus CE

PHYS1 is one native TI-BASIC program for TI-84 Plus CE. It requires no launcher,
assembly shell, jailbreak, AppVar, or separate dependency. It draws fraction
bars and positions exponents and subscripts on the calculator's graph display.
The 22 formula pages cover kinematics, free fall, and projectile motion from
the attached notes; see [docs/formulas.md](docs/formulas.md).

On the calculator, open **PRGM → PHYS1**. It is a program, so it appears in
PRGM, not APPS. Use up/down to choose a topic and ENTER to open it. In a topic,
down/right/ENTER advances, up goes back, left returns to the menu, and CLEAR exits.

## Build

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
make PYTHON=.venv/bin/python
```

The sole calculator file is `bin/PHYS1.8xp`. Its source is `src/PHYS1.tib`.
The build converts the source into calculator tokens and checks that the
saved file decodes back to the exact source.

## Device compatibility and verification

The connected calculator reported TI-84 Plus CE OS 5.8.0. TI removed direct
assembly launching in OS 5.6, which is why the earlier `Asm(` wrapper failed.
This build is a TI-BASIC program. The latest `PHYS1.8xp` was transferred over
USB using the send-only command-line helper. A file read back from the device
matched the built program data exactly. The program was **not executed** on the
calculator, so its launch and navigation have not been verified at runtime.

An attempted USB deletion of the previous `PHYS1L` launcher timed out, and the
user reported a calculator crash and RAM reset immediately afterward. Earlier
USB delete calls had returned success without actually removing the variable.
Do not interpret a transfer/delete return code as proof of device state. The
later transfer happened only after the owner explicitly requested it; no
further deletion, program execution, or physical keypress was attempted. Only
the two prior project program variables were backed up on the computer; this
was not a full RAM backup.

`tools/TiConnectSend.java` only sends PHYS1 when absent, or confirms an exact
existing copy. It refuses to overwrite a different PHYS1, and cannot delete,
execute, or press calculator keys. It requires the TI Connect CE Java runtime
and JAR; it does not launch the TI Connect CE graphical interface.
