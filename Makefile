CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude
LDFLAGS ?=

CLANGXX ?= clang++
CLANGFLAGS ?= -std=c++20 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic -stdlib=libc++
CLANGLDFLAGS ?= -stdlib=libc++

TARGET := benchmark
CLANG_TARGET := benchmark-clang

BENCHMARK_SOURCE := benchmarks/benchmark.cpp
SOURCES := $(sort $(wildcard src/jessesort_*.cpp))

OBJECTS := $(SOURCES:.cpp=.o) benchmarks/benchmark.o
CLANG_OBJECTS := $(SOURCES:.cpp=.clang.o) benchmarks/benchmark.clang.o

HEADERS := $(sort $(wildcard include/jessesort/*.h))

.PHONY: all clang bench bench-clang smoke clean clean-results help

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

src/%.o: src/%.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

benchmarks/benchmark.o: $(BENCHMARK_SOURCE) $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clang: $(CLANG_TARGET)

$(CLANG_TARGET): $(CLANG_OBJECTS)
	$(CLANGXX) $(CLANG_OBJECTS) $(CLANGLDFLAGS) -o $@

src/%.clang.o: src/%.cpp $(HEADERS)
	$(CLANGXX) $(CPPFLAGS) $(CLANGFLAGS) -c $< -o $@

benchmarks/benchmark.clang.o: $(BENCHMARK_SOURCE) $(HEADERS)
	$(CLANGXX) $(CPPFLAGS) $(CLANGFLAGS) -c $< -o $@

# Canonical benchmark entrypoint: 10k + 100k only, isolated cells, resumable
# Full 500-trial cells; 10k checkpoints after each 14-input algorithm group, 100k after each cell.
bench: $(TARGET)
	./benchmark.sh

# Clang/libc++ equivalent of the canonical benchmark.
bench-clang: $(CLANG_TARGET)
	BENCHMARK_BIN=./$(CLANG_TARGET) ./benchmark.sh

# Quick correctness/build check only; not suitable for performance conclusions.
smoke: $(TARGET)
	./$(TARGET) 2 10000 1 "" simulated Random

clean:
	rm -f $(TARGET) $(CLANG_TARGET) src/*.o benchmarks/*.o *.out *.exe

clean-results:
	rm -rf results

help:
	@echo "make                Build benchmark binary with default g++ compiler"
	@echo "make clang          Build benchmark-clang with clang++/libc++"
	@echo "make bench          Resume canonical 10k + 100k benchmark at next incomplete cell"
	@echo "make bench-clang    Resume canonical benchmark using benchmark-clang"
	@echo "make smoke          Quick correctness/build check"
	@echo "make clean          Remove build outputs"
	@echo "make clean-results  Remove benchmark results/checkpoints"
