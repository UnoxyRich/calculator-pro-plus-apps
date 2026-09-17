# MATHAPP — TI-84 Plus CE Math Solver

This is a native C app for the TI-84 Plus CE. It accepts expressions with
implicit multiplication, fractions, powers, `sqrt(x)`, and `abs(x)`.

The current vertical slice includes:

- automatic linear, quadratic, and rational-polynomial equation solving;
- exact rational and simplified radical answers, including complex roots;
- recorded solving steps and original-denominator checks;
- exact domain, range, parity, and interval analysis for supported functions;
- quadratic transformation with exact vertex and intercept output; and
- piecewise branch analysis with exact bounds and overlap validation; and
- a calculator menu with input, answer, steps, and honest unsupported-case messages.

Polynomial interval analysis currently covers degree 0 through 2. The parser
also recognizes selected radical and absolute-value expressions for domain and
range work. Unsupported symbolic cases are reported instead of being replaced
with decimal guesses.

## Host verification

The default target runs the core tests on macOS or another C99 compiler:

```sh
make
```

The tests cover implicit multiplication, complex quadratic roots, rational
holes, forbidden candidates, parity, interval analysis, radical analysis, and
the planned quadratic scaling example.

## Native calculator build

Install the CE C/C++ Toolchain and make its `bin` directory available on
`PATH`, then run:

```sh
make ce
```

The CE Toolchain produces `bin/MATHAPP.8xp`. The native front end uses the TI
OS string-input and keypad routines, while `src/math_core.c` is shared with
the host test executable. The current workspace does not include CEdev, so the
`.8xp` build cannot be performed here yet.

## Input examples

```text
x^2+2x+3=0
(x^2-1)/(x-1)
x^3/x
sqrt(x^2)
x^2-3x-4
```

The existing physics formula program remains in the parent folder and is built
independently.

## Piecewise input

The native menu accepts up to four ordered branches in this form:

```text
piecewise(x|(-2,0);-x|(0,2))
```

Each branch is `expression|interval`, and interval endpoints may be open or
closed, for example `(-2,0]` or `[0,4)`. Branches must not overlap. The app
combines branch domains, exact polynomial ranges, slope intervals, concavity,
and parity when the reflected branches can be proved symbolically.
