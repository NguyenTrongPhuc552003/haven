# E7: replacement public releases built by CI (HAVEN side)

**Status:** owner-approved plan (2026-11-13). Design and expectations by
Claude, merged before the workflow exists; results added by a later pull
request, expectations never edited after them. Labels: **[F]** fact from
our code, records, CI logs or release metadata, **[O]** observation made
for this record, **[I]** inference, **[P]** proposal. The ELMOS half (the
Linux demo) has its own record, ELMOS `docs/developer/demo-release.md`.

## Problem

- [F] The owner deleted haven's GitHub pre-release `try-20261103` on
  2026-11-13 (haven #86 removed the dead links). Its Codeberg copy still
  exists, and `elmos demo haven` still works only because ELMOS falls back
  to that mirror (ELMOS `core/domain/demo/demo.go`, `HavenTag`,
  `HavenBundle`, `MirrorBase`).
- [F] That bundle was built by hand on the owner's Mac
  (`scripts/release/make-try-bundle.sh` at `ab6f825`).
- [O] 2026-11-13, on the Codeberg copy of
  `haven-try-v0.6.0-162-gab6f825.tar.gz`: every one of its 21 tar entries
  names the Mac's login as owner (BSD tar stores the user and group names
  of the files). The file contents do not contain the login or the host
  name (search over the extracted files: 0 matches). The names themselves
  are not written here.
