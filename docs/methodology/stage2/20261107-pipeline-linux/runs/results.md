# Pipeline with Linux in partition A (cortex-a72)

| Cell | Load | B budget | Checks | B probe: samples, avg / max (us), < 1 ms, 1 to 10 ms, >= 10 ms | Linux /init at |
| - | - | - | - | - | - |
| idle-Boff | idle | off | all 11 ok | 500, 541 / 1510, 494, 6, 0 | 0.550974 s |
| cpu-Boff | cpu | off | all 11 ok | 500, 507 / 2066, 498, 2, 0 | 0.560912 s |
| mem-Boff | mem | off | all 11 ok | 500, 273 / 1261, 499, 1, 0 | 0.635520 s |
| idle-B2ms | idle | 2 ms / 10 ms | all 11 ok | 500, 3974 / 8929, 249, 251, 0 | 0.562115 s |
