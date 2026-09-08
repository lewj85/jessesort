use std::env;
use std::ffi::CStr;
use std::fs::{self, File};
use std::io::{BufWriter, Write};
use std::os::raw::c_char;
use std::path::Path;
use std::time::Instant;

const BASE_SEED: u32 = 0x8A5CD789;

const PATTERNS: [(&str, i32); 14] = [
    ("Random", 0),
    ("Sorted", 1),
    ("Reverse", 2),
    ("Sorted+Noise(5%)", 3),
    ("Sorted+Noise(10%)", 12),
    ("Random%25", 4),
    ("Alternating", 5),
    ("Sawtooth", 6),
    ("MixedDirectionRuns", 13),
    ("BlockSorted", 7),
    ("OrganPipe", 8),
    ("Rotated", 9),
    ("MixedPhase3", 14),
    ("MixedPhase12", 15),
];

const JESSE_NAMES: [&str; 3] = [
    "simulated-direct_live-map",
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
}

#[derive(Clone, Copy)]
enum SuiteKind { Canonical, Ood }

struct Pattern {
    name: String,
    ordinal: usize,
    canonical_id: Option<i32>,
}

fn canonical_patterns() -> Vec<Pattern> {
    PATTERNS.iter().enumerate().map(|(ordinal, &(name, id))| Pattern {
        name: name.to_string(), ordinal, canonical_id: Some(id),
    }).collect()
}

fn ood_patterns() -> Vec<Pattern> {
    let count = unsafe { jesse_ood_family_count() };
    (0..count).map(|ordinal| {
        let ptr = unsafe { jesse_ood_family_name(ordinal) };
        assert!(!ptr.is_null(), "missing OOD family name at index {ordinal}");
        let name = unsafe { CStr::from_ptr(ptr) }.to_string_lossy().into_owned();
        Pattern { name, ordinal, canonical_id: None }
    }).collect()
}

fn ood_seed(family_index: usize, trial: usize, warmup: bool) -> u64 {
    if warmup {
        0xE679_1000_0000_0000u64 ^ ((family_index as u64) << 32) ^ ((trial + 1) as u64)
    } else {
        0xE679_0000_0000_0000u64
            ^ ((family_index as u64) << 32)
            ^ (((trial + 2) as u64).wrapping_mul(0x9E37_79B9_7F4A_7C15))
    }
}

