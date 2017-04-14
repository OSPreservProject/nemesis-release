#! /usr/bin/env python

"""
dcheckin; create and update project trees.


There are two things going on here. Firstly, we have the patch tree. This is
the canonical place where patches are stored. The patch tree consists of:

a file in the root called metaupdates, containing histories of each package
and branch. This file is partly documentation and partly exists to be RCS
locked to provide concurrency control on the patch tree.

a directory per package, contaning:
  a subdirectory per branch, containing:
    a bunch of files number 1 to N, where N is the current patch level
    a file called description, describing the branch
    a file called from, in all per the initial branch subdirectory,
    containig the string tuple (source branch, patch level of source branch),
    describing where the branch attaches to the branch it was forked from.

We then build a PRCS repository containing these packages as PRCS projects.
The PRCS repository is NOT CANONICAL: it may be rebuilt from the patch
repository.

The file dcheckin.rc must exist in the directory where dcheckin.py is invoked.
It contains a python dictionary, with field names 'project description' for
a text string to describe the project as a whole, 'patch tree' for the
path to the patch tree and 'prcs repository' for the path to the prcs
repository.

Limitations; rebuild cannot handle forks yet (can't be bothered to do the tree
ordering stuff yet). forks not really tested at all yet.
"""

import sys, time, os, string, md5, base64, pwd, tempfile, tokenize, rexec
import traceback, rfc822

def mesg ( str):
    sys.stderr.write('MESG: '+str+'\n')

vmesg = mesg # vmesg is for verbose message reporting

prcs_repo = None

checkout_collection = {}



vmesg('This is dcheckin.py')
argv = sys.argv

rcfilename = '~/.dcheckinrc'

if os.environ.has_key('DCHECKINRC_LOCATION'):
    rcfilename = os.environ['DCHECKINRC_LOCATION']

if len(argv)>3 and argv[1] == '-c':
    rcfilename = argv[2]
    argv = [argv[0]] + argv[3:]


try:
    o = open(os.path.expanduser(rcfilename), 'r')
except:
    sys.stderr.write('Configuration file '+rcfilename+' not readable\n')
    sys.exit(1)
rcdata = o.read()
o.close()
#try:
if 1:
    globals = eval(rcdata)
    patch_tree = globals['patch tree']
    prcs_repo = globals['prcs repository']
    if globals.has_key('description map'):
        desc_map = globals['description map']
    else:
        desc_map = {}
    if globals.has_key('publish info'):
        secrets = globals['publish info']
    else:
        secrets = []
    #autopublish = globals.has_key('autopublish') and globals['autopublish'] or None
#except:
#    sys.stderr.write('Configuration file dcheckin.rc corrupt\n')
#    sys.exit(1)

usage = """

dcheckin.py [-c rcfilename] create packagename firstbranch
dcheckin.py [-c rcfilename] multibranchcommit patchfilename packagename branch1 branch2 branch3 branch4...
dcheckin.py commit packagename branch patchfilename1 patchfilename2...
dcheckin.py [-c rcfilename] fork packagename oldbranch newbranch
dcheckin.py [-c rcfilename] apply patchfilename workdir
dcheckin.py [-c rcfilename] extract packagename branch
dcheckin.py [-c rcfilename] rebuild newrepodir
dcheckin.py [-c rcfilename] init
dcheckin.py [-c rcfilename] publish htmlfilename
dcheckin.py [-c rcfilename] webimport url [packagename]
dcheckin.py [-c rcfilename] fsimport filename


See the dpatch manual for further details
"""


fatal = 'fatal'


def maybemkdir(dirname):
    if os.path.exists(dirname): return
    components = string.split(dirname, '/')
    for i in range (1, len(components)+1):
        partpath = '/' + string.join(components[:i], '/')
        if not os.path.isdir(partpath):
            os.mkdir(partpath)

def shellwrap(str):
    strp = ''
    for char in str:
        if char == '"': strp = strp + '\\'
        if ord(char)<32 or ord(char)>=127:
            mesg('Bad character code '+`ord(char)`+' in filename')
            raise fatal
        strp = strp + char
    return '"'+strp+'"'

def paranoia(filename):
    prevchar = None
    if string.find(filename, '../') != -1 or string.find(filename, '/..') != -1:
        mesg('Bad double dot in filename')
        raise fatal
    for char in filename:
        if char in [' ', ';', '~'] or ord(char)<32 or ord(char)>=127 or (char == prevchar and char == '/'):
            mesg('Bad character code '+`ord(char)`+' in filename')
            raise fatal
        prevchar = char
def forkexec(args):
    pid = os.fork()
    if pid == 0:
        os.execvp(args[0], args)
    else:
        return os.wait(pid)

# Patch Interpreter is the thing that reads patch files

class AugmentedExec(rexec.RExec):
    ok_posix_names = ()
    def __init__(self, hooks = None, verbose = 0, myextras = {}):
        rexec.RExec.__init__(self, hooks, verbose)
        self.myextras = myextras

    def r_open(self, file, mode='r', buf=-1):
        raise IOError, "filesystem access forbidden"
    
    def r_import(self, mname, globals={}, locals={}, fromlist=[]):
        raise ImportError, "No imports allowed--ever"
           
    def r_exec(self, code):
        m = self.add_module('__main__')
        for key in self.myextras.keys():
            setattr(m, key, self.myextras[key])
        exec code in m.__dict__


