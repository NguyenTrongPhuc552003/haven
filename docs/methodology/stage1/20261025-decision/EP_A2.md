# EP-A2 adoption and focused final-tree acceptance

Classification: BUGFIX / DOCUMENTATION. Owner: HAVEN. The workspace owner
explicitly approved EP-A2 and its focused acceptance. **Adopted in the working
tree, uncommitted.** No additional production feature or coordinated source
change is included.

Base HEAD remains `f775304ce9443dc9861ba1880e4208c7ae19b314`. The existing four
local tracked differences are preserved in [preexisting-haven.patch](preexisting-haven.patch).
The only new production delta is nine added lines in
`src/common/string.c`; [ep-a2-adopted.patch](ep-a2-adopted.patch) records it.

## Identity and resulting behavior

The approved [review patch](../../stage0/20261025-corrections/0001-arm64-export-memcpy.patch)
SHA-256 was verified as
`d9c1b5189e7d3c15a89febf0ea30c56a4f8e7e394703544dbcf1e240b562dce2`.
`git -C haven apply --check <absolute-review-patch>` succeeded; the unchanged
patch was then applied using `git -C haven apply <absolute-review-patch>`.
The normal Git diff adds index metadata, so its raw patch bytes need not have
the review patch's hash; the source hunk is the approved one.

```c
#ifdef HAVEN_ARCH_ARM64
/* GCC can emit a memcpy libcall for aggregate copies in freestanding code.
 * The haven/string.h macro only redirects calls present in the C source. */
void *memcpy(void *dst, const void *src, size_t n)
{
        return hv_memcpy(dst, src, n);
}
#endif
```

Compiler-generated copies can now resolve the standard ABI name to HAVEN's
existing byte-copy helper. Public source aliases remain as before. This does
not implement the other standard memory entries or change their semantics.
No compiler flag, PSCI, CPU startup, IRQ, timer or page-table source changed.

## Actual-tree checks

Executed against the actual workspace source, with isolated fresh build paths:

```sh
python3 haven/docs/methodology/stage1/20261025-decision/accept_epa2.py \
  --haven haven --work /private/tmp/haven-stage1-epa2-final
```

The [runner](accept_epa2.py) captures the complete source diff, compiler/tools,
exact configure/build argv/cwd, effective compile commands and outputs.
Default: QEMU platform, MinSizeRel, existing target flags. Strict diagnostic:
the same configuration with `CMAKE_C_FLAGS=-mstrict-align` in that isolated
build only. This does not adopt strict alignment in production configuration.

| Acceptance | Result |
| --- | --- |
| Reviewed patch identity and clean applicability | PASS |
| Preserve pre-existing tracked changes byte-for-byte | PASS, excluding only `src/common/string.c` from the comparison |
| Fresh default build of actual source | PASS |
| Fresh strict diagnostic build of actual source | PASS |
| Defined standard ABI symbol | PASS: `memcpy` present; no new standard `memset`, `memmove` or `memcmp` export |
| Undefined references | PASS: none in the string object or final `haven.elf`, both configurations |
| Helper call graph / disassembly | PASS: wrapper forwards to `hv_memcpy`; no helper → standard memory entry recursion in either object |
| Existing tracked diff whitespace check | PASS |
| Native and ARM64 semantic execution | REUSED PASS evidence as explained below; not rerun |

The current GCC is15.2.0. Existing RWX LOAD-segment and discarded build-id
warnings remain. No boot success, alignment safety, DMA or timing conclusion
follows from these builds.

## Why semantic execution was reused

The final helper source hash exactly matches both earlier target source
members in the preserved integration runtime archive:

- `runtime/target-abi-default-source/src/common/string.c`
- `runtime/target-abi-strict-source/src/common/string.c`

The compiler and relevant target options match the reviewed execution setup.
The earlier native fixture and actual ARM64 standard-name calls tested4,160
offset/length cases, return pointers, zero length, exact self-copy and canaries.
Those helper semantics are unchanged. The final-tree builds cover the current
local source context; a broad QEMU rerun would repeat the unchanged helper
execution with temporary boot scaffolding. No such rerun was needed.

Hash matches and checks are recorded in [ep-a2-summary.json](ep-a2-summary.json).
The earlier [runtime record](../../stage0/20261025-integration-design/RUNTIME.md)
retains its narrower firmware/guest limitations. The earlier host-guard check
also applies because the exact wrapper remains inside the ARM64 guard.

## Remaining limitations and preservation

A1 remains an unresolved independent execution/memory contract risk. The
known default-layout failure was reproduced before and after this wrapper;
it is not repaired by an ABI export. Existing boot/PSCI/secondary, IRQ and timer
findings also remain open. Other toolchains/optimization changes need their
own focused input checks. Complete GCC runtime coverage is not claimed.

All129 final-tree evidence files were preserved under ignored
`build/evidence/stage1-20261025-epa2/` and in Git-visible
[ep-a2-evidence.tar.gz](ep-a2-evidence.tar.gz), SHA-256
`77eaa882a421546e936e864ccc8e88dce4a8efef13d2652f1ddf95a1330ead87`.
[ep-a2-inventory.json](ep-a2-inventory.json) hashes every member; all were read
back and verified. The bundle contains images, object files, caches/compile
commands, full diff, logs, script and exact helper source. `commands.json` and
`execution.json` identify actual execution and120-second command bounds. All
commands exited normally with code0.

Neither the new source hunk nor its evidence is committed/pushed. A checkpoint
inventory separately records which inputs exist only locally. No approved
backup destination was inferred from the repositories' ordinary Git remotes.
