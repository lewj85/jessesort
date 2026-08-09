#include "v1_actual_piles.h"
#include "v2_simulated.h"
#include "v3_inplace_simulated.h"
#include "v4_single_overflow.h"
#include "v5_deferred_bands.h"
#include "v6_live_bands.h"
#include "v7_avx2.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

enum class InputType {
    Random, Sorted, Reverse, NearlySorted, Random100,
    Alternating, Sawtooth, BlockSorted, OrganPipe, Rotated
};

static std::string_view name(InputType t) {
    switch (t) {
        case InputType::Random: return "Random";
        case InputType::Sorted: return "Sorted";
        case InputType::Reverse: return "Reverse";
        case InputType::NearlySorted: return "Sorted+Noise(5%)";
        case InputType::Random100: return "Random%100";
        case InputType::Alternating: return "Alternating";
        case InputType::Sawtooth: return "Sawtooth";
        case InputType::BlockSorted: return "BlockSorted";
        case InputType::OrganPipe: return "OrganPipe";
        case InputType::Rotated: return "Rotated";
    }
    return "?";
}

static constexpr std::array<std::string_view, 8> kAlgorithmNames{
    "V1", "V2", "V3", "V4", "V5", "V6", "V7", "std::sort"
};

static void generate(std::vector<int>& v, InputType t, std::mt19937& rng) {
    const std::size_t n = v.size();
    switch (t) {
        case InputType::Random: {
            std::uniform_int_distribution<int> d(
                std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            for (int& x : v) x = d(rng);
            break;
        }
        case InputType::Sorted:
            std::iota(v.begin(), v.end(), 0);
            break;
        case InputType::Reverse:
            std::iota(v.rbegin(), v.rend(), 0);
            break;
        case InputType::NearlySorted: {
            std::iota(v.begin(), v.end(), 0);
            std::bernoulli_distribution change(0.05);
            std::uniform_int_distribution<int> d(0, static_cast<int>(n) - 1);
            for (int& x : v) if (change(rng)) x = d(rng);
            break;
        }
        case InputType::Random100: {
            std::uniform_int_distribution<int> d(0, 99);
            for (int& x : v) x = d(rng);
            break;
        }
        case InputType::Alternating: {
            int lo = 0;
            int hi = static_cast<int>(n);
            for (std::size_t i = 0; i < n; ++i)
                v[i] = (i % 2 == 0) ? lo-- : hi++;
            break;
        }
        case InputType::Sawtooth: {
            const std::size_t period = std::max<std::size_t>(1, n / 20);
            for (std::size_t i = 0; i < n; ++i)
                v[i] = static_cast<int>(i % period);
            break;
        }
        case InputType::BlockSorted: {
            std::iota(v.begin(), v.end(), 0);
            const std::size_t blockSize = std::max<std::size_t>(1, n / 20);
            const std::size_t fullBlocks = n / blockSize;
            std::vector<std::size_t> ids(fullBlocks);
            std::iota(ids.begin(), ids.end(), 0);
            std::shuffle(ids.begin(), ids.end(), rng);
            const auto source = v;
            for (std::size_t dest = 0; dest < fullBlocks; ++dest) {
                std::copy_n(
                    source.begin() + static_cast<std::ptrdiff_t>(ids[dest] * blockSize),
                    blockSize,
                    v.begin() + static_cast<std::ptrdiff_t>(dest * blockSize));
            }
            break;
        }
        case InputType::OrganPipe:
            for (std::size_t i = 0; i < n; ++i)
                v[i] = static_cast<int>(i <= n / 2 ? i : n - i);
            break;
        case InputType::Rotated: {
            std::iota(v.begin(), v.end(), 0);
            std::uniform_int_distribution<std::size_t> d(0, n - 1);
            std::rotate(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(d(rng)), v.end());
            break;
        }
    }
}

template <class F>
static double timed(F&& f) {
    const auto start = std::chrono::steady_clock::now();
    f();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count();
}

static void run_algorithm(int id, std::vector<int>& values) {
    switch (id) {
        case 0: jessesort::actual_piles::sort(values); break;
        case 1: jessesort::simulated::sort(values); break;
        case 2: jessesort::simulated_inplace_flatten::sort(values); break;
        case 3: jessesort::simulated_early_freeze_single_overflow::sort(values); break;
        case 4: jessesort::simulated_early_freeze::sort(values); break;
        case 5: jessesort::simulated_early_freeze_live::sort(values); break;
        case 6: jessesort::simulated_simd_v7::sort(values); break;
        case 7: std::sort(values.begin(), values.end()); break;
    }
}

static double quantile_sorted(const std::vector<double>& sorted, double q) {
    if (sorted.empty()) return std::numeric_limits<double>::quiet_NaN();
    if (sorted.size() == 1) return sorted.front();
    const double pos = q * static_cast<double>(sorted.size() - 1);
    const auto lo = static_cast<std::size_t>(std::floor(pos));
    const auto hi = static_cast<std::size_t>(std::ceil(pos));
    const double frac = pos - static_cast<double>(lo);
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

struct Stats {
    double median = 0.0;
    double p25 = 0.0;
    double p75 = 0.0;
    double iqr = 0.0;
    double mad = 0.0;
};

static Stats stats(const std::vector<double>& values) {
    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    Stats s;
    s.median = quantile_sorted(sorted, 0.50);
    s.p25 = quantile_sorted(sorted, 0.25);
    s.p75 = quantile_sorted(sorted, 0.75);
    s.iqr = s.p75 - s.p25;
    std::vector<double> deviations;
    deviations.reserve(values.size());
    for (double x : values) deviations.push_back(std::abs(x - s.median));
    std::sort(deviations.begin(), deviations.end());
    s.mad = quantile_sorted(deviations, 0.50);
    return s;
}

struct TrialRecord {
    unsigned seed = 0;
    int trial = 0;
    std::array<double, 8> time{};
    std::array<int, 8> order_position{};
};

static std::array<int, 8> execution_order(std::size_t seed_index, int trial) {
    std::array<int, 8> order{0,1,2,3,4,5,6,7};
    const int block = trial / 8;
    const int rotation = static_cast<int>((trial + seed_index * 3) % 8);
    std::rotate(order.begin(), order.begin() + rotation, order.end());
    if (block % 2 == 1) std::reverse(order.begin(), order.end());
    return order;
}

static unsigned trial_rng_seed(unsigned base_seed, std::size_t n, InputType input, int trial) {
    // Stable deterministic mixing; each trial gets a fresh input but all algorithms
    // within that trial receive exactly the same source vector.
    std::uint64_t x = base_seed;
    x ^= static_cast<std::uint64_t>(n) + 0x9e3779b97f4a7c15ULL + (x << 6) + (x >> 2);
    x ^= static_cast<std::uint64_t>(input) * 0xbf58476d1ce4e5b9ULL;
    x ^= static_cast<std::uint64_t>(trial) * 0x94d049bb133111ebULL;
    return static_cast<unsigned>(x ^ (x >> 32));
}

static std::filesystem::path make_run_directory() {
    namespace fs = std::filesystem;
    fs::create_directories("results");

    const std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    std::ostringstream base;
    base << "run_" << std::put_time(&tm, "%Y%m%d_%H%M%S");

    fs::path candidate = fs::path("results") / base.str();
    for (int suffix = 1; fs::exists(candidate); ++suffix)
        candidate = fs::path("results") / (base.str() + "_" + std::to_string(suffix));

    fs::create_directories(candidate);
    return candidate;
}

static void write_progress(const std::filesystem::path& path,
                           std::string_view input,
                           int seed_index,
                           int seed_count,
                           int trial_done,
                           int trials_per_seed,
                           std::string_view state) {
    std::ofstream out(path, std::ios::trunc);
    out << "state=" << state << '\n';
    out << "input=" << input << '\n';
    out << "seed_index=" << seed_index << '/' << seed_count << '\n';
    out << "trial=" << trial_done << '/' << trials_per_seed << '\n';
    out.flush();
}

static void write_metadata(const std::filesystem::path& path,
                           int trials_per_seed, std::size_t n, int seed_count, int warmups) {
    std::ofstream out(path);
    const std::time_t now = std::time(nullptr);
    out << "JesseSort benchmark metadata\n";
    out << "generated=" << std::put_time(std::gmtime(&now), "%Y-%m-%dT%H:%M:%SZ") << '\n';
    out << "n=" << n << '\n';
    out << "trials_per_seed=" << trials_per_seed << '\n';
    out << "seed_count=" << seed_count << '\n';
    out << "total_trials_per_input=" << static_cast<long long>(trials_per_seed) * seed_count << '\n';
    out << "warmup_trials_per_input=" << warmups << '\n';
    out << "clock=std::chrono::steady_clock\n";
    out << "hardware_concurrency=" << std::thread::hardware_concurrency() << '\n';
#ifdef __VERSION__
    out << "compiler=" << __VERSION__ << '\n';
#endif
#ifdef __AVX2__
    out << "avx2_compiled=1\n";
#else
    out << "avx2_compiled=0\n";
#endif
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("model name", 0) == 0) {
            out << "cpu_" << line << '\n';
            break;
        }
    }
    out << "method=paired same-input trials; rotating/reversing execution order; validation outside timer\n";
    out << "warning=absolute timings and saved std::sort medians are not portable across VM/host sessions\n";
}

