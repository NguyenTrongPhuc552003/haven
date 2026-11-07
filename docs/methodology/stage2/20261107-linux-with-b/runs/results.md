# Linux with partition B runs

| Run | Profile | Read at | Read-back | CPU table (granted / runs) | Partition B | Linux reached |
| - | - | - | - | - | - | - |
| Y1 | qemu-psci | marker | RESULT 1 finding(s): CPU 2 PPIs: interrupt enable drift (enabled without a grant: 26) | CPU 0: 1/1, CPU 1: 0/0, CPU 2: 2/2, CPU 3: 0/0 | started | KDAL-DEMAND-END |
| Y2 | qemu-psci | breakpoint at 0x40000000 (stop reply T05 | RESULT 1 finding(s): CPU 2 PPIs: interrupt enable drift (enabled without a grant: 26) | CPU 0: 1/1, CPU 1: 0/0, CPU 2: 2/2, CPU 3: 0/0 | started | no Linux output |
| Y3 | qemu-direct | marker | RESULT 1 finding(s): CPU 2: CPU assignment drift (granted to partition 2, HAVEN runs no partition) | CPU 0: 1/1, CPU 1: 0/0, CPU 2: 2/0, CPU 3: 0/0 | inactive | KDAL-DEMAND-END |
