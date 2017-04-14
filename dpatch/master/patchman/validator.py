#! /usr/bin/env python

"""

A stand alone tool to validate patches in a patchman archive

Usage:

validator.py archive_root branch

"""

import os, string, tempfile, sys

def fatal(str):
    sys.stderr.write(str+'\n')
    sys.exit(1)

def system(command):
    command = command + ' 2>&1'
    sys.stderr.write('Command: %s\n' % command)
    o = os.popen(command, 'r')
    output = o.read()
    result = o.close()
    sys.stderr.write('Output: '+ output+'\n')
    if result == None: result = 0
    sys.stderr.write('Result: '+ `result`+'\n')
    return (result,output)


if len(sys.argv) != 4:
    fatal('Usage: validator.py patchman_archive_root dpatch_archive_root branch\n')

contrib_dir = sys.argv[1]
dpatch_archive = sys.argv[2]
branch = sys.argv[3]

if not os.path.isdir(contrib_dir):
    fatal('No patchman archive')
if not os.path.isdir(contrib_dir + '/' + branch):
    fatal('No branch directory in archive')
if not os.path.isdir(dpatch_archive):
    fatal('No dpatch archive')

spl = string.split(branch, ':')
if len(spl) != 2:
    fatal('Invalid branch name')
(package, branchname) = spl


if not os.path.isdir(dpatch_archive+'/'+package):
    fatal('No such dpatch package')
if not os.path.isdir(dpatch_archive+'/'+package+'/'+branchname):
    fatal('No such dpatch branch in package')
    


donework = 1
while donework:
    donework = 0
    contents = os.listdir(contrib_dir + '/' + branch)
    
    if 'Validator' in contents:
        fatal('Validator file lock present')
    try:
        o = open(contrib_dir+'/'+branch+'/Validator', 'w')
        o.write('running\n')
        o.close()
        sys.stderr.write('Got validator lock\n')

        dpatch_contents = os.listdir(dpatch_archive + '/'+ package+'/'+branchname)
        i = 1 
        while `i` in dpatch_contents: i = i + 1
        i = i - 1
        validations = {}
        sys.stderr.write('Validating against dpatch %s %s pl %s\n' % (package, branchname, i))
        if 'Valid' in contents:
            o = open(contrib_dir+'/'+branch+'/Valid')
            lines = o.readlines()
            o.close()
            for line in lines:
                spl = string.split(line[:-1], ' ')
                if len(spl) == 3:
                    if not validations.has_key(spl[0]):
                        validations[spl[0]] = {}
                    validations[spl[0]][spl[1]] = eval(spl[2])
        for file in contents:
            if donework: break
            spl = string.split(file, ':')
            if len(spl) == 3 and spl[2] == 'log' and (not string.join(spl[0:2], ':') in contents):
                # remove stale log
                os.unlink(contrib_dir+'/'+branch+'/'+file)
            if len(spl) == 2:
                # ie it's a patch file and not a data file
                if not validations.has_key(file): validations[file] = {}
                if not validations[file].has_key(`i`):
                    # okay, we want to test it
                    tempdir = tempfile.mktemp()
                    os.mkdir(tempdir)
                    sys.stderr.write('In directory '+tempdir+'\n')
                    (result,prcsoutput) = system('(cd '+tempdir+' && prcs checkout -f -r '+branchname+'.@ '+package+')')
                    if result:
                        sys.stderr.write('Unable to checkout '+package+' branch '+branchname+'; leaving patch as pending\n')
                    else:
                        #system('(cd '+tempdir+'/master && find .)')
                        (result,output) = system('dcheckin.py apply '+contrib_dir+'/'+branch+'/'+file+' '+tempdir)
                        if result in  [0,None]: happy = 1
                        else: happy = 0
                        sys.stderr.write(`(happy,result)`+'\n')
                        sys.stderr.write('Patch %s to %s level %d %s\n' % (file, branch, i, happy and 'succeded' or 'failed'))
                        validations[file][`i`] = happy
                        donework = 1
                        o = open(contrib_dir+'/'+branch+'/'+file+':log', 'w')
                        o.write('Validation log for '+package+' branch '+branchname+'\n')
                        o.write('checkout output:\n')
                        o.write(prcsoutput)
                        o.write('patch application output:\n')
                        o.write(output)
                        o.close()
                    system('rm -rf '+tempdir)
        contents = os.listdir(contrib_dir + '/' + branch)
        for patch in validations.keys():
            if (len(string.split(patch, ':')) != 2) or (not patch in contents):
                del validations[patch]
            
        o = open(contrib_dir + '/' + branch + '/Valid', 'w')
        for patch in validations.keys():
            for level in validations[patch].keys():
                o.write('%s %s %d\n' % (patch, level, validations[patch][level]))
    finally:
        try:
            os.unlink(contrib_dir +'/'+branch+'/Validator')
        except:
            pass