class PatchInterpreter:
    def __init__(self, workdir, packagename, branchname):
        self.email = 'anon@nowhere'
        self.authorname = 'Stranger'
        self.ove = 'XXX no description\n'
        self.ts = 'never'
        self.log = []
        self.packagename = packagename
        self.branchname = branchname
        self.workdir = workdir
        maybemkdir(self.workdir+'/master')
    def author_email(self, email):
        self.email = email
        vmesg('Author email %s' % email)
    def author_name(self, name):
        self.authorname = name
        vmesg('Author friendly name %s ' % name)
    def overview(self, ove):
        self.ove = ove
        vmesg('Overview %s ' %ove)
    def date(self, ts):
        self.ts = ts
    def create(self, description, filename, contents, extended_description=''):
        paranoia(filename)
        vmesg('Create %s' %filename)
        if os.path.exists(self.workdir+'/master/'+filename):
            mesg('Error: creating file '+filename+' that already exists!')
            raise fatal
        (subpath, subfile) = os.path.split(filename)
        if not os.path.isdir(self.workdir+'/master/'+subpath):
            vmesg('Directory '+subpath+' does not exist; creating it implicitly')
            #print 'Implicitly creating it'
            spl = string.split(subpath, '/')
            #print 'Components are ',spl
            for i in range(1,len(spl)+1):
                #print 'Done up to',spl[:i]
                subsubpath = string.join(spl[:i], '/')
                #print 'Testing for '+subsubpath
                if not os.path.isdir(self.workdir+'/master/'+subsubpath):
                    self.mkdir('NEW DIRECTORY', subsubpath)
        #print 'Opening '+self.workdir+'/master/'+filename
        o = open(self.workdir+'/master/'+filename, 'w')
        o.write(contents)
        o.close()
        #checkin('master'+filename, description+extended_description)
        self.log = self.log + [pad(filename)+'++ '+description]
    def remove(self, description, filename):
        paranoia(filename)
        vmesg('Remove '+filename)
        if not os.path.isfile(self.workdir+'/master/'+filename):
            mesg('Error: removing file '+filename+' that does not exist')
            raise fatal
        os.unlink(self.workdir+'/master/'+filename)
        self.log = self.log + [pad(filename)+'XX '+description]
    def rmdir(self, description, directory):
        paranoia(directory)
        vmesg('Rmdir '+directory)
        if not os.path.isdir(self.workdir+'/master/'+directory):
            mesg('Error: removing directory '+directory+' that does not exist')
            raise fatal
        vmesg('Directory exists')        
        if len(os.listdir(self.workdir+'/master/'+directory)) != 0:
            mesg('Error: removing directory '+directory+' that is not empty')
            mesg('Contents of '+directory+' are '+`os.listdir('master/'+directory)`)
            raise fatal
        vmesg('Directory is empty')
        os.rmdir(self.workdir+'/master/'+directory)
        self.log = self.log + [pad(directory)+'!! '+description]
        
    def replace(self, description, filename, contents, extended_description=''):
        paranoia(filename)
        vmesg('Replace '+filename)
        if not os.path.exists(self.workdir+'/master/'+filename):
            mesg('Error: replacing file '+filename+' that does not exist')
            raise fatal
        #checkout('master/'+filename, lock=1)
        o = open(self.workdir+'/master/'+filename, 'w')
        o.write(contents)
        o.close()
        #checkin('master'+filename, description+extended_description)
        self.log = self.log + [pad(filename)+'-- '+description]
    def caption(self, description):
        self.log = self.log + [caption]
    def move(self, description, fromf, tof):
        paranoia(fromf)
        paranoia(tof)
        vmesg('Move %s to %s' % (fromf,tof))        
        try:
            os.rename(self.workdir+'/master/'+fromf,self.workdir+'/master/'+tof)
        except:
            print 'Error; Moving file from ',fromf,' to ',tof,' failed'
            raise fatal
        self.log = self.log + [pad(fromf) + '>> ' + pad(tof), description]
    def mkdir(self, description, directory):
        paranoia(directory)
        vmesg('Mkdir '+directory)
        if os.path.exists(self.workdir+'/master/'+directory):
            vmesg('Warning; directory '+directory+' already exists; been told to create it')
            #raise fatal
        maybemkdir(self.workdir+'/master/'+directory)
        self.log = self.log + [pad(directory) + '** ' + description]

    def chmod(self, description, filename, permissions):
        paranoia(filename)
        vmesg('Chmod %s %s' %(permissions,filename))
        if not os.path.exists(self.workdir+'/master/'+filename):
            vmesg('Error; cannot chmod nonexistent file '+filename)
            raise fatal
        result = forkexec(['chmod',permissions, self.workdir+'/master/'+filename]) in [0,None]
        vmesg('chmod result %s' % `result`)
        if result != 0:
            vmesg('chmod failed')
            raise fatal
        self.log = self.log + [pad(filename) + '!! ' + permissions]

    def assert_md5(self, filename, digestin):
        paranoia(filename)
        o = open(self.workdir+'/master/'+filename, 'r')
        data= o.read()
        o.close()
        m = md5.new()
        m.update(data)
        digest = m.digest()
        digeststr = strtohex(digest)
        if digeststr == digestin:
            mesg('MD5 assertion succeeded for '+filename)
        else:
            mesg('MD5 assertion failed: filename '+filename+' has md5 sum '+digeststr + '; patch asserts sum '+digestin)
            raise fatal
        
    def patch(self, description, filename, patch,major=0, extended_description=''):
        paranoia(filename)
        if not os.path.exists(self.workdir+'/master/'+filename):
            mesg('Error: patching nonexistent file '+filename+'!')
            (dir, part) = os.path.split(self.workdir+'/master/'+filename)
            system('ls -l '+shellwrap(dir))
            raise fatal
        vmesg('Patching file '+filename)
        o = os.popen('patch '+shellwrap(self.workdir+'/master/'+filename), 'w')
        o.write(patch)
        vmesg('Patch passed to GNU patch; waiting')
        result = o.close()
        if result != None and (type(result) == type(1) and result != 0):
            mesg('Error: patch failed to apply; patch returned '+`result`+'!')
            raise fatal
        vmesg('Patch to file %s accepted'%filename)
        self.log = self.log + [pad(filename) + '-- '+ description]
    
    def finalize(self, patchlevel):
        vmesg('Writing updates file for '+self.packagename+' level '+`patchlevel`)
        if not self.packagename:
            error('Finalize called on an unnamed package')
        vmesg('Reading '+self.packagename+'_updates')
        o = open(self.packagename+'_updates', 'r')
        lines = o.readlines()
        o.close()
        if len(lines)<3:
            error('Updates file is too short!')
        vmesg('Writing '+self.packagename+'_updates')
        o = open(self.packagename+'_updates', 'w')
        o.write(lines[0])
        o.write(lines[1])
        o.write(lines[2])
        (uid,_,_,_,gcos,_,_) = pwd.getpwuid(os.getuid())

        authorstr = ('#%.5d ' %patchlevel)+ uid+' pp '+self.authorname + ' ('+self.email+') '+self.ts
        o.write(authorstr+'\n')
        o.write('-'*len(authorstr)+'\n')
        o.write(self.ove+'\n')
        for logentry in self.log:
            o.write(logentry+'\n')
        o.write('\n')
        for line in lines[3:]: o.write(line)
        o.close()
        lines = []
        if os.path.exists(patch_tree+'/allupdates') and os.path.isfile(patch_tree+'/allupdates'):
            o = open(patch_tree+'/allupdates', 'r')
            lines = o.readlines()
            o.close()
        index = 0

        o = open(patch_tree+'/allupdates', 'w')
        while index < len(lines):
            if lines[index][:3] in ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat']:
                if comparetime(lines[index][:24], self.ts):
                    break
            o.write(lines[index])
            index = index + 1
            
        authorstr = ('#%.5d ' %patchlevel)+ uid+' pp '+self.authorname + ' ('+self.email+')'
        o.write(self.ts+' package '+self.packagename+' branch '+self.branchname+'\n')
        o.write(authorstr+'\n')
        o.write('-'*len(authorstr)+'\n')
        o.write(self.ove+'\n')
        for logentry in self.log:
            o.write(logentry+'\n')
        o.write('\n')
        for line in lines[index:]:
            o.write(line)
        o.close()
    def base64(self, str):
        return base64.decodestring(str)
    def execute(self, patch):
        dict = {
            'author_email' : self.author_email,
            'author_name' : self.author_name,
            'overview' : self.overview,
            'date' : self.date,
            'mkdir' : self.mkdir,
            'rmdir' : self.rmdir,
            'create' : self.create,
            'remove' : self.remove,
            'assert_md5' : self.assert_md5,
            'patch': self.patch,
            'move': self.move,
            'caption': self.caption,
            'replace': self.replace,
            'chmod': self.chmod,
            'base64' : self.base64
            }
        r = AugmentedExec(verbose = 1, myextras = dict)
        # this is a hack becuase the Python interpreter isn't up to it
        vmesg('Splitting patch')
        index = 0
        length= len(patch)
        data = ''
        percent = 0
        if length == 0:
            sys.stderr.write('Warning; zero length patch\n')
            return
        oldindex = None
        while index < length:
            if oldindex == index:
                sys.stderr.write('Badness! splitter stuck\n')
                index = index + 1
            oldindex = index
            while index < length and patch[index] == '\n':
                # skip over blank lines right away
                data = data +'\n'
                index = index + 1
            if index == length: break
            endofline = string.find(patch, '\n', index)
            if endofline == -1: endofline = len(patch)-index+1
            line = patch[index:endofline+1]
            index = index + len(line)
            
            if line == '#end of ddiff command\012':
                try:
                #print 'Statement length',len(data)
                    r.r_exec(data)
                    data = ''
                except fatal:
                    error('Fatal error due to patch')
                except:
                    # must of got confused about an incomplete
                    # segment
                    data = data + line
            else:
                data = data + line
        r.r_exec(data)      

