CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude -Ibenchmarks/infrastructure
LDFLAGS ?=

CLANGXX ?= clang++
CLANGFLAGS ?= -std=c++20 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic -stdlib=libc++
CLANGLDFLAGS ?= -stdlib=libc++

BENCH_CONFIG ?= benchmarks/config/benchmark_layouts.json
CONFIG_GENERATOR := benchmarks/config/generate_benchmark_layouts.py
PIPELINE_REGISTRY := benchmarks/config/pipeline_registry.json
GENERATED_LAYOUT_HEADER := benchmarks/generated/benchmark_layouts.h
GENERATED_LAYOUT_SHELL := benchmarks/generated/benchmark_layouts.sh
GENERATED_LAYOUT_RESOLVED := benchmarks/generated/benchmark_layouts.resolved.json
HEADERS := $(sort $(shell find include/jessesort -type f \( -name '*.h' -o -name '*.inc' \)))
INPUT_GENERATORS := benchmarks/infrastructure/input_generators.h

TARGET := benchmark
CLANG_TARGET := benchmark-clang
OOD_TARGET := benchmark-ood
OOD_CLANG_TARGET := benchmark-ood-clang

.PHONY: all clang bench bench-clang bench-ood bench-ood-clang benchmark-config \
        benchmark-layouts benchmark-layouts-clang smoke smoke-ood test-compile clean clean-results help

all: $(TARGET)

benchmark-config:
	python3 $(CONFIG_GENERATOR) --config $(BENCH_CONFIG) --registry $(PIPELINE_REGISTRY) \
	  --header $(GENERATED_LAYOUT_HEADER) --shell $(GENERATED_LAYOUT_SHELL) --resolved $(GENERATED_LAYOUT_RESOLVED)

$(GENERATED_LAYOUT_HEADER): $(BENCH_CONFIG) $(PIPELINE_REGISTRY) $(CONFIG_GENERATOR)
	$(MAKE) BENCH_CONFIG=$(BENCH_CONFIG) benchmark-config

# Aggregate binaries are for correctness/development only. Canonical performance
# uses one separately compiled executable per layout via benchmark-layouts.
$(TARGET): benchmarks/canonical/benchmark.cpp $(INPUT_GENERATORS) $(HEADERS) $(GENERATED_LAYOUT_HEADER)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) benchmarks/canonical/benchmark.cpp $(LDFLAGS) -o $@

$(OOD_TARGET): benchmarks/ood/benchmark.cpp benchmarks/ood/ood_families.h $(INPUT_GENERATORS) $(HEADERS) $(GENERATED_LAYOUT_HEADER)
	$(CXX) $(CPPFLAGS) -Ibenchmarks/ood $(CXXFLAGS) benchmarks/ood/benchmark.cpp $(LDFLAGS) -o $@

clang: $(CLANG_TARGET)

$(CLANG_TARGET): benchmarks/canonical/benchmark.cpp $(INPUT_GENERATORS) $(HEADERS) $(GENERATED_LAYOUT_HEADER)
	$(CLANGXX) $(CPPFLAGS) $(CLANGFLAGS) benchmarks/canonical/benchmark.cpp $(CLANGLDFLAGS) -o $@

$(OOD_CLANG_TARGET): benchmarks/ood/benchmark.cpp benchmarks/ood/ood_families.h $(INPUT_GENERATORS) $(HEADERS) $(GENERATED_LAYOUT_HEADER)
	$(CLANGXX) $(CPPFLAGS) -Ibenchmarks/ood $(CLANGFLAGS) benchmarks/ood/benchmark.cpp $(CLANGLDFLAGS) -o $@

benchmark-layouts:
	BENCH_COMPILER=gcc BENCH_CXX="$(CXX)" BENCH_CXXFLAGS="$(CXXFLAGS)" BENCH_LDFLAGS="$(LDFLAGS)" \
	  BENCH_CONFIG="$(BENCH_CONFIG)" ./benchmarks/canonical/build_layout_binaries.sh

