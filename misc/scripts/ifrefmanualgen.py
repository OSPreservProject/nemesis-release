#! /usr/bin/env python

import sys
import os
import regex
import regsub
import re
import string
import commands
import StringIO

# set to 0 to disable the MIDDL to tex munging and just dump plain ASCII
translate = 1

#-------------------------------------------------------------- Patterns:

# Regular expression fragments:

whitespace_re   = '\([ \t]*\)'
comment_re      = '--\(.*\)$'
quotation_re    = '"\([^"]*\)"'
breaking_re     = '\([ \t\\.]\)'

# The patterns we're interested in:

whitespace_pat   = regex.compile (whitespace_re)
comment_line_pat = regex.compile (whitespace_re + comment_re)
bare_comment_pat = regex.compile (whitespace_re + '--' + whitespace_re + '$')
blank_line_pat   = regex.compile (whitespace_re + '$')
quotation_pat    = regex.compile (quotation_re)
breaking_pat     = regex.compile (breaking_re)

#---------------------------------------------------------- The translator:

tab_spaces = '  '

def TTline (line, first) :
  if first : res = ''
  else     : res = '\\hfil\\break'

  if len (line) > 0 :
    if line[-1] == '\n' :
      line = line[:-1]

  bits = regsub.split (regsub.gsub ('\t', tab_spaces, line), '`')

  tt = 1
  for bit in bits:
    if tt:
      res = res + '\\verb`' + bit + '`'
    else:
      res = res + TTquotes (bit)
    tt = not tt

  return res + '\n'

def TTquotes (line) :
  res  = ''
  bits = regsub.split (line, '"')
  tt   = 0
  for bit in bits:
    if tt:
      res = res + '{\\tt '
      for c in bit:
	res = res + QuoteChar (c)
      res = res + '}'
    else:
      res = res + underscorefilter(bit)
    tt = not tt

  return res 

def underscorefilter(str):
    # why doesn't this work
    #newstr = re.sub(r'([\\^])_', r'\1\\_', str)
    #if newstr != str:
    #    print 'underscore filter hit\n',newstr
    #return newstr
    #return string.replace(str, '_', '\_')
    # hack
    index = 0
    newstr = ''
    while index < len(str):
        if str[index] == '_' and index>0 and str[index-1] != '\\':
            newstr = newstr + '\\'
            sys.stderr.write('*')
        newstr = newstr + str[index]
        index = index + 1
    return newstr


def QuoteChar (c) :
  if   c == '%' : return '\\char\'045{}'
  elif c == '_' : return '\\char\'137{}'
  elif c == '&' : return '\\char\'046{}'
  elif c == '$' : return '\\char\'044{}'
  elif c == '^' : return '\\char\'136{}'
  elif c == '#' : return '\\char\'043{}'
  elif c == '{' : return '\\char\'173{}'
  elif c == '}' : return '\\char\'175{}'
  elif c == '*' : return '\\char\'052{}'
  elif c == '\\': return '\\char\'134{}'
  else :          return c


#def TTquotes (line) :
#  res  = ''
#  bits = regsub.split (line, '"')
#  tt   = 0
#  for bit in bits:
#    if tt:
#      res = res + '\\verb`'
#      for c in bit:
#	if breaking_pat.match (c) != -1 :
#	  res = res + '`\\penalty0\\verb`' + c
#	else :
#	  res = res + c
#      res = res + '`'
#    else:
#      res = res + bit
#    tt = not tt
#
#  return res
#
#  return regsub.gsub (quotation_pat, '\\verb`\\1`', line)