# Finder is a tool that walks over a tree and finds all teh filenames,
# symlinks, danglies and directories in it
#
# XXX; this code is duplicated between here and ddiff.py
#
class Finder:
    def __init__(self, surpressbackups, root = './'):
        self.root = root
        self.files = {}
        self.symlinks = {}
        self.danglies = {}
        self.directories = {}
        self.surpressbackups = surpressbackups
        os.path.walk(root, self.visit, None)
    def visit(self, bogus, dirname, names):
        dirname = dirname[len(self.root):]
        self.directories[dirname] = names
        sys.stderr.write('.')
        for name in names:
            if self.surpressbackups and name[-1] == '~': continue
            if dirname:
                filename = dirname + '/' + name
            else:
                filename = name
            if os.path.isdir(filename): pass
            islink = os.path.islink(self.root + filename)
            isfile = os.path.isfile(self.root + filename)
            #print self.root+filename
            if islink and isfile: self.symlinks[filename] = 1
            if (not islink) and isfile: self.files[filename] = 1
            if islink and (not isfile): self.danglies[filename] = 1
    def newdirs(self, original):
        l = []
        directories = self.directories.keys()
        directories.sort()
        for directory in directories:
            if not os.path.isdir(original + '/'+ directory):
                l.append(directory)
        return l
    def filenames(self):
        filenames = self.files.keys()
        filenames.sort()
        return filenames

