# PHYS1 — Physics Formulas for TI-84 Plus CE

PHYS1 is one native TI-BASIC program for TI-84 Plus CE. It requires no launcher,
assembly shell, jailbreak, AppVar, or separate dependency. It draws fraction
bars and positions exponents and subscripts on the calculator's graph display.
The 22 formula pages cover kinematics, free fall, and projectile motion from
the attached notes; see [docs/formulas.md](docs/formulas.md).

On the calculator, open **PRGM → PHYS1**. It is a program, so it appears in
PRGM, not APPS. Use up/down to choose a topic and ENTER to open it. In a topic,
up/down or right advances pages, left returns to the menu, and CLEAR exits.

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
This build uses TI-BASIC instructions supported by the CE OS. The previous
`PHYS1L` launcher was removed from the connected calculator, and an earlier
revision of `PHYS1` was installed as a type 5 TI-BASIC program over USB. The
latest build includes additional formula details and needs a final transfer.

The USB transfer interface reported success, but its remote execute call did
not show a changed screen. Launch and navigation on the calculator still need
to be verified before claiming that the app works.
