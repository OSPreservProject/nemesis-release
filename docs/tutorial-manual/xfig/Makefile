# Makefile to turn xfig .fig files into .eps ones.  Currently this is
# just a placeholder in case any .fig files come along...

default: graphics

clean:
	rm -f \#*\# *.eps *.bak *~

graphics: \

%.eps: %.fig
	fig2dev -L ps $< $@


