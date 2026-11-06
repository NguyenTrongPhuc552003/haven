# Read-back v8 runs

| Run | Case | Read at | Read-back | Guest-enabled interrupts | Denied lines in log |
| - | - | - | - | - | - |
| V1 | B0 | marker | RESULT enforced state matches the grant | INTID 33 on CPU 0: guest 1, GIC 1; INTID 27 on CPU 0: guest 1, GIC 1 | 0 |
| V2 | T4 | time limit (45 s, partition A did not print 'KDAL-DEMAND-END') | RESULT enforced state matches the grant | INTID 33 on CPU 0: waiting for the guest; INTID 27 on CPU 0: waiting for the guest | 210 |
| V3 | T5 | time limit (45 s, partition A did not print 'KDAL-DEMAND-END') | RESULT enforced state matches the grant | INTID 33 on CPU 0: waiting for the guest; INTID 27 on CPU 0: waiting for the guest | 0 |
| V4 | H3 | marker | RESULT enforced state matches the grant | INTID 33 on CPU 0: guest 1, GIC 1; INTID 27 on CPU 0: guest 1, GIC 1 | 0 |
| V5 | B0 | marker | RESULT enforced state matches the grant | INTID 33 on CPU 0: waiting for the guest; INTID 27 on CPU 0: waiting for the guest | 0 |
| V6 | H3 | marker | RESULT 1 finding(s): FINDING SPI 33: interrupt enable drift (enabled in the GIC, never enabled by the guest) | INTID 33 on CPU 0: guest 0, GIC 1; INTID 27 on CPU 0: guest 1, GIC 1 | 0 |