int main(int argc, char** argv) {
    // Benchmark methodology default (E066): two seeds x 500 trials = 1000 paired
    // observations per workload. For sub-1.5% claims, increase to >=1000 trials/seed.
    const int trials_per_seed = argc > 1 ? std::max(1, std::atoi(argv[1])) : 500;
    const std::size_t n = argc > 2
        ? static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10))
        : 100000;
    const int seed_count = argc > 3 ? std::clamp(std::atoi(argv[3]), 1, 2) : 2;
    const int warmups = argc > 4 ? std::max(0, std::atoi(argv[4])) : 2;
    const std::array<unsigned, 2> base_seeds{9100000u, 123456789u};

    const std::filesystem::path run_dir = make_run_directory();
    const std::filesystem::path summary_path = run_dir / "benchmark_results.csv";
    const std::filesystem::path raw_path = run_dir / "benchmark_trials.csv";
    const std::filesystem::path metadata_path = run_dir / "benchmark_metadata.txt";
    const std::filesystem::path progress_path = run_dir / "benchmark_progress.txt";

    std::ofstream summary(summary_path);
    summary << "input,n,seed_scope,trials,algorithm,median_us,p25_us,p75_us,iqr_us,mad_us,"
               "std_median_us,ratio_of_medians,median_paired_ratio,p25_paired_ratio,p75_paired_ratio\n";
    summary.flush();

    std::ofstream raw(raw_path);
    raw << std::setprecision(17);
    raw << "input,n,seed,trial,algorithm,order_position,time_us,std_sort_us,paired_ratio_to_std\n";
    raw.flush();

    write_metadata(metadata_path, trials_per_seed, n, seed_count, warmups);
    write_progress(progress_path, "startup", 0, seed_count, 0, trials_per_seed, "starting");

    const std::vector<InputType> inputs{
        InputType::Random, InputType::Sorted, InputType::Reverse,
        InputType::NearlySorted, InputType::Random100, InputType::Alternating,
        InputType::Sawtooth, InputType::BlockSorted, InputType::OrganPipe,
        InputType::Rotated};

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "trials/seed=" << trials_per_seed
              << " seeds=" << seed_count
              << " total/input=" << static_cast<long long>(trials_per_seed) * seed_count
              << " n=" << n << '\n';
    std::cout << "results_dir=" << run_dir.string() << '\n';

    for (InputType input : inputs) {
        write_progress(progress_path, name(input), 0, seed_count, 0, trials_per_seed, "warming_up");
        // Short untimed warmup using separate generated inputs.
        for (int w = 0; w < warmups; ++w) {
            std::mt19937 rng(trial_rng_seed(0xA5A5A5A5u, n, input, w));
            std::vector<int> source(n);
            generate(source, input, rng);
            for (int id = 0; id < 8; ++id) {
                auto values = source;
                run_algorithm(id, values);
                if (!std::is_sorted(values.begin(), values.end())) {
                    std::cerr << "warmup validation failure: " << name(input)
                              << " algorithm=" << kAlgorithmNames[id] << '\n';
                    return 1;
                }
            }
        }

        std::vector<TrialRecord> records;
        records.reserve(static_cast<std::size_t>(trials_per_seed * seed_count));

        for (int si = 0; si < seed_count; ++si) {
            const unsigned base_seed = base_seeds[static_cast<std::size_t>(si)];
            for (int trial = 0; trial < trials_per_seed; ++trial) {
                std::mt19937 rng(trial_rng_seed(base_seed, n, input, trial));
                std::vector<int> source(n);
                generate(source, input, rng);
                auto expected = source;
                std::sort(expected.begin(), expected.end());

                TrialRecord rec;
                rec.seed = base_seed;
                rec.trial = trial;
                const auto order = execution_order(static_cast<std::size_t>(si), trial);

                for (int pos = 0; pos < 8; ++pos) {
                    const int id = order[static_cast<std::size_t>(pos)];
                    auto values = source;
                    const double elapsed = timed([&]{ run_algorithm(id, values); });
                    if (values != expected) {
                        std::cerr << "validation failure: " << name(input)
                                  << " seed=" << base_seed
                                  << " trial=" << trial
                                  << " algorithm=" << kAlgorithmNames[static_cast<std::size_t>(id)] << '\n';
                        return 1;
                    }
                    rec.time[static_cast<std::size_t>(id)] = elapsed;
                    rec.order_position[static_cast<std::size_t>(id)] = pos;
                }
                records.push_back(rec);

                // Checkpoint this completed paired trial immediately. The raw file is
                // independent of any previous run because each invocation gets its own
                // results/run_* directory. Flush in small batches so a tool/terminal
                // timeout cannot discard a long benchmark that is still executing.
                const double std_time = rec.time[7];
                for (int id = 0; id < 8; ++id) {
                    const double ratio = rec.time[static_cast<std::size_t>(id)] / std_time;
                    raw << name(input) << ',' << n << ',' << rec.seed << ',' << rec.trial << ','
                        << kAlgorithmNames[static_cast<std::size_t>(id)] << ','
                        << rec.order_position[static_cast<std::size_t>(id)] << ','
                        << rec.time[static_cast<std::size_t>(id)] << ',' << std_time << ',' << ratio << '\n';
                }
                if (((trial + 1) % 10) == 0 || trial + 1 == trials_per_seed) {
                    raw.flush();
                    write_progress(progress_path, name(input), si + 1, seed_count,
                                   trial + 1, trials_per_seed, "running");
                }
            }
        }

        std::array<std::vector<double>, 8> all_times;
        std::array<std::vector<double>, 8> all_ratios;
        for (auto& v : all_times) v.reserve(records.size());
        for (auto& v : all_ratios) v.reserve(records.size());

        for (const TrialRecord& rec : records) {
            const double std_time = rec.time[7];
            for (int id = 0; id < 8; ++id) {
                const double ratio = rec.time[static_cast<std::size_t>(id)] / std_time;
                all_times[static_cast<std::size_t>(id)].push_back(rec.time[static_cast<std::size_t>(id)]);
                all_ratios[static_cast<std::size_t>(id)].push_back(ratio);
            }
        }

        const Stats std_stats = stats(all_times[7]);
        std::array<Stats, 8> time_stats{};
        std::array<Stats, 8> ratio_stats{};
        for (int id = 0; id < 8; ++id) {
            time_stats[static_cast<std::size_t>(id)] = stats(all_times[static_cast<std::size_t>(id)]);
            ratio_stats[static_cast<std::size_t>(id)] = stats(all_ratios[static_cast<std::size_t>(id)]);
            const auto& ts = time_stats[static_cast<std::size_t>(id)];
            const auto& rs = ratio_stats[static_cast<std::size_t>(id)];
            summary << name(input) << ',' << n << ",ALL," << records.size() << ','
                    << kAlgorithmNames[static_cast<std::size_t>(id)] << ','
                    << ts.median << ',' << ts.p25 << ',' << ts.p75 << ',' << ts.iqr << ',' << ts.mad << ','
                    << std_stats.median << ',' << ts.median / std_stats.median << ','
                    << rs.median << ',' << rs.p25 << ',' << rs.p75 << '\n';
        }

        // Also emit per-seed summaries to make seed sensitivity visible.
        for (int si = 0; si < seed_count; ++si) {
            const unsigned target_seed = base_seeds[static_cast<std::size_t>(si)];
            std::array<std::vector<double>, 8> seed_times;
            std::array<std::vector<double>, 8> seed_ratios;
            for (const TrialRecord& rec : records) {
                if (rec.seed != target_seed) continue;
                const double std_time = rec.time[7];
                for (int id = 0; id < 8; ++id) {
                    seed_times[static_cast<std::size_t>(id)].push_back(rec.time[static_cast<std::size_t>(id)]);
                    seed_ratios[static_cast<std::size_t>(id)].push_back(rec.time[static_cast<std::size_t>(id)] / std_time);
                }
            }
            const Stats seed_std = stats(seed_times[7]);
            for (int id = 0; id < 8; ++id) {
                const Stats ts = stats(seed_times[static_cast<std::size_t>(id)]);
                const Stats rs = stats(seed_ratios[static_cast<std::size_t>(id)]);
                summary << name(input) << ',' << n << ',' << target_seed << ',' << trials_per_seed << ','
                        << kAlgorithmNames[static_cast<std::size_t>(id)] << ','
                        << ts.median << ',' << ts.p25 << ',' << ts.p75 << ',' << ts.iqr << ',' << ts.mad << ','
                        << seed_std.median << ',' << ts.median / seed_std.median << ','
                        << rs.median << ',' << rs.p25 << ',' << rs.p75 << '\n';
            }
        }

        summary.flush();
        raw.flush();
        write_progress(progress_path, name(input), seed_count, seed_count,
                       trials_per_seed, trials_per_seed, "input_complete");

        std::cout << std::setw(18) << name(input)
                  << " std=" << std_stats.median << "us"
                  << " | V1=" << ratio_stats[0].median
                  << " V2=" << ratio_stats[1].median
                  << " V3=" << ratio_stats[2].median
                  << " V4=" << ratio_stats[3].median
                  << " V5=" << ratio_stats[4].median
                  << " V6=" << ratio_stats[5].median
                  << " V7=" << ratio_stats[6].median << '\n';
        std::cout.flush();
    }

    write_progress(progress_path, "complete", seed_count, seed_count,
                   trials_per_seed, trials_per_seed, "complete");
    summary.flush();
    raw.flush();
    std::cout << "Wrote " << summary_path.string() << ", "
              << raw_path.string() << ", " << metadata_path.string()
              << ", " << progress_path.string() << '\n';
    return 0;
}
