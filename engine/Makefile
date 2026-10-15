# PawnHub engine — Linux/macOS build, the companion to engine\build.ps1 (Windows).
#
# Portable by default: C++20, -O2, all warnings. No -march flag so the same
# Makefile works on any CPU (including Apple Silicon), no -static because the
# macOS linker cannot statically link libc++.
#
# To reproduce the exact Dockerfile build instead, use:
#     make EXTRA_FLAGS="-march=x86-64-v2 -static"
#
# Targets:
#     make            build engine/build/engine
#     make selftest   build, then run the full selftest suite
#     make clean      remove build artifacts

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra
EXTRA_FLAGS ?=

SRCS := $(wildcard src/*.cpp)
HDRS := $(wildcard src/*.h)
BIN  := build/engine

.PHONY: all selftest clean

all: $(BIN)

$(BIN): $(SRCS) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(EXTRA_FLAGS) $(SRCS) -o $@

# The selftest resolves nets/nnue.bin and nets/verify.csv relative to the CWD,
# so it always runs from the REPO ROOT (cd ..), not from engine/.
selftest: all
	cd .. && ./engine/$(BIN) --selftest

clean:
	rm -rf build
