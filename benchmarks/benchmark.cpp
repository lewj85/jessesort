#include <jessesort/v1_actual_piles.h>
#include <jessesort/v2_simulated.h>
#include <jessesort/v3_inplace_simulated.h>
#include <jessesort/v4_single_overflow.h>
#include <jessesort/v5_deferred_bands.h>
#include <jessesort/v6_live_bands.h>
#include <jessesort/v7_avx2.h>

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
#include <map>
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

static constexpr std::array<InputType, 10> kInputs{
    InputType::Random, InputType::Sorted, InputType::Reverse,
    InputType::NearlySorted, InputType::Random100, InputType::Alternating,
    InputType::Sawtooth, InputType::BlockSorted, InputType::OrganPipe,
    InputType::Rotated
};

static constexpr std::array<std::size_t, 4> kDefaultSizes{
    1000, 10000, 100000, 1000000
};

static std::string size_label(std::size_t n) {
    if (n == 1000) return "1k";
    if (n == 10000) return "10k";
    if (n == 100000) return "100k";
    if (n == 1000000) return "1m";
    return std::to_string(n);
}

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

static std::array<int, 8> execution_order(int trial) {
    std::array<int, 8> order{0,1,2,3,4,5,6,7};
    const int block = trial / 8;
    const int rotation = trial % 8;
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
                           std::size_t n,
                           std::string_view input,
                           int trial_done,
                           int trials_for_size,
                           std::string_view state) {
    std::ofstream out(path, std::ios::trunc);
    out << "state=" << state << '\n';
    out << "n=" << n << '\n';
    out << "input=" << input << '\n';
    out << "trial=" << trial_done << '/' << trials_for_size << '\n';
    out.flush();
}

static int trials_for_size(int base_trials, std::size_t n, bool sweeping_all_sizes) {
    if (sweeping_all_sizes && n >= 1000000) return std::max(1, base_trials / 10);
    return base_trials;
}

static void write_metadata(const std::filesystem::path& path,
                           int base_trials, const std::vector<std::size_t>& sizes,
                           bool sweeping_all_sizes, int warmups) {
    std::ofstream out(path);
    const std::time_t now = std::time(nullptr);
    out << "JesseSort benchmark metadata\n";
    out << "generated=" << std::put_time(std::gmtime(&now), "%Y-%m-%dT%H:%M:%SZ") << '\n';
    out << "sizes=";
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        if (i) out << ',';
        out << sizes[i];
    }
    out << '\n';
    out << "trial_seed_policy=one deterministic unique seed per trial; same input shared by all algorithms\n";
    out << "base_trials=" << base_trials << '\n';
    for (std::size_t n : sizes)
        out << "trials_" << size_label(n) << '=' << trials_for_size(base_trials, n, sweeping_all_sizes) << '\n';
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
    out << "method=paired same-input trials; fresh deterministic input seed per trial; rotating/reversing execution order; validation outside timer\n";
    out << "warning=absolute timings and saved std::sort medians are not portable across VM/host sessions\n";
}

struct BenchmarkCell {
    bool complete = false;
    Stats time_stats{};
    Stats ratio_stats{};
};

static int algorithm_id(std::string_view n) {
    for (std::size_t i = 0; i < kAlgorithmNames.size(); ++i)
        if (kAlgorithmNames[i] == n) return static_cast<int>(i);
    return -1;
}

static int input_id(std::string_view n) {
    for (std::size_t i = 0; i < kInputs.size(); ++i)
        if (name(kInputs[i]) == n) return static_cast<int>(i);
    return -1;
}

struct LoadedTrial {
    TrialRecord rec{};
    std::array<bool, 8> seen{};

    bool complete() const {
        return std::all_of(seen.begin(), seen.end(), [](bool v) { return v; });
    }
};

using LoadedRecords = std::map<std::pair<std::size_t, int>, std::map<int, LoadedTrial>>;

static std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) fields.push_back(field);
    return fields;
}

static LoadedRecords load_raw_records(const std::filesystem::path& raw_path) {
    LoadedRecords loaded;
    std::ifstream in(raw_path);
    std::string line;
    if (!std::getline(in, line)) return loaded; // header
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const auto f = split_csv_line(line);
        if (f.size() != 9) continue;
        const int iid = input_id(f[0]);
        const int aid = algorithm_id(f[4]);
        if (iid < 0 || aid < 0) continue;
        try {
            const std::size_t n = static_cast<std::size_t>(std::stoull(f[1]));
            const int trial = std::stoi(f[3]);
            auto& lt = loaded[{n, iid}][trial];
            lt.rec.seed = static_cast<unsigned>(std::stoul(f[2]));
            lt.rec.trial = trial;
            lt.rec.order_position[static_cast<std::size_t>(aid)] = std::stoi(f[5]);
            lt.rec.time[static_cast<std::size_t>(aid)] = std::stod(f[6]);
            lt.seen[static_cast<std::size_t>(aid)] = true;
        } catch (...) {
            // Ignore a partially written final line after abrupt termination.
        }
    }
    return loaded;
}