fn generate_source(kind: SuiteKind, pattern: &Pattern, n: usize, trial: usize,
                   warmup: bool) -> (u64, Vec<u64>) {
    let mut source = vec![0u64; n];
    match kind {
        SuiteKind::Canonical => {
            let id = pattern.canonical_id.expect("canonical pattern id");
            let base = if warmup { 0xA5A5_A5A5 } else { BASE_SEED };
            let seed = trial_seed(base, n, id, trial);
            unsafe { jesse_generate_u64(source.as_mut_ptr(), n, id, seed) };
            (seed as u64, source)
        }
        SuiteKind::Ood => {
            let seed = ood_seed(pattern.ordinal, trial, warmup);
            unsafe { jesse_generate_ood_u64(source.as_mut_ptr(), n, pattern.ordinal, seed) };
            (seed, source)
        }
    }
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

fn median(mut v: Vec<f64>) -> f64 {
    v.sort_by(|a, b| a.partial_cmp(b).unwrap());
    let n = v.len();
    if n % 2 == 1 { v[n / 2] } else { (v[n / 2 - 1] + v[n / 2]) * 0.5 }
}

fn run_ipnsort(source: &[u64]) -> (f64, Vec<u64>) {
    let mut values = source.to_vec();
    let start = Instant::now();
    ipnsort::sort(&mut values);
    let us = start.elapsed().as_secs_f64() * 1_000_000.0;
    (us, values)
}

fn run_jesse(id: usize, source: &[u64]) -> (f64, Vec<u64>) {
    let mut out = vec![0u64; source.len()];
    let us = unsafe {
        match id {
            1 => jesse_production_u64(source.as_ptr(), source.len(), out.as_mut_ptr()),
            2 => jesse_noalloc_u64(source.as_ptr(), source.len(), out.as_mut_ptr()),
            3 => jesse_strict_noalloc_u64(source.as_ptr(), source.len(), out.as_mut_ptr()),
            _ => unreachable!(),
        }
    };
    (us, out)
}

fn execution_order(trial: usize) -> [usize; 4] {
    let mut order = [0usize, 1, 2, 3]; // ipnsort + three maintained JesseSort public paths
    let block = trial / order.len();
    let rotation = trial % order.len();
    order.rotate_left(rotation);
    if block % 2 == 1 {
        order.reverse();
    }
    order
}

fn run_suite(kind: SuiteKind, patterns: &[Pattern], n: usize, trials: usize,
             warmups: usize, raw: &mut BufWriter<File>, summary: &mut String) -> std::io::Result<()> {
    let suite_name = match kind { SuiteKind::Canonical => "canonical", SuiteKind::Ood => "ood" };
    let heading = if suite_name == "ood" { "Expanded OOD" } else { "Canonical" };
    summary.push_str(&format!("## {heading}\n\n"));
    summary.push_str("| Input | simulated-direct_live-map | adaptive-noalloc | strict-noalloc | ipnsort |\n");
    summary.push_str("|---|---:|---:|---:|---:|\n");

    for pattern in patterns {
        let pattern_name = &pattern.name;
        for w in 0..warmups {
            let (_, source) = generate_source(kind, pattern, n, w, true);
            let mut expected = source.clone();
            expected.sort();
            let (_, ip) = run_ipnsort(&source);
            assert_eq!(ip, expected, "warmup ipnsort failure: {pattern_name}");
            for (j, name) in JESSE_NAMES.iter().enumerate() {
                let (_, got) = run_jesse(j + 1, &source);
                assert_eq!(got, expected, "warmup {name} failure: {pattern_name}");
            }
        }

        let mut times: [Vec<f64>; 4] = std::array::from_fn(|_| Vec::with_capacity(trials));
        for trial in 0..trials {
            let (seed, source) = generate_source(kind, pattern, n, trial, false);
            let mut expected = source.clone();
            expected.sort();
            let order = execution_order(trial);
            for (pos, &alg) in order.iter().enumerate() {
                let (us, got) = if alg == 0 { run_ipnsort(&source) } else { run_jesse(alg, &source) };
                assert_eq!(got, expected,
                    "validation failure: suite={suite_name} pattern={pattern_name} trial={trial} alg={alg}");
                times[alg].push(us);
                let alg_name = if alg == 0 { "ipnsort" } else { JESSE_NAMES[alg - 1] };
                writeln!(raw, "{},{},{},{},{},{},{},{:.17}",
                    suite_name, pattern_name, n, trial, seed, alg_name, pos, us)?;
            }
            raw.flush()?;
        }

        let medians: [f64; 4] = std::array::from_fn(|i| median(times[i].clone()));
        let ip = medians[0];
        let row = format!(
            "| {} | {:.4} ({:.3} us) | {:.4} ({:.3} us) | {:.4} ({:.3} us) | 1.0000 ({:.3} us) |\n",
            pattern_name, medians[1] / ip, medians[1], medians[2] / ip, medians[2],
            medians[3] / ip, medians[3], ip);
        summary.push_str(&row);
        print!("{}", row);
    }
    summary.push('\n');
    Ok(())
}

fn main() -> std::io::Result<()> {
    let args: Vec<String> = env::args().collect();
    let n: usize = args.get(1).and_then(|s| s.parse().ok()).unwrap_or(10_000);
    let trials: usize = args.get(2).and_then(|s| s.parse().ok()).unwrap_or(500);
    let warmups: usize = args.get(3).and_then(|s| s.parse().ok()).unwrap_or(2);
    let suite = args.get(4).map(String::as_str).unwrap_or("canonical");
    if !matches!(suite, "canonical" | "ood" | "all") {
        eprintln!("suite must be canonical, ood, or all");
        std::process::exit(2);
    }

    fs::create_dir_all("results")?;
    let raw_path = Path::new("results/raw.csv");
    let summary_path = Path::new("results/summary.md");

    let mut raw = BufWriter::new(File::create(raw_path)?);
    writeln!(raw, "suite,pattern,n,trial,seed,algorithm,order_position,time_us")?;

    let mut summary = String::new();
    summary.push_str("# ipnsort vs maintained JesseSort production/noalloc paths\n\n");
    summary.push_str(&format!(
        "- type: u64\n- n: {}\n- trials per pattern: {}\n- warmups per pattern: {}\n- suite: {}\n- shared cold-like preconditioner: false\n- input encoding: order-preserving int32-to-u64\n\n",
        n, trials, warmups, suite
    ));

    if suite == "canonical" || suite == "all" {
        run_suite(SuiteKind::Canonical, &canonical_patterns(), n, trials, warmups, &mut raw, &mut summary)?;
    }
    if suite == "ood" || suite == "all" {
        let patterns = ood_patterns();
        assert_eq!(patterns.len(), 47, "maintained OOD suite must contain 47 families");
        run_suite(SuiteKind::Ood, &patterns, n, trials, warmups, &mut raw, &mut summary)?;
    }

    fs::write(summary_path, &summary)?;
    println!("\nWrote {}", raw_path.display());
    println!("Wrote {}", summary_path.display());
    Ok(())
}
