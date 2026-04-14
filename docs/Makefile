# PDF outputs are written to this directory (docs/). Auxiliary files: .latex-build/
# Usage: from this directory, run: make
# Uses latexmk if on PATH; otherwise pdflatex (2–3 passes for stable TOC/refs).

LATEXDIR := latex
BUILD := .latex-build

.PHONY: all clean user-guide stage-final-report

all: user-guide stage-final-report

$(BUILD):
	mkdir -p $(BUILD)

user-guide: $(BUILD)
	cd $(LATEXDIR) && \
	if command -v latexmk >/dev/null 2>&1; then \
		latexmk -pdf -interaction=nonstopmode -f -outdir=../$(BUILD) user_guide.tex; \
	else \
		pdflatex -interaction=nonstopmode -output-directory=../$(BUILD) user_guide.tex && \
		pdflatex -interaction=nonstopmode -output-directory=../$(BUILD) user_guide.tex; \
	fi
	cp $(BUILD)/user_guide.pdf User_Guide.pdf

stage-final-report: user-guide $(BUILD)
	cd $(LATEXDIR) && \
	if command -v latexmk >/dev/null 2>&1; then \
		latexmk -pdf -interaction=nonstopmode -f -outdir=../$(BUILD) stageFinal.tex; \
	else \
		pdflatex -interaction=nonstopmode -output-directory=../$(BUILD) stageFinal.tex && \
		pdflatex -interaction=nonstopmode -output-directory=../$(BUILD) stageFinal.tex && \
		pdflatex -interaction=nonstopmode -output-directory=../$(BUILD) stageFinal.tex; \
	fi
	cp $(BUILD)/stageFinal.pdf .

clean:
	rm -rf $(BUILD)
