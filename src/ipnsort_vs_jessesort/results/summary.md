# ipnsort vs current JesseSort HEAD

- type: u64
- n: 100000
- trials per pattern: 500
- warmups per pattern: 2
- shared cold-like preconditioner: false
- inputs: E189 canonical JesseSort benchmark inputs, order-preserving int->u64 encoding

| Pattern | ipnsort µs | V1 µs | V2 µs | V5 µs | V2/ipnsort | best Jesse/ipnsort |
|---|---:|---:|---:|---:|---:|---:|
| Random | 935.874 | 1122.952 | 1118.870 | 1120.156 | 1.196 | 1.196 |
| Sorted | 21.238 | 21.244 | 31.832 | 28.248 | 1.499 | 1.000 |
| Reverse | 27.117 | 31.072 | 94.377 | 38.141 | 3.480 | 1.146 |
| Sorted+Noise(5%) | 990.978 | 1881.452 | 845.748 | 1152.646 | 0.853 | 0.853 |
| Sorted+Noise(10%) | 1026.375 | 2280.802 | 1172.635 | 1454.644 | 1.143 | 1.143 |
| Random%25 | 252.484 | 105.755 | 105.345 | 105.541 | 0.417 | 0.417 |
| Alternating | 900.421 | 1502.089 | 258.032 | 523.664 | 0.287 | 0.287 |
| Sawtooth | 588.284 | 2198.497 | 1212.097 | 1893.091 | 2.060 | 2.060 |
| MixedDirectionRuns | 893.446 | 1304.870 | 303.015 | 693.691 | 0.339 | 0.339 |
| BlockSorted | 817.308 | 1344.300 | 261.424 | 584.482 | 0.320 | 0.320 |
| OrganPipe | 995.253 | 936.735 | 177.397 | 256.078 | 0.178 | 0.178 |
| Rotated | 762.149 | 869.727 | 108.520 | 507.721 | 0.142 | 0.142 |
