GIT_FILES=$(wildcard *.gif)
PUML_FILES=$(wildcard *.puml)
SVG_FILES=$(PUML_FILES:.puml=.svg)
EXTRA_DIST=$(GIT_FILES) $(PUML_FILES) $(SVG_FILES)

all: $(SVG_FILES)

.puml.svg:
	plantuml -tsvg $<
