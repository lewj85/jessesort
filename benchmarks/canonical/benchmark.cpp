#include "benchmark_seed.h"
#ifndef JESSESORT_BENCH_LAYOUT_HEADER
#define JESSESORT_BENCH_LAYOUT_HEADER "../generated/benchmark_layouts.h"
#endif
#include JESSESORT_BENCH_LAYOUT_HEADER

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
    Random = 0, Sorted = 1, Reverse = 2, NearlySorted = 3, Random25 = 4,
    Alternating = 5, Sawtooth = 6, BlockSorted = 7, OrganPipe = 8, Rotated = 9,
    SortedNoise10 = 12, MixedDirectionRuns = 13,
    MixedPhase3 = 14, MixedPhase12 = 15
};

static std::string_view name(InputType t) {
    switch (t) {
        case InputType::Random: return "Random";
        case InputType::Sorted: return "Sorted";
        case InputType::Reverse: return "Reverse";
        case InputType::NearlySorted: return "Sorted+Noise(5%)";
        case InputType::Random25: return "Random%25";
        case InputType::Alternating: return "Alternating";
        case InputType::Sawtooth: return "Sawtooth";
        case InputType::BlockSorted: return "BlockSorted";
        case InputType::SortedNoise10: return "Sorted+Noise(10%)";
        case InputType::MixedDirectionRuns: return "MixedDirectionRuns";
        case InputType::OrganPipe: return "OrganPipe";
        case InputType::Rotated: return "Rotated";
        case InputType::MixedPhase3: return "MixedPhase3";
        case InputType::MixedPhase12: return "MixedPhase12";
    }
    return "?";
}

using jessesort_benchmark_config::AlgorithmSpec;
static constexpr auto& kAlgorithmSpecs = jessesort_benchmark_config::kAlgorithmSpecs;
static constexpr std::size_t kAlgorithmCount = jessesort_benchmark_config::kAlgorithmCount;
static constexpr std::string_view kBenchmarkLayoutSha256 = jessesort_benchmark_config::kLayoutSha256;

static constexpr std::string_view kBenchmarkMethodVersion = "E695-maintained-isolated-cell-v3";


static constexpr std::array<InputType, 12> kBaselineInputs{
    InputType::Random, InputType::Sorted, InputType::Reverse,
    InputType::NearlySorted, InputType::SortedNoise10, InputType::Random25,
    InputType::Alternating, InputType::Sawtooth, InputType::MixedDirectionRuns,
    InputType::BlockSorted, InputType::OrganPipe, InputType::Rotated
};

static constexpr std::array<InputType, 14> kInputs{
    InputType::Random, InputType::Sorted, InputType::Reverse,
    InputType::NearlySorted, InputType::SortedNoise10, InputType::Random25,
    InputType::Alternating, InputType::Sawtooth, InputType::MixedDirectionRuns,
    InputType::BlockSorted, InputType::OrganPipe, InputType::Rotated,
    InputType::MixedPhase3, InputType::MixedPhase12
};

static constexpr std::array<std::size_t, 2> kDefaultSizes{
    10000, 100000
};