- [I] From reading `make-try-bundle.sh` and `cmake/reproducible.cmake`:
  the script builds from a `git archive` copy outside any checkout, so the
  build finds no commit time and embeds the build time ("Haven
  SOURCE_DATE_EPOCH: not set"); two builds of one commit then differ.

## Design

1. **`scripts/release/make-try-bundle.sh`** (same output layout and name):
   - exports `SOURCE_DATE_EPOCH` as the HEAD commit time unless the
     caller set it, so the images in the bundle are the images a checkout
     build gives;
   - writes the archive with every entry owned by uid 0 and gid 0 with no
     user or group name (GNU tar: `--owner=0 --group=0 --numeric-owner`,
     sorted names, entry times set to `SOURCE_DATE_EPOCH`; BSD tar on
     macOS: `--uid 0 --gid 0 --uname '' --gname ''`), compressed with
     `gzip -n` (no name or time in the gzip header);
   - also writes `SHA256SUMS` (one line, the archive), the file `elmos
     demo` reads first.
2. **`scripts/release/check-public-assets.py`** (standard library only):
   checks files and directories before they become public. Archives
   (`.tar.gz`) are opened and every member is read; `.gz` files are read
   decompressed. It fails on (a) a tar entry not owned by uid 0 and gid 0
   or with a user or group name other than empty or `root`; (b) a
   `/Users/<name>` path (a macOS home directory); (c) a `Linux version`
   banner whose builder is not `haven@ci`; (d) any word in the
   environment variable `RELEASE_DENY` (comma separated, case-insensitive),
   in file contents, member names or entry owner names. It lists
   `/home/<name>` paths as notes, not failures (the CI runner's own
   `/home/runner` is expected). `RELEASE_DENY` is how the Mac's login and
   host name are checked without writing them anywhere: the check on the
   Mac sets it at run time from `id -un` and `scutil --get LocalHostName`;
   CI has no copy of them (no secret holds them).
3. **`.github/workflows/try-release.yml`**, `workflow_dispatch` only,
   input `tag` (form `try-YYYYMMDD`):
   - job **build** (`ubuntu-24.04`, `contents: write`): refuses a tag that
     already has a release or a Git tag; installs the cross toolchain of
     the `arm64-qemu` job; runs `make-try-bundle.sh dist`; builds the
     qemu-psci profile in the checkout and compares its `haven.bin` with
     the bundle's `images/good/haven.bin` (`cmp`); runs
     `check-public-assets.py` on `dist/` and on the release notes; creates
     a **draft** pre-release for the tag at the dispatched commit, title
     ending in `[untested]`, with the archive, its `.sha256` and
     `SHA256SUMS`. A draft is visible only to people with write access to
     the repository and creates no Git tag until it is published.
   - jobs **test-linux** (`ubuntu-24.04`, Ubuntu's QEMU 8.2.2) and
     **test-macos** (`macos-15`, Homebrew's QEMU and dtc): download the
     draft's assets through the API by release id with `GITHUB_TOKEN`,
     check `SHA256SUMS`, unpack into an empty directory, run `./try.sh`
     there and check its output.
   - job **mark**: edits the draft's title and notes with both test
     results. The title loses `[untested]` only if test-macos passed;
     otherwise it ends in `[FAILED: do not publish]`.
   - Nothing in the workflow publishes a release, deletes one or uploads
     an Actions artifact (artifacts of a public repository can be
     downloaded by any signed-in user; the draft is the private channel
     between the jobs).
4. **Publication is the owner's.** After the run, the coordinator asks the
   owner for an explicit yes on the specific draft (tag, commit, asset
   hashes, test results, the local name check below). Before that yes the
   draft stays a draft. After publication the owner deletes the old
   Codeberg release (`try-20261103`); deleting is the owner's job.
5. **Local name check before the yes** (on the Mac, by the coordinator or
   Claude): download the draft's assets with `gh release download` and
   run `check-public-assets.py` with `RELEASE_DENY` set from `id -un` and
   `scutil --get LocalHostName`.
6. **Pointing users at the new release** is one pull request in ELMOS
   (`HavenTag`, `HavenBundle`, tests) and one in haven-docs (quick start,
   landing page), prepared after the drafts exist and **merged only after
   the owner has published them**. haven's README line about the withdrawn
   pre-release changes in the results pull request of this record, also
   after publication.

Why the macOS job decides the title [I]: the bundle's audience runs it on
their own machines, and `try.sh` says that QEMU 9.2 or later is needed
for the DMA part; Ubuntu 24.04's QEMU 8.2.2 has no SMMU stage 2, so HAVEN
withholds the DMA grant there (E4, E5 records). The Linux run is recorded
for comparison.

## Expectations

| Row | Check | Expected result |
| - | - | - |
| T1 | build job | succeeds; the archive is `haven-try-v0.6.0-<n>-g<7 hex>.tar.gz` for the dispatched commit (`git describe`), about 70 KB (old: 72,276 bytes) |
| T2 | `cmp` of the bundle's `images/good/haven.bin` with the checkout's qemu-psci build | identical |
| T3 | `check-public-assets.py` in CI | exit 0: every tar entry uid 0, gid 0, no names; no `/Users/` path; no failure; notes, if any, only `/home/runner` |
| T4 | the draft | exists, `draft: true`, `prerelease: true`, the three assets, target the dispatched commit; no Git tag of that name exists after the run; no release was published and no Actions artifact exists for the run |
| T5 | test-macos (`./try.sh`, Homebrew QEMU, 9.2 or later predicted) | step 1 `partition-b-check: PASS (cortex-a72)`; step 2 `RESULT enforced state matches the grant`; step 3 `partition-b-check: PASS (cortex-a72)` again and a `FINDING EL2 / partition 2: partition memory mapped at EL2`, with `RESULT 1 finding(s)` (the count is a prediction, the class decides the job) |
| T6 | test-linux (QEMU 8.2.2) | steps 1 and 3 as T5 (both partition checks PASS, the EL2 finding present); step 2 not clean: at least one finding about SMMU stream 8, because HAVEN withholds the DMA grant on this QEMU (prediction; recorded, does not fail the job) |
| T7 | mark job | title without `[untested]`; notes list the QEMU versions and the results of T5 and T6 |
| T8 | local name check on the Mac (`RELEASE_DENY` from `id -un` and the host name) | exit 0 on the downloaded draft assets |
| T9 | old bundle, same local check | exit 1, failing on rule (a) for its 21 entries and on rule (d) for the same owner names (the observation above), so the check can see what it is meant to see |

Verdict rule as before: a row holds only if the observation matches as
written; a mismatch is a prediction error, recorded and explained, never
edited away. Not shown by this record: that the bundle runs on Windows
(WSL2) or on Intel Macs; that the published release is fetched by `elmos
demo haven` (that is the ELMOS pull request's check, after publication).
