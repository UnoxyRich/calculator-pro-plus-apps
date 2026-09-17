# USB transfer helper

`TiConnectCeCli.java` calls TI Connect CE's installed command-line backend.
It can install, remove, list, or request execution of calculator variables.
The remote execution request has not yet provided proof of a visible launch;
use a calculator screen check to verify it. No helper command sends keypresses.

The TI Connect CE JAR and its matching Java runtime must be supplied from a
locally installed copy of TI Connect CE. The only file needed on the
calculator for this project is `bin/PHYS1.8xp`.
