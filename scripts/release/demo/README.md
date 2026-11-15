# R1 demo bundle

Private: built by CI from the private ELMOS SDK kernel; not for
publication (docs/methodology/stage2/20261115-r1-demo-bundle).

| Scene | Run | Needs |
| - | - | - |
| A to C | unpack `scenes-a-c/haven-try-*.tar.gz`, then `./try.sh` there | QEMU 9.2 or later, python3, dtc |
| D | `./run-scene-d.py` | QEMU, python3 |
| E | `ecosystem.lock.json` (the pins) and the haven Actions page | a browser |

`MANIFEST.sha256` lists every file: `shasum -a 256 -c MANIFEST.sha256`.
