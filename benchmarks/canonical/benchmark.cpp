#include "benchmark_seed.h"
#include "input_generators.h"
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
    Diverging = 5, Sawtooth = 6, OrganPipe = 8, Rotated = 9,
    MixedDirectionRuns = 13
};

static std::string_view name(InputType t) {
    switch (t) {
        case InputType::Random: return "Random";
        case InputType::Sorted: return "Sorted";
        case InputType::Reverse: return "Reverse";
        case InputType::NearlySorted: return "Sorted+Noise(5%)";
        case InputType::Random25: return "Random%25";
        case InputType::Diverging: return "Diverging";
        case InputType::Sawtooth: return "Sawtooth";
        case InputType::MixedDirectionRuns: return "MixedDirectionRuns";
        case InputType::OrganPipe: return "OrganPipe";
        case InputType::Rotated: return "Rotated";
    }
    return "?";
}

using jessesort_benchmark_config::AlgorithmSpec;
static constexpr auto& kAlgorithmSpecs = jessesort_benchmark_config::kAlgorithmSpecs;
static constexpr std::size_t kAlgorithmCount = jessesort_benchmark_config::kAlgorithmCount;
static constexpr std::string_view kBenchmarkLayoutSha256 = jessesort_benchmark_config::kLayoutSha256;

static constexpr std::string_view kBenchmarkMethodVersion = "E756-forked-block-v1";


// Deliberately small core suite. Additional inputs live only in the shared
// additional-suite generator, avoiding dormant second implementations here.
static constexpr std::array<InputType, 10> kInputs{
    InputType::Random, InputType::Sorted, InputType::Reverse,
    InputType::NearlySorted, InputType::Random25,
    InputType::Diverging, InputType::Sawtooth, InputType::MixedDirectionRuns,
    InputType::OrganPipe, InputType::Rotated
};

static constexpr std::array<std::size_t, 2> kDefaultSizes{
    10000, 100000
};

static std::string size_label(std::size_t n) {
    if (n == 1000) return "1k";
    if (n == 10000) return "10k";
    if (n == 100000) return "100k";
    if (n == 1000000) return "1m";
    return std::to_string(n);
}

static void generate(std::vector<int>& v, InputType t, std::mt19937& rng) {
    namespace inputs = jessesort::bench::inputs;
    switch (t) {
        case InputType::Random: inputs::random(v, rng); break;
        case InputType::Sorted: inputs::sorted(v); break;
        case InputType::Reverse: inputs::reverse(v); break;
        case InputType::NearlySorted: inputs::sorted_noise(v, rng, 0.05); break;
        case InputType::Random25: inputs::random_cardinality(v, rng, 25); break;
        case InputType::Diverging: inputs::diverging(v); break;
        case InputType::Sawtooth: inputs::sawtooth(v); break;
        case InputType::MixedDirectionRuns: inputs::mixed_direction_runs(v, rng); break;
        case InputType::OrganPipe: inputs::organ_pipe(v); break;
        case InputType::Rotated: inputs::rotated(v, rng); break;
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
                           bool sweeping_all_sizes, int warmups, std::string_view input_filter,
                           int trial_offset, int fork_index, int total_timed_trials) {
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
    out << "fork_index=" << fork_index << '\n';
    out << "trial_offset=" << trial_offset << '\n';
    out << "total_timed_trials=" << total_timed_trials << '\n';
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
    out << "method=E756 forked-block benchmark: one algorithm/input/size/fork per process; timed seeds are globally unique across forks; reference validation occurs after the timed call\n";
    out << "cache_policy=warm-state within each independent fork; two fork-specific warmups precede a bounded timed block; no single warmup history owns the full cell\n";
    out << "warmup_seed_policy=E756 fork-specific deterministic warmup seeds; distinct across fork/index and guaranteed disjoint from all concrete timed RNG seeds for the complete cell\n";
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

    const int trial_offset = argc > 7 ? std::max(0, std::atoi(argv[7])) : 0;
    const int fork_index = argc > 8 ? std::max(0, std::atoi(argv[8])) : 0;
    const int total_timed_trials = argc > 9 ? std::max(base_trials, std::atoi(argv[9])) : base_trials;

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

    write_metadata(metadata_path, base_trials, sizes, sweeping_all_sizes, warmups, selected_input_arg, trial_offset, fork_index, total_timed_trials);

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
                    const int global_trial = trial_offset + t;
                    auto jt = it->second.find(global_trial);
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
    std::cout << " fork=" << fork_index << " trial_offset=" << trial_offset;
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
                    const int global_trial = trial_offset + trial;
                    auto jt = loaded_it->second.find(global_trial);
                    if (jt != loaded_it->second.end() && jt->second.complete(selected)) ++resumed_trials;
                }
            }

            if (resumed_trials < trials_this_size) {
                write_progress(progress_path, n, name(input), resumed_trials, trials_this_size,
                               resumed_trials ? "resuming" : "warming_up");
                for (int w = 0; w < warmups; ++w) {
                    const unsigned warm_seed = jessesort::bench::warmup_seed_for_fork(
                        base_seed, n, static_cast<int>(input), fork_index, w, total_timed_trials);
                    std::mt19937 rng(warm_seed);
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
                const int global_trial = trial_offset + trial;
                if (loaded_it != loaded.end()) {
                    auto jt = loaded_it->second.find(global_trial);
                    if (jt != loaded_it->second.end() && jt->second.complete(selected)) {
                        records.push_back(jt->second.rec);
                        continue;
                    }
                }

                const unsigned input_seed = trial_rng_seed(base_seed, n, input, global_trial);
                std::mt19937 rng(input_seed);
                std::vector<int> source(n);
                generate(source, input, rng);

                TrialRecord rec;
                rec.seed = input_seed;
                rec.trial = global_trial;
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
