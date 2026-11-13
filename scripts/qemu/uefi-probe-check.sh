#!/usr/bin/env bash
# Run haven-probe.efi under UEFI firmware at EL2 in QEMU (L1 of
# docs/methodology/stage2/20261109-uefi-loading/) and check its output.
#
# Usage: scripts/qemu/uefi-probe-check.sh OUTDIR [seconds]
#   OUTDIR   directory with haven-probe.efi (boot/efi/build.sh OUTDIR)
#   seconds  time limit (default: 120)
# Firmware: $UEFI_FW, else the first of the Ubuntu and Homebrew paths below.
# Writes OUTDIR/evidence/uefi-probe-uart.log; exits 0 only if every check holds.
set -euo pipefail

OUT="${1:?usage: uefi-probe-check.sh OUTDIR [seconds]}"
LIMIT="${2:-120}"
FW="${UEFI_FW:-}"
for f in /usr/share/qemu-efi-aarch64/QEMU_EFI.fd /usr/share/AAVMF/AAVMF_CODE.fd \
	/opt/homebrew/share/qemu/edk2-aarch64-code.fd /usr/local/share/qemu/edk2-aarch64-code.fd; do
	[ -z "$FW" ] && [ -f "$f" ] && FW="$f"
done
[ -n "$FW" ] || { echo "uefi-probe-check: no AArch64 UEFI firmware found (set UEFI_FW)"; exit 2; }
[ -f "$OUT/haven-probe.efi" ] || { echo "uefi-probe-check: missing $OUT/haven-probe.efi"; exit 2; }

ESP="$OUT/esp"
LOG="$OUT/evidence/uefi-probe-uart.log"
rm -rf "$ESP" && mkdir -p "$ESP" "$OUT/evidence"
cp "$OUT/haven-probe.efi" "$ESP/"
printf 'fs0:\r\nhaven-probe.efi\r\nreset -s\r\n' >"$ESP/startup.nsh"
# The firmware image is copied and padded to 64 MiB, the pflash size.
cp "$FW" "$OUT/fw.fd" && chmod u+w "$OUT/fw.fd"
truncate -s 64M "$OUT/fw.fd"
echo "uefi-probe-check: firmware $FW"

rm -f "$LOG"
set +e
timeout "$LIMIT" qemu-system-aarch64 \
	-machine virt,virtualization=on,gic-version=3 -cpu max -smp 1 -m 2G \
	-display none -monitor none -nic none -serial "file:$LOG" \
	-drive if=pflash,format=raw,readonly=on,file="$OUT/fw.fd" \
	-drive file=fat:rw:"$ESP",format=raw,media=disk
rc=$?
set -e
echo "---- UART ($LOG) ----"
tr -d '\r' <"$LOG" | grep -a "probe:" || true
echo "---------------------"

val() { tr -d '\r' <"$LOG" | grep -a "^probe: $1 " | head -1 | awk '{print $3}'; }
fail=0
check() { if [ "$2" = 1 ]; then echo "ok      $1"; else echo "FAILED  $1"; fail=1; fi; }
has() { tr -d '\r' <"$LOG" | grep -aqF "$1" && echo 1 || echo 0; }
bit() { [ -n "$1" ] && [ $(( ($1 >> $2) & 1 )) = "$3" ] && echo 1 || echo 0; }

check "P1 probe ran and QEMU powered off (rc=$rc)" "$([ "$(has 'probe: HAVEN-PROBE 1')$(has 'probe: end')" = 11 ] && [ "$rc" = 0 ] && echo 1 || echo 0)"
el=$(val CurrentEL); hcr=$(val HCR_EL2); sctlr=$(val SCTLR_EL2)
check "P2 CurrentEL 2" "$([ "$el" = 0x0000000000000002 ] && echo 1 || echo 0)"
check "P3 HCR_EL2 TGE=1 E2H=0" "$([ "$(bit "$hcr" 27 1)$(bit "$hcr" 34 0)" = 11 ] && echo 1 || echo 0)"
check "P4 SCTLR_EL2 M=1 C=1 I=1" "$([ "$(bit "$sctlr" 0 1)$(bit "$sctlr" 2 1)$(bit "$sctlr" 12 1)" = 111 ] && echo 1 || echo 0)"
mp=$(val MPIDR_EL1); fr=$(val CNTFRQ_EL0); m1=$(val ID_AA64MMFR1_EL1)
check "P5 MPIDR 0x80000000, CNTFRQ non-zero, VH=1" "$([ "$mp" = 0x0000000080000000 ] && [ -n "$fr" ] && [ $((fr)) -ne 0 ] && [ $(( (m1 >> 8) & 0xf )) -ge 1 ] && echo 1 || echo 0)"
conv=0
while read -r a; do [ $((a)) -ge $((0x40000000)) ] && [ $((a)) -lt $((0xC0000000)) ] && conv=1; done \
	< <(tr -d '\r' <"$LOG" | grep -a "^probe: mem .* Conventional" | awk '{print $3}')
check "P6 conventional memory inside QEMU RAM" "$conv"
check "P7 ACPI 2.0 or device tree table listed" "$([ "$(has 'probe: table ACPI20')$(has 'probe: table DTB')" != 00 ] && echo 1 || echo 0)"
check "P8 shell continued after the probe (reset -s ran)" "$([ "$rc" = 0 ] && echo 1 || echo 0)"
[ "$fail" -eq 0 ] && echo "uefi-probe-check: PASS" || echo "uefi-probe-check: FAIL"
exit "$fail"
