@echo off
setlocal
cd /d "%~dp0"

if not exist ".latex-build" mkdir ".latex-build"

where latexmk >nul 2>nul
if errorlevel 1 goto :use_pdflatex

echo === latexmk: User_Guide.pdf ===
pushd latex
latexmk -pdf -interaction=nonstopmode -f -outdir=..\.latex-build user_guide.tex
if errorlevel 1 exit /b 1
popd
copy /Y ".latex-build\user_guide.pdf" "User_Guide.pdf" >nul

echo === latexmk: stageFinal.pdf ===
pushd latex
latexmk -pdf -interaction=nonstopmode -f -outdir=..\.latex-build stageFinal.tex
if errorlevel 1 exit /b 1
popd
copy /Y ".latex-build\stageFinal.pdf" ".\"

goto :done

:use_pdflatex
echo latexmk not found; using pdflatex (ensure TeX is on PATH^)
echo === pdflatex: User_Guide.pdf ===
pushd latex
pdflatex -interaction=nonstopmode -output-directory=..\.latex-build user_guide.tex
if errorlevel 1 exit /b 1
pdflatex -interaction=nonstopmode -output-directory=..\.latex-build user_guide.tex
if errorlevel 1 exit /b 1
popd
copy /Y ".latex-build\user_guide.pdf" "User_Guide.pdf" >nul

echo === pdflatex: stageFinal.pdf ===
pushd latex
pdflatex -interaction=nonstopmode -output-directory=..\.latex-build stageFinal.tex
if errorlevel 1 exit /b 1
pdflatex -interaction=nonstopmode -output-directory=..\.latex-build stageFinal.tex
if errorlevel 1 exit /b 1
pdflatex -interaction=nonstopmode -output-directory=..\.latex-build stageFinal.tex
if errorlevel 1 exit /b 1
popd
copy /Y ".latex-build\stageFinal.pdf" ".\"

:done
echo Done. PDFs are in: %cd%
exit /b 0
