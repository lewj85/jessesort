#ifndef JESSESORT_VARIANT_HEADER
#error "Define JESSESORT_VARIANT_HEADER"
#endif
#ifndef JESSESORT_VARIANT_NAMESPACE
#error "Define JESSESORT_VARIANT_NAMESPACE"
#endif
#ifndef JESSESORT_VARIANT_NAME
#error "Define JESSESORT_VARIANT_NAME"
#endif

// The selected JesseSort header is supplied by the Makefile through
// -DJESSESORT_VARIANT_HEADER='"jessesort/...hpp"'.
#include JESSESORT_VARIANT_HEADER

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <string_view>
#include <vector>

// Synthetic input families used to compare one JesseSort variant with std::sort.
enum class InputType {
    Random,
    Sorted,
    Reverse,
    NearlySorted,
    Random100,
    Jitter,
    Alternating,
    Sawtooth,
    BlockSorted,
    OrganPipe,
    Rotated
};

void printBuildConfiguration()
{
#ifdef _LIBCPP_VERSION
    std::cout << "Standard library: libc++ "
              << _LIBCPP_VERSION << '\n';
#elif defined(__GLIBCXX__)
    std::cout << "Standard library: libstdc++ "
              << __GLIBCXX__ << '\n';
#else
    std::cout << "Standard library: unknown\n";
#endif

#ifdef __clang__
    std::cout << "Compiler: Clang "
              << __clang_major__ << '.'
              << __clang_minor__ << '.'
              << __clang_patchlevel__ << '\n';
#elif defined(__GNUC__)
    std::cout << "Compiler: GCC "
              << __GNUC__ << '.'
              << __GNUC_MINOR__ << '.'
              << __GNUC_PATCHLEVEL__ << '\n';
#else
    std::cout << "Compiler: unknown\n";
#endif
}

// Human-readable labels for the output table.
std::string_view name(InputType type) {
    switch (type) {
        case InputType::Random:       return "Random";
        case InputType::Sorted:       return "Sorted";
        case InputType::Reverse:      return "Reverse";
        case InputType::NearlySorted: return "Sorted+Noise(3%)";
        case InputType::Random100:    return "Random%100";
        case InputType::Jitter:       return "Jitter";
        case InputType::Alternating:  return "Alternating";
        case InputType::Sawtooth:     return "Sawtooth";
        case InputType::BlockSorted:  return "BlockSorted";
        case InputType::OrganPipe:    return "OrganPipe";
        case InputType::Rotated:      return "Rotated";
    }
    return "Unknown";
}

// Fill `values` with one deterministic synthetic distribution.
// The caller controls determinism by supplying a seeded PRNG.
void generate(std::vector<int>& values, InputType type, std::mt19937& rng) {
    const std::size_t n = values.size();
    std::uniform_int_distribution<int> fullRange(
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()
    );

    for (int& value : values) {
        value = fullRange(rng);
    }

    switch (type) {
        case InputType::Random:
            break;

        case InputType::Sorted:
            std::sort(values.begin(), values.end());
            break;

        case InputType::Reverse:
            std::sort(values.begin(), values.end());
            std::reverse(values.begin(), values.end());
            break;

        case InputType::NearlySorted: {
            std::iota(values.begin(), values.end(), 0);
            std::uniform_real_distribution<double> coin(0.0, 1.0);
            std::uniform_int_distribution<int> replacement(0, static_cast<int>(n - 1));
            for (int& value : values) {
                if (coin(rng) < 0.03) {
                    value = replacement(rng);
                }
            }
            break;
        }

        case InputType::Random100:
            for (int& value : values) {
                value %= 100;
            }
            break;

        case InputType::Jitter: {
            std::sort(values.begin(), values.end());
            std::uniform_int_distribution<int> noise(-10, 10);
            for (int& value : values) {
                value += noise(rng);
            }
            break;
        }

        case InputType::Alternating: {
            int low = 0;
            int high = static_cast<int>(n);
            for (std::size_t i = 0; i < n; ++i) {
                values[i] = (i % 2 == 0) ? low-- : high++;
            }
            break;
        }

        case InputType::Sawtooth: {
            const std::size_t period = std::max<std::size_t>(1, n / 20);
            for (std::size_t i = 0; i < n; ++i) {
                values[i] = static_cast<int>(i % period);
            }
            break;
        }

        case InputType::BlockSorted: {
            std::iota(values.begin(), values.end(), 0);
            const std::size_t blockSize = std::max<std::size_t>(1, n / 20);
            const std::size_t blockCount = (n + blockSize - 1) / blockSize;

            std::vector<std::size_t> blockIds(blockCount);
            std::iota(blockIds.begin(), blockIds.end(), 0);
            std::shuffle(blockIds.begin(), blockIds.end(), rng);

            const auto source = values;
            for (std::size_t destinationBlock = 0; destinationBlock < blockCount;
                 ++destinationBlock) {
                const std::size_t sourceFirst = blockIds[destinationBlock] * blockSize;
                const std::size_t sourceLast = std::min(n, sourceFirst + blockSize);
                const std::size_t destinationFirst = destinationBlock * blockSize;

                std::copy(
                    source.begin() + static_cast<std::ptrdiff_t>(sourceFirst),
                    source.begin() + static_cast<std::ptrdiff_t>(sourceLast),
                    values.begin() + static_cast<std::ptrdiff_t>(destinationFirst)
                );
            }
            break;
        }

        case InputType::OrganPipe:
            for (std::size_t i = 0; i < n; ++i) {
                values[i] = static_cast<int>(i <= n / 2 ? i : n - i);
            }
            break;

        case InputType::Rotated: {
            std::sort(values.begin(), values.end());
            std::uniform_int_distribution<std::size_t> offset(0, n - 1);
            std::rotate(values.begin(), values.begin() + offset(rng), values.end());
            break;
        }
    }
}

