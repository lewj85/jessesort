// E264 shared high-entropy pointer backend.
//
// Deliberately included inside each owner namespace rather than wrapped in a
// shared namespace. This preserves the E260-E263 function/template identities
// and call/codegen boundaries local to each architectural owner while
// maintaining one source copy of the behaviorally identical backend.
//
// No include guard: this fragment is intentionally instantiated in multiple
// owner namespaces in the same translation unit.

template <typename T, typename Less>
inline void highEntropyCompareExchange(T* a, std::size_t i, std::size_t j, Less less) {
    T x = a[i], y = a[j];
    const bool swap = less(y, x);
    a[i] = swap ? y : x;
    a[j] = swap ? x : y;
}

template <typename T, typename Less>
inline void highEntropyNetwork9(T* a, Less less) {
    static constexpr unsigned p[][2] = {{0,3},{1,7},{2,5},{4,8},{0,7},{2,4},{3,8},{5,6},{0,2},{1,3},{4,5},{7,8},{1,4},{3,6},{5,7},{0,1},{2,4},{3,5},{6,8},{2,3},{4,5},{6,7},{1,2},{3,4},{5,6}};
    if constexpr (specializedIntegralEligible<T, Less> && sizeof(T) == 8) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 32
#endif
        for (const auto& q : p) highEntropyCompareExchange(a, q[0], q[1], less);
    } else {
        for (const auto& q : p) highEntropyCompareExchange(a, q[0], q[1], less);
    }
}

template <typename T, typename Less>
inline void highEntropyNetwork13(T* a, Less less) {
    static constexpr unsigned p[][2] = {{0,12},{1,10},{2,9},{3,7},{5,11},{6,8},{1,6},{2,3},{4,11},{7,9},{8,10},{0,4},{1,2},{3,6},{7,8},{9,10},{11,12},{4,6},{5,9},{8,11},{10,12},{0,5},{3,8},{4,7},{6,11},{9,10},{0,1},{2,5},{6,9},{7,8},{10,11},{1,3},{2,4},{5,6},{9,10},{1,2},{3,4},{5,7},{6,8},{2,3},{4,5},{6,7},{8,9},{3,4},{5,6}};
    if constexpr (specializedIntegralEligible<T, Less> && sizeof(T) == 8) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 64
#endif
        for (const auto& q : p) highEntropyCompareExchange(a, q[0], q[1], less);
    } else {
        for (const auto& q : p) highEntropyCompareExchange(a, q[0], q[1], less);
    }
}

template <typename T, typename Less>
inline void highEntropyInsertionFinish(T* a, std::size_t n, Less less) {
    if (n < 2) return;
    std::size_t pre = 1;
    if (n >= 13) { highEntropyNetwork13(a, less); pre = 13; }
    else if (n >= 9) { highEntropyNetwork9(a, less); pre = 9; }
    for (std::size_t i = pre; i < n; ++i) {
        T x = a[i];
        if (!less(x, a[i - 1])) continue;
        std::size_t j = i;
        do { a[j] = a[j - 1]; --j; } while (j && less(x, a[j - 1]));
        a[j] = x;
    }
}

template <typename T, typename Less>
inline void highEntropySmallSortBaseline(T* a, std::size_t n, Less less) {
    if (n < 18) {
        highEntropyInsertionFinish(a, n, less);
        return;
    }

    const std::size_t mid = n / 2;
    highEntropyInsertionFinish(a, mid, less);
    highEntropyInsertionFinish(a + mid, n - mid, less);

    std::array<T, 32> scratch{};
    std::size_t left = 0;
    std::size_t right = mid;
    std::ptrdiff_t leftRev = static_cast<std::ptrdiff_t>(mid) - 1;
    std::ptrdiff_t rightRev = static_cast<std::ptrdiff_t>(n) - 1;
    std::size_t out = 0;
    std::ptrdiff_t outRev = static_cast<std::ptrdiff_t>(n) - 1;

    for (std::size_t i = 0; i < mid; ++i) {
        const bool takeRight = less(a[right], a[left]);
        scratch[out++] = takeRight ? a[right++] : a[left++];

        const bool takeRightRev =
            !less(a[static_cast<std::size_t>(rightRev)],
                  a[static_cast<std::size_t>(leftRev)]);
        scratch[static_cast<std::size_t>(outRev--)] =
            takeRightRev ? a[static_cast<std::size_t>(rightRev--)]
                         : a[static_cast<std::size_t>(leftRev--)];
    }

    if ((n & 1u) != 0u) {
        const bool leftNonempty =
            static_cast<std::ptrdiff_t>(left) <= leftRev;
        scratch[out] = leftNonempty ? a[left] : a[right];
    }
    std::copy_n(scratch.begin(), n, a);
}