class MiddlXlater:

  def init (self, name, outfd = sys.stdout):
    self.outfd = outfd
    self.name =  name
    return self

  def parse (self, fd):
    self.preamble ()

    self.lines       = fd.readlines ()
    self.nlines      = len (self.lines)
    self.line        = 0
    self.prog_header = ''

    self.skip_header ()
    while self.line < self.nlines:
      self.program_blocks ()
      if self.line < self.nlines: self.comment_blocks ()

    self.postamble ()


  def preamble (self):
    self.outfd.write ('%% preamble\n')
    self.outfd.write ('\\index{' + self.name + '@\\texttt{' + self.name + '}|(}\n')

  def postamble (self):
    self.outfd.write ('\n\n%% postamble\n')
    self.outfd.write ('\\index{' + self.name + '@\\texttt{' + self.name + '}|)}\n')

  def skip_header (self):
    while blank_line_pat.match (self.lines [self.line]) == -1 :
      self.line = self.line + 1

    self.line = self.line + 1

  def skip_blank_lines (self):
    start_line = self.line

    while self.line < self.nlines and \
	blank_line_pat.match (self.lines [self.line]) != -1:
      self.line = self.line + 1

    return self.line - start_line

  def program_blocks (self):
    self.skip_blank_lines ()
    blank_lines = 0
    first = 1

    self.outfd.write ('{ ' + self.prog_header)
    self.prog_header = ''

    while self.line < self.nlines:
      line = self.lines [self.line]

      if comment_line_pat.match (line) != -1 : break

      while blank_lines > 0:
	self.outfd.write (TTline ('', 0))
	blank_lines = blank_lines - 1

      split_line = regsub.split (line, '--')
      self.outfd.write (TTline (split_line[0], first))
      first = 0
      if len (split_line) > 1:
	self.outfd.write ('{\\sl --- ' + TTquotes (split_line[1]) + '}\n')

      self.line = self.line + 1
      blank_lines = self.skip_blank_lines ()

    self.outfd.write ('}\n\n')
    return
    

  def comment_blocks (self):
    col = 0; i = 0; line = self.lines [self.line]
    while line [i] != '-':
      if line [i] == ' ' : col = col + 1
      else:                col = col + 2
      i = i + 1

    if col > 2 :
      self.outfd.write ('\\nobreak\\vskip-.5\\baselineskip{\\leftskip=1em')
      self.comment_block ('\\sl')
      self.outfd.write ('\\par}\n')

    while self.line < self.nlines:
      self.skip_blank_lines ()
      if comment_line_pat.match (self.lines [self.line]) == -1 : return
      if not self.subsection () :
	self.comment_block ('\\rm')

  def comment_block (self, font):
    self.outfd.write ('{' + font + '\n')
    first_tt = 1
    while self.line < self.nlines and \
	comment_line_pat.match (self.lines [self.line]) != -1 :
      line = comment_line_pat.group (2)
      if len (line) == 0:
	self.outfd.write ('\n')
	first_tt = 1
      elif line[0] == '|':
	self.outfd.write (TTline (line[1:], first_tt))
	first_tt = 0
      elif line[0] == '\\':
	self.prog_header = self.prog_header + line + '\n'
	first_tt = 1
      else:
	self.outfd.write (TTquotes (line) + '\n')
	first_tt = 1
      self.line = self.line + 1

    self.outfd.write ('}\n\n')

  def subsection (self):
    if self.nlines - self.line < 4 : return 0
    if bare_comment_pat.match (self.lines[self.line])   == -1 : return 0
    if comment_line_pat.match (self.lines[self.line+1]) == -1 : return 0
    if bare_comment_pat.match (self.lines[self.line+2]) == -1 : return 0
    if blank_line_pat.match   (self.lines[self.line+3]) == -1 : return 0

    self.outfd.write ('\n\n\\intfsection{' + \
	string.strip (comment_line_pat.group (2)) + '}\n\n')
    self.line = self.line + 4
    return 1

def dofile(filename):
  o = open(filename, 'r')
  data= o.read()
  o.close()
  if not translate:  
      print '\begin{verbatim}\n'+data+'\n\end{verbatim}'
      return
  
  try:
    out = StringIO.StringIO()
    xl = MiddlXlater ()
    xl.init('middl2tex', out)
    fp = StringIO.StringIO(data)
    xl.parse (fp)
    fp.close ()
    print out.getvalue()
  except IOError, msg:
    print '%s: %s' % ('Middl translator', msg)
    sys.exit (1)
  
