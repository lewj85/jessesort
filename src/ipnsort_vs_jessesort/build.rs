use std::env;
use std::fs;
use std::path::PathBuf;
use std::process::Command;

fn run(mut cmd: Command) {
    eprintln!("running: {:?}", cmd);
    let status = cmd.status().expect("failed to start build command");
    if !status.success() {
        panic!("build command failed: {:?}", cmd);
    }
}

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let repo = manifest.join("../..").canonicalize().expect("resolve enclosing JesseSort repository");
    let include = repo.join("include");
    let ood_include = repo.join("benchmarks/ood");
    let bridge = manifest.join("cpp/bridge.cpp");

    if !repo.exists() {
        panic!("enclosing JesseSort repository missing");
    }

    println!("cargo:rerun-if-changed={}", bridge.display());
    println!("cargo:rerun-if-changed={}", repo.join("include").display());
    println!("cargo:rerun-if-changed={}", repo.join("benchmarks/ood/ood_families.h").display());
    println!("cargo:rerun-if-changed={}", repo.join("src").display());

    let mut sources = vec![bridge];
    let src_dir = repo.join("src");
    let mut repo_sources: Vec<PathBuf> = fs::read_dir(&src_dir)
        .expect("read jessesort src")
        .filter_map(|e| e.ok().map(|x| x.path()))
        .filter(|p| p.extension().and_then(|s| s.to_str()) == Some("cpp"))
        .collect();
    repo_sources.sort();
    sources.extend(repo_sources);

    let mut objects = Vec::new();
    for (idx, src) in sources.iter().enumerate() {
        let obj = out_dir.join(format!("obj_{idx}.o"));
        let mut c = Command::new(env::var("CXX").unwrap_or_else(|_| "g++".into()));
        c.arg("-std=c++20")
            .arg("-O3")
            .arg("-march=native")
            .arg("-DNDEBUG")
            .arg("-I").arg(&include)
            .arg("-I").arg(&ood_include)
            .arg("-c").arg(src)
            .arg("-o").arg(&obj);
        run(c);
        objects.push(obj);
    }

    let lib = out_dir.join("libjessesort_bridge.a");
    let mut ar = Command::new("ar");
    ar.arg("rcs").arg(&lib);
    for obj in &objects {
        ar.arg(obj);
    }
    run(ar);

    println!("cargo:rustc-link-search=native={}", out_dir.display());
    println!("cargo:rustc-link-lib=static=jessesort_bridge");
    println!("cargo:rustc-link-lib=dylib=stdc++");
}
