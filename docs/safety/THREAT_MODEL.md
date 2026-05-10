# Threat Model

Adversary assumptions:
- A partition may be compromised or faulty.
- The adversary can trigger high CPU load and malformed device requests.

Security goals:
- Prevent unauthorized memory/device access across partitions.
- Bound timing interference across critical domains.

Non-goals:
- Defense against all microarchitectural side channels.
- Runtime dynamic partition migration safety.