def read_archive():
    
    contents = os.listdir(patch_tree)
    # build a list of packages
    packages = []
    for file in contents:
        if file == 'RCS': continue
        if os.path.isdir(patch_tree+'/'+file):
            packages.append(file)
    packagelist = []

    # for each package
    for package in packages:
        #print 'Package ',package
        packagedir = patch_tree + '/' + package
        # build a list of brancehs
        contents = os.listdir(packagedir)
        branches = []
        for file in contents:
            if os.path.isdir(packagedir+'/'+file):
                branches.append(file)
        # for each branch
        firstbranch = None
        branchlist = []
        for branch in branches:
            print 'Branch',branch,'of',package
            branchdir = packagedir+'/'+branch
            o = open(branchdir+'/from', 'r')
            data = o.read()
            o.close()
            datae = eval(data)
            if datae == None: firstbranch = branch
            branchlist.append( (branch, datae, patchlevel(package, branch)) )
        if not firstbranch:
            error('Could not find first branch of '+package)

        if len(branchlist) > 1:
            mesg('cannot handle forks!')
            
        packagelist.append( (package, branchlist, firstbranch) )
    return packagelist




# a simple wrapper around system that informs the user what is being
# done

def system(str, verbose=1):
    if verbose: vmesg('Command '+str)
    result = os.system(str)
    if result: vmesg('Result '+`result`)
    if result == 0: return 1
    else: return 0

def normalizetime(str):
    t = rfc822.parsedate(str)
    if t: return time.mktime(t)
    return ''

def comparetime(alpha, beta):
    alphap = normalizetime(alpha)
    betap = normalizetime(beta)
    #print 'Alpha',alphap,'Beta',betap,alpha,beta
    return alphap < betap

def pad(str):
    if len(str)>35:
        return str + ' '
    return str + ((35-len(str))*' ')

def entertempdir():
    currentdir = os.getcwd()
    workdir = tempfile.mktemp()
    os.mkdir(workdir)
    os.chdir(workdir)
    print '(In temporary directory '+workdir+' )'
    return (currentdir, workdir)

def destroytempdir( (currentdir, workdir) ):
    os.chdir(currentdir)
    system('rm -rf '+shellwrap(workdir))

def prcs( command ):
    return system('PRCS_REPOSITORY='+prcs_repo+' prcs '+command )

def error( str ):
    sys.stderr.write('ERROR: '+str+'\n')
    sys.exit(1)


vmesg = mesg

def trace( str ):
    sys.stderr.write(str)

def patchlevel( packagename, branch ):
    if not os.path.isdir(patch_tree +'/'+packagename + '/' + branch):
        error('Unknown branch '+branch+' of package '+packagename)
    l = os.listdir(patch_tree + '/' + packagename + '/' + branch)
    level = 0
    numbers = {}
    for item in l:
        try:
            x = eval(item)
            if type(x) == type(1):
                numbers[x] = 1
                if x > level: level = x
        except: pass
    for i in range(1,level+1):
        if not numbers.has_key(i):
            error('Branch '+branch+' of pacakge '+packagename+' missing patch level '+`i`)
        
    return level

def checkout(file, lock=0):
    if lock:
        return system('co -l '+file)
    if os.path.exists(file): return 1
    return system('co '+file)

def checkin(file, description):
    (path,shortfile) = os.path.split(file)

    if path != '':
        if not os.path.isdir(path+'/RCS'):
            system('mkdir '+path+'/RCS')
    else:
        system('mkdir -p RCS')
    if path == '':
        command = 'ci -u '+shortfile
    else:
        command = '(cd '+path+' && ci -u '+shortfile+')'
    print command
    o = os.popen(command, 'w')
    o.write(description)
    o.write('\n\n')
    o.close()
    return 1 # happy

class Updates:
    def __init__(self, filename):
        self.filename = filename
        self.locked = 0
    def lock(self):
        vmesg('Locking '+self.filename)
        if not checkout(self.filename, lock=1):
            error('Could not lock '+self.filename)
        self.locked = 1
            
        (uid,_,_,_,gcos,_,_) = pwd.getpwuid(os.getuid())
        des = uid + ' ' + time.ctime(time.time())
        self.metadoclist = []
        self.document(uid + ' ' + time.ctime(time.time()))
        self.document(len(des)*'-')
    def document(self, str):
        if not self.locked: error('Attempt to write to non locked Updates object\n')
        vmesg('META: '+str)
        self.metadoclist.append(str+'\n')
    def release(self):
        if not self.locked: error('Attempt to write to non locked Updates object\n')
        vmesg('Releasing '+self.filename)
        try:
            o = open(self.filename , 'r')
            lines = [ o.readline(), o.readline(), o.readline() ]
            rest = o.readlines()
            o.close()
        except:
            error('Could not read metaupdates file during release')
        try:
            contents = lines + self.metadoclist + ['\n'] + rest
            o = open(self.filename, 'w')
            for line in contents:
                o.write(line)
            o.close()
        except IOError:
            error('Could not write metaupdates file during release')

        if not checkin(self.filename, ''):
            error('Could not unlock '+self.filename)        
        self.metadoclist = []
        self.locked = 0
    def abort(self):
        if not self.locked: error('Attempt to write to non locked Updates object\n')
        print 'Aborting metachange'
        if not checkin(self.filename, ''):
            error('Could not unlock '+self.filename)        
        self.locked = 0
        
