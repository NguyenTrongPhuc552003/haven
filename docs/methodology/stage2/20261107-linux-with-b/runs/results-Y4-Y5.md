# Linux with partition B runs

| Run | Profile | Read at | Read-back | CPU table (granted / runs) | Partition B | Linux reached |
| - | - | - | - | - | - | - |
| Y4 | qemu-psci | marker | RESULT enforced state matches the grant | CPU 0: 1/1, CPU 1: 0/0, CPU 2: 2/2, CPU 3: 0/0 | started | KDAL-DEMAND-END |
| Y5 | qemu-psci | breakpoint at 0x40000000 (stop reply T05 | RESULT enforced state matches the grant | CPU 0: 1/1, CPU 1: 0/0, CPU 2: 2/2, CPU 3: 0/0 | started | no Linux output |
