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

echo === latexmk: final_report.pdf ===
pushd latex
latexmk -pdf -interaction=nonstopmode -f -outdir=..\.latex-build final_report.tex
if errorlevel 1 exit /b 1
popd
copy /Y ".latex-build\final_report.pdf" ".\"

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

echo === pdflatex: final_report.pdf ===
pushd latex
pdflatex -interaction=nonstopmode -output-directory=..\.latex-build final_report.tex
if errorlevel 1 exit /b 1
pdflatex -interaction=nonstopmode -output-directory=..\.latex-build final_report.tex
if errorlevel 1 exit /b 1
pdflatex -interaction=nonstopmode -output-directory=..\.latex-build final_report.tex
if errorlevel 1 exit /b 1
popd
copy /Y ".latex-build\final_report.pdf" ".\"

:done
echo Done. PDFs are in: %cd%
exit /b 0
