#!/usr/bin/env python3
"""Generate canonical benchmark C++/shell configuration from validated pipeline recipes.

A recipe chooses a known-valid base pipeline and may override only slots that the
registry explicitly exposes. Folder organization is descriptive; it does not imply
arbitrary module interchangeability.
"""
from __future__ import annotations
import argparse, hashlib, json, pathlib, re, sys

MODULE_KEY_RE = re.compile(r"^[A-Za-z0-9_.-]+$")

def die(msg: str) -> None:
    raise SystemExit(f"benchmark-config: {msg}")

def load_json(path: pathlib.Path):
    try:
        return json.loads(path.read_text())
    except Exception as exc:
        die(f"cannot read {path}: {exc}")

def canonical_json(obj) -> str:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=True)

def resolve_layout(layout, registry):
    if not isinstance(layout, dict):
        die("every layouts[] item must be an object")
    name = layout.get("name")
    base = layout.get("base")
    if not isinstance(name, str) or not name:
        die("layout name must be a non-empty string")
    if not isinstance(base, str) or base not in registry:
        die(f"layout {name!r} references unknown base {base!r}")
    if any(c in name for c in "|\n\r"):
        die(f"layout name {name!r} contains a forbidden delimiter")

    spec = registry[base]
    modules = dict(spec.get("modules", {}))
    exposed = spec.get("overrides", {})
    requested = layout.get("overrides", {})
    if not isinstance(requested, dict):
        die(f"layout {name!r}: overrides must be an object")

    for slot, choice in requested.items():
        if not MODULE_KEY_RE.match(slot):
            die(f"layout {name!r}: invalid override slot {slot!r}")
        if slot not in exposed:
            die(f"layout {name!r}: base {base!r} does not expose override slot {slot!r}")
        allowed = exposed[slot]
        if choice not in allowed:
            die(f"layout {name!r}: {slot}={choice!r} is not allowed; choose one of {allowed}")
        modules[slot] = choice

    unknown = set(layout) - {"name", "base", "overrides", "enabled"}
    if unknown:
        die(f"layout {name!r}: unknown keys: {sorted(unknown)}")

    return {
        "name": name,
        "base": base,
        "header": spec["header"],
        "call": spec["call"],
        "modules": modules,
        "overrides": requested,
    }

def cpp_string(s: str) -> str:
    return json.dumps(s)

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="benchmarks/config/benchmark_layouts.json")
    ap.add_argument("--registry", default="benchmarks/config/pipeline_registry.json")
    ap.add_argument("--header", default="benchmarks/generated/benchmark_layouts.h")
    ap.add_argument("--shell", default="benchmarks/generated/benchmark_layouts.sh")
    ap.add_argument("--resolved", default="benchmarks/generated/benchmark_layouts.resolved.json")
    ap.add_argument("--only-layout", default=None, help="emit only one enabled layout by recipe name")
    args = ap.parse_args()

    config_path = pathlib.Path(args.config)
    registry_path = pathlib.Path(args.registry)
    config = load_json(config_path)
    registry_doc = load_json(registry_path)
    if config.get("schema_version") != 1 or registry_doc.get("schema_version") != 1:
        die("unsupported schema_version (expected 1)")
    pipelines = registry_doc.get("pipelines")
    layouts = config.get("layouts")
    if not isinstance(pipelines, dict) or not isinstance(layouts, list) or not layouts:
        die("registry.pipelines must be an object and config.layouts a non-empty list")

    resolved = []
    seen = set()
    for layout in layouts:
        if layout.get("enabled", True) is False:
            continue
        item = resolve_layout(layout, pipelines)
        if item["name"] in seen:
            die(f"duplicate layout name {item['name']!r}")
        seen.add(item["name"])
        resolved.append(item)
    if not resolved:
        die("configuration enables no layouts")
    if args.only_layout is not None:
        matches = [x for x in resolved if x["name"] == args.only_layout]
        if not matches:
            die(f"--only-layout {args.only_layout!r} is not an enabled layout in the configuration")
        resolved = matches

    # Layout identity is based only on the resolved recipes, not on config path or
    # unrelated enabled layouts. This is essential for per-pipeline benchmark binaries:
    # the same resolved pipeline must compile identically when another roster member is
    # added or removed. Suite-level config/registry hashes are recorded separately.
    identity = {"schema_version": 1, "layouts": resolved}
    layout_hash = hashlib.sha256(canonical_json(identity).encode()).hexdigest()
    provenance = {
        "schema_version": 1,
        "config": str(config_path),
        "registry": str(registry_path),
        "layouts": resolved,
        "layout_sha256": layout_hash,
    }

    headers = []
    for x in resolved:
        h = x["header"]
        if h not in headers:
            headers.append(h)

    hlines = [
        "// GENERATED FILE. Edit benchmarks/config/*.json, not this file.",
        "#pragma once",
        "#include <algorithm>",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdlib>",
        "#include <string_view>",
        "#include <vector>",
    ]
    for h in headers:
        hlines.append(f"#include <{h}>")
    hlines += [
        "",
        "namespace jessesort_benchmark_config {",
        "struct AlgorithmSpec {",
        "    std::string_view short_name;",
        "    std::string_view base_pipeline;",
        "    std::string_view header_file;",
        "    std::string_view module_layout;",
        "    bool default_selected;",
        "};",
        f"inline constexpr std::string_view kLayoutSha256 = {cpp_string(layout_hash)};",
        f"inline constexpr std::array<AlgorithmSpec, {len(resolved)}> kAlgorithmSpecs{{{{",
    ]
    for x in resolved:
        module_layout = ";".join(f"{k}={v}" for k, v in sorted(x["modules"].items()))
        hlines.append(
            "    {%s, %s, %s, %s, true}," % tuple(map(cpp_string, [x["name"], x["base"], x["header"], module_layout]))
        )
    hlines += [
        "}};",
        "inline constexpr std::size_t kAlgorithmCount = kAlgorithmSpecs.size();",
        "inline void run_algorithm(std::size_t id, std::vector<int>& values) {",
        "    switch (id) {",
    ]
    for i, x in enumerate(resolved):
        call = x["call"]
        if call == "std::sort":
            stmt = "std::sort(values.begin(), values.end());"
        else:
            stmt = f"{call}(values);"
        hlines.append(f"        case {i}: {stmt} break;")
    hlines += [
        "        default: std::abort();",
        "    }",
        "}",
        "} // namespace jessesort_benchmark_config",
        "",
    ]

    shell_names = " ".join(json.dumps(x["name"]) for x in resolved)
    slines = [
        "# GENERATED FILE. Edit benchmarks/config/*.json, not this file.",
        f"BENCH_LAYOUT_SHA256={json.dumps(layout_hash)}",
        f"ROUTINE_ALGORITHMS=({shell_names})",
        "",
    ]

    hp = pathlib.Path(args.header); hp.parent.mkdir(parents=True, exist_ok=True)
    sp = pathlib.Path(args.shell); sp.parent.mkdir(parents=True, exist_ok=True)
    rp = pathlib.Path(args.resolved); rp.parent.mkdir(parents=True, exist_ok=True)
    hp.write_text("\n".join(hlines))
    sp.write_text("\n".join(slines))
    rp.write_text(json.dumps(provenance, indent=2, sort_keys=True) + "\n")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
