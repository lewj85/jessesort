use std::env;
use std::ffi::CStr;
use std::fs::{self, File};
use std::io::{BufWriter, Write};
use std::os::raw::c_char;
use std::path::Path;
use std::process::Command;
use std::time::Instant;

const BASE_SEED: u32 = 0x8A5CD789;
const POSITIONS: [char; 5] = ['A', 'B', 'C', 'D', 'E'];

const PATTERNS: [(&str, i32); 10] = [
    ("Random", 0),
    ("Sorted", 1),
    ("Reverse", 2),
    ("Sorted+Noise(5%)", 3),
    ("Random%25", 4),
    ("Diverging", 5),
    ("Sawtooth", 6),
    ("MixedDirectionRuns", 13),
    ("OrganPipe", 8),
    ("Rotated", 9),
];

const ALGORITHM_NAMES: [&str; 4] = [
    "ipnsort",
    "simulated-direct_live-phase",
    "adaptive-noalloc",
    "strict-noalloc",
];

extern "C" {
    fn jesse_generate_u64(out: *mut u64, n: usize, input_type: i32, seed: u32);
    fn jesse_ood_family_count() -> usize;
    fn jesse_ood_family_name(family_index: usize) -> *const c_char;
    fn jesse_generate_ood_u64(out: *mut u64, n: usize, family_index: usize, seed: u64);
    fn jesse_production_u64(input: *const u64, n: usize, output: *mut u64) -> f64;
    fn jesse_noalloc_u64(input: *const u64, n: usize, output: *mut u64) -> f64;
    fn jesse_strict_noalloc_u64(input: *const u64, n: usize, output: *mut u64) -> f64;
    fn jesse_run_five_u64(algorithm: i32, inputs: *const u64, n: usize, times_us: *mut f64, outputs: *mut u64) -> i32;
}

#[derive(Clone, Copy)]
enum SuiteKind {
    Canonical,
    Ood,
}

impl SuiteKind {
    fn name(self) -> &'static str {
        match self {
            SuiteKind::Canonical => "canonical",
            SuiteKind::Ood => "ood",
        }
    }
}

struct Pattern {
    name: String,
    ordinal: usize,
    canonical_id: Option<i32>,
}

fn canonical_patterns() -> Vec<Pattern> {
    PATTERNS
        .iter()
        .enumerate()
        .map(|(ordinal, &(name, id))| Pattern {
            name: name.to_string(),
            ordinal,
            canonical_id: Some(id),
        })
        .collect()
}

fn ood_patterns() -> Vec<Pattern> {
    let count = unsafe { jesse_ood_family_count() };
    (0..count)
        .map(|ordinal| {
            let ptr = unsafe { jesse_ood_family_name(ordinal) };
            assert!(!ptr.is_null(), "missing OOD family name at index {ordinal}");
            let name = unsafe { CStr::from_ptr(ptr) }
                .to_string_lossy()
                .into_owned();
            Pattern {
                name,
                ordinal,
                canonical_id: None,
            }
        })
        .collect()
}

fn trial_seed(base_seed: u32, n: usize, input_ordinal: i32, trial: usize) -> u32 {
    let mut x = base_seed as u64;
    x ^= (n as u64)
        .wrapping_add(0x9e3779b97f4a7c15)
        .wrapping_add(x << 6)
        .wrapping_add(x >> 2);
    x ^= (input_ordinal as u64).wrapping_mul(0xbf58476d1ce4e5b9);
    x ^= (trial as u64).wrapping_mul(0x94d049bb133111eb);
    (x ^ (x >> 32)) as u32
}

fn ood_seed(family_index: usize, trial: usize) -> u64 {
    0xE679_0000_0000_0000u64
        ^ ((family_index as u64) << 32)
        ^ (((trial + 2) as u64).wrapping_mul(0x9E37_79B9_7F4A_7C15))
}