def create_fork(packagename, oldbranch, newbranch, mlocked = None):
    # patch level
    if mlocked:
        updates=mlocked
        updates_mine = 0
    else:
        updates = Updates(patch_tree + '/metaupdates')
        updates.lock()
        updates_mine = 1
        
    packdir = patch_tree + '/' + packagename
    if not os.path.exists(packdir):
        error('Package '+packagename+ ' does not exist to be forked!\n')
    if os.path.exists(packdir+'/'+newbranch):
        error('Branch '+newbranch+' of '+packagename+' already exists!\n')
    if oldbranch:
        if not os.path.exists(packdir+'/'+oldbranch):
            error('Branch '+oldbranch+' of '+package+' does not exist to be forked!\n')
    os.mkdir(packdir+'/'+newbranch)
    o = open(packdir+'/'+newbranch+'/from', 'w')
    pl = None
    if oldbranch:
        pl = patchlevel(packagename, oldbranch)
        trace('Forking '+packagename+' branch '+oldbranch+' at patchlevel '+`pl`+' to form '+newbranch+'\n')
        o.write('('+oldbranch+','+`pl`+')\n')
    else:
        o.write('None\n')
    o.close()
    if pl:
        updates.document('branch("'+packagename+'", "'+newbranch+'", "'+oldbranch+'", '+`pl`+' "'+newbranch+'")')
    else:
        updates.document('branch("'+packagename+'", "'+newbranch+'")')
    if updates_mine:
        updates.release()
    
    # prcs level; no action (only get actions on checkins)

def gen_prcs_prj_file(o, packagename, branch, version=0, filedb=None, first=None):
    if filedb == None:
        #build one
        filedb = {}
        finder = Finder(surpressbackups = 1)
        for dir in finder.directories.keys():
            if dir != '':
                filedb[dir] = ':directory'
        for file in finder.files.keys():
            if not file in [packagename + '.prj', '.'+packagename+'.prcs_aux']:
                filedb[file] = ':no-keywords'
    o.write(';; -*- Prcs -*-\n')
    o.write(';; created by dcheckin.py on '+time.ctime(time.time())+'\n')
    o.write('(Created-By-Prcs-Version 1 2 8)\n') # XXX lies, damned lies
    o.write('(Project-Description "''")\n')
    o.write('(Project-Version '+packagename+' '+branch+' '+`version`+')\n')
    print 'PRCS new version ',version
    if not first:
        o.write('(Parent-Version -*- -*- -*-)\n')
    else:
        o.write('(Parent-Version '+packagename+' '+branch+' '+`first`+')\n')
    o.write('(Version-Log "Empty project.")\n')
    o.write('(New-Version-Log "Initial version.")\n')
    o.write('(Checkin-Time "'+time.ctime(time.time())+'")\n')
    o.write('(Checkin-Login unknown)\n') # fixme
    o.write('(Populate-Ignore ())\n') # ???
    o.write('(Project-Keywords)\n') # ???
    o.write('(Files\n')
    items = filedb.keys()
    items.sort()
    for item in items:
        o.write('('+item+' () '+filedb[item]+')\n')
    o.write(')\n')
    o.write('(Merge-Parents)\n')
    o.write('(New-Merge-Parents)\n')

def create_package(packagename, firstbranch):
    # patch tree level
    updates = Updates(patch_tree+'/metaupdates')
    updates.lock()
    
    try:
        packdir = patch_tree + '/' + packagename
        if os.path.exists(packdir):
            error('Package '+packagename+' already exists when creating!\n')
        print 'Created package '+packagename
        os.mkdir(patch_tree + '/'+packagename)
        updates.document('package("'+packagename+'")')
        create_fork(packagename, None, firstbranch, mlocked = updates)
        print 'Releasing metaupdates after create package'
        updates.release()
    except os.error:
        updates.abort()
        traceback.print_exc()
        error('Create package failed')
        
    print 'Package created; updating PRCS repository'

    prcs_create_initial_branch(packagename, firstbranch)

def prcs_create_initial_branch(packagename, firstbranch):
    dirs = entertempdir()
    
    print 'Creating an updates file'
    if desc_map.has_key(packagename): line = desc_map[packagename]
    else:
        line = 'Package '+packagename+'\n'

    print 'Creating initial version of project'
    
    # ...by hand!
    o = open(packagename + '.prj', 'w')
    gen_prcs_prj_file(o, packagename, firstbranch, version=0, filedb = { packagename+'_updates' : ':no-keywords'}, first=None)
    o.close()
    system('cat '+packagename+'.prj')
    o = open(packagename+'_updates', 'w')
    o.write(line)
    o.write('='*(len(line)-1))
    o.write('\n\n')
    o.close()
    prcs('populate -d -f '+packagename)
    prcs('checkin -f '+packagename)
    destroytempdir(dirs)

