# Personal names removed from committed logs (2026-11-13)

Run logs, build outputs and notes recorded on the owner's Mac carried the
Mac's login name and host name: in the Linux kernel banner
(`Linux version ... (<login>@<host>.local)`), in absolute paths written
before paths were sanitized, and in tool output. On 2026-11-13 the owner
approved replacing them in the current files:

| Was | Now |
| - | - |
| `<login>@<host>.local` | `user@host` |
| `<host>.local`, `<host>` | `host` |
| `/Users/<login>` | `/Users/user` |
| `-Users-<login>-` (scratchpad directory names) | `-Users-user-` |
| `<login>` elsewhere | `user` |

542 text files changed, 1,529 lines, one string for another; nothing else
in them changed. No binary file contained either name. What this changes
for the records:

- Every reading, finding, count and verdict is unchanged; only the name in
  the banner or path differs.
- Hash lists in the records (for example `DUMP_SHA256SUMS`) cover binary
  dumps or full logs kept outside the repository, none of which changed.
- A log line quoted elsewhere with the old name (for example in a past
  commit message) no longer matches the file byte for byte.
- Earlier commits still contain the names: history was not rewritten
  (owner decision, 2026-11-08). CI logs carry the neutral `haven@ci`.

From now on, logs from the Mac are written with the same replacement before
they are committed.
