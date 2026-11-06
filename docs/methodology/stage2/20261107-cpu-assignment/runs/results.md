# CPU assignment runs

| Run | Build | Grant | Read at | Read-back | CPU table (granted / HAVEN runs) |
| - | - | - | - | - | - |
| X1 | linux | grant-l4-guest.json | marker | RESULT 2 finding(s): CPU 1: CPU assignment drift (granted to partition 1, HAVEN runs no partition); CPU 2: CPU assignment drift (granted to partition 2, HAVEN runs no partition) | CPU 0: 1/1, CPU 1: 1/0, CPU 2: 2/0, CPU 3: 0/0 |
| X2 | linux | grant-l4-cpus.json | marker | RESULT 1 finding(s): CPU 2: CPU assignment drift (granted to partition 2, HAVEN runs no partition) | CPU 0: 1/1, CPU 1: 0/0, CPU 2: 2/0, CPU 3: 0/0 |
| X3 | psci | grant-budget.json | marker | RESULT 1 finding(s): CPU 1: CPU assignment drift (granted to partition 1, HAVEN runs no partition) | CPU 0: 1/1, CPU 1: 1/0, CPU 2: 2/2, CPU 3: 0/0 |
| X4 | linux-S1 | grant-l4-cpus.json | breakpoint at 0x40000000 (stop reply T05 | RESULT 2 finding(s): CPU 0: CPU assignment drift (granted to partition 1, HAVEN runs partition 2); CPU 2: CPU assignment drift (granted to partition 2, HAVEN runs no partition) | CPU 0: 1/2, CPU 1: 0/0, CPU 2: 2/0, CPU 3: 0/0 |
| X5 | linux | grant-l4-cpus.json | breakpoint at 0x40000000 (stop reply T05 | RESULT 1 finding(s): CPU 2: CPU assignment drift (granted to partition 2, HAVEN runs no partition) | CPU 0: 1/1, CPU 1: 0/0, CPU 2: 2/0, CPU 3: 0/0 |