sys.stderr.write('!')
o = os.popen('find . -name \*.if', 'r')
iffiles = map(lambda x: x[:-1], o.readlines())
o.close()

interfaces = {}
for filename in iffiles:
    spl = string.split(filename, '/')
    interfacename = spl[-1]
    if interfacename[-3:] != '.if':
        sys.stderr.write('Skipping invalid interface name %s\n' % interfacename)
    interfacename = interfacename[:-3]
    if not interfaces.has_key(interfacename): interfaces[interfacename] = []
    interfaces[interfacename].append(filename)

interfacenames = interfaces.keys()
interfacenames.sort()



print r"""
\documentclass[a4paper]{report}
\usepackage{a4wide}%      from ctan macros/latex/contrib/other/misc/a4wide.sty
\usepackage{epsfig}
\usepackage{palatino}
\usepackage{latexsym}
\usepackage{makeidx}
\usepackage{html}
\def\intfsection{\subsubsection*}
%\def\dab{\@ifstar{\@dab{\em}}{\@dab{\relax}}}
%\def\@dab#1#2#3{\expandafter\gdef\csname#2\endcsname{{#1#3\/} (#2)%
%                \expandafter\gdef\csname#2\endcsname{#2}}}
\def\dab{}

\renewcommand{\ttdefault}{cmtt} % Don't like courier
\makeatletter
% make captions hang properly (and SEPARATELY!!)
% (this replaces that which appears in report.sty)
\long\def\@makecaption#1#2{%
   \vskip 10\p@
   \setbox\@tempboxa\hbox{#1: #2}%
   \ifdim \wd\@tempboxa >\hsize
       \@hangfrom{#1: }#2\par
     \else
       \hbox to\hsize{\hfil\box\@tempboxa\hfil}%
   \fi}


\makeatother

% some useful ones
%\dab{QoS}{Quality of Service}
%\dab{DEC}{Digital Equipment Corporation}
%\dab{DAN}{Desk Area Network}
%\dab{DIB}{Domain Information Block}
%\dab{DIT}{Domain Information Table}
%\dab{DM}{Domain Manager}
%\dab{IDC}{Inter-Domain Communication}
%\dab{KCS}{kernel critical section}
%\dab{NTSC}{Nemesis Trusted System Call}
%\dab{SVAS}{single virtual address space}

\def\MIDDL{{\sc middl}}
\def\Clanger{{\sc Clanger}}

\font\dmetenss=cmss10
\font\dmetenssi=cmssi10
\font\dmeeightss=cmssq8
\font\dmeeightssi=cmssqi8
\newcommand{\epigraph}[3]{
  {\let\rm=\dmetenss \let\sl=\dmetenssi
   \begin{flushright}
    {\sl #1}
    \smallskip\noindent\rm--- #2\unskip\enspace(#3)
   \end{flushright}
  }
}



\setlength{\parskip}{\baselineskip}
\setlength{\parindent}{0pt}
\raggedbottom

% define an abbreviation
% \dab{abbr}{long version} defines \<abbr> to produce <long version> (<abbr>)
%                          first time, just <abbr> subsequently
% \dab*{abb}{long version} emphasises <long version> first time

\def\dab{\@ifstar{\@dab{\em}}{\@dab{\relax}}}
\def\@dab#1#2#3{\expandafter\gdef\csname#2\endcsname{{#1#3\/} (#2)%
                \expandafter\gdef\csname#2\endcsname{#2}}}

\begin{document}
\begin{rawhtml}
<BODY BGCOLOR="#FFFFFF">
\end{rawhtml}
"""

for interface in interfacenames:
    print '\\chapter{'+interface+'}'
    filenames = interfaces[interface]
    sys.stderr.write('.')
    
    if len(filenames) == 1 and 0:
        # no ambiguity here
        dofile(filenames[0])
    else:
        if len(filenames)>1: print 'There are multiple versions of '+interface
        filenames.sort()
        for filename in filenames:
            print '\\subsection*{'+underscorefilter(filename)+'}'
            dofile(filenames[0])
print r"""
\end{document}
"""







