# E229 — Layer-tagged variation migration

## 229A — Migration plan

E229 replaces opaque variation-number filenames for maintained implementations with descriptive layer-tagged names while preserving V1–V11 as historical identifiers in all pre-E229 experiment records.

The migration is intentionally staged. Each step is completed and checked before the next begins:

1. **229A — document this migration plan.** No algorithm or benchmark behavior changes.
2. **229B — freeze/document the pre-E225 V1–V11 legacy identities** and explain why E225/E227 direct-to-merge descendants are preserved as separate variations rather than silently replacing the Patience-centered controls.
3. **229C — finalize the layer-tag schema and authoritative old-ID → new-name tables** for the 20 maintained headers: 11 pre-E225 controls plus 9 E225/E227 direct-merge descendants.
4. **229D — rename only headers and source translation units.** Recover V1–V11 from the audited/corrected E224 checkpoint (pre-E225); recover the direct-merge descendants of V1–V9 from E228. Do not modify algorithm bodies or benchmark dispatch in this step.
5. **229E — choose the small representative default benchmark set.** All 20 remain available; only a focused subset runs under `make run` by default.
6. **229F — refactor benchmark naming/selection.** Replace V-number dispatch with a descriptor map from short names to the layer-tagged implementations. Preserve generators, seeds, timing, validation, resume, trial counts, and statistics.
7. **229G — run all maintained variations once** through the canonical benchmark to validate the migration and establish a fresh post-E229 baseline.
8. **229H — final documentation pass.** Update the top-level experiment-log/README descriptions and E229 record to the new schema while leaving historical experiment entries using V1/V2/etc. unchanged.

### Safety / provenance rules

- The audited/corrected E224 checkpoint is the sole source for pre-E225 V1–V11 controls. The invalid E224 counting-sort candidate is never used as production source.
- E228 is the source for the E225/E227 direct-merge descendants of V1–V9.
- V10/V11 do not receive direct-merge descendants because E225's natural-run backend uses O(n) auxiliary storage and would violate their allocation-free identity.
- Renaming does not imply behavior changes. Namespace/body changes are out of scope for 229D.
- Historical experiment-log entries retain the V-number terminology under which their results were produced.

## 229B — Frozen pre-E225 V1–V11 legacy identities and E225 divergence

The pre-E225 V1–V11 implementations are preserved as maintained legacy controls. Their source of truth for E229 is the audited/corrected E224 checkpoint, which predates E225 and does **not** contain the invalid E224 counting-sort experiment in production code.

| Legacy ID | Pre-E225 defining behavior |
|---|---|
| V1 | Physical vector-backed dual-Patience piles; non-freezing; shared adjacent-pair post-flatten merge policy. |
| V2 | Simulated dual-Patience games using value tails + packed blueprint tags; non-freezing; streaming buffered reconstruction; primary speed-oriented simulated control. |
| V3 | V2-style simulated non-freezing decomposition with in-place permutation-cycle flattening; keeps the V3 reconstruction identity even when slower. |
| V4 | Simulated early-freeze family; nominal 50% freeze decision with retained power-of-two extension policy; one large deferred overflow run; adaptive adjacent/PowerSort-style merge scheduling under its retained conditions. |
| V5 | Simulated early-freeze family; deferred 32-element overflow bands; exact full bands use the retained fixed 32-element network; variation-specific merge deferral retained. |
| V6 | Simulated early-freeze family; live/adaptive overflow handling using 32-element live bands or natural-run overflow paths based on discovered structure; adjacent/TimSort-style merge routing retained. |
| V7 | V2-derived AVX2/capped-probe research path; first 128 values use small per-game pile caps with temporary probe spill; no general freeze. |
| V8 | Linked-blueprint locality family; linked element/span reconstruction remains defining when Patience reconstruction is reached, including retained first-merge fusion behavior. |
| V9 | Index-tail/genericity family; pile-tail metadata stores source indices rather than copies of `T`; move-only/non-copyable-capable path remains defining. |
| V10 | Allocation-free bounded baseline; fixed 63-asc + 64-desc source-index tails and <=64 fixed run descriptors; unsupported geometries use allocation-free partition/heapsort fallback. |
| V11 | Allocation-free shared-tail + low-run-merge extension; shared physical 127-index pool with logical 63/64 split, gated in-place low-run merging, E217 live run-descriptor reclamation past 64 runs, and E218 early repeated-overlap/Sawtooth routing. |

