use std::env;
use std::fs::{self, File};
use std::io::{BufWriter, Write};
use std::path::Path;
use std::time::Instant;

const BASE_SEED: u32 = 0x8A5CD789;

const PATTERNS: [(&str, i32); 12] = [
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
];

const JESSE_NAMES: [&str; 6] = [
    "physical",
    "simulated",
    "simulated-direct",
    "indexed",
    "noalloc",
    "noalloc-low-run",
];

extern "C" {
    fn jesse_generate_u64(out: *mut u64, n: usize, input_type: i32, seed: u32);
    fn jesse_physical_u64(input: *const u64, n: usize, output: *mut u64) -> f64;
    fn jesse_simulated_u64(input: *const u64, n: usize, output: *mut u64) -> f64;
    fn jesse_simulated_direct_u64(input: *const u64, n: usize, output: *mut u64) -> f64;
    fn jesse_indexed_u64(input: *const u64, n: usize, output: *mut u64) -> f64;
    fn jesse_noalloc_u64(input: *const u64, n: usize, output: *mut u64) -> f64;
    fn jesse_noalloc_low_run_u64(input: *const u64, n: usize, output: *mut u64) -> f64;
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
            1 => jesse_physical_u64(source.as_ptr(), source.len(), out.as_mut_ptr()),
            2 => jesse_simulated_u64(source.as_ptr(), source.len(), out.as_mut_ptr()),
            3 => jesse_simulated_direct_u64(source.as_ptr(), source.len(), out.as_mut_ptr()),
            4 => jesse_indexed_u64(source.as_ptr(), source.len(), out.as_mut_ptr()),
            5 => jesse_noalloc_u64(source.as_ptr(), source.len(), out.as_mut_ptr()),
            6 => jesse_noalloc_low_run_u64(source.as_ptr(), source.len(), out.as_mut_ptr()),
            _ => unreachable!(),
        }
    };
    (us, out)
}

fn execution_order(trial: usize) -> [usize; 7] {
    let mut order = [0usize, 1, 2, 3, 4, 5, 6]; // ipnsort + six JesseSort targets
    let block = trial / order.len();
    let rotation = trial % order.len();
    order.rotate_left(rotation);
    if block % 2 == 1 {
        order.reverse();
    }
    order
}

fn main() -> std::io::Result<()> {
    let args: Vec<String> = env::args().collect();
    let n: usize = args.get(1).and_then(|s| s.parse().ok()).unwrap_or(10_000);
    let trials: usize = args.get(2).and_then(|s| s.parse().ok()).unwrap_or(500);
    let warmups: usize = args.get(3).and_then(|s| s.parse().ok()).unwrap_or(2);

    fs::create_dir_all("results")?;
    let raw_path = Path::new("results/raw.csv");
    let summary_path = Path::new("results/summary.md");

    let mut raw = BufWriter::new(File::create(raw_path)?);
    writeln!(raw, "pattern,n,trial,seed,algorithm,order_position,time_us")?;

    let mut summary = String::new();
    summary.push_str("# ipnsort vs layer-tagged JesseSort (E229)\n\n");
    summary.push_str(&format!(
        "- type: u64\n- n: {}\n- trials per pattern: {}\n- warmups per pattern: {}\n- shared cold-like preconditioner: false\n- inputs: E189 canonical JesseSort benchmark inputs, order-preserving int->u64 encoding\n\n",
        n, trials, warmups
    ));
    summary.push_str("| Pattern | ipnsort µs | physical µs | simulated µs | simulated-direct µs | indexed µs | noalloc µs | noalloc-low-run µs | simulated/ipnsort | simulated-direct/ipnsort | best Jesse/ipnsort |\n");
    summary.push_str("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");

    for &(pattern_name, pattern_id) in &PATTERNS {
        for w in 0..warmups {
            let seed = trial_seed(0xA5A5A5A5, n, pattern_id, w);
            let mut source = vec![0u64; n];
            unsafe { jesse_generate_u64(source.as_mut_ptr(), n, pattern_id, seed) };

            let mut expected = source.clone();
            expected.sort(); // validation oracle intentionally distinct from ipnsort

            let (_, ip) = run_ipnsort(&source);
            assert_eq!(ip, expected, "warmup ipnsort failure: {pattern_name}");
            for (j, name) in JESSE_NAMES.iter().enumerate() {
                let (_, got) = run_jesse(j + 1, &source);
                assert_eq!(got, expected, "warmup {name} failure: {pattern_name}");
            }
        }

        let mut times: [Vec<f64>; 7] = std::array::from_fn(|_| Vec::with_capacity(trials));

        for trial in 0..trials {
            let seed = trial_seed(BASE_SEED, n, pattern_id, trial);
            let mut source = vec![0u64; n];
            unsafe { jesse_generate_u64(source.as_mut_ptr(), n, pattern_id, seed) };

            let mut expected = source.clone();
            expected.sort();

            let order = execution_order(trial);
            for (pos, &alg) in order.iter().enumerate() {
                let (us, got) = if alg == 0 {
                    run_ipnsort(&source)
                } else {
                    run_jesse(alg, &source)
                };
                assert_eq!(
                    got, expected,
                    "validation failure: pattern={pattern_name} trial={trial} alg={alg}"
                );
                times[alg].push(us);
                let alg_name = if alg == 0 { "ipnsort" } else { JESSE_NAMES[alg - 1] };
                writeln!(
                    raw, "{},{},{},{},{},{},{:.17}",
                    pattern_name, n, trial, seed, alg_name, pos, us
                )?;
            }
            raw.flush()?;
        }

        let medians: [f64; 7] = std::array::from_fn(|i| median(times[i].clone()));
        let ip = medians[0];
        let best = medians[1..].iter().copied().fold(f64::INFINITY, f64::min);

        summary.push_str(&format!(
            "| {} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} |\n",
            pattern_name,
            ip,
            medians[1], medians[2], medians[3], medians[4], medians[5], medians[6],
            medians[2] / ip, medians[3] / ip, best / ip
        ));

        println!(
            "| {} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} | {:.3} |",
            pattern_name,
            ip,
            medians[1], medians[2], medians[3], medians[4], medians[5], medians[6],
            medians[2] / ip, medians[3] / ip, best / ip
        );
    }

    fs::write(summary_path, &summary)?;
    println!("\nWrote {}", raw_path.display());
    println!("Wrote {}", summary_path.display());
    Ok(())
}