benchmark-layouts-clang:
	BENCH_COMPILER=clang BENCH_CXX="$(CLANGXX)" BENCH_CXXFLAGS="$(CLANGFLAGS)" BENCH_LDFLAGS="$(CLANGLDFLAGS)" \
	  BENCH_CONFIG="$(BENCH_CONFIG)" ./benchmarks/canonical/build_layout_binaries.sh

# E760 routine canonical benchmark: 10k + 100k, 100 fresh execs/cell x
# five distinct preconstructed timed inputs = 500 observations. A is fresh;
# B is transitional; C-E are canonical history-conditioned. Arithmetic means are the primary cell statistic.
# summary.md shows only CDE history-conditioned tables; CSVs retain full ABCDE diagnostics.
bench:
	BENCH_COMPILER=gcc BENCH_CXX="$(CXX)" BENCH_CXXFLAGS="$(CXXFLAGS)" BENCH_LDFLAGS="$(LDFLAGS)" \
	  BENCH_CONFIG="$(BENCH_CONFIG)" ./benchmarks/canonical/run.sh

bench-clang:
	BENCH_COMPILER=clang BENCH_CXX="$(CLANGXX)" BENCH_CXXFLAGS="$(CLANGFLAGS)" BENCH_LDFLAGS="$(CLANGLDFLAGS)" \
	  BENCH_CONFIG="$(BENCH_CONFIG)" ./benchmarks/canonical/run.sh

# E760 expanded additional suite: 100k, 100 fresh execs/cell x five timed inputs.
# Override with OOD_N/OOD_EXECS.
bench-ood:
	BENCH_COMPILER=gcc BENCH_CXX="$(CXX)" BENCH_CXXFLAGS="$(CXXFLAGS)" BENCH_LDFLAGS="$(LDFLAGS)" \
	  BENCH_CONFIG="$(BENCH_CONFIG)" ./benchmarks/ood/run.sh

bench-ood-clang:
	BENCH_COMPILER=clang BENCH_CXX="$(CLANGXX)" BENCH_CXXFLAGS="$(CLANGFLAGS)" BENCH_LDFLAGS="$(CLANGLDFLAGS)" \
	  BENCH_CONFIG="$(BENCH_CONFIG)" ./benchmarks/ood/run.sh

# Quick correctness/build checks only; not performance evidence.
smoke: $(TARGET) test-compile
	./$(TARGET) 2 10000 1 "" simulated-direct_live-phase Random

smoke-ood: benchmark-layouts
	@bin=benchmarks/generated/layout_bins/gcc/simulated-direct_live-phase/ood; \
	  $$bin AlternatingDirectionRuns 10000 2 1; \
	  test `$$bin --list-families | wc -l` -eq 50

test-compile:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/compile/public_api.cpp -o /tmp/jessesort-public-api-smoke
	/tmp/jessesort-public-api-smoke

clean:
	rm -f $(TARGET) $(CLANG_TARGET) $(OOD_TARGET) $(OOD_CLANG_TARGET) *.out *.exe
	rm -rf benchmarks/generated/layout_bins
	find src benchmarks -name '*.o' -delete

clean-results:
	rm -rf results

help:
	@echo "make                    Build aggregate GCC canonical benchmark (development/smoke)"
	@echo "make clang              Build aggregate Clang/libc++ canonical benchmark"
	@echo "make benchmark-config   Validate/generate maintained benchmark roster"
	@echo "make benchmark-layouts  Build isolated GCC canonical+OOD binaries for every layout"
	@echo "make benchmark-layouts-clang  Same isolated binaries with Clang/libc++"
	@echo "make bench              Run canonical 10k+100k suite (100 execs x ABCDE; Markdown summary = CDE only)"
	@echo "make bench-clang        Run canonical suite with Clang/libc++"
	@echo "make bench-ood          Run 50-family 100k additional suite (GCC; 500 trials/cell)"
	@echo "make bench-ood-clang    Run OOD suite with Clang/libc++"
	@echo "make smoke              Quick canonical/public-API correctness check"
	@echo "make smoke-ood          Quick OOD generator/layout correctness check"
	@echo "make clean              Remove build outputs"
	@echo "make clean-results      Remove benchmark results/checkpoints"
