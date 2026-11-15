# KDAL `kdality dt-gen`: investigation, and the fix proposed

Owner-approved investigation, 2026-11-14 (found in fix list part 5 of
[20261113-quality-gate](../20261113-quality-gate/README.md)). KDAL at
`5622c47`. The fix below is proposed; its option is the owner's choice.

## Symptom

`kdality dt-gen examples/kdc_hello/uart_hello.kdh -o DIR` exits 1 with
"cannot create '': Invalid argument" (on the Mac, 2026-11-14); the run in
fix list part 5 printed "cannot create '}'".

## Findings

- **F1, every run fails [from the code, and the run above].** Since
  KDAL `644429d` (2026-04-19, "harden dtgen file handling"),
  `open_output_in_dir()` checks the whole file name, `<name>.dtso`, with
  `is_safe_filename_component()`, which allows only letters, digits, `_`
  and `-`. The dot fails, so every call returns EINVAL before a file is
  opened.
- **F2, the message prints memory that was never written [from the
  code].** `generate_dtso()` prints `path`, which `open_output_in_dir()`
  fills only after that check: an uninitialised buffer (undefined
  behaviour; hence `'}'` once and `''` now).
- **F3, the scanner reads another syntax [from the code].** `parse_kdh()`
  is a line scanner for the simplified syntax of `docs/language_guide.md`
  (`device NAME {`, `register_map`, `NAME 0xOFF rw;`). All 9 `.kdh` files in
  the repository use the full syntax (`device class NAME {`, `registers {
  NAME : u32 @ 0x000 rw ...; }`). With F1 fixed, `uart_hello.kdh` would
  give a device named `class` and no registers.
- **F4, the test cannot fail [from the code].**
  `scripts/e2e/test_compiler.sh` counts a failing `dt-gen` as SKIP
  ("alpha limitation") and then looks for `.dts` or `.dtsi` files, while
  the tool writes `.dtso`. This is why F1 went unseen for seven months.
- **F5, the parser already reads both syntaxes [from the code].**
  `compiler/parser.c` takes `registers` and `register_map` blocks and
  keeps the device name, class, `compatible`, each register's offset and
  the signals in the AST (`compiler/include/ast.h`).
- **F6, two reg sizes [from the code].** The file's `reg` size is the
  highest register offset plus 4 (at least 0x100); the summary printed
  by `kdality_dtgen()` uses the last register's offset instead, so the two
  differ when registers are not in ascending order. A comment promises a
  power-of-2 rounding that the code does not do.

## Options

- **A, minimal.** Check the device name, not the file name (F1); print
  the name the file would get (F2); make the e2e check strict (F4); one
  reg-size function (F6). `dt-gen` then works only for the simplified
  syntax, which no file in the repository uses (F3 stays).
- **B, recommended.** A, and `dt-gen` reads the `.kdh` with the compiler
  library (`kdal_read_file`, `kdal_lex`, `kdal_parse`) instead of its own
  scanner: both syntaxes work, and the second parser (about 145 lines of
  `parse_kdh`) goes. Errors in the file are then reported the way
  `kdalc` reports them.

## Expectations (for B; regression test first)

| Row | Check | Expected |
| - | - | - |
| T1 | the strict e2e check, added first, on KDAL `main` | fails: `dt-gen` exits 1 on `uart_hello.kdh` and on a simplified-syntax copy of the language guide's `led.kdh` |
| T2 | the same check after B | passes: `UartHello.dtso` with `compatible = "kdal,UartHello"` and its 6 registers (DATA to IMSC) listed; `led.dtso` with `compatible = "example,led-gpio"` and 2 registers |
| T3 | `dtc -@ -I dts -O dtb` on both outputs | exit 0 (warnings recorded as they are) |
| T4 | a missing output directory; a device name that is not a safe file name | exit 1, the message names the directory or the name; no uninitialised text |
| T5 | KDAL's CI | green; e2e loses its dt-gen SKIP; gate: no new CRITICAL or MAJOR; jscpd duplicates not up |
| T6 | the summary `dt-gen` prints | the same reg size as the file's `reg` |

## Owner's choice and an amendment (before code)

Owner chose option B, 2026-11-14. Found while planning the test:

- **F7, the e2e suite does not run in CI [from the code].** KDAL's CI runs
  `scripts/ci/test.sh` (smoke tests), the gate and KUnit; `scripts/e2e/`
  runs only by hand (`cmake --build ... --target e2e`). A check added
  there alone would not run in CI.

So T1 to T4 are one script, `scripts/ci/dtgen-check.sh KDALITY`, run by
the smoke tests in CI (Linux and macOS, with `dtc` installed for T3) and
by `scripts/e2e/test_compiler.sh` in place of its old block. The
simplified-syntax input is a fixture, `tests/dtgen/led.kdh`, copied from
the language guide. The file reader, lexer and parser calls that `lint`
and `simulate` each repeat become one library function,
`kdal_parse_file()`, which `dt-gen` uses too. T1 is shown by a CI run of
the pull request's first commit (the script alone).

| Row | Check | Expected |
| - | - | - |
| T7 | the parse helper | `lint` and `simulate` behave as before on the examples (their output unchanged); jscpd duplicates not up |

## Results (KDAL #36)

The pull request's first commit (`8ca8a38`) holds the check alone; the
fix follows in `f629597` and `80220c6` (the first was committed before
the pre-push gate stopped its push over 8 new MAJOR findings on edited
lines, so the fix is two commits with the same subject). Merged as
`8a8e041`.

| Row | Observed | Verdict |
| - | - | - |
| T1 | CI on `8ca8a38`, Linux and macOS: the smoke tests fail; `dt-gen` refuses both `uart_hello.kdh` and `led.kdh` with "cannot create '}" (the uninitialised buffer of F2) | holds |
| T2 | CI on `80220c6`, both systems: `UartHello.dtso` with `compatible = "kdal,UartHello"` and 6 registers; `led.dtso` with `compatible = "example,led-gpio"` and 2 registers | holds |
| T3 | `dtc -@` compiles both outputs in CI (installed there; no "not installed" note) and on the Mac | holds |
| T4 | a missing output directory: exit 1, the message names it; a file that does not parse: exit 1 with the parser's message | holds |
| T5 | KDAL CI green; KUnit 40 of 40; gate MAJOR 367 to 326 (41 findings of the removed code fixed, baseline rewritten, none new); jscpd duplicated lines 344 to 324; on the Mac the e2e suite now passes dt-gen instead of skipping it | holds |
| T6 | the reg size printed and written come from one function (`reg_size`); the check compares them for both inputs | holds |
| T7 | `lint` on the 9 `.kdh` files and `simulate` on every `.kdc`: the 103 output lines identical before and after | holds |

**Found on the way.**
- **F8, `compatible` is ignored by the code generator [from the code].**
  `compiler/codegen.c` always writes the driver's match string as
  `kdal,<name>`; a `.kdh` that names its own (`led.kdh`:
  `example,led-gpio`) gets an overlay its driver does not match. The
  language guide says `compatible` is for OF matching, so the generator
  is the one to change; for a later decision.
- **F9, `argv[0]` taken as input [from the code].** The subcommands get
  their own name as `argv[0]` and loop from 0, so `kdality dt-gen` with
  no file reported "cannot resolve 'dt-gen'". Fixed in `dt-gen`; `lint`
  and `simulate` have the same loop.
- **F10, more failures counted as skips [observed, e2e on the Mac].**
  `kdality compile i2c_sensor.kdc`, `lint common.kdh` and `test-gen` are
  reported as "alpha" skips, the same masking as F4; with F7 (the suite
  is not in CI) these may hide real failures.