### Why E225/E227 are separate descendants

E225 added a generic bounded-natural-run pre-router: when the input is proven, without mutation, to consist of a small bounded set of long useful natural runs, the implementation can bypass Patience decomposition/blueprint construction and merge those source runs directly. E227 propagated that route to V1 and V3–V9.

That route is a large performance win, but it also changes which algorithmic phase performs the work. On accepted inputs it can bypass the very physical/simulated/frozen/linked/indexed Patience mechanism that a legacy variation exists to measure. Therefore E229 does **not** redefine pre-E225 V1–V9 to include direct merge. Instead it preserves both branches:

- **pre-E225 control:** retains the historical Patience-centered behavior;
- **direct-merge descendant:** adds the E225 natural-run pre-router in front of the same underlying variation and falls back to that variation when the route rejects.

This gives future experiments both a stable architectural control and the faster adaptive descendant. It also prevents an optimization that bypasses a defining phase from silently erasing the experimental identity of the original variation.

V10/V11 remain single allocation-free identities. E225's direct natural-run backend uses O(n) buffered storage, so attaching it to V10/V11 would violate their defining no-allocation constraint rather than create a legitimate descendant.

## 229C — Final layer-tag schema and authoritative names

### Filename/name grammar

Maintained variation filenames use:

`jessesort_<decomposition>_<router>_<reconstruction-and-merge>[_<other-policy>...].h`

The matching `.cpp` translation unit uses the identical stem.

- **Underscores separate major layers.**
- **Dashes combine multiple tags within one layer.**
- Names are intentionally verbose. Experimental clarity and grepability take priority over short filenames.
- Historical V numbers remain valid only as legacy aliases/history labels; new experiments should use the layer-tag name or a benchmark short descriptor that maps to it.

### Layer 1 — base decomposition architecture

Current tags:

- `physical` — literal physical Patience piles (legacy V1 family).
- `simulated` — value-tail + blueprint simulated Patience decomposition (legacy V2/V3 family).
- `simulated-frozen` — simulated Patience with retained early-freeze semantics (legacy V4–V6 family). `frozen` never implicitly means simulated; the compound tag says both.
- `simulated-avx2` — simulated decomposition with the defining AVX2 capped-probe front end (legacy V7).
- `linked` — linked-blueprint locality architecture (legacy V8).
- `indexed` — source-index pile-tail architecture/genericity path (legacy V9).
- `noalloc` — bounded allocation-free architecture (legacy V10).
- `noalloc-low-run-merge` — allocation-free architecture extended with gated low-run in-place merging/reclamation (legacy V11).

Future decomposition tags should describe a representation or decomposition mechanism, not a transient numeric threshold. A future `physical-frozen` and `simulated-frozen` can coexist explicitly.

### Layer 2 — front-end/router policy

Current tags:

- `probe-routed` — pre-E225 allocating-family routing: existing monotone/probe/shape decisions may tune the Patience path, but there is no E225 direct natural-run merge escape.
- `direct-merge-probe-routed` — E225/E227 bounded-natural-run direct-merge route is attempted first; rejection continues into the same underlying `probe-routed` variation.
- `bounded-fallback` — V10 bounded no-allocation route with unsupported geometry sent to allocation-free partition/heapsort fallback.
- `overlap-routed` — V11's allocation-free route including E218 early repeated-overlap/Sawtooth handling plus its bounded low-run path.
- `repeated-probe` is reserved for the planned multi-probe router and is not attached to any E229 variation yet.

Future router names should identify the actual policy rather than an experiment number.

### Layer 3 — reconstruction / merge policy

Current compound tags:

