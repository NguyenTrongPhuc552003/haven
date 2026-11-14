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