static void summarize_records(const std::vector<TrialRecord>& records,
                              InputType input, std::size_t n,
                              std::ostream& summary,
                              std::array<BenchmarkCell, 8>& cells) {
    std::array<std::vector<double>, 8> all_times;
    std::array<std::vector<double>, 8> all_ratios;
    for (auto& v : all_times) v.reserve(records.size());
    for (auto& v : all_ratios) v.reserve(records.size());
    for (const TrialRecord& rec : records) {
        const double std_time = rec.time[7];
        for (int id = 0; id < 8; ++id) {
            all_times[static_cast<std::size_t>(id)].push_back(rec.time[static_cast<std::size_t>(id)]);
            all_ratios[static_cast<std::size_t>(id)].push_back(rec.time[static_cast<std::size_t>(id)] / std_time);
        }
    }
    const Stats std_stats = stats(all_times[7]);
    for (int id = 0; id < 8; ++id) {
        const Stats ts = stats(all_times[static_cast<std::size_t>(id)]);
        const Stats rs = stats(all_ratios[static_cast<std::size_t>(id)]);
        summary << name(input) << ',' << n << ",ALL," << records.size() << ','
                << kAlgorithmNames[static_cast<std::size_t>(id)] << ','
                << ts.median << ',' << ts.p25 << ',' << ts.p75 << ',' << ts.iqr << ',' << ts.mad << ','
                << std_stats.median << ',' << ts.median / std_stats.median << ','
                << rs.median << ',' << rs.p25 << ',' << rs.p75 << '\n';
        cells[static_cast<std::size_t>(id)] = BenchmarkCell{true, ts, rs};
    }
}

static void write_markdown_report(
    const std::filesystem::path& path,
    int base_trials,
    bool sweeping_all_sizes,
    const std::vector<std::size_t>& sizes,
    const std::vector<std::vector<std::array<BenchmarkCell, 8>>>& cells) {

    std::ofstream out(path, std::ios::trunc);
    out << std::fixed << std::setprecision(4);
    out << "# JesseSort benchmark results\n\n"
        << "- seed policy: one deterministic unique seed per trial; paired across all algorithms\n"
        << "- trials by size: ";
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        if (i) out << ", ";
        out << size_label(sizes[i]) << '=' << trials_for_size(base_trials, sizes[i], sweeping_all_sizes);
    }
    out << "\n- sizes: ";
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        if (i) out << ", ";
        out << size_label(sizes[i]);
    }
    out << "\n\nCells show **median paired ratio vs std::sort (median time in us)**. "
           "Values below 1.0 are faster than std::sort. Pending cells have not completed yet.\n\n";

    for (std::size_t id = 0; id < kAlgorithmNames.size(); ++id) {
        out << "## " << kAlgorithmNames[id] << "\n\n";
        out << "| Input |";
        for (std::size_t n : sizes) out << ' ' << size_label(n) << " |";
        out << "\n|---|";
        for (std::size_t i = 0; i < sizes.size(); ++i) out << "---:|";
        out << '\n';

        for (std::size_t input_index = 0; input_index < kInputs.size(); ++input_index) {
            out << "| " << name(kInputs[input_index]) << " |";
            for (std::size_t size_index = 0; size_index < sizes.size(); ++size_index) {
                const BenchmarkCell& cell = cells[size_index][input_index][id];
                if (cell.complete) {
                    out << ' ' << cell.ratio_stats.median << " ("
                        << cell.time_stats.median << ") |";
                } else {
                    out << " pending |";
                }
            }
            out << '\n';
        }
        out << '\n';
    }
    out.flush();
}

