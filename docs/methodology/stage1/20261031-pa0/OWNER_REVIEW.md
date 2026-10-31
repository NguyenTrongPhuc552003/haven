# P-A0 owner review sheet (independent oracle review)

**Purpose.** `NEXT_EXPERIMENT.md` requires "an independent oracle/case review
or a separately authored held-out mutation before scoring a distinguishing
result". The frozen cases in `freeze/cases.json` were drafted by Claude, so
Claude cannot review them. This sheet lets the workspace owner perform that
review. It explains each case in plain words and quotes the frozen input
verbatim. It deliberately gives no opinion on whether the expected verdicts
are right: that judgement is the review.

**Integrity.** `freeze/SHA256SUMS` verified OK on 2026-11-01 for
`freeze/cases.json` and `baseline/harness.c`. If you change any expected
verdict, the freeze changes and step 3 (baseline run) must be repeated.

## Terms

- **E (envelope):** what the trusted operator allows. Frozen E: one 4 KiB
  model resource "P" at IPA 0x10000 mapped to PA 0x90000, owned by partition 1
  on CPU 0, read/write, 32-bit accesses, only register offset 0x20 permitted,
  no DMA, no IRQ. All addresses are fictional host-test data.
- **D (demand):** what the backend driver says it needs (untrusted).
- **G (grant):** what HAVEN admits.
- **H (effective binding):** what the enforcement actually allows.
- **ACCEPT / REJECT:** what a correct system should do with the case.
- **control:** a case the existing baseline is expected to handle; it checks the
  harness, not the research idea. **candidate:** a case that could show RQ-A
  adds something. **non-scoring:** recorded but never counted.

## Part 1: is each expected verdict correct?

Mark **Y** (correct), **N** (wrong, say what it should be) or **?** (unsure).

| Case | Kind        | Frozen input (verbatim)                                                       | In plain words                                                                                                             | Expected | Your mark | Note |
| ---- | ----------- | ----------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- | -------- | --------- | ---- |
| V1   | control     | ring demand: guest RAM only, no G/H resources                                 | The KDAL software ring needs only guest RAM it already has; no device resource is requested.                               | ACCEPT   |           |      |
| V2   | control     | D/G/H all match E exactly, read/write32 at P+0x20                             | Demand, grant and enforcement all equal the envelope; a 32-bit access to the one permitted register.                       | ACCEPT   |           |      |
| F1   | control     | G ipa 0xFFFFFFFFFFFFF000 size 0x2000 (base+size overflow)                     | The grant's address range wraps past the top of the address space.                                                         | REJECT   |           |      |
| F2   | control     | partition 2 already holds pa 0x90000; G owner 1 requests it                   | Two partitions would own the same physical page.                                                                           | REJECT   |           |      |
| F3   | control     | G owner 2 on unowned P; E owner is 1                                          | The page is granted to partition 2, but the envelope says only partition 1 may own it.                                     | REJECT   |           |      |
| F4   | control     | artifact version and hash do not match record                                 | A configuration file is a different version than the one recorded.                                                         | REJECT   |           |      |
| F5   | candidate   | G ipa 0x10000 -> pa 0xA0000; each range valid, association differs from E     | Every address is individually valid, but the guest address is wired to a different physical page than the envelope allows. | REJECT   |           |      |
| F6   | candidate   | E_ro; effective binding H permits writes                                      | The envelope says read-only, but the enforced mapping allows writes (the MM1 situation).                                   | REJECT   |           |      |
| F7   | non-scoring | no source-backed immutable identity semantics; no lifecycle epoch invented    | A "stale resource identity" case that cannot be defined without inventing semantics, so it is recorded but never scored.   | N/A      |           |      |
| F8   | candidate   | E_sub; D needs 0x20..0x24, page G also exposes forbidden 0x40                 | The driver needs only one register, but granting the whole 4 KiB page also exposes a forbidden register on the same page.  | REJECT   |           |      |
| F9   | candidate   | E_irq; prospective IRQ 40 assigned target_cpu 1; model only, never a ring IRQ | A model interrupt owned by partition 1 on CPU 0 is routed to CPU 1 instead.                                                | REJECT   |           |      |
| F10  | candidate   | D/G/H all agree on page Q 0x20000 outside E                                   | Demand, grant and enforcement agree with each other, but on a page the operator never allowed.                             | REJECT   |           |      |

## Part 2: questions about the case set as a whole

1. Is the frozen envelope E a reasonable stand-in for "what a trusted operator
   allows" for this pilot? (Y / N / note)
2. Is any important kind of mistake missing? If yes, describe it in one line;
   it becomes your own held-out case (strongest form of independence).
3. Are any two cases really the same check under different names? (Y / N / which)

## Part 3: sign-off

- Reviewer: ______________________  Date: __________
- Overall: the expected verdicts are fit for scoring  /  need changes (list above)

After you fill this in, Claude records its SHA-256 in `stages/CHECKPOINT.md`,
notes in `NEXT_EXPERIMENT.md` that the independent review condition is met (or
which verdicts changed), and P-A0 continues from step 4.
