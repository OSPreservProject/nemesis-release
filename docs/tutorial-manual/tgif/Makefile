# Makefile to turn tgif .obj files into .eps ones.

default: graphics

clean:
	rm -f \#*\# *.eps *.bak *~

graphics: \
	mstr-build.eps \
	mstr-sparse-build.eps \
	mstr-shad-build.eps

%.eps: %.obj
	tgif -print -eps $<