- `adjacent-adaptive-buffered` — buffered reconstruction/merge family whose ordinary merge tree is adjacent-pair based with retained shape/kernel adaptations.
- `inplace-flatten-adjacent-adaptive-buffered` — V3-style in-place blueprint cycle flattening followed by the shared adaptive adjacent merge machinery.
- `adjacent-powersort-adaptive-buffered` — primarily adjacent buffered merging with retained conditional PowerSort-style scheduling.
- `adjacent-timsort-adaptive-buffered` — primarily adjacent buffered merging with retained conditional TimSort-style scheduling.
- `linked-fused-adjacent-adaptive-buffered` — linked reconstruction with retained first-merge fusion and adaptive adjacent downstream merging.
- `inplace-adaptive` — allocation-free in-place run handling whose path depends on proven geometry.

`-adaptive` means discovered shape can select non-fixed merge/reconstruction behavior. `-fixed` is reserved for a genuinely fixed merge/reconstruction policy. When a primary merge choice exists, it appears before `-adaptive`/`-fixed` (for example `adjacent-adaptive`, `powersort-fixed`).

### Layer 4 — other policy tags

Current examples used in E229:

- `single-overflow`
- `deferred-bands32`
- `live-bands32`
- `capped-probe8x128`
- `move-only`
- `partition-heapsort-fallback`
- `run-reclaim64`

Numeric tags are allowed here when the number is a defining architectural constant worth distinguishing, not merely a tunable benchmark parameter.

### Authoritative E229 names — preserved pre-E225 controls

| Legacy ID | E229 layer-tagged name |
|---|---|
| V1 | `jessesort_physical_probe-routed_adjacent-adaptive-buffered` |
| V2 | `jessesort_simulated_probe-routed_adjacent-adaptive-buffered` |
| V3 | `jessesort_simulated_probe-routed_inplace-flatten-adjacent-adaptive-buffered` |
| V4 | `jessesort_simulated-frozen_probe-routed_adjacent-powersort-adaptive-buffered_single-overflow` |
| V5 | `jessesort_simulated-frozen_probe-routed_adjacent-adaptive-buffered_deferred-bands32` |
| V6 | `jessesort_simulated-frozen_probe-routed_adjacent-timsort-adaptive-buffered_live-bands32` |
| V7 | `jessesort_simulated-avx2_probe-routed_adjacent-adaptive-buffered_capped-probe8x128` |
| V8 | `jessesort_linked_probe-routed_linked-fused-adjacent-adaptive-buffered` |
| V9 | `jessesort_indexed_probe-routed_adjacent-adaptive-buffered_move-only` |
| V10 | `jessesort_noalloc_bounded-fallback_inplace-adaptive_partition-heapsort-fallback` |
| V11 | `jessesort_noalloc-low-run-merge_overlap-routed_inplace-adaptive_run-reclaim64` |

### Authoritative E229 names — E225/E227 direct-merge descendants

| Parent legacy ID | E229 direct-merge descendant |
|---|---|
| V1 | `jessesort_physical_direct-merge-probe-routed_adjacent-adaptive-buffered` |
| V2 | `jessesort_simulated_direct-merge-probe-routed_adjacent-adaptive-buffered` |
| V3 | `jessesort_simulated_direct-merge-probe-routed_inplace-flatten-adjacent-adaptive-buffered` |
| V4 | `jessesort_simulated-frozen_direct-merge-probe-routed_adjacent-powersort-adaptive-buffered_single-overflow` |
| V5 | `jessesort_simulated-frozen_direct-merge-probe-routed_adjacent-adaptive-buffered_deferred-bands32` |
| V6 | `jessesort_simulated-frozen_direct-merge-probe-routed_adjacent-timsort-adaptive-buffered_live-bands32` |
| V7 | `jessesort_simulated-avx2_direct-merge-probe-routed_adjacent-adaptive-buffered_capped-probe8x128` |
| V8 | `jessesort_linked_direct-merge-probe-routed_linked-fused-adjacent-adaptive-buffered` |
| V9 | `jessesort_indexed_direct-merge-probe-routed_adjacent-adaptive-buffered_move-only` |

These 20 names are the maintained E229 variation bank. The direct-merge descendants reuse the same downstream implementation as their parent after the E225 route rejects; the name therefore changes only the router layer.

## 229E — Default benchmark set decision

All 20 maintained variations remain buildable/selectable. `make run` should intentionally exercise only six representative configurations so routine output remains readable and runtime does not scale linearly with every research branch added to the bank.

### Chosen six defaults

