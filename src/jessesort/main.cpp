#include "jesseSort.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <fftw3.h>
#include <fstream>

// Input types
enum class InputType {
    Random,
    Sorted,
    Reverse,
    NearlySorted,
    DuplicateHeavy,
    Jitter,
    Adversarial,
    Sawtooth,
    BlockSorted,
    OrganPipe,
    Rotated,
    Signal
};

std::string input_name(InputType t) {
    switch (t) {
        case InputType::Random:         return "Random";
        case InputType::Sorted:         return "Sorted";
        case InputType::Reverse:        return "Reverse";
        case InputType::NearlySorted:   return "Sorted+Noise(5%)";
        case InputType::DuplicateHeavy: return "Random+Repeats(50%)";
        case InputType::Jitter:         return "Jitter";
        case InputType::Adversarial:    return "Alternating";
        case InputType::Sawtooth:       return "Sawtooth";
        case InputType::BlockSorted:    return "BlockSorted";
        case InputType::OrganPipe:      return "OrganPipe";
        case InputType::Rotated:        return "Rotated";
        case InputType::Signal:         return "Signal";
    }
    return "";
}

// Output wave for visualizing/debugging
void visualize(std::vector<int>& arr){
    std::ofstream out("arr.txt");
    for (auto v : arr) out << v << " ";
    out.close();
}

std::vector<double> generate_phased_wave_double(
    std::mt19937& rng,
    size_t n,
    size_t n_phases = 10
) {
    std::vector<double> values;
    values.reserve(n);

    size_t n_phase = std::max<size_t>(1, n / n_phases);

    std::uniform_real_distribution<double> coin(0.0, 1.0);

    for (size_t p = 0; p < n_phases; ++p) {
        size_t start_idx = p * n_phase;
        size_t end_idx = (p == n_phases - 1) ? n : start_idx + n_phase;
        size_t len = end_idx - start_idx;

        double max_amp = static_cast<double>(len) / 1.5;
        std::uniform_real_distribution<double> amp_dist(0.5 * max_amp, max_amp);

        double amp = amp_dist(rng);
        bool hill = coin(rng) < 0.5;

        double start = hill ? -amp : amp;
        double end   = hill ?  amp : -amp;

        for (size_t i = 0; i < len; ++i) {
            double x = M_PI * i / (len - 1);
            double val = start + (end - start) * (1.0 - std::cos(x)) / 2.0;
            values.push_back(val);
        }
    }

    return values;
}