template <typename T, typename Less>
inline void highEntropySmallSortIpnsortStyle(T* a, std::size_t n, Less less) {
    // Rust's current integer network path is already algorithmically almost
    // identical to Jesse's: 9/13 network + insertion completion, then a
    // bidirectional merge for len>=18. E166 tests its remaining branchless
    // pointer-arithmetic merge shape and single-loop half finishing.
    if (n < 18) {
        highEntropyInsertionFinish(a, n, less);
        return;
    }

    const std::size_t mid = n / 2;

    // Use one loop-shaped dispatch to mirror Rust's anti-unrolling structure.
    T* region = a;
    std::size_t regionLen = mid;
    for (;;) {
        highEntropyInsertionFinish(region, regionLen, less);
        if (region != a) break;
        region = a + mid;
        regionLen = n - mid;
    }

    std::array<T, 32> scratch{};
    const T* left = a;
    const T* right = a + mid;
    const T* leftRev = a + mid - 1;
    const T* rightRev = a + n - 1;
    T* dst = scratch.data();
    T* dstRev = scratch.data() + n - 1;

    for (std::size_t k = 0; k < mid; ++k) {
        const bool takeLeft = !less(*right, *left);
        const T* srcUp = takeLeft ? left : right;
        *dst++ = *srcUp;
        left += static_cast<std::size_t>(takeLeft);
        right += static_cast<std::size_t>(!takeLeft);

        const bool takeRightRev = !less(*rightRev, *leftRev);
        const T* srcDown = takeRightRev ? rightRev : leftRev;
        *dstRev-- = *srcDown;
        rightRev -= static_cast<std::size_t>(takeRightRev);
        leftRev -= static_cast<std::size_t>(!takeRightRev);
    }

    if ((n & 1u) != 0u) {
        const T* leftEnd = leftRev + 1;
        const bool leftNonempty = left < leftEnd;
        *dst = *(leftNonempty ? left : right);
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(a, scratch.data(), n * sizeof(T));
    } else {
        std::copy_n(scratch.begin(), n, a);
    }
}

struct HighEntropySmallSortMetrics {
    std::uint64_t calls = 0;
    std::uint64_t elements = 0;
    std::uint64_t nanoseconds = 0;
};

template <typename T, typename Less>
inline void highEntropySmallSort(
    T* a, std::size_t n, Less less,
    bool enableIpnsortStyleSmallSort = false,
    HighEntropySmallSortMetrics* metrics = nullptr
) {
    using SmallClock = std::chrono::steady_clock;
    const auto begin = metrics ? SmallClock::now() : SmallClock::time_point{};

    if (enableIpnsortStyleSmallSort && n >= 18)
        highEntropySmallSortIpnsortStyle(a, n, less);
    else
        highEntropySmallSortBaseline(a, n, less);

    if (metrics) {
        const auto end = SmallClock::now();
        ++metrics->calls;
        metrics->elements += n;
        metrics->nanoseconds += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count());
    }
}

template <typename T, typename Less>
inline std::size_t highEntropyMedian3Index(T* a, std::size_t x, std::size_t y, std::size_t z, Less less) {
    if (less(a[y], a[x])) std::swap(x, y);
    if (less(a[z], a[y])) std::swap(y, z);
    if (less(a[y], a[x])) std::swap(x, y);
    return y;
}

template <typename T, typename Less>
inline std::size_t highEntropyAdaptiveMedian3Rec(
    T* a,
    std::size_t x,
    std::size_t y,
    std::size_t z,
    std::size_t sectionSize,
    Less less
) {
    // E164: C++ translation of ipnsort's adaptive pseudomedian geometry.
    // Each level samples the starts of the 0/8, 4/8, and 7/8 subregions.
    if (sectionSize * 8 >= 64) {
        const std::size_t s = sectionSize / 8;
        if (s != 0) {
            x = highEntropyAdaptiveMedian3Rec(
                a, x, x + 4 * s, x + 7 * s, s, less);
            y = highEntropyAdaptiveMedian3Rec(
                a, y, y + 4 * s, y + 7 * s, s, less);
            z = highEntropyAdaptiveMedian3Rec(
                a, z, z + 4 * s, z + 7 * s, s, less);
        }
    }
    return highEntropyMedian3Index(a, x, y, z, less);
}

