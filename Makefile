CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude
LDFLAGS ?=

TARGET := benchmark
CLANG_TARGET := benchmark-clang

BENCHMARK_SOURCE := benchmarks/benchmark.cpp
SOURCES := src/v1_actual_piles.cpp \
           src/v2_simulated.cpp \
           src/v3_inplace_simulated.cpp \
           src/v4_single_overflow.cpp \
           src/v5_deferred_bands.cpp \
           src/v6_live_bands.cpp \
           src/v7_avx2.cpp

OBJECTS := $(SOURCES:.cpp=.o) benchmarks/benchmark.o
CLANG_OBJECTS := $(SOURCES:.cpp=.clang.o) benchmarks/benchmark.clang.o

HEADERS := include/jessesort/v1_actual_piles.h \
           include/jessesort/v2_simulated.h \
           include/jessesort/v3_inplace_simulated.h \
           include/jessesort/v4_single_overflow.h \
           include/jessesort/v5_deferred_bands.h \
           include/jessesort/v6_live_bands.h \
           include/jessesort/v7_avx2.h

CLANG_CXX := clang++
CLANG_CXXFLAGS := $(CXXFLAGS) -stdlib=libc++
CLANG_LDFLAGS := $(LDFLAGS) -stdlib=libc++

.PHONY: all clang run run500 run-clang run500-clang \
        bench200 bench500 bench1000 \
        bench200-clang bench500-clang bench1000-clang \
        smoke smoke-clang clean clean-results

all: $(TARGET)

clang: $(CLANG_TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(CLANG_TARGET): $(CLANG_OBJECTS)
	$(CLANG_CXX) $(CLANG_OBJECTS) $(CLANG_LDFLAGS) -o $@

src/%.o: src/%.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

benchmarks/benchmark.o: $(BENCHMARK_SOURCE) $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

src/%.clang.o: src/%.cpp $(HEADERS)
	$(CLANG_CXX) $(CPPFLAGS) $(CLANG_CXXFLAGS) -c $< -o $@

benchmarks/benchmark.clang.o: $(BENCHMARK_SOURCE) $(HEADERS)
	$(CLANG_CXX) $(CPPFLAGS) $(CLANG_CXXFLAGS) -c $< -o $@

# Canonical benchmark: 500 distinct paired inputs at 1k/10k/100k and 50 at 1m.
# Every trial uses a new deterministic seed shared by V1-V7 and std::sort.
run run500 bench500: $(TARGET)
	./$(TARGET) 500 all 2

run-clang run500-clang bench500-clang: $(CLANG_TARGET)
	./$(CLANG_TARGET) 500 all 2

# Faster/larger sweeps retain the same 10:1 reduction at 1m.
bench200: $(TARGET)
	./$(TARGET) 200 all 2

bench1000: $(TARGET)
	./$(TARGET) 1000 all 2

bench200-clang: $(CLANG_TARGET)
	./$(CLANG_TARGET) 200 all 2

bench1000-clang: $(CLANG_TARGET)
	./$(CLANG_TARGET) 1000 all 2

# Quick correctness/build checks; not suitable for performance conclusions.
smoke: $(TARGET)
	./$(TARGET) 2 10000 1

smoke-clang: $(CLANG_TARGET)
	./$(CLANG_TARGET) 2 10000 1

clean:
	rm -f $(TARGET) $(CLANG_TARGET) \
	      src/*.o benchmarks/*.o \
	      src/*.clang.o benchmarks/*.clang.o \
	      *.out *.exe

# Benchmark artifacts are deliberately ephemeral and never used as future inputs.
clean-results:
	rm -rf results
