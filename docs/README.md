# Documentation

## PDFs (`docs/`)

Course PDFs live **in this directory** (not under `latex/`):

| File | Source (if you build from LaTeX) |
|------|-----------------------------------|
| `stageFinal.pdf` | `latex/stageFinal.tex` |
| `User_Guide.pdf` | `latex/user_guide.tex` |
| `stageOne.pdf` | Course Stage 1 proposal (archived submitted PDF) |
| `stageTwo.pdf` | Course Stage 2 development report (archived submitted PDF) |

You can **replace `stageFinal.pdf`** with your own exported PDF; the Makefile only overwrites it when you run `make` / `build-docs.bat`.

LaTeX sources and screenshots are under `latex/` and `images/`.

## Rebuild from LaTeX

Intermediate files go to `docs/.latex-build/` (gitignored); finished PDFs are copied to `docs/`.

- **macOS / Linux / Git Bash:** from `docs/`, run `make`.
- **Windows (cmd):** run `build-docs.bat` from `docs/`.

The Makefile uses `latexmk` if it is on your `PATH`; otherwise it runs `pdflatex` enough times for stable references. Requires a TeX distribution (TeX Live or MiKTeX) with `pdflatex`; `latexmk` is optional.
