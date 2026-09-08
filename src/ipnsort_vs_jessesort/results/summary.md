# ipnsort vs maintained JesseSort production/noalloc paths

- type: u64
- n: 100000
- trials per pattern: 500
- warmups per pattern: 2
- shared cold-like preconditioner: false
- inputs: 14 canonical JesseSort benchmark inputs, order-preserving int->u64 encoding

| Input | simulated-direct_live-phase | adaptive-noalloc | strict-noalloc | ipnsort |
|---|---:|---:|---:|---:|
| Random | 1.0636 (990.824 us) | 1.0389 (967.873 us) | 1.0371 (966.135 us) | 1.0000 (931.593 us) |
| Sorted | 1.9767 (42.334 us) | 1.9652 (42.087 us) | 1.9661 (42.107 us) | 1.0000 (21.416 us) |
| Reverse | 1.0792 (51.815 us) | 0.6400 (30.726 us) | 0.6401 (30.731 us) | 1.0000 (48.012 us) |
| Sorted+Noise(5%) | 1.1959 (1181.024 us) | 1.1588 (1144.326 us) | 1.0659 (1052.611 us) | 1.0000 (987.522 us) |
| Sorted+Noise(10%) | 1.5475 (1608.702 us) | 1.3946 (1449.769 us) | 1.0154 (1055.538 us) | 1.0000 (1039.560 us) |
| Random%25 | 1.1469 (291.232 us) | 1.1097 (281.768 us) | 1.1140 (282.864 us) | 1.0000 (253.921 us) |
| Alternating | 1.4975 (1353.640 us) | 1.0716 (968.650 us) | 1.0736 (970.459 us) | 1.0000 (903.957 us) |
| Sawtooth | 0.9908 (577.197 us) | 1.1072 (644.994 us) | 2.5144 (1464.832 us) | 1.0000 (582.566 us) |
| MixedDirectionRuns | 0.6912 (613.343 us) | 0.2643 (234.559 us) | 0.3375 (299.450 us) | 1.0000 (887.331 us) |
| BlockSorted | 0.6669 (540.102 us) | 0.2521 (204.155 us) | 0.2522 (204.281 us) | 1.0000 (809.853 us) |
| OrganPipe | 0.5046 (499.101 us) | 0.1181 (116.767 us) | 0.1180 (116.760 us) | 1.0000 (989.092 us) |
| Rotated | 0.0494 (37.106 us) | 0.0768 (57.719 us) | 0.0770 (57.867 us) | 1.0000 (751.246 us) |
| MixedPhase3 | 1.3681 (1293.582 us) | 0.9379 (886.839 us) | 1.0932 (1033.716 us) | 1.0000 (945.549 us) |
| MixedPhase12 | 1.2617 (1118.160 us) | 1.0735 (951.382 us) | 1.8055 (1600.053 us) | 1.0000 (886.231 us) |