std::vector<double> fft_convolve(
    const std::vector<double>& a,
    const std::vector<double>& b
) {
    size_t n = a.size();

    std::vector<fftw_complex> A(n), B(n), C(n);

    for (size_t i = 0; i < n; ++i) {
        A[i][0] = a[i]; A[i][1] = 0.0;
        B[i][0] = b[i]; B[i][1] = 0.0;
    }

    fftw_plan planA = fftw_plan_dft_1d(n, A.data(), A.data(), FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan planB = fftw_plan_dft_1d(n, B.data(), B.data(), FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(planA);
    fftw_execute(planB);

    for (size_t i = 0; i < n; ++i) {
        double re = A[i][0]*B[i][0] - A[i][1]*B[i][1];
        double im = A[i][0]*B[i][1] + A[i][1]*B[i][0];
        C[i][0] = re;
        C[i][1] = im;
    }

    fftw_plan planC = fftw_plan_dft_1d(n, C.data(), C.data(), FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(planC);

    std::vector<double> result(n);
    for (size_t i = 0; i < n; ++i)
        result[i] = C[i][0] / static_cast<double>(n);

    fftw_destroy_plan(planA);
    fftw_destroy_plan(planB);
    fftw_destroy_plan(planC);

    return result;
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
            //visualize(arr);
            break;

        case InputType::Sorted:
            std::sort(arr.begin(), arr.end());
            //visualize(arr);
            break;

        case InputType::Reverse:
            std::sort(arr.begin(), arr.end());
            std::reverse(arr.begin(), arr.end());
            //visualize(arr);
            break;

        case InputType::NearlySorted: {
            // 5% chance to be a random value
            std::sort(arr.begin(), arr.end());
            size_t swaps = static_cast<size_t>(0.05 * n);
            std::uniform_int_distribution<size_t> idx(0, n - 1);
            for (size_t i = 0; i < swaps; ++i) {
                std::swap(arr[idx(rng)], arr[idx(rng)]);
            }
            //visualize(arr);
            break;
        }

        case InputType::DuplicateHeavy: {
            // 50% chance to repeat previous value
            std::uniform_int_distribution<int> val_dist(0, static_cast<int>(n));
            std::uniform_real_distribution<double> repeat_chance(0.0, 1.0);

            int prev = val_dist(rng);
            arr[0] = prev;

            for (size_t i = 1; i < n; ++i) {
                if (repeat_chance(rng) < 0.50) {
                    arr[i] = prev;
                } else {
                    prev = val_dist(rng);
                    arr[i] = prev;
                }
            }
            //visualize(arr);
            break;
        }


        case InputType::Jitter: {
            std::sort(arr.begin(), arr.end());

            // Add small random jitter around each value
            std::uniform_int_distribution<int> jitter(-10, 10);
            for (int& v : arr) {
                v += jitter(rng);  // modify in place
            }
            // visualize(arr);
            break;
        }

        case InputType::Adversarial: {
            // Alternating high/low
            int lo = 0;
            int hi = static_cast<int>(n);
            for (size_t i = 0; i < n; ++i) {
                arr[i] = (i % 2 == 0) ? lo++ : hi--;
            }
            //visualize(arr);
            break;
        }

        case InputType::Sawtooth: {
            // Repeated ascending runs: 0..k, 0..k, ...
            size_t period = std::max<size_t>(1, n / 20);
            for (size_t i = 0; i < n; ++i) {
                arr[i] = static_cast<int>(i % period);
            }
            //visualize(arr);
            break;
        }

        case InputType::BlockSorted: {
            size_t block = std::max<size_t>(1, n / 20);
            // Fill sorted
            for (size_t i = 0; i < n; ++i) {
                arr[i] = static_cast<int>(i);
            }

            // Create block index list
            size_t num_blocks = (n + block - 1) / block;
            std::vector<size_t> block_ids(num_blocks);
            std::iota(block_ids.begin(), block_ids.end(), 0);

            // Shuffle block order
            std::shuffle(block_ids.begin(), block_ids.end(), rng);

            // Create copy of original
            std::vector<int> temp = arr;

            // Reassemble blocks in shuffled order
            for (size_t i = 0; i < num_blocks; ++i) {
                size_t src_block = block_ids[i];
                size_t src_start = src_block * block;
                size_t src_end = std::min(n, src_start + block);

                size_t dst_start = i * block;

                std::copy(temp.begin() + src_start,
                        temp.begin() + src_end,
                        arr.begin() + dst_start);
            }
            //visualize(arr);
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
            //visualize(arr);
            break;
        }

        case InputType::Rotated: {
            // Sorted array rotated by random offset
            std::sort(arr.begin(), arr.end());
            std::uniform_int_distribution<size_t> rot(0, n - 1);
            size_t k = rot(rng);
            std::rotate(arr.begin(), arr.begin() + k, arr.end());
            //visualize(arr);
            break;
        }

        case InputType::Signal: {
            // Convolves 3 waves with random phase, frequency, amplitude
            auto generate_phased_wave_double =
                [&](size_t n_phase, size_t n_phases) {
                    std::uniform_real_distribution<double> coin(0.0, 1.0);

                    std::vector<double> values;
                    values.reserve(n_phase * n_phases);

                    double max_amp = n_phase / 1.5;
                    std::uniform_real_distribution<double> amp_dist(0.5 * max_amp, max_amp);

                    for (size_t p = 0; p < n_phases; ++p) {
                        double amp = amp_dist(rng);
                        bool hill = coin(rng) < 0.5;
                        double start = hill ? -amp : amp;
                        double end   = hill ?  amp : -amp;

                        for (size_t i = 0; i < n_phase; ++i) {
                            double x = M_PI * i / (n_phase - 1);
                            double val = start + (end - start) *
                                (1.0 - std::cos(x)) / 2.0;
                            values.push_back(val);
                        }
                    }
                    return values;
                };

            auto fft_convolve =
                [](const std::vector<double>& a,
                const std::vector<double>& b) {
                    size_t n = a.size();
                    std::vector<fftw_complex> A(n), B(n), C(n);
                    for (size_t i = 0; i < n; ++i) {
                        A[i][0] = a[i]; A[i][1] = 0.0;
                        B[i][0] = b[i]; B[i][1] = 0.0;
                    }

                    fftw_plan planA = fftw_plan_dft_1d(n, A.data(), A.data(),
                                                    FFTW_FORWARD, FFTW_ESTIMATE);
                    fftw_plan planB = fftw_plan_dft_1d(n, B.data(), B.data(),
                                                    FFTW_FORWARD, FFTW_ESTIMATE);
                    fftw_execute(planA);
                    fftw_execute(planB);

                    for (size_t i = 0; i < n; ++i) {
                        double re = A[i][0]*B[i][0] - A[i][1]*B[i][1];
                        double im = A[i][0]*B[i][1] + A[i][1]*B[i][0];
                        C[i][0] = re;
                        C[i][1] = im;
                    }

                    fftw_plan planC = fftw_plan_dft_1d(n, C.data(), C.data(),
                                                    FFTW_BACKWARD, FFTW_ESTIMATE);
                    fftw_execute(planC);

                    std::vector<double> result(n);
                    for (size_t i = 0; i < n; ++i) {
                        result[i] = C[i][0] / n;
                    }

                    fftw_destroy_plan(planA);
                    fftw_destroy_plan(planB);
                    fftw_destroy_plan(planC);

                    return result;
                };

            // Match output length to arr.size()
            size_t n_phase = std::max<size_t>(1, n / 50);
            size_t n_phases = 50;

            auto wave1 = generate_phased_wave_double(n_phase, n_phases);
            auto wave2 = generate_phased_wave_double(n_phase, n_phases);
            auto wave3 = generate_phased_wave_double(n_phase, n_phases);

            auto conv12 = fft_convolve(wave1, wave2);
            auto conv123 = fft_convolve(conv12, wave3);

            // Ensure size matches arr
            if (conv123.size() != n) {
                conv123.resize(n);
            }

            // Scale so max amplitude == n, avoids convolution explosion
            double max_abs = 0.0;
            for (double v : conv123) {
                max_abs = std::max(max_abs, std::abs(v));
            }

            double scale = (max_abs > 0.0)
                ? static_cast<double>(n) / max_abs
                : 1.0;

            for (size_t i = 0; i < n; ++i) {
                arr[i] = static_cast<int>(std::round(conv123[i] * scale));
            }
            //visualize(arr);
            break;
        }
    }
}


// Main
int main() {
    std::vector<size_t> sizes = {
        1'000, 10'000, 100'000, 1'000'000//, 10'000'000
    };
    int trials = 20;

    std::vector<InputType> inputs = {
        InputType::Random,
        InputType::Sorted,
        InputType::Reverse,
        InputType::NearlySorted,
        InputType::DuplicateHeavy,
        InputType::Jitter,
        InputType::Adversarial,
        InputType::Sawtooth,
        InputType::BlockSorted,
        InputType::OrganPipe,
        InputType::Rotated,
        InputType::Signal,
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
                arr = jesseSort(arr);
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

                if (!std::equal(arr.begin(), arr.end(), arr2.begin())) {
                    std::cerr << "Error: arrays differ! JesseSort did not match reference sort.\n";
                }
                // auto mismatch = std::mismatch(arr.begin(), arr.end(), arr2.begin());

                // if (mismatch.first != arr.end()) {
                //     size_t idx = std::distance(arr.begin(), mismatch.first);

                //     std::cerr << "Error: arrays differ at index " << idx << "\n";
                //     std::cerr << "JesseSort: " << arr[idx]
                //             << " | Reference: " << arr2[idx] << "\n";

                //     // Print small neighborhood for context
                //     size_t start = (idx >= 3) ? idx - 3 : 0;
                //     size_t end   = std::min(idx + 4, arr.size());

                //     std::cerr << "\nContext (JesseSort):\n";
                //     for (size_t i = start; i < end; ++i)
                //         std::cerr << "[" << i << "]=" << arr[i] << " ";
                //     std::cerr << "\n\nContext (Reference):\n";
                //     for (size_t i = start; i < end; ++i)
                //         std::cerr << "[" << i << "]=" << arr2[i] << " ";
                //     std::cerr << "\n";
                // }
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