def commit(patchname, packagename, branchlist, replay = 0):
    if len(branchlist) == 0:
        error('No branches specified in commit')
    if type(patchname) == type(''):
        if not os.path.exists(patchname):
            error('Patch '+patchname+' not found')
    print 'Reading patch',patchname
    o = open(patchname, 'r')
    patch= o.read()
    o.close()
    # XXX; Python is broken! This raises:
    #    SystemError: com_addbyte: byte out of range
    # codel = [compile(patch, patchname, 'exec')]

    for (branch,pl) in branchlist:
        if not os.path.exists(patch_tree + '/' + packagename + '/' + branch):
            error('Branch '+branch+' of '+packagename+' does not exist')
    if not replay:
        updates = Updates(patch_tree+'/metaupdates')
        updates.lock()
    prcs_happy = 1
    
    try:
        if not os.path.isdir(patch_tree + '/' + packagename):
            error('Unknown pacakge '+packagename)
        # first check that all the branches exist and are vlaid
        branchinfolist = []
        for (branch,pl) in branchlist:
            branchdir = patch_tree+'/'+packagename+'/'+branch
            if not os.path.isdir(patch_tree + '/' + packagename + '/' + branch):
                error('Unknown branch '+branch+' of '+packagename)
            if pl == None:
                pl = patchlevel(packagename, branch)+1
            branchinfolist.append( (branch, branchdir, pl) )
        print 'Making checkins to PRCS repositories'
        print branchinfolist
        dirinfolist = []

        for (branch, branchdir, pl) in branchinfolist:
            # add to the repository (raising an exception if it is illegal
            workdir = tempfile.mktemp()
            cwd = os.getcwd()
            print '(In working directory '+workdir+')'
            (overview, workdir) = prcs_checkin(patch, packagename, branch, pl)
            os.chdir(cwd)
            dirinfolist.append( (workdir, branch, pl) )
        print 'All branches accept patch'
        prcs_happy = 0
        if not replay:
            for (branch, branchdir, pl) in branchinfolist:
                print 'Commiting to branch ',branch
                # and record the patch
                o = open(branchdir + '/' + `pl`, 'w')
                o.write(patch)
                o.close()
                updates.document('commit("'+packagename+'", "'+branch+'", '+`pl`+', "'+overview+'")')
        print 'Updating PRCS'
        for (directory, branch, pl) in dirinfolist:
            prcs_commit(directory, packagename, branch, pl)
        if not replay:
            print 'Checkin succesful'
            updates.release()
    except:
        if not replay:
            print 'Releasing metaupdates after error'
            updates.abort()
        if not prcs_happy:
            error('############################################################ PRCS REPO CORRUPTED')
        traceback.print_exc()
        sys.exit(1)
    if globals.has_key('postcheckinhook'):
        system( globals['postcheckinhook'])
        
def prcs_checkin(patch, packagename, branch, pl):
    cwd = os.getcwd()
    want = packagename+' '+branch+' '+`pl`
    print 'prcs checkin ',want
    print 'available ',checkout_collection
    if checkout_collection.has_key(want):
        print 'obtaining directory'
        workdir = checkout_collection[want]
        del checkout_collection[want]
        os.chdir(workdir)
    else:
        workdir = tempfile.mktemp()
        os.mkdir(workdir)
        os.chdir(workdir)
        # get hold of a working copy for PRCS to work with
        print 'Checkin to patchlevel ',pl
        frombranch = branch
        fromversion = '@'
        if pl == 1:
            try:
                o = open(patch_tree+'/'+packagename+'/'+branch+'/from', 'r')
            except:
                error('No from file in '+packagename+'/'+branch)
            data= o.read()
            o.close()
            try:
                fromtuple = eval(data)
            except:
                error('Corrupt from file in '+packagename+'/'+branch)
            if fromtuple:
                (frombranch, fromversion) = fromtuple
                print 'Branching from version ',fromtuple
                fromversion= `fromversion`
        if not prcs('checkout -f -r '+frombranch+'.'+fromversion+ ' '+ packagename):
            error('PRCS checkout failed')
    #system('ls -lR')
    #system('cat '+packagename+'.prj')
    i = PatchInterpreter(workdir, packagename, branch)

    print 'Executing patch'
    try:
        i.execute(patch)
    except fatal:
        error('patch failed')
    
    i.finalize(pl) # write the updates files
    os.chdir(cwd)
    print 'PRCS checkin ',packagename, branch, pl,'complete'
    return (i.ove, workdir)

def prcs_commit(dir, packagename, branch, pl):
    print 'PRCS commit ',packagename,branch,pl,' in ',dir
    cwd = os.getcwd()
    os.chdir(dir)
    # the way recommended in the PRCS doc
    prcs('populate -d -f '+packagename)
    # then we are supposed to update the new version log
    
    # my first way of doing it
    #o = open(packagename+'.prj', 'w')
    #gen_prcs_prj_file(o, packagename, branch, version=pl, filedb = None, first=pl-1)
    #o.close()
    #system('cat '+packagename+'.prj')    
    
    prcs('checkin -f')
    print 'Changing back to ',cwd
    os.chdir(cwd)
    checkout_collection[packagename+' '+branch+' '+`pl+1`] = dir
    print 'Checkout collection is now '+`checkout_collection`

def init():
    os.mkdir(patch_tree)
    print 'Creating a metaupdates file'
    if desc_map.has_key('project'):
        line = desc_map['project']
    else:
        print 'Enter project description line\n>',
        line = sys.stdin.readline()
    o = open(patch_tree+'/metaupdates', 'w')
    o.write(line)
    o.write((len(line)-1)*'-'+'\n')
    o.write('\n')
    o.close()
    checkin(patch_tree + '/metaupdates', '')    

