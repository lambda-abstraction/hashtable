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

DIAGRAM_SRCS = $(wildcard $(DIAGRAMS)/*.puml)
DIAGRAM_PNGS = $(patsubst $(DIAGRAMS)/%.puml,$(GENERATED)/%.png,$(DIAGRAM_SRCS))

all: $(PDF)

$(BENCH): $(SOURCES)
	cd $(CODE_DIR) && cmake -B build && cmake --build build

$(GENERATED):
	mkdir -p $@

$(GENERATED)/%.png: $(DIAGRAMS)/%.puml | $(GENERATED)
	plantuml --format png --output-dir $(ABS_GENERATED) $<

$(BENCH_CSV) $(LOAD_CSV): $(BENCH) | $(GENERATED)
	$(BENCH) --bench-csv $(BENCH_CSV) --load-csv $(LOAD_CSV)

$(STAMP): build.py $(BENCH_CSV) $(LOAD_CSV) | $(GENERATED)
	python3 build.py --bench $(BENCH_CSV) --load $(LOAD_CSV) --output $(GENERATED) --table-n 1000 10000 100000
	touch $@

$(PDF): $(TYPST) $(DIAGRAM_PNGS) $(STATIC)/tests_output.png $(STAMP)
	typst compile paper.typ $(PDF)

clean:
	rm -rf $(GENERATED)/*
	rm -f $(PDF)

.PHONY: all clean
