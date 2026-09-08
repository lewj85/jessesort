// E264 shared vector high-entropy sampling/direct-entry router.
//
// Included inside simulated/direct/linked owner namespaces. The phase-range
// engine keeps its span-adapted entry layer, but calls the same pointer backend.
// No include guard: intentional multi-namespace instantiation.

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool highEntropySampleCandidate(const std::vector<T>& arr, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)less; return false;
    } else {
        if (arr.size() < 10000) return false;
        std::array<std::uint64_t, 128> keys{};
        std::array<unsigned char, 128> valid{};
        std::size_t distinct = 0;
        const std::size_t end = std::min<std::size_t>(64, arr.size());
        for (std::size_t i = 0; i < end; ++i) {
            const std::uint64_t key = specializedIntegralKey(arr[i]);
            std::size_t slot = static_cast<std::size_t>((key * 11400714819323198485ULL) >> 57);
            for (;;) {
                if (!valid[slot]) { valid[slot] = 1; keys[slot] = key; ++distinct; break; }
                if (keys[slot] == key) break;
                slot = (slot + 1) & 127u;
            }
        }
        return distinct >= 44;
    }
}

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool trySortHighEntropyPartitionDirect(std::vector<T>& arr, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)less; return false;
    } else {
        if (!highEntropySampleCandidate(arr, less)) return false;
        highEntropyQuickSortProduction(
            arr.data(), arr.size(),
            2 * static_cast<int>(std::bit_width(arr.size())), less,
            false, T{});
        return true;
    }
}

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool dominantValueSampleCandidate(const std::vector<T>& arr, T& dominant, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)dominant; (void)less; return false;
    } else {
        if (arr.size() < 10000) return false;
        T candidate = arr[0];
        int balance = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            if (arr[i] == candidate) ++balance;
            else if (--balance < 0) { candidate = arr[i]; balance = 1; }
        }
        std::size_t firstEight = 0;
        for (std::size_t i = 0; i < 8; ++i) firstEight += arr[i] == candidate ? 1u : 0u;
        if (firstEight < 6) return false;
        std::size_t sampleCount = 0;
        const std::size_t end = std::min<std::size_t>(64, arr.size());
        for (std::size_t i = 0; i < end; ++i) sampleCount += arr[i] == candidate ? 1u : 0u;
        if (sampleCount < 56) return false;
        dominant = candidate;
        return true;
    }
}


// Shared pre-Patience router used by physical through avx2 propagation experiments.  The
// caller is responsible for cheap structural/monotonic guards before entering
// this helper.  Returning true means the input was fully sorted by a
// specialized backend and no pile representation should be built.
template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool trySpecializedPrePatienceRoutes(std::vector<T>& arr, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)less;
        return false;
    } else {
        T dominant = arr[0];
        if (dominantValueSampleCandidate(arr, dominant, less)) {
            highEntropyQuickSort(
                arr.data(), arr.size(),
                2 * static_cast<int>(std::bit_width(arr.size())), less,
                false, T{}, false,
                true, nullptr, false);
            return true;
        }

        if (lowCardinalityDirectionGate(arr, less) &&
            lowCardinalitySampleCandidate(arr, less) &&
            trySortLowCardinalityDirectConfirmed(arr, less)) {
            return true;
        }

        bool highEntropyPrefixAlternates = false;
        if (arr.size() >= 8) {
            int previousDirection = 0;
            highEntropyPrefixAlternates = true;
            for (std::size_t i = 1; i < 8; ++i) {
                int direction = 0;
                if (less(arr[i - 1], arr[i])) direction = 1;
                else if (less(arr[i], arr[i - 1])) direction = -1;
                if (direction == 0 ||
                    (previousDirection != 0 && direction == previousDirection)) {
                    highEntropyPrefixAlternates = false;
                    break;
                }
                previousDirection = direction;
            }
        }
        if (!highEntropyPrefixAlternates &&
            trySortHighEntropyPartitionDirect(arr, less)) {
            return true;
        }
        return false;
    }
}