template <typename T, typename Less>
inline std::size_t highEntropyPivotIndex(
    T* a, std::size_t n, Less less, bool enableAdaptiveLargePivot = true
) {
    // E164: recursive adaptive sampling pays for itself only on large high-entropy
    // partitions. Dominant-value/equality-filtered routes explicitly keep the
    // previous median-of-3 / fixed-ninther policy.
    if (enableAdaptiveLargePivot && n >= 8192) {
        const std::size_t s = n / 8;
        return highEntropyAdaptiveMedian3Rec(
            a, 0, 4 * s, 7 * s, s, less);
    }
    if (n < 128) return highEntropyMedian3Index(a, 0, n / 2, n - 1, less);
    const std::size_t s = n / 8, m = n / 2;
    const std::size_t a1 = highEntropyMedian3Index(a, 0, s, 2 * s, less);
    const std::size_t a2 = highEntropyMedian3Index(a, m - s, m, m + s, less);
    const std::size_t a3 = highEntropyMedian3Index(a, n - 1 - 2 * s, n - 1 - s, n - 1, less);
    return highEntropyMedian3Index(a, a1, a2, a3, less);
}

template <typename T, typename Pred>
inline std::size_t highEntropyCyclicPred(T* a, std::size_t len, Pred pred) {
    if (len == 0) return 0;
    T gap = a[0];
    T* gapPos = a;
    std::size_t num = 0;
    std::size_t i = 1;
    auto body = [&](T x, T* right) {
        const bool take = pred(x);
        T* left = a + num;
        *gapPos = *left;
        *left = x;
        gapPos = right;
        num += static_cast<std::size_t>(take);
    };
    for (; i + 1 < len; i += 2) { body(a[i], a + i); body(a[i + 1], a + i + 1); }
    for (; i < len; ++i) body(a[i], a + i);
    const bool take = pred(gap);
    T* left = a + num;
    *gapPos = *left;
    *left = gap;
    num += static_cast<std::size_t>(take);
    return num;
}

template <typename T, typename Pred>
inline std::size_t highEntropyCyclicPredIpnsortStyle8(
    T* v, std::size_t len, Pred pred
) {
    if (len == 0) return 0;

    T gapValue;
    std::memcpy(&gapValue, v, sizeof(T));
    T* gapPos = v;
    T* right = v + 1;
    std::size_t numLt = 0;

    auto body = [&](T* rightPtr) {
        const bool rightIsLt = pred(*rightPtr);
        T* left = v + numLt;
        std::memmove(gapPos, left, sizeof(T));
        std::memcpy(left, rightPtr, sizeof(T));
        gapPos = rightPtr;
        numLt += static_cast<std::size_t>(rightIsLt);
    };

    T* const end = v + len;
    T* const unrollEnd = v + len - 1;
    while (right < unrollEnd) {
        body(right); ++right;
        body(right); ++right;
    }
    while (right < end) {
        body(right);
        ++right;
    }

    const bool gapIsLt = pred(gapValue);
    T* left = v + numLt;
    std::memmove(gapPos, left, sizeof(T));
    std::memcpy(left, &gapValue, sizeof(T));
    numLt += static_cast<std::size_t>(gapIsLt);
    return numLt;
}

template <typename T, typename Less>
std::size_t highEntropyPartitionIpnsortStyle8(
    T* a, std::size_t n, std::size_t pivotIndex, Less less
) {
    std::swap(a[0], a[pivotIndex]);
    const T pivot = a[0];
    const std::size_t p = highEntropyCyclicPredIpnsortStyle8(
        a + 1, n - 1, [&](const T& x) { return less(x, pivot); });
    std::swap(a[0], a[p]);
    return p;
}

template <typename T, typename Less>
std::size_t highEntropyPartitionIpnsortStyle8KnownPivot(
    T* a, std::size_t n, std::size_t pivotIndex, Less less, T pivot
) {
    std::swap(a[0], a[pivotIndex]);
    const std::size_t p = highEntropyCyclicPredIpnsortStyle8(
        a + 1, n - 1, [&](const T& x) { return less(x, pivot); });
    std::swap(a[0], a[p]);
    return p;
}

template <typename T, typename Less>
inline std::size_t highEntropyPartition(
    T* a, std::size_t n, std::size_t pivotIndex, Less less
) {
    // E334 audit: E165's u64-only ipnsort-style gap-loop specialization no
    // longer improves the integrated backend under isolated-process timing.
    // Use the common Jesse cyclic kernel for production. The ipnsort-style
    // helpers remain above as reproducible experimental/reference code.
    std::swap(a[pivotIndex], a[n - 1]);
    const T pivot = a[n - 1];
    const std::size_t p = highEntropyCyclicPred(
        a, n - 1, [&](const T& x) { return less(x, pivot); });
    std::swap(a[p], a[n - 1]);
    return p;
}

