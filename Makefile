CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -pedantic
LDFLAGS  ?= -pthread
BIN      := bin
OBJ      := build

COMMON_SRC   := $(wildcard src/common/*.cpp)
COMMON_OBJ   := $(COMMON_SRC:src/%.cpp=$(OBJ)/%.o)
LOSSLESS_SRC := $(wildcard src/lossless/*.cpp)
LOSSLESS_OBJ := $(LOSSLESS_SRC:src/%.cpp=$(OBJ)/%.o)
HEADERS      := $(wildcard src/common/*.hpp src/lossless/*.hpp)

PART1    := img_hist img_quant img_cmp
PART2    := img_lossless_enc img_lossless_dec
TARGETS  := $(addprefix $(BIN)/,$(PART1) $(PART2)) $(BIN)/img_hist_plot.py

.PHONY: all clean test bench

all: $(TARGETS)

$(addprefix $(BIN)/,$(PART1)): $(BIN)/%: $(OBJ)/%.o $(COMMON_OBJ) | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(addprefix $(BIN)/,$(PART2)): $(BIN)/%: $(OBJ)/%.o $(LOSSLESS_OBJ) $(COMMON_OBJ) | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ)/%.o: src/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc -c -o $@ $<

$(BIN)/img_hist_plot.py: scripts/img_hist_plot.py | $(BIN)
	cp $< $@
	chmod +x $@

$(BIN):
	mkdir -p $@

test: all
	./tests/run_tests.sh

# JPEG-LS front-end for the state-of-the-art benchmark (needs libcharls-dev)
$(BIN)/charls_cli: tools/charls_cli.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $< -lcharls

bench: all $(BIN)/charls_cli
	python3 scripts/benchmark_sota.py

clean:
	rm -rf $(BIN) $(OBJ)

.SECONDARY: $(COMMON_OBJ) $(LOSSLESS_OBJ) $(addprefix $(OBJ)/,$(PART1:=.o) $(PART2:=.o))