// Return the middle sample after sorting. Trial count is intentionally odd (30 is
// even, so this selects the upper middle sample, matching the previous benchmark).
double median(std::vector<double>& samples) {
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

// Measure one callable with a monotonic clock. The raw durations are retained only
// long enough to calculate the final JesseSort/std::sort ratio.
template<class Function>
double timed(Function&& function) {
    const auto begin = std::chrono::steady_clock::now();
    function();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(end - begin).count();
}

int main() {

    printBuildConfiguration();

    // Warmups are executed but never added to the timing vectors. Their purpose is
    // to stabilize instruction caches, branch predictors, allocator state, and CPU
    // frequency before measured trials begin.
    constexpr int warmupTrials = 3;

    // Timed trials used for each input family and size.
    constexpr int timedTrials = 30;

    const std::vector<std::size_t> sizes{1'000, 10'000, 100'000, 1'000'000};
    const std::vector<InputType> inputs{
        InputType::Random,
        InputType::Sorted,
        InputType::Reverse,
        InputType::NearlySorted,
        InputType::Random100,
        InputType::Jitter,
        InputType::Alternating,
        InputType::Sawtooth,
        InputType::BlockSorted,
        InputType::OrganPipe,
        InputType::Rotated
    };

    std::cout << JESSESORT_VARIANT_NAME << " versus std::sort ("
              << warmupTrials << " unmeasured warmups + median of "
              << timedTrials << " timed trials; lower ratio is better)\n";

    // Ratio table. A value below 1.0 means JesseSort was faster than std::sort.
    std::cout << std::left << std::setw(22) << "Elements";
    for (const std::size_t n : sizes) {
        std::cout << std::setw(16) << std::to_string(n);
    }
    std::cout << '\n' << std::string(22 + sizes.size() * 16, '-') << '\n';

    for (const InputType input : inputs) {
        std::cout << std::left << std::setw(22) << name(input);

        for (const std::size_t n : sizes) {
            // ---------------------------
            // Unmeasured warmup iterations
            // ---------------------------
            for (int warmup = 0; warmup < warmupTrials; ++warmup) {
                // Use seeds distinct from timed trials so warmups do not duplicate
                // the measured inputs exactly.
                const unsigned seed = static_cast<unsigned>(
                    100'000 + warmup + n + static_cast<std::size_t>(input) * 1'000
                );
                std::mt19937 rng(seed);

                std::vector<int> source(n);
                generate(source, input, rng);

                auto candidate = source;
                auto reference = source;

                JESSESORT_VARIANT_NAMESPACE::sort(candidate);
                std::sort(reference.begin(), reference.end());

                // Warmups still validate correctness, but their durations are not
                // recorded anywhere.
                if (candidate != reference) {
                    std::cerr << "Warmup correctness failure: " << name(input)
                              << " n=" << n << " warmup=" << warmup << '\n';
                    return 2;
                }
            }

            // ----------------------
            // Measured trial section
            // ----------------------
            std::vector<double> variantTimes;
            std::vector<double> standardTimes;
            variantTimes.reserve(timedTrials);
            standardTimes.reserve(timedTrials);

            for (int trial = 0; trial < timedTrials; ++trial) {
                const unsigned seed = static_cast<unsigned>(
                    trial + 1 + n + static_cast<std::size_t>(input) * 10'000
                );
                std::mt19937 rng(seed);

                std::vector<int> source(n);
                generate(source, input, rng);

                auto candidate = source;
                auto reference = source;

                variantTimes.push_back(timed([&] {
                    JESSESORT_VARIANT_NAMESPACE::sort(candidate);
                }));

                standardTimes.push_back(timed([&] {
                    std::sort(reference.begin(), reference.end());
                }));

                if (candidate != reference) {
                    std::cerr << "Correctness failure: " << name(input)
                              << " n=" << n << " trial=" << trial << '\n';
                    return 2;
                }
            }

            const double variantMedian = median(variantTimes);
            const double standardMedian = median(standardTimes);
            const double ratio = variantMedian / standardMedian;

            std::cout << std::setw(16) << std::fixed << std::setprecision(3) << ratio;
        }

        std::cout << '\n';
    }
}