int main(int argc, char** argv) {
    // Canonical sweep: 500 distinct deterministic trial inputs at 1k/10k/100k
    // and 50 at 1m. A targeted single-size run uses the requested trial count
    // without the 1m reduction. Every algorithm, including std::sort, receives
    // the exact same source vector within each paired trial.
    const int base_trials = argc > 1 ? std::max(1, std::atoi(argv[1])) : 500;

    std::vector<std::size_t> sizes;
    const bool sweeping_all_sizes = !(argc > 2 && std::string_view(argv[2]) != "all");
    if (!sweeping_all_sizes) {
        sizes.push_back(static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10)));
    } else {
        sizes.assign(kDefaultSizes.begin(), kDefaultSizes.end());
    }

    const int warmups = argc > 3 ? std::max(0, std::atoi(argv[3])) : 2;
    const unsigned base_seed = 0x8A5CD789u;

    const bool resuming = argc > 4 && std::string_view(argv[4]).size() > 0;
    const std::filesystem::path run_dir = resuming ? std::filesystem::path(argv[4]) : make_run_directory();
    if (resuming && !std::filesystem::exists(run_dir)) {
        std::cerr << "resume directory does not exist: " << run_dir << '\n';
        return 2;
    }
    const std::filesystem::path summary_path = run_dir / "benchmark_results.csv";
    const std::filesystem::path raw_path = run_dir / "benchmark_trials.csv";
    const std::filesystem::path markdown_path = run_dir / "benchmark_results.md";
    const std::filesystem::path metadata_path = run_dir / "benchmark_metadata.txt";
    const std::filesystem::path progress_path = run_dir / "benchmark_progress.txt";

    LoadedRecords loaded = resuming ? load_raw_records(raw_path) : LoadedRecords{};

    std::ofstream summary(summary_path, std::ios::trunc);
    summary << "input,n,seed_scope,trials,algorithm,median_us,p25_us,p75_us,iqr_us,mad_us,"
               "std_median_us,ratio_of_medians,median_paired_ratio,p25_paired_ratio,p75_paired_ratio\n";
    summary << std::setprecision(17);

    std::ofstream raw;
    if (resuming) {
        raw.open(raw_path, std::ios::app);
    } else {
        raw.open(raw_path, std::ios::trunc);
        raw << "input,n,seed,trial,algorithm,order_position,time_us,std_sort_us,paired_ratio_to_std\n";
    }
    raw << std::setprecision(17);
    raw.flush();

    if (!resuming) write_metadata(metadata_path, base_trials, sizes, sweeping_all_sizes, warmups);

    std::vector<std::vector<std::array<BenchmarkCell, 8>>> cells(
        sizes.size(), std::vector<std::array<BenchmarkCell, 8>>(kInputs.size()));

    // Reconstruct finalized cells from raw complete trials. This also rewrites the
    // summary CSV without duplicate rows after any number of resumes.
    for (std::size_t si = 0; si < sizes.size(); ++si) {
        const std::size_t n = sizes[si];
        const int expected_trials = trials_for_size(base_trials, n, sweeping_all_sizes);
        for (std::size_t ii = 0; ii < kInputs.size(); ++ii) {
            std::vector<TrialRecord> existing;
            const auto key = std::make_pair(n, static_cast<int>(ii));
            auto it = loaded.find(key);
            if (it != loaded.end()) {
                for (int t = 0; t < expected_trials; ++t) {
                    auto jt = it->second.find(t);
                    if (jt != it->second.end() && jt->second.complete()) existing.push_back(jt->second.rec);
                }
            }
            if (static_cast<int>(existing.size()) == expected_trials)
                summarize_records(existing, kInputs[ii], n, summary, cells[si][ii]);
        }
    }
    summary.flush();
    write_markdown_report(markdown_path, base_trials, sweeping_all_sizes, sizes, cells);
    write_progress(progress_path, sizes.front(), "startup", 0, trials_for_size(base_trials, sizes.front(), sweeping_all_sizes), "starting");

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "# JesseSort benchmark results\n\n"
              << "seed_policy=unique_deterministic_seed_per_trial trials_by_size=";
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << size_label(sizes[i]) << ':'
                  << trials_for_size(base_trials, sizes[i], sweeping_all_sizes);
    }
    std::cout << " sizes=";
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << size_label(sizes[i]);
    }
    std::cout << "\nresults_dir=" << run_dir.string();
    if (resuming) std::cout << " (resuming)";
    std::cout << "\n\n";

    for (std::size_t size_index = 0; size_index < sizes.size(); ++size_index) {
        const std::size_t n = sizes[size_index];
        const int trials_this_size = trials_for_size(base_trials, n, sweeping_all_sizes);
        for (std::size_t input_index = 0; input_index < kInputs.size(); ++input_index) {
            const InputType input = kInputs[input_index];
            const auto key = std::make_pair(n, static_cast<int>(input_index));
            auto loaded_it = loaded.find(key);
            std::vector<TrialRecord> records;
            records.reserve(static_cast<std::size_t>(trials_this_size));
            int resumed_trials = 0;
            if (loaded_it != loaded.end()) {
                for (int trial = 0; trial < trials_this_size; ++trial) {
                    auto jt = loaded_it->second.find(trial);
                    if (jt != loaded_it->second.end() && jt->second.complete()) ++resumed_trials;
                }
            }

            if (resumed_trials < trials_this_size) {
                write_progress(progress_path, n, name(input), resumed_trials, trials_this_size,
                               resumed_trials ? "resuming" : "warming_up");
                for (int w = 0; w < warmups; ++w) {
                    std::mt19937 rng(trial_rng_seed(0xA5A5A5A5u, n, input, w));
                    std::vector<int> source(n);
                    generate(source, input, rng);
                    for (int id = 0; id < 8; ++id) {
                        auto values = source;
                        run_algorithm(id, values);
                        if (!std::is_sorted(values.begin(), values.end())) {
                            std::cerr << "warmup validation failure: n=" << n << ' ' << name(input)
                                      << " algorithm=" << kAlgorithmNames[id] << '\n';
                            return 1;
                        }
                    }
                }
            }

            for (int trial = 0; trial < trials_this_size; ++trial) {
                if (loaded_it != loaded.end()) {
                    auto jt = loaded_it->second.find(trial);
                    if (jt != loaded_it->second.end() && jt->second.complete()) {
                        records.push_back(jt->second.rec);
                        continue;
                    }
                }

                const unsigned input_seed = trial_rng_seed(base_seed, n, input, trial);
                std::mt19937 rng(input_seed);
                std::vector<int> source(n);
                generate(source, input, rng);
                auto expected = source;
                std::sort(expected.begin(), expected.end());

                TrialRecord rec;
                rec.seed = input_seed;
                rec.trial = trial;
                const auto order = execution_order(trial);

                for (int pos = 0; pos < 8; ++pos) {
                    const int id = order[static_cast<std::size_t>(pos)];
                    auto values = source;
                    const double elapsed = timed([&]{ run_algorithm(id, values); });
                    if (values != expected) {
                        std::cerr << "validation failure: n=" << n << ' ' << name(input)
                                  << " seed=" << input_seed << " trial=" << trial
                                  << " algorithm=" << kAlgorithmNames[static_cast<std::size_t>(id)] << '\n';
                        return 1;
                    }
                    rec.time[static_cast<std::size_t>(id)] = elapsed;
                    rec.order_position[static_cast<std::size_t>(id)] = pos;
                }
                records.push_back(rec);

                const double std_time = rec.time[7];
                for (int id = 0; id < 8; ++id) {
                    const double ratio = rec.time[static_cast<std::size_t>(id)] / std_time;
                    raw << name(input) << ',' << n << ',' << rec.seed << ',' << rec.trial << ','
                        << kAlgorithmNames[static_cast<std::size_t>(id)] << ','
                        << rec.order_position[static_cast<std::size_t>(id)] << ','
                        << rec.time[static_cast<std::size_t>(id)] << ',' << std_time << ',' << ratio << '\n';
                }
                raw.flush();
                write_progress(progress_path, n, name(input), static_cast<int>(records.size()),
                               trials_this_size, "running");
            }

            if (static_cast<int>(records.size()) != trials_this_size) {
                std::cerr << "resume reconstruction error: n=" << n << ' ' << name(input)
                          << " have=" << records.size() << " expected=" << trials_this_size << '\n';
                return 3;
            }

            summarize_records(records, input, n, summary, cells[size_index][input_index]);

            summary.flush();
            raw.flush();
            write_markdown_report(markdown_path, base_trials, sweeping_all_sizes, sizes, cells);
            write_progress(progress_path, n, name(input), trials_this_size, trials_this_size, "input_size_complete");

            std::cout << "Completed n=" << size_label(n) << " input=" << name(input) << '\n';
            std::cout.flush();
        }
    }

    write_progress(progress_path, sizes.back(), "complete", trials_for_size(base_trials, sizes.back(), sweeping_all_sizes),
                   trials_for_size(base_trials, sizes.back(), sweeping_all_sizes), "complete");
    summary.flush();
    raw.flush();
    write_markdown_report(markdown_path, base_trials, sweeping_all_sizes, sizes, cells);
    std::cout << "Wrote " << markdown_path.string() << ", "
              << summary_path.string() << ", " << raw_path.string() << ", "
              << metadata_path.string() << ", " << progress_path.string() << '\n';
    return 0;
}
