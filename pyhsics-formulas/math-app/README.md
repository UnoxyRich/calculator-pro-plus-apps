# MATHAPP — TI-84 Plus CE Math Solver

This is a native C app for the TI-84 Plus CE. It accepts expressions with
implicit multiplication, exact fractions and terminating decimals, powers,
`sqrt(x)`, `abs(x)`, vertical absolute-value bars, and the imaginary unit `i`.

The current vertical slice includes:

- one automatic input screen that routes equations and functions to the correct
  tool, plus a guided piecewise editor for calculator-keyboard entry;
- automatic linear, quadratic, and rational-polynomial equation solving;
- exact rational and simplified radical answers, including complex roots;
- recorded solving steps and original-denominator checks;
- exact domain, range, parity, and interval analysis for supported functions;
- constant, linear, and quadratic transformation with exact formula, intercept,
  dilation, translation, reflection, and vertex-form output (quadratics also get
  vertices); and
- piecewise branch analysis with exact bounds, merged ranges, and overlap validation;
- function composition for supported exact patterns;
- exact average-rate calculations and three-point quadratic fitting; and
- verified two-radical equations and linear equations with complex coefficients; and
- a calculator menu with input, answer, steps, and honest unsupported-case messages.

The result viewer shows long values and steps one page at a time so exact
intervals, radicals, and recorded operations remain readable on the CE LCD.

Polynomial interval analysis currently covers degree 0 through 3. Exact
interval rules also cover affine absolute values, linear square roots, and
linear-over-linear rational functions. The parser recognizes selected radical
and absolute-value expressions for domain and range work. Unsupported symbolic
cases are reported instead of being replaced with decimal guesses.

## Host verification

The default target runs the core tests on macOS or another C99 compiler:

```sh
make
```

The tests cover automatic-style equation/function parsing, implicit
multiplication, exact fractions and radicals, complex quadratic roots,
rational holes and forbidden candidates, even/odd/neither parity, normal and
piecewise interval analysis, radical and absolute-value analysis, reflections,
and the quadratic scaling example.
The suite also exercises cubic intervals, worksheet piecewise behavior,
function composition, complex coefficients, two-radical verification,
average rates, regression, and exact decimal normalization.

## Native calculator build

Install the CE C/C++ Toolchain and make its `bin` directory available on
`PATH`, then run:

```sh
make ce
```

The CE Toolchain produces `bin/MATHAPP.8xp`; if the program exceeds one CE
AppVar, it also produces `bin/MATHAPP.8xp.*.8xv` sidecar files. Transfer the
launcher and every generated sidecar together. The native front end uses the
TI OS string-input and keypad routines. Its input adapter converts TI-ASCII
keypad glyphs such as square root, square, reciprocal, negation, and italic
`i` into the parser's readable form before solving. `src/math_core.c` is
shared with the host test executable.

## CEmu emulator test

CEmu requires a ROM dumped from your own TI-84 Plus CE; this project does not
bundle or download TI firmware. After obtaining a ROM, run:

```sh
python3 tools/cemu_suite.py --rom /path/to/your/CE.rom
```

The suite transfers the built `.8xp` plus any generated `.8xv` sidecars, saves
CEmu screenshots and a state, and
uses CEmu's TI-OS launch path to verify the program starts. Open
`tools/emulator_preview.html` to see the LCD over the downloaded calculator
body photograph. A saved `.ce` state can also be passed with `--image` for the
boot/transfer checks; `--rom` is required for the separate TI-OS launch check.
Without a user-provided ROM/state, the host math tests still run, but a real
calculator boot cannot be honestly claimed.

## Input examples

```text
x^2+2x+3=0
(x^2-1)/(x-1)
x^3/x
sqrt(x^2)
x^2-3x-4
sqrt(x-5)+sqrt(10-x)=3
(2+i)x=5
|x+2|+|x-2|
```

The existing physics formula program remains in the parent folder and is built
independently.

The `More tools` submenu provides function composition, average rate of change,
and exact quadratic fitting from three `(x,y)` points. When a parsed expression
needs a symbolic method outside the exact rules above, the app reports that
limitation instead of returning a decimal guess.

## Piecewise input

The host parser accepts up to four ordered branches in either of these forms:

```text
piecewise(x|(-2,0);-x|(0,2))
piecewise(x|-2<x<0;-x|0<x<2)
```

Each branch is `expression|interval` or `expression|lower<x<upper`; `<=` marks
a closed endpoint. Interval endpoints may also be written explicitly, for
example `(-2,0]` or `[0,4)`. Branches must not overlap. The app combines
branch domains, exact polynomial ranges, slope intervals, concavity, and
parity when the reflected branches can be proved symbolically.

On the physical calculator, choose `Piecewise analyze` and enter each branch
through prompts for the expression, exact lower and upper bounds, and whether
each endpoint is closed (`0` or `1`). The `Average rate` tool has the same
piecewise toggle, so it never requires typing punctuation that is not present
on the TI-84 keyboard.

The native `Solve equation` screen also uses separate left-side and right-side
prompts, so equation solving does not depend on finding the `=` token through
the TI-84 test menu. The automatic text parser still accepts the compact
`left=right` form for host-side and saved-input workflows.
