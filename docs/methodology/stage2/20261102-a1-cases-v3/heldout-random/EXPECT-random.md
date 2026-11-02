# Randomly drawn held-out cases: expectations (written before the run)

**Why random.** The owner asked Claude to write and run the held-out cases
(2026-11-02). Faults chosen by Claude would not be held out from the tool's
author, so they were drawn instead: `../heldout_random.py` at haven
`e47b722`, all 1,062 single-point mutants of the enforcement code, seed text
`haven e47b722 | owner 2026-11-02 14:21: You should write and run them in
yourself, not me` (SHA-256 in `draw.json`). The first four that build were
kept; one drawn mutant (`stage2.c` line 123, `||` to `&&`) did not build
and was skipped. Claude saw each patch only after the draw, and wrote the
expectations below before running anything. They are Claude's, not the
owner's; frozen with the patches in `../freeze/SHA256SUMS-v3-random`.

| Case | Mutation | Expected read-back | Unit tests | Boot check | Reasoning |
| - | - | - | - | - | - |
| R1 | `stage2.c:115` region size check `== 0U` becomes `== 1U` | no finding | pass (low confidence: a test may submit a zero-size region) | pass | no configured region has size 0 or 1; a size-1 region already fails the alignment check; equivalent for HAVEN's configuration |
| R2 | `gic_v3.c:77` SPI priority loop starts at register 9, not 8 | no finding | pass | pass | INTIDs 32-35 keep reset priority 0 instead of 0xA0; UART still delivered. A real priority inversion (partition interrupt above HAVEN's 0x40), but read-back does not read priorities (blind spot, as I7) |
| R3 | `smmu_v3.c:358` first init command left uninitialised | SMMU enable drift and SMMU command queue fault; possibly a probe failure, because HAVEN stops before building stage-2 tables | pass | fail | the garbage or zero opcode is illegal, the queue stops with an error, `smmu_v3_init` returns HV_EIO and HAVEN panics |
| R4 | `partition.c:374` `!=` becomes `==` after mapping partition B | missing binding on partition A's regions; DMA enforcement drift on SID 8; missing interrupt binding on SPI 33 | pass | fail | a successful B mapping now unmaps A and panics, so grants and interrupt routes are never applied |