fn generate_source(kind: SuiteKind, pattern: &Pattern, n: usize, trial: usize) -> (u64, Vec<u64>) {
    let mut source = vec![0u64; n];
    match kind {
        SuiteKind::Canonical => {
            let id = pattern.canonical_id.expect("canonical pattern id");
            let seed = trial_seed(BASE_SEED, n, id, trial);
            unsafe { jesse_generate_u64(source.as_mut_ptr(), n, id, seed) };
            (seed as u64, source)
        }
        SuiteKind::Ood => {
            let seed = ood_seed(pattern.ordinal, trial);
            unsafe { jesse_generate_ood_u64(source.as_mut_ptr(), n, pattern.ordinal, seed) };
            (seed, source)
        }
    }
}

fn coprime_multiplier(total: usize) -> usize {
    fn gcd(mut a: usize, mut b: usize) -> usize {
        while b != 0 {
            let r = a % b;
            a = b;
            b = r;
        }
        a
    }
    if total <= 1 {
        return 1;
    }
    let mut a = 137 % total;
    if a == 0 {
        a = 1;
    }
    while gcd(a, total) != 1 {
        a = (a + 2) % total;
        if a == 0 {
            a = 1;
        }
    }
    a
}

fn trial_ids(exec_index: usize, execs: usize) -> [usize; 5] {
    let total = 5 * execs;
    let mult = coprime_multiplier(total);
    let off = 31 % total;
    std::array::from_fn(|p| {
        let stratum = (p + exec_index) % 5;
        let x = exec_index + execs * stratum;
        (mult * x + off) % total
    })
}

fn median(mut v: Vec<f64>) -> f64 {
    v.sort_by(|a, b| a.partial_cmp(b).unwrap());
    let n = v.len();
    if n % 2 == 1 {
        v[n / 2]
    } else {
        (v[n / 2 - 1] + v[n / 2]) * 0.5
    }
}

fn child_algorithm_id(name: &str) -> Option<usize> {
    ALGORITHM_NAMES.iter().position(|&x| x == name)
}

fn run_child(args: &[String]) -> std::io::Result<()> {
    // --child SUITE PATTERN N TOTAL ALGORITHM T0 T1 T2 T3 T4
    if args.len() != 11 {
        eprintln!("internal child usage: --child SUITE PATTERN N TOTAL ALGORITHM T0 T1 T2 T3 T4");
        std::process::exit(2);
    }
    let suite = match args[1].as_str() {
        "canonical" => SuiteKind::Canonical,
        "ood" => SuiteKind::Ood,
        _ => {
            eprintln!("bad child suite");
            std::process::exit(2);
        }
    };
    let patterns = match suite {
        SuiteKind::Canonical => canonical_patterns(),
        SuiteKind::Ood => ood_patterns(),
    };
    let pattern_name = &args[2];
    let pattern = patterns
        .iter()
        .find(|p| &p.name == pattern_name)
        .unwrap_or_else(|| panic!("unknown pattern: {pattern_name}"));
    let n: usize = args[3].parse().expect("n");
    let total: usize = args[4].parse().expect("total");
    let alg_name = &args[5];
    let alg = child_algorithm_id(alg_name).unwrap_or_else(|| panic!("unknown algorithm: {alg_name}"));
    let ids: [usize; 5] = std::array::from_fn(|i| args[6 + i].parse().expect("trial id"));
    assert!(ids.iter().all(|&id| id < total), "trial id outside timed domain");

    // Construct all five sources and all timed working buffers before timing A.
    // No generation, allocation, correctness sorting, or target warmup occurs
    // between the five timed calls.
    let mut sources: [Vec<u64>; 5] = std::array::from_fn(|_| Vec::new());
    let mut seeds = [0u64; 5];
    for i in 0..5 {
        let (seed, source) = generate_source(suite, pattern, n, ids[i]);
        seeds[i] = seed;
        sources[i] = source;
    }

    let mut us = [0.0f64; 5];
    let mut values: [Vec<u64>; 5] = std::array::from_fn(|i| sources[i].clone());

    if alg == 0 {
        for i in 0..5 {
            let start = Instant::now();
            ipnsort::sort(&mut values[i]);
            us[i] = start.elapsed().as_secs_f64() * 1_000_000.0;
        }
    } else {
        let mut flat_inputs = Vec::with_capacity(5 * n);
        for source in &sources {
            flat_inputs.extend_from_slice(source);
        }
        let mut flat_outputs = vec![0u64; 5 * n];
        let rc = unsafe {
            jesse_run_five_u64(
                alg as i32,
                flat_inputs.as_ptr(),
                n,
                us.as_mut_ptr(),
                flat_outputs.as_mut_ptr(),
            )
        };
        assert_eq!(rc, 0, "JesseSort five-call bridge failed");
        for i in 0..5 {
            values[i].copy_from_slice(&flat_outputs[i * n..(i + 1) * n]);
        }
    }

    // Validate only after E so correctness work cannot condition C/D/E.
    for i in 0..5 {
        let mut expected = sources[i].clone();
        expected.sort();
        assert_eq!(values[i], expected, "{alg_name} validation failure at position {}", POSITIONS[i]);
    }

    for i in 0..5 {
        println!("{},{},{},{:.17}", i, ids[i], seeds[i], us[i]);
    }
    Ok(())
}

