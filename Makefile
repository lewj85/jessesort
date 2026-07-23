CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude
LDFLAGS ?=

BUILD_DIR := build
BENCH_SRC := benchmarks/main.cpp

SIM_BIN    := $(BUILD_DIR)/bench_simulated
FREEZE_BIN := $(BUILD_DIR)/bench_early_freeze
ACTUAL_BIN := $(BUILD_DIR)/bench_actual_piles

SIM_HEADER    := include/jessesort/simulated.hpp
FREEZE_HEADER := include/jessesort/simulated_early_freeze.hpp
ACTUAL_HEADER := include/jessesort/actual_piles.hpp

.PHONY: all variants benchmark \
        simulated early-freeze actual \
        run-simulated run-early-freeze run-actual run-all \
        clang-libstdc++ clang-libc++ \
        clean help

all: variants

benchmark: run-all

variants: simulated early-freeze actual

simulated: $(SIM_BIN)
early-freeze: $(FREEZE_BIN)
actual: $(ACTUAL_BIN)

$(BUILD_DIR):
	mkdir -p $@

$(SIM_BIN): $(BENCH_SRC) $(SIM_HEADER) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) \
		-DJESSESORT_VARIANT_HEADER='"jessesort/simulated.hpp"' \
		-DJESSESORT_VARIANT_NAMESPACE=jessesort::simulated \
		-DJESSESORT_VARIANT_NAME='"Simulated JesseSort"' \
		$< $(LDFLAGS) -o $@

$(FREEZE_BIN): $(BENCH_SRC) $(FREEZE_HEADER) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) \
		-DJESSESORT_VARIANT_HEADER='"jessesort/simulated_early_freeze.hpp"' \
		-DJESSESORT_VARIANT_NAMESPACE=jessesort::simulated_early_freeze \
		-DJESSESORT_VARIANT_NAME='"Early-freeze JesseSort"' \
		$< $(LDFLAGS) -o $@

$(ACTUAL_BIN): $(BENCH_SRC) $(ACTUAL_HEADER) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) \
		-DJESSESORT_VARIANT_HEADER='"jessesort/actual_piles.hpp"' \
		-DJESSESORT_VARIANT_NAMESPACE=jessesort::actual_piles \
		-DJESSESORT_VARIANT_NAME='"Actual-pile JesseSort"' \
		$< $(LDFLAGS) -o $@

run-simulated: simulated
	./$(SIM_BIN)

run-early-freeze: early-freeze
	./$(FREEZE_BIN)

run-actual: actual
	./$(ACTUAL_BIN)

# Runs each benchmark as a separate process, one after another.
run-all: variants
	./$(SIM_BIN)
	./$(FREEZE_BIN)
	./$(ACTUAL_BIN)

# Rebuild all variants with Clang while retaining libstdc++.
clang-libstdc++:
	$(MAKE) clean
	$(MAKE) variants CXX=clang++

# Rebuild all variants with Clang and libc++.
# Requires libc++ and libc++abi development packages.
clang-libc++:
	$(MAKE) clean
	$(MAKE) variants CXX=clang++ \
		CXXFLAGS='$(CXXFLAGS) -stdlib=libc++' \
		LDFLAGS='$(LDFLAGS) -stdlib=libc++'

clean:
	rm -rf $(BUILD_DIR)

help:
	@printf '%s\n' \
	  'Build targets:' \
	  '  make variants          Build all three isolated benchmarks' \
	  '  make simulated         Build simulated-insertion variant' \
	  '  make early-freeze      Build early-freeze variant' \
	  '  make actual            Build actual-pile variant' \
	  '' \
	  'Run targets:' \
	  '  make run-simulated' \
	  '  make run-early-freeze' \
	  '  make run-actual' \
	  '  make run-all           Run three separate processes sequentially' \
	  '' \
	  'Compiler targets:' \
	  '  make clang-libstdc++   Build all with Clang + libstdc++' \
	  '  make clang-libc++      Build all with Clang + libc++' \
	  '' \
	  'Other:' \
	  '  make clean'
