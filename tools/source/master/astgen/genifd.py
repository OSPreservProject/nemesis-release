
import BE
import sys


def Go(kind, intflist, fp):
    intf = intflist[-1].obj
    ifdname = intf.name + '.if.d'
    sys.stdout.write('('+ifdname+')')
    sys.stdout.flush()
    f = open(ifdname, 'w')
    str = ''
    l = [ ('', '.ih'), ('','.def.c'), ('IDC_S_', '.c'), ('IDC_M_', '.c'), ('IDC_M_', '.h') ]
    for (prefix,postfix) in l:
	str= str +  prefix + intf.name + postfix + ' '
    str = str + ' : ' + ' \\\n\t./'+intf.name+'.if' +'\n'
    f.write(str)
    f.close()
    return [ifdname]

    