static std::size_t structured_scale(std::size_t n) {
    // Balanced sublinear structural scale: both typical run length and run count
    // grow with n. n^(2/3) yields about 100/464/2154/10000 elements per
    // run at 1k/10k/100k/1m, respectively.
    return std::max<std::size_t>(8, static_cast<std::size_t>(
        std::pow(static_cast<double>(n), 2.0 / 3.0)));
}

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
        case InputType::SortedNoise10: {
            std::iota(v.begin(), v.end(), 0);
            std::bernoulli_distribution change(0.10);
            std::uniform_int_distribution<int> d(0, static_cast<int>(n) - 1);
            for (int& x : v) if (change(rng)) x = d(rng);
            break;
        }
        case InputType::Random25: {
            std::uniform_int_distribution<int> d(0, 24);
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
            // Classic repeated ascending ramp with a hard reset, but scale the
            // period sublinearly so both run length and run count grow with n.
            // target ~= n^(2/3): about 10x100 at 1k, 22x464 at 10k,
            // 47x2154 at 100k, and 100x10000 at 1m.
            const std::size_t period = structured_scale(n);
            for (std::size_t i = 0; i < n; ++i)
                v[i] = static_cast<int>(i % period);
            break;
        }
        case InputType::MixedDirectionRuns: {
            // Seeded family of natural monotone runs. Both run count and run length
            // scale with n: target length ~= n^(2/3), with substantial per-run
            // variation. Direction, starting range, and positive step size vary
            // independently so this is not a repeated sawtooth or shuffled-block proxy.
            const std::size_t target = structured_scale(n);
            const std::size_t minLen = std::max<std::size_t>(4, target / 2);
            const std::size_t maxLen = std::max(minLen, target + target / 2);
            std::uniform_int_distribution<std::size_t> lenDist(minLen, maxLen);
            std::bernoulli_distribution descending(0.5);
            std::uniform_int_distribution<int> stepDist(1, 7);
            const long long span = std::min<long long>(
                static_cast<long long>(std::numeric_limits<int>::max()) / 8,
                std::max<long long>(1024, static_cast<long long>(n) * 8));
            std::uniform_int_distribution<long long> startDist(-span, span);

            std::size_t pos = 0;
            while (pos < n) {
                const std::size_t len = std::min(lenDist(rng), n - pos);
                const int step = stepDist(rng);
                const bool desc = descending(rng);
                long long startValue = startDist(rng);
                const long long runSpan = static_cast<long long>(len - 1) * step;
                if (desc && startValue - runSpan < std::numeric_limits<int>::min())
                    startValue = static_cast<long long>(std::numeric_limits<int>::min()) + runSpan;
                if (!desc && startValue + runSpan > std::numeric_limits<int>::max())
                    startValue = static_cast<long long>(std::numeric_limits<int>::max()) - runSpan;
                for (std::size_t k = 0; k < len; ++k) {
                    const long long delta = static_cast<long long>(k) * step;
                    v[pos + k] = static_cast<int>(desc ? startValue - delta : startValue + delta);
                }
                pos += len;
            }
            break;
        }
        case InputType::BlockSorted: {
            // Globally sorted unique values partitioned into variable-length
            // ascending blocks and then shuffled. Like MixedDirectionRuns, the
            // structural scale is centered on n^(2/3), but every block remains
            // ascending and retains its original disjoint value interval.
            std::iota(v.begin(), v.end(), 0);
            const std::size_t target = structured_scale(n);
            const std::size_t minLen = std::max<std::size_t>(4, target / 2);
            const std::size_t maxLen = std::max(minLen, target + target / 2);
            std::uniform_int_distribution<std::size_t> lenDist(minLen, maxLen);

            struct Block { std::size_t begin; std::size_t len; };
            std::vector<Block> blocks;
            for (std::size_t pos = 0; pos < n;) {
                const std::size_t len = std::min(lenDist(rng), n - pos);
                blocks.push_back({pos, len});
                pos += len;
            }
            std::shuffle(blocks.begin(), blocks.end(), rng);

            const auto source = v;
            std::size_t dest = 0;
            for (const Block& block : blocks) {
                std::copy_n(
                    source.begin() + static_cast<std::ptrdiff_t>(block.begin),
                    block.len,
                    v.begin() + static_cast<std::ptrdiff_t>(dest));
                dest += block.len;
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
        case InputType::MixedPhase3: {
            // Router-safety input: three contiguous phases with deliberately
            // different structural regimes. Sorted+Noise(5%) is first so a
            // prefix-only router is tempted by near-sorted structure; mixed
            // natural directions then exercise both dual-Patience games; the
            // final Random phase tests whether late entropy is re-detected.
            static constexpr std::array<InputType, 3> phases{
                InputType::NearlySorted, InputType::MixedDirectionRuns, InputType::Random
            };
            std::size_t begin = 0;
            for (std::size_t phase = 0; phase < phases.size(); ++phase) {
                const std::size_t end = n * (phase + 1) / phases.size();
                std::vector<int> part(end - begin);
                generate(part, phases[phase], rng);
                std::copy(part.begin(), part.end(),
                          v.begin() + static_cast<std::ptrdiff_t>(begin));
                begin = end;
            }
            break;
        }
        case InputType::MixedPhase12: {
            // Router-safety input: one contiguous phase from every original
            // baseline generator, in the canonical baseline order. Phase
            // lengths differ by at most one element and sum exactly to n.
            std::size_t begin = 0;
            for (std::size_t phase = 0; phase < kBaselineInputs.size(); ++phase) {
                const std::size_t end = n * (phase + 1) / kBaselineInputs.size();
                std::vector<int> part(end - begin);
                generate(part, kBaselineInputs[phase], rng);
                std::copy(part.begin(), part.end(),
                          v.begin() + static_cast<std::ptrdiff_t>(begin));
                begin = end;
            }
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
    jessesort_benchmark_config::run_algorithm(static_cast<std::size_t>(id), values);
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
    std::array<double, kAlgorithmCount> time{};
    std::array<int, kAlgorithmCount> order_position{};
};


static unsigned trial_rng_seed(unsigned base_seed, std::size_t n, InputType input, int trial) {
    return jessesort::bench::trial_seed(base_seed, n, static_cast<int>(input), trial);
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
                           bool sweeping_all_sizes, int warmups, std::string_view input_filter) {
    std::ofstream out(path);
    const std::time_t now = std::time(nullptr);
    out << "JesseSort benchmark metadata\n";
    out << "benchmark_method_version=" << kBenchmarkMethodVersion << '\n';
    out << "generated=" << std::put_time(std::gmtime(&now), "%Y-%m-%dT%H:%M:%SZ") << '\n';
    out << "sizes=";
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        if (i) out << ',';
        out << sizes[i];
    }
    out << '\n';
    out << "trial_seed_policy=one deterministic unique seed per trial; isolated processes regenerate identical inputs from the same seed schedule\n";
    out << "base_trials=" << base_trials << '\n';
    for (std::size_t n : sizes)
        out << "trials_" << size_label(n) << '=' << trials_for_size(base_trials, n, sweeping_all_sizes) << '\n';
    out << "warmup_trials_per_input=" << warmups << '\n';
    out << "input_filter=" << (input_filter.empty() ? "all" : input_filter) << '\n';
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
    out << "method=single-algorithm/single-input cell process when invoked by benchmarks/canonical/run.sh; fresh deterministic input seed per trial; reference validation occurs after the timed call; cross-cell aggregation is independent\n";
    out << "cache_policy=warm-state benchmark; selected algorithm receives per-input warmups; no forced hardware-cache flush; source buffer is copied immediately before timing\n";
    out << "validation_policy=std::sort reference is computed only after the timed call so it cannot prewarm the algorithm under test or the std::sort control\n";
    out << "warning=absolute timings and saved std::sort medians are not portable across VM/host sessions; CPU frequency, thermal state, scheduler placement, and host load remain external noise sources\n";
    out << "benchmark_layout_sha256=" << kBenchmarkLayoutSha256 << '\n';
    for (const auto& spec : kAlgorithmSpecs) {
        out << "variation_map=" << spec.short_name << '|' << spec.header_file
            << "|default=" << (spec.default_selected ? 1 : 0) << '\n';
        out << "pipeline_recipe=" << spec.short_name << "|base=" << spec.base_pipeline
            << "|modules=" << spec.module_layout << '\n';
    }
}

struct BenchmarkCell {
    bool complete = false;
    Stats time_stats{};
    Stats ratio_stats{};
};

static int algorithm_id(std::string_view n) {
    for (std::size_t i = 0; i < kAlgorithmSpecs.size(); ++i)
        if (kAlgorithmSpecs[i].short_name == n) return static_cast<int>(i);
    return -1;
}

static bool parse_selected_algorithm(
    std::string_view arg,
    std::array<bool, kAlgorithmCount>& selected,
    std::string& error) {
    selected.fill(false);
    if (arg.empty() || arg == "default" || arg == "all" || arg.find(',') != std::string_view::npos) {
        error = std::string(arg);
        return false;
    }
    const int id = algorithm_id(arg);
    if (id < 0) {
        error = std::string(arg);
        return false;
    }
    selected[static_cast<std::size_t>(id)] = true;
    return true;
}

static int input_id(std::string_view n) {
    for (std::size_t i = 0; i < kInputs.size(); ++i)
        if (name(kInputs[i]) == n) return static_cast<int>(i);
    return -1;
}

static bool parse_selected_input(std::string_view arg, std::array<bool, kInputs.size()>& selected) {
    selected.fill(false);
    if (arg.empty() || arg == "all") {
        selected.fill(true);
        return true;
    }
    const int id = input_id(arg);
    if (id < 0) return false;
    selected[static_cast<std::size_t>(id)] = true;
    return true;
}

struct LoadedTrial {
    TrialRecord rec{};
    std::array<bool, kAlgorithmCount> seen{};

    bool complete(const std::array<bool, kAlgorithmCount>& selected) const {
        for (std::size_t id = 0; id < kAlgorithmCount; ++id)
            if (selected[id] && !seen[id]) return false;
        return true;
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
        if (f.size() != 6) continue;
        const int iid = input_id(f[0]);
        const int aid = algorithm_id(f[4]);
        if (iid < 0 || aid < 0) continue;
        try {
            const std::size_t n = static_cast<std::size_t>(std::stoull(f[1]));
            const int trial = std::stoi(f[3]);
            auto& lt = loaded[{n, iid}][trial];
            lt.rec.seed = static_cast<unsigned>(std::stoul(f[2]));
            lt.rec.trial = trial;
            lt.rec.order_position[static_cast<std::size_t>(aid)] = 0;
            lt.rec.time[static_cast<std::size_t>(aid)] = std::stod(f[5]);
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
                              std::array<BenchmarkCell, kAlgorithmCount>& cells,
                              const std::array<bool, kAlgorithmCount>& selected) {
    std::array<std::vector<double>, kAlgorithmCount> all_times;
    for (auto& v : all_times) v.reserve(records.size());
    for (const TrialRecord& rec : records) {
        for (std::size_t id = 0; id < kAlgorithmCount; ++id) {
            if (!selected[id]) continue;
            all_times[id].push_back(rec.time[id]);
        }
    }
    for (std::size_t id = 0; id < kAlgorithmCount; ++id) {
        if (!selected[id]) continue;
        const Stats ts = stats(all_times[id]);
        summary << name(input) << ',' << n << ",ALL," << records.size() << ','
                << kAlgorithmSpecs[id].short_name << ','
                << ts.median << ',' << ts.p25 << ',' << ts.p75 << ',' << ts.iqr << ',' << ts.mad << '\n';
        cells[id] = BenchmarkCell{true, ts, {}};
    }
}

static void write_markdown_report(
    const std::filesystem::path& path,
    int base_trials,
    bool sweeping_all_sizes,
    const std::vector<std::size_t>& sizes,
    const std::vector<std::vector<std::array<BenchmarkCell, kAlgorithmCount>>>& cells,
    const std::array<bool, kAlgorithmCount>& selected) {

    std::ofstream out(path, std::ios::trunc);
    out << std::fixed << std::setprecision(4);
    out << "# JesseSort benchmark results\n\n"
        << "- seed policy: one deterministic unique seed per trial; identical seed schedule across isolated algorithm processes\n"
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
    out << "\n\nCells show **median time in us** for this isolated single-algorithm process. "
           "Use benchmarks/canonical/run.sh for cross-algorithm ratio tables. Pending cells have not completed yet.\n\n";

    for (std::size_t id = 0; id < kAlgorithmSpecs.size(); ++id) {
        if (!selected[id]) continue;
        out << "## " << kAlgorithmSpecs[id].short_name << "\n\n";
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
                    out << ' ' << cell.time_stats.median << " |";
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


static bool resume_metadata_matches_method(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) return false;
    const std::string wanted = "benchmark_method_version=" + std::string(kBenchmarkMethodVersion);
    std::string line;
    while (std::getline(in, line))
        if (line == wanted) return true;
    return false;
}

int main(int argc, char** argv) {
    // Canonical sweep defaults to 10k and 100k only. The routine benchmark runner
    // invokes one explicit size/algorithm/input cell per process for one uninterrupted
    // full trial population. The outer runner checkpoints only after a complete
    // cell commits; interrupted cells are rerun from scratch. Explicit non-default
    // sizes remain available only through direct binary invocation.
    const int base_trials = argc > 1 ? std::max(1, std::atoi(argv[1])) : 500;

    std::vector<std::size_t> sizes;
    const bool sweeping_all_sizes = !(argc > 2 && std::string_view(argv[2]) != "all");
    if (!sweeping_all_sizes) {
        sizes.push_back(static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10)));
    } else {
        sizes.assign(kDefaultSizes.begin(), kDefaultSizes.end());
    }

    const int warmups = argc > 3 ? std::max(0, std::atoi(argv[3])) : 2;
    const unsigned base_seed = jessesort::bench::kCanonicalBaseSeed;

    std::array<bool, kAlgorithmCount> selected{};
    const std::string_view selected_variations_arg = argc > 5 ? std::string_view(argv[5]) : std::string_view();
    std::string invalid_variation;
    if (!parse_selected_algorithm(selected_variations_arg, selected, invalid_variation)) {
        std::cerr << "benchmark requires exactly one algorithm per process; got: '" << invalid_variation
                  << "'\nUse ./benchmarks/canonical/run.sh for the routine multi-algorithm benchmark suite.\n";
        return 4;
    }

    std::array<bool, kInputs.size()> selected_inputs{};
    const std::string_view selected_input_arg = argc > 6 ? std::string_view(argv[6]) : std::string_view();
    if (!parse_selected_input(selected_input_arg, selected_inputs)) {
        std::cerr << "unknown canonical input filter: '" << selected_input_arg << "'\n";
        return 7;
    }

    const bool explicit_run_dir = argc > 4 && std::string_view(argv[4]).size() > 0;
    const std::filesystem::path run_dir = explicit_run_dir ? std::filesystem::path(argv[4]) : make_run_directory();
    const bool resuming = explicit_run_dir && std::filesystem::exists(run_dir / "benchmark_metadata.txt");
    if (explicit_run_dir && !std::filesystem::exists(run_dir)) {
        std::filesystem::create_directories(run_dir);
    }
    const std::filesystem::path summary_path = run_dir / "benchmark_results.csv";
    const std::filesystem::path raw_path = run_dir / "benchmark_trials.csv";
    const std::filesystem::path markdown_path = run_dir / "benchmark_results.md";
    const std::filesystem::path metadata_path = run_dir / "benchmark_metadata.txt";
    const std::filesystem::path progress_path = run_dir / "benchmark_progress.txt";

    if (resuming && !resume_metadata_matches_method(metadata_path)) {
        std::cerr << "resume directory uses an older/incompatible benchmark methodology: "
                  << run_dir << "\nStart a new run directory instead of mixing pre-E323 and E323 trials.\n";
        return 6;
    }

    LoadedRecords loaded = resuming ? load_raw_records(raw_path) : LoadedRecords{};

    std::ofstream summary(summary_path, std::ios::trunc);
    summary << "input,n,seed_scope,trials,algorithm,median_us,p25_us,p75_us,iqr_us,mad_us\n";
    summary << std::setprecision(17);

    std::ofstream raw;
    if (resuming) {
        raw.open(raw_path, std::ios::app);
    } else {
        raw.open(raw_path, std::ios::trunc);
        raw << "input,n,seed,trial,algorithm,time_us\n";
    }
    raw << std::setprecision(17);
    raw.flush();

    write_metadata(metadata_path, base_trials, sizes, sweeping_all_sizes, warmups, selected_input_arg);

    std::vector<std::vector<std::array<BenchmarkCell, kAlgorithmCount>>> cells(
        sizes.size(), std::vector<std::array<BenchmarkCell, kAlgorithmCount>>(kInputs.size()));

    // Reconstruct finalized cells from raw complete trials. This also rewrites the
    // summary CSV without duplicate rows after any number of resumes.
    for (std::size_t si = 0; si < sizes.size(); ++si) {
        const std::size_t n = sizes[si];
        const int expected_trials = trials_for_size(base_trials, n, sweeping_all_sizes);
        for (std::size_t ii = 0; ii < kInputs.size(); ++ii) {
            if (!selected_inputs[ii]) continue;
            std::vector<TrialRecord> existing;
            const auto key = std::make_pair(n, static_cast<int>(ii));
            auto it = loaded.find(key);
            if (it != loaded.end()) {
                for (int t = 0; t < expected_trials; ++t) {
                    auto jt = it->second.find(t);
                    if (jt != it->second.end() && jt->second.complete(selected)) existing.push_back(jt->second.rec);
                }
            }
            if (static_cast<int>(existing.size()) == expected_trials)
                summarize_records(existing, kInputs[ii], n, summary, cells[si][ii], selected);
        }
    }
    summary.flush();
    write_markdown_report(markdown_path, base_trials, sweeping_all_sizes, sizes, cells, selected);
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
    std::cout << " algorithm=" << selected_variations_arg;
    std::cout << " input=" << (selected_input_arg.empty() ? "all" : selected_input_arg);
    std::cout << "\nresults_dir=" << run_dir.string();
    if (resuming) std::cout << " (resuming)";
    std::cout << "\n\n";

    for (std::size_t size_index = 0; size_index < sizes.size(); ++size_index) {
        const std::size_t n = sizes[size_index];
        const int trials_this_size = trials_for_size(base_trials, n, sweeping_all_sizes);
        for (std::size_t input_index = 0; input_index < kInputs.size(); ++input_index) {
            if (!selected_inputs[input_index]) continue;
            const InputType input = kInputs[input_index];
            const auto key = std::make_pair(n, static_cast<int>(input_index));
            auto loaded_it = loaded.find(key);
            std::vector<TrialRecord> records;
            records.reserve(static_cast<std::size_t>(trials_this_size));
            int resumed_trials = 0;
            if (loaded_it != loaded.end()) {
                for (int trial = 0; trial < trials_this_size; ++trial) {
                    auto jt = loaded_it->second.find(trial);
                    if (jt != loaded_it->second.end() && jt->second.complete(selected)) ++resumed_trials;
                }
            }

            if (resumed_trials < trials_this_size) {
                write_progress(progress_path, n, name(input), resumed_trials, trials_this_size,
                               resumed_trials ? "resuming" : "warming_up");
                for (int w = 0; w < warmups; ++w) {
                    std::mt19937 rng(trial_rng_seed(0xA5A5A5A5u, n, input, w));
                    std::vector<int> source(n);
                    generate(source, input, rng);
                    for (std::size_t id = 0; id < kAlgorithmCount; ++id) {
                        if (!selected[id]) continue;
                        auto values = source;
                        run_algorithm(id, values);
                        if (!std::is_sorted(values.begin(), values.end())) {
                            std::cerr << "warmup validation failure: n=" << n << ' ' << name(input)
                                      << " algorithm=" << kAlgorithmSpecs[id].short_name << '\n';
                            return 1;
                        }
                    }
                }
            }

            for (int trial = 0; trial < trials_this_size; ++trial) {
                if (loaded_it != loaded.end()) {
                    auto jt = loaded_it->second.find(trial);
                    if (jt != loaded_it->second.end() && jt->second.complete(selected)) {
                        records.push_back(jt->second.rec);
                        continue;
                    }
                }

                const unsigned input_seed = trial_rng_seed(base_seed, n, input, trial);
                std::mt19937 rng(input_seed);
                std::vector<int> source(n);
                generate(source, input, rng);

                TrialRecord rec;
                rec.seed = input_seed;
                rec.trial = trial;
                std::size_t selected_id = 0;
                while (selected_id < kAlgorithmCount && !selected[selected_id]) ++selected_id;
                auto values = source;
                const double elapsed = timed([&]{ run_algorithm(static_cast<int>(selected_id), values); });

                // Validation must not precondition the timed algorithm. In particular,
                // timing std::sort after an untimed std::sort of the same input family
                // gives the control a repeat-call cache/predictor advantage.
                auto expected = source;
                std::sort(expected.begin(), expected.end());
                if (values != expected) {
                    std::cerr << "validation failure: n=" << n << ' ' << name(input)
                              << " seed=" << input_seed << " trial=" << trial
                              << " algorithm=" << kAlgorithmSpecs[selected_id].short_name << '\n';
                    return 1;
                }
                rec.time[selected_id] = elapsed;
                rec.order_position[selected_id] = 0;
                records.push_back(rec);

                raw << name(input) << ',' << n << ',' << rec.seed << ',' << rec.trial << ','
                    << kAlgorithmSpecs[selected_id].short_name << ','
                    << rec.time[selected_id] << '\n';
                raw.flush();
                write_progress(progress_path, n, name(input), static_cast<int>(records.size()),
                               trials_this_size, "running");
            }

            if (static_cast<int>(records.size()) != trials_this_size) {
                std::cerr << "resume reconstruction error: n=" << n << ' ' << name(input)
                          << " have=" << records.size() << " expected=" << trials_this_size << '\n';
                return 3;
            }

            summarize_records(records, input, n, summary, cells[size_index][input_index], selected);

            summary.flush();
            raw.flush();
            write_markdown_report(markdown_path, base_trials, sweeping_all_sizes, sizes, cells, selected);
            write_progress(progress_path, n, name(input), trials_this_size, trials_this_size, "input_size_complete");

            std::cout << "Completed n=" << size_label(n) << " input=" << name(input) << '\n';
            std::cout.flush();
        }
    }

    write_progress(progress_path, sizes.back(), "complete", trials_for_size(base_trials, sizes.back(), sweeping_all_sizes),
                   trials_for_size(base_trials, sizes.back(), sweeping_all_sizes), "complete");
    summary.flush();
    raw.flush();
    write_markdown_report(markdown_path, base_trials, sweeping_all_sizes, sizes, cells, selected);
    std::cout << "Wrote " << markdown_path.string() << ", "
              << summary_path.string() << ", " << raw_path.string() << ", "
              << metadata_path.string() << ", " << progress_path.string() << '\n';
    return 0;
}
