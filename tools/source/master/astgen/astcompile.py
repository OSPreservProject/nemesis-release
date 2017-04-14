# Copyright 1999 Dickon Reed

import os, sys, string
def error(mesg):
    sys.stderr.write('ERROR: '+mesg+'\n')
    sys.exit(1)

def compile(iffiles):
    if len(iffiles)>0:
        sys.stderr.write('Invoking front end (cl -A) on '+string.join(iffiles, ' ')+'\n')
        result = os.system('cl -A '+string.join(iffiles, ' '))
        if result != 0:
            error('Front end aborted (result code %s)' % `result`)
        for file in iffiles:
            astfile = file[:-3]+'.ast'
            if not os.path.exists(astfile):
                error('Front end failed to produce %s from %s' % (astfile, iffile))
