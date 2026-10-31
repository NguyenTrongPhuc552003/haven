| Case | Kind | Oracle | Baseline | Baseline checkpoint | Candidate | Candidate relation | Class |
| - | - | - | - | - | - | - | - |
| V1 | control | ACCEPT | ACCEPT | policy:none | ACCEPT | - |  |
| V2 | control | ACCEPT | ACCEPT | policy:none | ACCEPT | - |  |
| F1 | control | REJECT | REJECT | structural:range-overflow | REJECT | structural:range-overflow |  |
| F2 | control | REJECT | REJECT | policy:hv_stage2_map_partition | REJECT | - |  |
| F3 | control | REJECT | ACCEPT | policy:none | REJECT | R2:owner | field comparison against E |
| F4 | control | REJECT | REJECT | structural:version | REJECT | structural:version |  |
| F5 | candidate | REJECT | ACCEPT | policy:none | REJECT | R2:association | field comparison against E |
| F6 | candidate | REJECT | ACCEPT | policy:none | REJECT | R4:effective-access | effective binding (known defect MM1) |
| F7 | non-scoring | N/A | N/A | non-scoring | N/A | - |  |
| F8 | candidate | REJECT | ACCEPT | policy:none | REJECT | R3:granularity | granularity relation (non-structural) |
| F9 | candidate | REJECT | ACCEPT | policy:none | REJECT | R2:irq-route | field comparison against E |
| F10 | candidate | REJECT | ACCEPT | policy:none | REJECT | R2:outside-envelope | field comparison against E |

oracle agreement: baseline 5/11, candidate 11/11
false rejections of valid controls by candidate: none
candidate-only detections among candidate cases:
  F5: field comparison against E
  F6: effective binding (known defect MM1)
  F8: granularity relation (non-structural)
  F9: field comparison against E
  F10: field comparison against E
