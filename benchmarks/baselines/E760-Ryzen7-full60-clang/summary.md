# Canonical benchmark summary — CDE history-conditioned median only

Cells show `CDE median µs (std::sort / JesseSort median speedup)`; lower µs and higher × are better. Positions A (fresh) and B (transition) are intentionally excluded from these Markdown tables. Full ABCDE raw observations remain in `paired_trials.csv`; `summary.csv` preserves median diagnostics for all positions.

## n=10000

| Input | simulated-direct_live-phase | noalloc | strict-noalloc |
|---|---:|---:|---:|
| Random | 90.28 µs (3.15×) | **88.67 µs (3.20×)** | 89.44 µs (3.18×) |
| Sorted | 4.28 µs (1.53×) | **3.56 µs (1.84×)** | 3.63 µs (1.80×) |
| Reverse | **5.60 µs (1.59×)** | 6.39 µs (1.40×) | 5.61 µs (1.59×) |
| Sorted+Noise(5%) | **69.63 µs (1.55×)** | 107.80 µs (1.00×) | 93.11 µs (1.16×) |
| Random%25 | 31.71 µs (3.51×) | **30.92 µs (3.61×)** | 30.98 µs (3.59×) |
| Diverging | **44.31 µs (1.11×)** | 77.84 µs (0.65×) | 78.07 µs (0.66×) |
| Sawtooth | 79.72 µs (0.71×) | **60.78 µs (0.98×)** | 256.72 µs (0.22×) |
| MixedDirectionRuns | 60.65 µs (1.43×) | **56.81 µs (1.52×)** | 97.16 µs (0.90×) |
| OrganPipe | **24.98 µs (6.06×)** | 25.36 µs (5.96×) | 77.11 µs (1.96×) |
| Rotated | **5.18 µs (11.42×)** | 14.58 µs (4.00×) | 14.49 µs (4.10×) |

## n=100000

| Input | simulated-direct_live-phase | noalloc | strict-noalloc |
|---|---:|---:|---:|
| Random | 1062.08 µs (3.41×) | **1043.81 µs (3.48×)** | 1049.19 µs (3.46×) |
| Sorted | 42.57 µs (1.54×) | **35.79 µs (1.83×)** | 36.01 µs (1.82×) |
| Reverse | **57.62 µs (1.50×)** | 63.68 µs (1.36×) | 57.76 µs (1.50×) |
| Sorted+Noise(5%) | **743.23 µs (1.71×)** | 2018.78 µs (0.63×) | 1183.26 µs (1.08×) |
| Random%25 | 300.55 µs (3.66×) | 293.20 µs (3.75×) | **292.24 µs (3.76×)** |
| Diverging | **865.29 µs (1.11×)** | 1085.38 µs (0.88×) | 1090.48 µs (0.88×) |
| Sawtooth | **958.47 µs (1.17×)** | 1486.51 µs (0.75×) | 3799.16 µs (0.30×) |
| MixedDirectionRuns | **658.76 µs (1.36×)** | 1090.93 µs (0.82×) | 1200.74 µs (0.75×) |
| OrganPipe | **224.37 µs (10.81×)** | 300.64 µs (8.07×) | 319.43 µs (7.59×) |
| Rotated | **142.78 µs (3.97×)** | 149.90 µs (3.72×) | 144.86 µs (3.91×) |

