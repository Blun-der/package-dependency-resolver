# Package Dependency Resolver (C++)
#   make          build the program   -> build/pdr
#   make test     build and run the engine and command-line tests
#   make demo     analyse the bundled sample project
#   make graph    draw the sample and this program as SVG (needs graphviz)
#   make bench    time the algorithms on generated projects -> build/bench.csv + charts
#   make install  copy pdr to ~/.local/bin, so "pdr" works in any folder
#   make uninstall
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

ENGINE  = src/util.cpp src/parser.cpp src/graph.cpp src/trie.cpp \
          src/algorithms.cpp src/resolver.cpp src/analyzer.cpp src/json_report.cpp \
          src/dot_export.cpp src/rules.cpp
HEADERS = $(wildcard src/*.h)
SAMPLE  = ../sample/fad_demo
PREFIX ?= $(HOME)/.local

all: build/pdr

build/pdr: $(ENGINE) src/main.cpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(ENGINE) src/main.cpp

# -Wno-free-nonheap-object: GCC 16 gives a false warning inside its own
# std::vector<std::string> code when the tests are optimised.
build/pdr_tests: $(ENGINE) tests/engine_tests.cpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -Wno-free-nonheap-object -o $@ $(ENGINE) tests/engine_tests.cpp

build/pdr_bench: $(ENGINE) bench/bench.cpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(ENGINE) bench/bench.cpp

bench: build/pdr_bench
	./build/pdr_bench build/bench.csv
	python3 bench/plot.py build/bench.csv build

test: build/pdr_tests build/pdr
	./build/pdr_tests $(SAMPLE)
	sh tests/cli_test.sh ./build/pdr $(SAMPLE)

demo: build/pdr
	./build/pdr $(SAMPLE) --report

graph: build/pdr
	./build/pdr $(SAMPLE) --dot | dot -Tsvg -o build/sample-graph.svg
	./build/pdr . --dot | dot -Tsvg -o build/pdr-graph.svg
	@echo "wrote build/sample-graph.svg and build/pdr-graph.svg"

install: build/pdr
	install -Dm755 build/pdr $(DESTDIR)$(PREFIX)/bin/pdr
	@echo "installed $(DESTDIR)$(PREFIX)/bin/pdr"
	@case ":$$PATH:" in *":$(PREFIX)/bin:"*) ;; \
	  *) echo "note: $(PREFIX)/bin is not on your PATH; add this line to ~/.bashrc:"; \
	     echo '  export PATH="$(PREFIX)/bin:$$PATH"' ;; esac

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/pdr

clean:
	rm -rf build

.PHONY: all test demo graph bench install uninstall clean