| Role | Layer-tagged variation | Reason for default inclusion |
|---|---|---|
| Physical Patience control | `jessesort_physical_probe-routed_adjacent-adaptive-buffered` | Pre-E225 V1: literal physical piles; reference for the original decomposition. |
| Simulated Patience control | `jessesort_simulated_probe-routed_adjacent-adaptive-buffered` | Pre-E225 V2: primary simulated-games control and historical champion architecture. |
| Freeze-family control | `jessesort_simulated-frozen_probe-routed_adjacent-powersort-adaptive-buffered_single-overflow` | Pre-E225 V4: simplest retained 50%-freeze architecture with one overflow run. |
| Genericity/index-tail control | `jessesort_indexed_probe-routed_adjacent-adaptive-buffered_move-only` | Pre-E225 V9: meaningfully distinct source-index tails and move-only/non-copyable capability. |
| Allocation-free control | `jessesort_noalloc-low-run-merge_overlap-routed_inplace-adaptive_run-reclaim64` | Pre-E225 V11/E218: strongest current no-allocation branch; V10 remains available as the narrower bounded baseline. |
| Current allocating champion | `jessesort_simulated_direct-merge-probe-routed_adjacent-adaptive-buffered` | E225/E228 direct-merge descendant of V2; current best-performing general allocating configuration. |

### Why V11 rather than V10 in the default set

V10 remains valuable as the narrow allocation-free baseline, but routine benchmarking needs one no-allocation representative. V11 retains the no-heap identity while incorporating the successful bounded low-run merge/reclamation work, so it is the more representative current no-allocation configuration. V10 stays selectable for experiments specifically about the allocation-free baseline/fallback boundary.

### Why `indexed` is the sixth

The first five requested roles already cover physical vs simulated decomposition, freezing, allocation-free behavior, and the current direct-merge champion. Adding V3 would mainly add a reconstruction comparison, while V9 adds a genuinely different genericity contract: source-index tails and move-only/non-copyable support. For a small default suite, that is broader architectural coverage.

No code or benchmark behavior is changed in 229E; this section only fixes the default-set decision that 229F will implement.

## 229F — Benchmark refactor completed

The benchmark now exposes all 20 maintained layer-tagged variations through short descriptors. The descriptor→header mapping is a `constexpr AlgorithmSpec` table near the top of `benchmarks/benchmark.cpp`; each entry records the short CLI name, authoritative layer-tagged header filename, and whether it belongs to the six-player default set.

Selection semantics:

- omitted selector or `default` → the six 229E representatives + paired `std::sort`;
- `all` → all 20 JesseSort variations + paired `std::sort`;
- comma-separated descriptors such as `simulated,simulated-direct,noalloc-low-run` → only those JesseSort variations + paired `std::sort`.

`make run` / `make run500` now use the six-player default. `make run-all` explicitly requests all maintained variations.

To allow legacy and direct descendants to coexist in one executable, E229F mechanically gives duplicated branches unique C++ namespaces/include guards and retargets sibling includes to the matching legacy/direct branch. This is identity/build plumbing only; no sorting condition, threshold, comparator operation, generator, seed, timer, trial count, validation rule, resume rule, or statistic was intentionally changed. Both a six-default 10k correctness smoke and an all-20 10k correctness smoke passed.

## 229G — All-variation validation baseline completed

All 20 maintained JesseSort variations and paired `std::sort` completed the 14-input benchmark with 500 trials at 1k/10k/100k and 50 trials at 1m. No correctness failures occurred. CPU: Intel Xeon Platinum 8573C; compiler GCC 14.2.0. Full four-size tables are in `benchmarks/baselines/e229/e229_all_variations_combined.md`; compressed raw trials are retained per size.

The first attempted 100k timing run was discarded because multiple tool-level timeout/resume invocations overlapped. A clean 100k run was then executed as exactly one benchmark process and is the only 100k dataset included in the E229 baseline.

## 229H — Documentation migration completed

README and the authoritative top section of `experiment_log.txt` now use the layer-tag schema and short benchmark descriptors. Pre-E229 experiment records intentionally retain V1/V2/etc. terminology. E229 is the boundary after which new experiment records should identify maintained variations by layer tags/short descriptors rather than inventing new opaque V numbers.