def apply_patch(patchname, workdir):
    o = open(patchname, 'r')
    patch = o.read()
    o.close()
    i = PatchInterpreter(workdir, packagename=None, branchname=None)
    i.execute(patch)

def rebuild(rebuildtarget):
    global prcs_repo
    if prcs_repo == rebuildtarget:
        error('Specify a new empty directory for the new PRCS repository, not the old repository')
    prcs_repo = rebuildtarget
    print 'Trying to create',prcs_repo
    system('mkdir -p '+prcs_repo)
    system('rm -f '+patch_tree+'/allupdates')
    if not os.path.isdir(prcs_repo): error('Could not create '+prcs_repo)
    system('(cd '+prcs_repo+' && rm -rf *)')
    packagelist = read_archive()
    for (package, branchlist, firstbranch) in packagelist:
        print '**********************************************************************'
        print 'Package ',package
        prcs_create_initial_branch(package, firstbranch)

        # XXX; ought to do the tree ordering thing here
        (_, _, pl) = branchlist[0]

        for i in range(1,pl+1):
            patchfilename = patch_tree+'/'+package+'/'+firstbranch+'/'+`i`
            # replay mode commit
            print 'Commit level ',i,'\n\n\n'
            commit(patchfilename, package, [(firstbranch,i)], replay=1)

def publish(webfile):
    invocationarg = webfile
    if webfile in secrets.keys():
        webfile = secrets[webfile][0]
        print invocationarg, '->', webfile
    print 'Publishing ',webfile
    archive = read_archive()
    (webpath,_) = os.path.split(webfile)
    maybemkdir(webpath+'/pubpatch')
    o = open(webfile, 'w')
    o.write('<HTML>\n<HREAD>\n<TITLE> '+desc_map['project']+'; Patch Archive </TITLE>\n')
    o.write('<BODY TEXT="#000000" BGCOLOR="#FFFFFF" LINK="#0000BB" VLINK="#551A8B" ALINK="#FF0000">\n')
    o.write('<H1> '+ desc_map['project']+'</H1>\n')
    processedarchive = []
    for (package, branchlist, firstbranch) in archive:
        # lazy writing of package names
        lazy = 1
        branchlistp = []
        for (branch, datae, patchlevel) in branchlist:
            if secrets.has_key(invocationarg):
                (_, mode, specialbranches) = secrets[invocationarg]
                skip = 0
                if (mode == 'inclusive' and not ((package, branch) in specialbranches)): skip = 1
                if (mode == 'exclusive' and ((package, branch) in specialbranches)): skip = 1
                if skip:
                    print 'Skipping ',package,branch,' in ',invocationarg
                    continue
            branchlistp.append( (branch, datae, patchlevel) )
            if lazy:
                o.write('<H2> '+package+': '+(desc_map.has_key(package) and desc_map[package] or '(no description) ')+'</H2>\n<UL>\n')
                lazy = 0
                maybemkdir(webpath+'/pubpatch/'+package)
            maybemkdir(webpath+'/pubpatch/'+package+'/'+branch)
            fullpath = webpath + '/pubpatch/'+package+'/'+branch
            o.write('<LI> <H3> '+branch+'</H3> \n')
            for i in range(1, patchlevel+1):
                size = os.stat(patch_tree + '/' + package + '/' + branch + '/' + `i`)[6]
                if size < 1024: lenstr = `size`+' bytes'
                elif size < 1024*1024: lenstr = `size/1024`+'KB'
                else: lenstr = `size/(1024*1024)`+'MB'
                o.write('<A HREF="pubpatch/'+package+'/'+branch+'/'+`i`+'"> '+`i`+'</A> &nbsp; '+lenstr)
                fullfilename = fullpath + '/' + `i`
                if os.path.isfile(fullfilename) and not os.path.isfile(fullfilename):
                    sys.stderr.write('Warning; '+fullfilename+' is a file! Aborting! Perhaps this is your real patch archive; you should invoke publish with a filename of a file in a subdirectory that does not contain a directory pubpatch with files in\n')
                    sys.exit(1)
                try:
                    os.unlink(fullfilename)
                except: pass
                os.symlink(patch_tree+'/'+package+'/'+branch+'/'+`i`, fullfilename)
                if i < patchlevel: o.write(', &nbsp; &nbsp;')
        o.write('</UL>\n')
        if not lazy:
            processedarchive.append( (package, branchlistp, firstbranch) )

    o.write('summary '+`processedarchive`+'\n')
    o.write("</BODY></HTML>\n")
    o.close()

def webget(host, filename):
    from socket import *
    import time
    BUFSIZE = 1024    
    s = socket(AF_INET, SOCK_STREAM)
    try:
        s.connect(host, 80)
    except error, msg:
        sys.stderr.write('Connection failed: '+`msg`+'\n')
    s.send('GET '+filename+' HTTP/1.0\r\n\r\n')
    result = ''
    count = 0
    start = time.time()
    datalist = []
    transferred = 0
    
    while 1:
        data = s.recv(BUFSIZE)
        if not data:
            break
        datalist.append(data)
        transferred = transferred + len(data)
        if count % 256 == 255:
            if sys.stderr.isatty():
                now = time.time()
                sys.stderr.write(`(transferred, now-start, (transferred/(1024*(now - start))))`+'\n')
        count = count + 1
    index = 0

    result = string.join(datalist, '')
    while index < transferred:
        if result[index:index+4] == '\r\n\r\n':
            index = index + 4
            break
        index = index + 1

    if transferred < index:
        print 'Overrun'
        raise error
    print 'Downloaded ',transferred-index,'bytes'
    return result[index:]

