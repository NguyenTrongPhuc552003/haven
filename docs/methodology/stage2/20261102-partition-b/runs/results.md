# Partition B checks (haven 4c740a5)

| Check | CPU | Partition B check | As expected | Missing | Read-back (grant-partb) | As expected | Read-back (grant-v3) | As expected |
| - | - | - | - | - | - | - | - | - |
| K1 | cortex-a72 | PASS | yes | - | none | yes | CPU 2 PPIs: interrupt enable drift | yes |
| K1 | max | PASS | yes | - | none | yes | CPU 2 PPIs: interrupt enable drift | yes |
| K2 | cortex-a72 | FAIL | yes | B's read of A's IPA denied on CPU 2; B status magic; B reached stage 5 (done); B timer interrupts >= 8; B result: none | none | yes | CPU 2 PPIs: interrupt enable drift | yes |
| K2 | max | FAIL | yes | B's read of A's IPA denied on CPU 2; B status magic; B reached stage 5 (done); B timer interrupts >= 8; B result: none | none | yes | CPU 2 PPIs: interrupt enable drift | yes |
| K3 | cortex-a72 | FAIL | yes | B timer interrupts >= 8; B result: too few timer interrupts; no undelivered interrupt | none | yes | none | yes |
| K3 | max | FAIL | yes | B timer interrupts >= 8; B result: too few timer interrupts; no undelivered interrupt | none | yes | none | yes |
| K4 | cortex-a72 | FAIL | yes | B timer interrupts >= 8; B result: too few timer interrupts | CPU 0 PPIs: interrupt enable drift | yes | CPU 0 PPIs: interrupt enable drift | yes |
| K4 | max | FAIL | yes | B timer interrupts >= 8; B result: too few timer interrupts | CPU 0 PPIs: interrupt enable drift | yes | CPU 0 PPIs: interrupt enable drift | yes |