template <typename T, typename Less>
inline std::size_t highEntropyPartitionKnownPivot(
    T* a, std::size_t n, std::size_t pivotIndex, Less less, T pivot
) {
    (void)pivot;
    return highEntropyPartition(a, n, pivotIndex, less);
}

// E266: outlined branch-friendly heapsort fallback translated from current
// upstream ipnsort. This is deliberately cold in normal high-entropy sorting;
// its purpose is to make the introspective depth-limit escape substantially
// cheaper without enlarging the hot recursive driver.
template <typename T, typename Less>
inline void highEntropyHeapSiftDown(T* a, std::size_t len, std::size_t node, Less less) {
    for (;;) {
        std::size_t child = 2 * node + 1;
        if (child >= len) break;
        if (child + 1 < len)
            child += static_cast<std::size_t>(less(a[child], a[child + 1]));
        if (!less(a[node], a[child])) break;
        std::swap(a[node], a[child]);
        node = child;
    }
}

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void highEntropyHeapSortFallback(T* a, std::size_t len, Less less) {
    for (std::size_t i = len + len / 2; i-- > 0;) {
        std::size_t siftIdx;
        std::size_t activeLen;
        if (i >= len) {
            siftIdx = i - len;
            activeLen = len;
        } else {
            std::swap(a[0], a[i]);
            siftIdx = 0;
            activeLen = i;
        }
        highEntropyHeapSiftDown(a, activeLen, siftIdx, less);
    }
}

struct HighEntropyControlFlowMetrics {
    std::uint64_t functionEntries = 0;
    std::uint64_t loopIterations = 0;
    std::uint64_t maxCallDepth = 0;
};


template <typename T, typename Less>
inline bool highEntropyAncestorEquivalent(const T& ancestor, const T& pivot, Less less) {
    if constexpr (specializedIntegralEligible<T, Less>) {
        // E335 audit cleanup: for the specialized integral/default-less domain,
        // order-equivalence is exact value equality.  Avoid paying an ordered
        // comparator on the overwhelmingly non-equal Random path while keeping
        // generic strict-weak-order semantics unchanged.
        return ancestor == pivot;
    } else {
        return !less(ancestor, pivot);
    }
}
template <typename T, typename Less>
void highEntropyQuickSort(T* a, std::size_t n, int depth, Less less,
                          bool hasAncestor = false, T ancestor = T{},
                          bool enableAdaptiveLargePivot = true,
                          bool enableIpnsortStyleSmallSort = true,
                          HighEntropySmallSortMetrics* smallSortMetrics = nullptr,
                          bool iterateRight = true,
                          HighEntropyControlFlowMetrics* controlMetrics = nullptr,
                          std::uint64_t callDepth = 1) {
    if (controlMetrics) {
        ++controlMetrics->functionEntries;
        controlMetrics->maxCallDepth =
            std::max(controlMetrics->maxCallDepth, callDepth);
    }

    for (;;) {
        if (controlMetrics) ++controlMetrics->loopIterations;

        if (n <= 32) {
            using SmallClock = std::chrono::steady_clock;
            const auto smallBegin =
                smallSortMetrics ? SmallClock::now() : SmallClock::time_point{};

            // E340: duplicate filtering can leave 18-32 element leaves that are
            // already entirely equal.  On the specialized integral/default-less
            // path, exact equality is the ordering equivalence relation, so avoid
            // paying the full E166 smallsort for those leaves.  Restrict the scan
            // to E166-sized leaves; smaller leaves keep the cheaper insertion path.
            if constexpr (specializedIntegralEligible<T, Less>) {
                if (n >= 18) {
                    bool allEqual = true;
                    for (std::size_t qi = 1; qi < n; ++qi) {
                        if (a[qi] != a[0]) { allEqual = false; break; }
                    }
                    if (allEqual) {
                        if (smallSortMetrics) {
                            const auto smallEnd = SmallClock::now();
                            ++smallSortMetrics->calls;
                            smallSortMetrics->elements += n;
                            smallSortMetrics->nanoseconds += static_cast<std::uint64_t>(
                                std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    smallEnd - smallBegin).count());
                        }
                        return;
                    }
                }
            }

            if (n < 18) {
                highEntropyInsertionFinish(a, n, less);
            } else if constexpr (specializedIntegralEligible<T, Less>) {
                if (enableIpnsortStyleSmallSort)
                    highEntropySmallSortIpnsortStyle(a, n, less);
                else
                    highEntropySmallSortBaseline(a, n, less);
            } else {
                highEntropySmallSortBaseline(a, n, less);
            }

            if (smallSortMetrics) {
                const auto smallEnd = SmallClock::now();
                ++smallSortMetrics->calls;
                smallSortMetrics->elements += n;
                smallSortMetrics->nanoseconds += static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        smallEnd - smallBegin).count());
            }
            return;
        }

        if (depth <= 0) {
            highEntropyHeapSortFallback(a, n, less);
            return;
        }

        const std::size_t pi =
            highEntropyPivotIndex(a, n, less, enableAdaptiveLargePivot);
        const T pivot = a[pi];

        if (hasAncestor && highEntropyAncestorEquivalent(ancestor, pivot, less)) {
            const std::size_t eq = highEntropyCyclicPred(
                a, n, [&](const T& x) { return !less(ancestor, x); });

            if (!iterateRight) {
                highEntropyQuickSort(
                    a + eq, n - eq, depth - 1, less, false, T{},
                    enableAdaptiveLargePivot, enableIpnsortStyleSmallSort,
                    smallSortMetrics, false, controlMetrics, callDepth + 1);
                return;
            }

            a += eq;
            n -= eq;
            --depth;
            hasAncestor = false;
            ancestor = T{};
            continue;
        }

        const std::size_t p = highEntropyPartition(a, n, pi, less);

        if (!iterateRight) {
            highEntropyQuickSort(
                a, p, depth - 1, less, false, T{},
                enableAdaptiveLargePivot, enableIpnsortStyleSmallSort,
                smallSortMetrics, false, controlMetrics, callDepth + 1);
            highEntropyQuickSort(
                a + p + 1, n - p - 1, depth - 1, less, true, pivot,
                enableAdaptiveLargePivot, enableIpnsortStyleSmallSort,
                smallSortMetrics, false, controlMetrics, callDepth + 1);
            return;
        }

        // E167 candidate: recurse only into the left partition and iterate on
        // the right, matching ipnsort's control-flow shape.
        highEntropyQuickSort(
            a, p, depth - 1, less, false, T{},
            enableAdaptiveLargePivot, enableIpnsortStyleSmallSort,
            smallSortMetrics, true, controlMetrics, callDepth + 1);

        a += p + 1;
        n -= p + 1;
        --depth;
        hasAncestor = true;
        ancestor = pivot;
    }
}


