CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -pedantic
BIN      := bin
OBJ      := build

COMMON_SRC := $(wildcard src/common/*.cpp)
COMMON_OBJ := $(COMMON_SRC:src/%.cpp=$(OBJ)/%.o)
PROGRAMS   := img_hist img_quant img_cmp
TARGETS    := $(addprefix $(BIN)/,$(PROGRAMS)) $(BIN)/img_hist_plot.py

.PHONY: all clean test

all: $(TARGETS)

$(BIN)/%: $(OBJ)/%.o $(COMMON_OBJ) | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ)/%.o: src/%.cpp src/common/image.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc -c -o $@ $<

$(BIN)/img_hist_plot.py: scripts/img_hist_plot.py | $(BIN)
	cp $< $@
	chmod +x $@

$(BIN):
	mkdir -p $@

test: all
	./tests/run_tests.sh

clean:
	rm -rf $(BIN) $(OBJ)

.SECONDARY: $(COMMON_OBJ) $(addprefix $(OBJ)/,$(PROGRAMS:=.o))