fn algorithm_order(exec_index: usize) -> [usize; 4] {
    let mut order = [0usize, 1, 2, 3];
    let block = exec_index / order.len();
    let rotation = exec_index % order.len();
    order.rotate_left(rotation);
    if block % 2 == 1 {
        order.reverse();
    }
    order
}

fn run_five_fresh_process(
    suite: SuiteKind,
    pattern: &Pattern,
    n: usize,
    total: usize,
    alg: usize,
    ids: &[usize; 5],
) -> std::io::Result<[(usize, usize, u64, f64); 5]> {
    let exe = env::current_exe()?;
    let mut cmd = Command::new(exe);
    cmd.arg("--child")
        .arg(suite.name())
        .arg(&pattern.name)
        .arg(n.to_string())
        .arg(total.to_string())
        .arg(ALGORITHM_NAMES[alg]);
    for id in ids {
        cmd.arg(id.to_string());
    }
    let output = cmd.output()?;
    if !output.status.success() {
        eprintln!("child failed: {}", String::from_utf8_lossy(&output.stderr));
        std::process::exit(output.status.code().unwrap_or(1));
    }
    let text = String::from_utf8(output.stdout).expect("child stdout utf8");
    let lines: Vec<&str> = text.lines().filter(|x| !x.trim().is_empty()).collect();
    assert_eq!(lines.len(), 5, "child returned wrong row count: {text:?}");
    let parsed: Vec<(usize, usize, u64, f64)> = lines
        .iter()
        .map(|line| {
            let f: Vec<&str> = line.split(',').collect();
            assert_eq!(f.len(), 4, "bad child row: {line}");
            (
                f[0].parse().unwrap(),
                f[1].parse().unwrap(),
                f[2].parse().unwrap(),
                f[3].parse().unwrap(),
            )
        })
        .collect();
    Ok(parsed.try_into().ok().unwrap())
}