// E263: fixed-control production driver for the ordinary high-entropy route.
// Keeping profiling/experimental knobs out of this recursive body avoids GCC
// clone proliferation while preserving the same pivot, partition, smallsort,
// equality-filter, and heapsort semantics.
template <typename T, typename Less>
void highEntropyQuickSortProduction(
    T* a, std::size_t n, int depth, Less less,
    bool hasAncestor = false, T ancestor = T{}
) {
    for (;;) {
        if (n <= 32) {
            if constexpr (specializedIntegralEligible<T, Less>) {
                if (n >= 18) {
                    bool allEqual = true;
                    for (std::size_t qi = 1; qi < n; ++qi) {
                        if (a[qi] != a[0]) { allEqual = false; break; }
                    }
                    if (allEqual) return;
                }
            }
            if (n < 18) {
                highEntropyInsertionFinish(a, n, less);
            } else if constexpr (specializedIntegralEligible<T, Less>) {
                highEntropySmallSortIpnsortStyle(a, n, less);
            } else {
                highEntropySmallSortBaseline(a, n, less);
            }
            return;
        }
        if (depth <= 0) {
            highEntropyHeapSortFallback(a, n, less);
            return;
        }
        const std::size_t pi = highEntropyPivotIndex(a, n, less, true);
        const T pivot = a[pi];
        if (hasAncestor && highEntropyAncestorEquivalent(ancestor, pivot, less)) {
            const std::size_t eq = highEntropyCyclicPred(
                a, n, [&](const T& x) { return !less(ancestor, x); });
            a += eq;
            n -= eq;
            --depth;
            hasAncestor = false;
            ancestor = T{};
            continue;
        }
        const std::size_t p = highEntropyPartitionKnownPivot(a, n, pi, less, pivot);
        highEntropyQuickSortProduction(a, p, depth - 1, less, false, T{});
        a += p + 1;
        n -= p + 1;
        --depth;
        hasAncestor = true;
        ancestor = pivot;
    }
}

