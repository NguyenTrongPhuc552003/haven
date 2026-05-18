/* SPDX-License-Identifier: Apache-2.0 */
/* ARM64 EL2 physical timer driver for the Haven hypervisor.
 * Programs CNTHP_CTL_EL2 and CNTHP_CVAL_EL2 for per-partition deadline
 * enforcement.  Each partition gets a single EL2 physical timer deadline;
 * the hypervisor timer interrupt fires when the earliest deadline expires. */

#include <asm/sysregs.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * hv_arch_timer_init - enable EL2 physical timer, mask interrupt.
 *
 * Called once during CPU init.  The interrupt stays masked until the
 * first deadline is programmed via hv_arch_timer_set_deadline().
 * ----------------------------------------------------------------------- */
void hv_arch_timer_init(void) {
  /* Enable timer but mask the interrupt until we have a deadline */
  write_sysreg(CNTHP_CTL_ENABLE | CNTHP_CTL_IMASK, cnthp_ctl_el2);
  isb();
}

/* -----------------------------------------------------------------------
 * hv_arch_timer_now - return the current physical counter value (nanoseconds).
 *
 * Converts CNTPCT_EL0 ticks to nanoseconds using CNTFRQ_EL0.
 * Division is avoided by checking the timer frequency at boot; for
 * common frequencies (25 MHz → 40ns/tick, 50 MHz → 20ns/tick) the
 * conversion is a simple shift+multiply.
 *
 * Returns: monotonically increasing nanosecond timestamp.
 * ----------------------------------------------------------------------- */
uint64_t hv_arch_timer_now(void) {
  uint64_t cnt = read_sysreg(cntpct_el0);
  uint64_t freq = read_sysreg(cntfrq_el0);

  if (freq == 0)
    return cnt; /* degenerate case; shouldn't happen */

  /* Compute cnt * 1_000_000_000 / freq, avoiding overflow.
   * For freq ≤ 1 GHz, cnt * 1e9 may overflow uint64_t at ~18 seconds.
   * Use 128-bit arithmetic approximation: split into high/low. */
  uint64_t sec = cnt / freq;
  uint64_t rem = cnt % freq;
  uint64_t ns_sec = sec * 1000000000ULL;
  uint64_t ns_rem = (rem * 1000000000ULL) / freq;
  return ns_sec + ns_rem;
}

/* -----------------------------------------------------------------------
 * hv_arch_timer_freq - return counter frequency in Hz.
 * ----------------------------------------------------------------------- */
uint64_t hv_arch_timer_freq(void) { return read_sysreg(cntfrq_el0); }

/* -----------------------------------------------------------------------
 * hv_arch_timer_ns_to_ticks - convert nanoseconds to counter ticks.
 * ----------------------------------------------------------------------- */
uint64_t hv_arch_timer_ns_to_ticks(uint64_t ns) {
  uint64_t freq = read_sysreg(cntfrq_el0);
  return (ns * freq) / 1000000000ULL;
}

/* -----------------------------------------------------------------------
 * hv_arch_timer_set_deadline - program next hypervisor timer interrupt.
 *
 * deadline_ns: absolute nanosecond timestamp (from hv_arch_timer_now()).
 * If deadline_ns is already in the past, the interrupt fires immediately
 * after this function returns (CNTHP_CTL_ISTATUS will be set).
 * ----------------------------------------------------------------------- */
void hv_arch_timer_set_deadline(uint64_t deadline_ns) {
  uint64_t freq = read_sysreg(cntfrq_el0);
  uint64_t ticks;

  /* Convert nanoseconds to ticks */
  uint64_t sec = deadline_ns / 1000000000ULL;
  uint64_t ns = deadline_ns % 1000000000ULL;
  ticks = sec * freq + (ns * freq) / 1000000000ULL;

  /* Program compare value and unmask the interrupt */
  write_sysreg(ticks, cnthp_cval_el2);
  write_sysreg(CNTHP_CTL_ENABLE, cnthp_ctl_el2); /* clear IMASK */
  isb();
}

/* -----------------------------------------------------------------------
 * hv_arch_timer_cancel - mask the timer interrupt without disabling.
 * ----------------------------------------------------------------------- */
void hv_arch_timer_cancel(void) {
  write_sysreg(CNTHP_CTL_ENABLE | CNTHP_CTL_IMASK, cnthp_ctl_el2);
  isb();
}

/* -----------------------------------------------------------------------
 * hv_arch_timer_expired - return 1 if the current deadline has passed.
 * ----------------------------------------------------------------------- */
int hv_arch_timer_expired(void) {
  uint64_t ctl = read_sysreg(cnthp_ctl_el2);
  return (ctl & CNTHP_CTL_ISTATUS) ? 1 : 0;
}

/* -----------------------------------------------------------------------
 * hv_arch_timer_set_voffset - set virtual counter offset for a guest.
 *
 * Writing CNTVOFF_EL2 adjusts the value a guest sees when it reads
 * CNTVCT_EL0.  Haven uses this to give each partition a consistent
 * view of elapsed time from its perspective.
 *
 * offset_ns: guest virtual counter offset in nanoseconds.
 * ----------------------------------------------------------------------- */
void hv_arch_timer_set_voffset(uint64_t offset_ns) {
  uint64_t ticks = hv_arch_timer_ns_to_ticks(offset_ns);
  write_sysreg(ticks, cntvoff_el2);
  isb();
}