def webimport(url, kind = 'web', wantpackagename = None):
    print kind,'import ',url
    if kind == 'web':
        if url[:6] == 'file:/':
            kind = 'fs'
            url = url[5:]
        else:
            if url[:7] != 'http://':
                sys.stderr.write('HTTP URL not recognised\n')
                sys.exit(1)
            endofhostname = 7+string.find(url[7:], '/')
            hostname = url[7:7+string.find(url[7:], '/')]
            path = url[endofhostname:string.rfind(url, '/')]
            indexname = url[string.rfind(url, '/')+1:]
            print hostname, path, indexname
            content = webget(hostname, path + '/'  + indexname)
    if kind == 'fs':
        o = open(url, 'r')
        content = o.read()
        o.close()
        (path, _) = os.path.split(url)
    spl = string.split(content, '\n')
    if len(spl)>4 and len(spl[-3])>10 and spl[-3][:8] == 'summary ':
        info = eval(spl[-3][8:])
    else:
        sys.stderr.write('Patch description page not recognised\n')
        sys.stderr.write(content)
        sys.exit(1)
    for line in spl:
        if line[:5] == '<H2> ':
            splitpoint = string.find(line[5:], ':')
            packagename = line[5:splitpoint+5]
            title = line[splitpoint+6:]+'\n'
            #print packagename, title
            desc_map[packagename] = title
        if line[:5] == '<H1> ':
            print 'Project ',line[:5]
            desc_map['project'] = line[5:]+'\n'
            

    if not os.path.exists(patch_tree):
        init()

    print 'Description map now '+`desc_map`
    packagelist = read_archive()

    packmap = {}
    for (package, branchlist, firstbranch) in packagelist:
        packmap[package]  = {}        
        for (branch, datae, patchlevel) in branchlist:
            packmap[package][branch] = patchlevel
    print packmap
    for (package, branchlist, firstbranch) in info:
        if wantpackagename and package != wantpackagename:
            print 'Skipping package ',package,'; we are looking for',wantpackagename
            continue
        fromlevel = None
        if not packmap.has_key(package):
            print 'No package ',package,'; creating it'
            create_package(package, firstbranch)
            packmap[package] = {}
            packmap[package][firstbranch] = 0
        # XXX; for the moment only handle one branch called live
        actions = 1
        while actions:
            actions = 0
            for (branch, setup, pl) in branchlist:
                if setup != None:
                    print 'Dont know how to handle setup',setup
                    continue
                if not packmap[package].has_key(branch):
                    print 'Dont know how to create new branch',branch
                    sys.exit(1)
                else:
                    lpl = packmap[package][branch]
                    print package,branch,'Local patchlevel ',lpl,'Remote patchlevel ',pl
                    
                    for i in range(lpl+1, pl+1):
                        if kind == 'web':
                            print 'Downloading ',package,branch,i
                            patch = webget(hostname, path + '/pubpatch/'+package+'/'+branch+'/'+`i`)
                        else:
                            o = open(path+'/pubpatch/'+package+'/'+branch+'/'+`i`)
                            patch = o.read()
                            o.close()
                        tempfilename = tempfile.mktemp()
                        o = open(tempfilename, 'w')
                        o.write(patch)
                        o.close()
                        print 'Commiting ',i                        
                        commit(tempfilename, package, [(branch, None)])
                        print 'Done ',i
                        os.unlink(tempfilename)
                        
    #print webget(hostname, path + '/patches/dpatch/live/10')
            
startdir = os.getcwd()

if len(argv) >= 4 and argv[1] == 'multibranchcommit':
    commit(argv[2], argv[3], map(argv[4:], lambda x : (x,none)))
elif len(argv) >= 4 and argv[1] == 'commit':
    for patch in argv[4:]:
        print 'Commit ',patch,'to',argv[2],'branch',argv[3]
        os.chdir(startdir)
        commit(patch, argv[2], [(argv[3], None)])
elif len(argv) == 4 and argv[1] == 'create':
    create_package(argv[2], argv[3])
elif len(argv) == 5 and argv[1] == 'fork':
    create_fork(argv[2], argv[3], argv[4])
elif len(argv) == 2 and argv[1] == 'init':
    init()
elif len(argv) == 4 and argv[1] == 'apply':
    vmesg('Patch name '+argv[2]+' workdir '+argv[3])
    system('ls -lr '+argv[3])
    apply_patch(argv[2], argv[3])
elif len(argv) == 3 and argv[1] == 'rebuild':
    if argv[2][0] != '/':
        sys.stderr.write('Use an absolute path for the PRCS repository\n')
        sys.exit(1)
    rebuild(argv[2])
elif len(argv) == 3 and argv[1] == 'publish':
    publish(argv[2])
elif len(argv) in [3,4] and argv[1] == 'webimport':
    if len(argv) == 3:
        webimport(argv[2], kind='web')
    if len(argv) == 4:
        webimport(argv[2], kind='web', wantpackagename=argv[3])
elif len(argv) == 3 and argv[1] == 'fsimport':
    webimport(argv[2], kind='fs')
else:
    sys.stderr.write(usage)
    sys.exit(1)

for directory in checkout_collection.values():
    print 'Removing working copies in',directory
    
    system('rm -rf '+directory)
