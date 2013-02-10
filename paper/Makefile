all: paper.pdf

paper.dvi: paper.tex motivation.tex intro.tex
	latex paper.tex
	bibtex paper
	latex paper.tex
	latex paper.tex

paper.ps: paper.dvi
	dvips -e 0 -o paper.ps paper.dvi

paper.pdf: paper.tex
	pdflatex paper.tex
	bibtex paper
	pdflatex paper.tex
	pdflatex paper.tex

.PHONY: all clean
clean:
	rm -f *~ *.dvi *.log *.aux *.bbl *.toc *.blg *.eps

nuke: clean
	rm -f *.ps *.pdf
