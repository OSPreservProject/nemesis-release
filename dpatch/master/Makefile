

all: dpatch.ps dpatch/index.html

dpatch.ps: dpatch.tex
	latex dpatch.tex
	latex dpatch.tex
	dvips -o dpatch.ps dpatch.dvi
	rm -rf dpath.aux dpatch.log dpatch.toc dpatch.aux

dpatch/index.html: dpatch.tex
	latex2html -split 0 -toc_depth 3 -t dpatch -no_navigation dpatch.tex

clean:
	rm -rf dpatch/ dpatch.aux dpath.dvi dpatch.log dpatch.ps dpatch.toc dpatch.dvi