fn run_suite(
    kind: SuiteKind,
    patterns: &[Pattern],
    n: usize,
    execs: usize,
    raw: &mut BufWriter<File>,
    summary: &mut String,
) -> std::io::Result<()> {
    let suite_name = kind.name();
    let heading = if suite_name == "ood" { "Expanded OOD" } else { "Canonical" };
    summary.push_str(&format!("## {heading}\n\n"));
    summary.push_str("| Input | simulated-direct_live-phase | adaptive-noalloc | strict-noalloc | ipnsort |\n");
    summary.push_str("|---|---:|---:|---:|---:|\n");
    let total = 5 * execs;

    for pattern in patterns {
        let mut times: [Vec<f64>; 4] = std::array::from_fn(|_| Vec::with_capacity(total));
        for exec_index in 0..execs {
            let ids = trial_ids(exec_index, execs);
            for &alg in &algorithm_order(exec_index) {
                let rows = run_five_fresh_process(kind, pattern, n, total, alg, &ids)?;
                for (position, trial_id, seed, us) in rows {
                    assert_eq!(trial_id, ids[position], "trajectory trial mismatch");
                    times[alg].push(us);
                    writeln!(
                        raw,
                        "{},{},{},{},{},{},{},{},{:.17}",
                        suite_name,
                        pattern.name,
                        n,
                        exec_index,
                        POSITIONS[position],
                        trial_id,
                        seed,
                        ALGORITHM_NAMES[alg],
                        us
                    )?;
                }
            }
            raw.flush()?;
        }

        // Published comparison excludes A (fresh) and B (transition).  Each
        // algorithm therefore contributes 3*execs CDE observations.
        let cde_medians: [f64; 4] = std::array::from_fn(|alg| {
            let mut cde = Vec::with_capacity(3 * execs);
            for exec_index in 0..execs {
                let base = exec_index * 5;
                cde.extend_from_slice(&times[alg][base + 2..base + 5]);
            }
            median(cde)
        });
        let ip = cde_medians[0];
        let row = format!(
            "| {} | {:.4}× ({:.3} us) | {:.4}× ({:.3} us) | {:.4}× ({:.3} us) | 1.0000× ({:.3} us) |\n",
            pattern.name,
            ip / cde_medians[1], cde_medians[1],
            ip / cde_medians[2], cde_medians[2],
            ip / cde_medians[3], cde_medians[3],
            ip
        );
        summary.push_str(&row);
        print!("{}", row);
    }
    summary.push('\n');
    Ok(())
}

fn main() -> std::io::Result<()> {
    let args: Vec<String> = env::args().collect();
    if args.get(1).map(String::as_str) == Some("--child") {
        return run_child(&args[1..]);
    }

    let n: usize = args.get(1).and_then(|s| s.parse().ok()).unwrap_or(10_000);
    let execs: usize = args.get(2).and_then(|s| s.parse().ok()).unwrap_or(100);
    let suite = args.get(3).map(String::as_str).unwrap_or("canonical");
    if execs == 0 {
        eprintln!("execs must be positive");
        std::process::exit(2);
    }
    if !matches!(suite, "canonical" | "ood" | "all") {
        eprintln!("suite must be canonical, ood, or all");
        std::process::exit(2);
    }

    fs::create_dir_all("results")?;
    let raw_path = Path::new("results/raw.csv");
    let summary_path = Path::new("results/summary.md");

    let mut raw = BufWriter::new(File::create(raw_path)?);
    writeln!(raw, "suite,pattern,n,exec,position,trial_id,seed,algorithm,time_us")?;

    let mut summary = String::new();
    summary.push_str("# ipnsort vs maintained JesseSort production/noalloc paths\n\n");
    summary.push_str(&format!(
        "- type: u64\n- n: {}\n- fresh five-input executions per pattern/algorithm: {}\n- timed observations per pattern/algorithm: {}\n- published channel: CDE median ({} observations)\n- suite: {}\n- target warmups: none; A=fresh, B=transition\n- process isolation: one fresh process per algorithm per ABCDE trajectory\n- input encoding: order-preserving int32-to-u64\n\n",
        n,
        execs,
        5 * execs,
        3 * execs,
        suite
    ));

    if suite == "canonical" || suite == "all" {
        run_suite(SuiteKind::Canonical, &canonical_patterns(), n, execs, &mut raw, &mut summary)?;
    }
    if suite == "ood" || suite == "all" {
        let patterns = ood_patterns();
        assert_eq!(patterns.len(), 50, "maintained additional suite must contain 50 families");
        run_suite(SuiteKind::Ood, &patterns, n, execs, &mut raw, &mut summary)?;
    }

    raw.flush()?;
    fs::write(summary_path, &summary)?;
    println!("\nWrote {}", raw_path.display());
    println!("Wrote {}", summary_path.display());
    Ok(())
}
