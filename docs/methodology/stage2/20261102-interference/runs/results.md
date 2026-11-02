# Interference pipeline in QEMU (haven fe7db3f); latency values are host-bound, not results

| Config | CPU | Pipeline ok | B samples | B latency avg / max (us) | < 1 ms | 1 to 10 ms | >= 10 ms |
| - | - | - | - | - | - | - | - |
| none | cortex-a72 | yes | 500 | 3990 / 8970 | 247 | 253 | 0 |
| none | max | yes | 500 | 4007 / 9068 | 246 | 254 | 0 |
| cpu | cortex-a72 | yes | 500 | 3973 / 9278 | 248 | 252 | 0 |
| cpu | max | yes | 500 | 5111 / 9260 | 192 | 308 | 0 |
| mem | cortex-a72 | yes | 500 | 5361 / 9154 | 177 | 323 | 0 |
| mem | max | yes | 500 | 5348 / 9050 | 181 | 319 | 0 |
| none-B-unthrottled | cortex-a72 | yes | 500 | 526 / 2385 | 497 | 3 | 0 |
| none-B-unthrottled | max | yes | 500 | 522 / 2025 | 496 | 4 | 0 |
