# fwscan: read-only scans of vendor UEFI firmware images

Offline tools that read a vendor firmware image (SPI flash image or update
package) and print ACPI tables found in it, including those inside the LZMA
streams EDK2 uses for its firmware volumes. They never modify the image.
Vendor images are not part of this repository; the CIX P1 board images live
in the workspace's read-only `data/` folder.

| Tool | Prints | Used by |
| - | - | - |
| `iort_scan.py IMAGE...` | each distinct IORT (OEM `CIXTEK`): SMMUv3 bases and flags, ID mappings, named components | `docs/porting/cix-p1-ledger.md` (rows 1, 3 to 5) |
| `acpi_scan.py IMAGE...` | each distinct SPCR, DBG2 and GTDT (OEM `CIXTEK`): console UART base and interrupt, debug port, timer interrupts; flattened device trees by size | `docs/porting/cix-p1-loading.md` (section 6) |

Tables are printed once per SHA-256 (first 12 hex digits), with where they
were found (`raw` or `lzma@OFFSET`), so identical tables across images can
be compared by hash.

Tests: `python3 -m unittest discover -s tools/fwscan -p "test_*.py"`. They
build small tables in memory (no vendor bytes) and run in CI.
