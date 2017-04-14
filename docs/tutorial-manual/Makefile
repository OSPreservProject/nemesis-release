# pristine -- Remove all derived files
# clean -- Remove everything apart from the final postscript
# wipe -- Remove a half-made .dvi file

default:all

all: tutorial

neatly:	pristine Tutorial clean

tutorial: wipe graphics Tutorial.dvi Tutorial.ps

pristine: clean
	rm -f Tutorial.ps

clean:
	rm -f .word_count *.aux *.dvi *.bbl *.blg *.glo *.idx *~ *.ilg *.log *.ind *.toc *.bak core
	cd xfig && gnumake clean
	cd tgif && gnumake clean

wipe:
	rm -f Tutorial.dvi

graphics:
	cd xfig && gnumake graphics
	cd tgif && gnumake graphics

%.ps: %.dvi
	dvips -t a4 -o $@ $<

%.dvi: %.tex
	latex $<
	latex $<

%.eps: %.fig
	fig2dev -L ps $< $@
