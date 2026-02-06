#include "jesseSort.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

// Input types
enum class InputType {
    Random,
    Sorted,
    Reverse,
    NearlySorted,
    DuplicateHeavy,
    Adversarial,
    Sawtooth,
    BlockSorted,
    OrganPipe,
    Rotated
};

std::string input_name(InputType t) {
    switch (t) {
        case InputType::Random:         return "Random";
        case InputType::Sorted:         return "Sorted";
        case InputType::Reverse:        return "Reverse";
        case InputType::NearlySorted:   return "NearlySorted(5%)";
        case InputType::DuplicateHeavy: return "Duplicates(5%)";
        case InputType::Adversarial:    return "Alternating";
        case InputType::Sawtooth:       return "Sawtooth";
        case InputType::BlockSorted:    return "BlockSorted";
        case InputType::OrganPipe:      return "OrganPipe";
        case InputType::Rotated:        return "Rotated";
    }
    return "";
}

// Input generation
void generate_input(std::vector<int>& arr,
                    InputType type,
                    std::mt19937& rng) {
    const size_t n = arr.size();
    std::uniform_int_distribution<int> dist(
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()
    );

    // Base random fill
    for (auto& x : arr) {
        x = dist(rng);
    }

    switch (type) {
        case InputType::Random:
            break;

        case InputType::Sorted:
            std::sort(arr.begin(), arr.end());
            break;

        case InputType::Reverse:
            std::sort(arr.begin(), arr.end());
            std::reverse(arr.begin(), arr.end());
            break;

        case InputType::NearlySorted: {
            std::sort(arr.begin(), arr.end());
            size_t swaps = static_cast<size_t>(0.05 * n);
            std::uniform_int_distribution<size_t> idx(0, n - 1);
            for (size_t i = 0; i < swaps; ++i) {
                std::swap(arr[idx(rng)], arr[idx(rng)]);
            }
            break;
        }

        case InputType::DuplicateHeavy: {
            size_t unique = std::max<size_t>(1, n / 20); // ~5%
            std::uniform_int_distribution<int> small(0, static_cast<int>(unique));
            for (auto& x : arr) {
                x = small(rng);
            }
            break;
        }

        case InputType::Adversarial: {
            // alternating high/low
            int lo = 0;
            int hi = static_cast<int>(n);
            for (size_t i = 0; i < n; ++i) {
                arr[i] = (i % 2 == 0) ? lo++ : hi--;
            }
            break;
        }

        case InputType::Sawtooth: {
            // Repeated ascending runs: 0..k, 0..k, ...
            size_t period = std::max<size_t>(1, n / 20);
            for (size_t i = 0; i < n; ++i) {
                arr[i] = static_cast<int>(i % period);
            }
            break;
        }

        case InputType::BlockSorted: {
            // Sorted blocks, blocks randomly permuted
            size_t block = std::max<size_t>(1, n / 20);
            for (size_t i = 0; i < n; ++i) {
                arr[i] = static_cast<int>(i);
            }

            for (size_t i = 0; i < n; i += block) {
                std::sort(arr.begin() + i,
                        arr.begin() + std::min(n, i + block));
            }

            std::shuffle(arr.begin(), arr.end(), rng);
            break;
        }

        case InputType::OrganPipe: {
            // Increases to midpoint, then decreases
            for (size_t i = 0; i < n; ++i) {
                if (i <= n / 2) {
                    arr[i] = static_cast<int>(i);
                } else {
                    arr[i] = static_cast<int>(n - i);
                }
            }
            break;
        }

        case InputType::Rotated: {
            // Sorted array rotated by random offset
            std::sort(arr.begin(), arr.end());
            std::uniform_int_distribution<size_t> rot(0, n - 1);
            size_t k = rot(rng);
            std::rotate(arr.begin(), arr.begin() + k, arr.end());
            break;
        }

    }
}


// Main
int main() {
    std::vector<size_t> sizes = {
        1'000, 10'000, 100'000, 1'000'000, 10'000'000
    };

    std::vector<InputType> inputs = {
        InputType::Random,
        InputType::Sorted,
        InputType::Reverse,
        InputType::NearlySorted,
        InputType::DuplicateHeavy,
        InputType::Adversarial,
        InputType::Sawtooth,
        InputType::BlockSorted,
        InputType::OrganPipe,
        InputType::Rotated
    };

    // Table headers
    std::cout << "\n                      Number of Input Values\n";
    std::cout << std::left << std::setw(22) << "Input Type";
    for (auto n : sizes) {
        std::cout << std::setw(14) << n;
    }
    std::cout << "\n";

    std::cout << std::string(22 + sizes.size() * 14, '-') << "\n";

    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();

    // Benchmark loop
    for (auto input : inputs) {
        std::cout << std::left << std::setw(22) << input_name(input);

        for (auto n : sizes) {
            int trials = 20;

            std::vector<double> jesse_times;
            std::vector<double> std_times;
            jesse_times.reserve(trials);
            std_times.reserve(trials);

            for (int t = 0; t < trials; ++t) {
                std::mt19937 rng(t + 1);

                std::vector<int> arr(n);
                generate_input(arr, input, rng);
                std::vector<int> arr2 = arr;

                // JesseSort
                start = std::chrono::high_resolution_clock::now();
                jesseSort(arr);
                end = std::chrono::high_resolution_clock::now();
                jesse_times.push_back(
                    std::chrono::duration<double>(end - start).count()
                );

                // std::sort
                start = std::chrono::high_resolution_clock::now();
                std::sort(arr2.begin(), arr2.end());
                end = std::chrono::high_resolution_clock::now();
                std_times.push_back(
                    std::chrono::duration<double>(end - start).count()
                );
            }

            double jesse_mean =
                std::accumulate(jesse_times.begin(), jesse_times.end(), 0.0)
                / trials;

            double std_mean =
                std::accumulate(std_times.begin(), std_times.end(), 0.0)
                / trials;

            // Print JesseSort / std::sort ratio (compact + interpretable)
            double ratio = jesse_mean / std_mean;
            std::cout << std::setw(14) << std::fixed << std::setprecision(3)
                      << ratio;
        }

        std::cout << "\n";
    }

    return 0;
}
