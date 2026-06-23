CODE_DIR   = ../code
BUILD_DIR  = $(CODE_DIR)/build
BENCH      = $(BUILD_DIR)/bench
SOURCES    = $(CODE_DIR)/CMakeLists.txt $(wildcard $(CODE_DIR)/*.cpp) $(wildcard $(CODE_DIR)/*.hpp)

DIAGRAMS   = diagrams
STATIC     = assets/static
GENERATED  = assets/generated
ABS_GENERATED = "$(abspath $(GENERATED))"

BENCH_CSV  = $(GENERATED)/benchmark.csv
LOAD_CSV   = $(GENERATED)/load_benchmark.csv
STAMP      = $(GENERATED)/.generated

TYPST      = paper.typ template.typ
PDF        = paper.pdf

all: $(PDF)

# This is kind of messy, but it does build the benchmark executable if it doesnt exist
$(BENCH): $(SOURCES)
	cd $(CODE_DIR) && cmake -B build && cmake --build build

# Use | $(GENERATED) to ensure output directory exists
$(GENERATED):
	mkdir -p $@

# Render diagrams
$(GENERATED)/uml.png: $(DIAGRAMS)/uml.puml | $(GENERATED)
	plantuml --format png --output-dir $(ABS_GENERATED) $<
$(GENERATED)/insert_flow.png: $(DIAGRAMS)/insert_flow.puml | $(GENERATED)
	plantuml --format png --output-dir $(ABS_GENERATED) $<

# Run the benchmark suite
$(BENCH_CSV) $(LOAD_CSV): $(BENCH) | $(GENERATED)
	$(BENCH) --bench-csv $(BENCH_CSV) --load-csv $(LOAD_CSV)

# Generate tables and plots for the coursepaper from the benchmark results
$(STAMP): build.py $(BENCH_CSV) $(LOAD_CSV) | $(GENERATED)
	python3 build.py --bench $(BENCH_CSV) --load $(LOAD_CSV) --output $(GENERATED) --table-n 1000 10000 100000
	touch $@

$(PDF): $(TYPST) \
        $(GENERATED)/uml.png \
        $(GENERATED)/insert_flow.png \
        $(STATIC)/tests_output.png \
        $(STAMP)
	typst compile paper.typ $(PDF)

clean:
	rm -rf $(GENERATED)/*
	rm -f $(PDF)

.PHONY: all clean
