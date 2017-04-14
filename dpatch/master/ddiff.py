#! /usr/bin/env python

"""
ddiff; generate dpatch files showing differences between different source
kinds of source trees.

Unimplemented so far:

paranoia
movements

"""

usage = """
ddiff generates dpatch files describing the changes between two source trees.
It can be invoked in one of several ways, depending on the kind of private tree
in use.

ddiff.py [options] prcsdiff [prcs_diff_options]

  invoke prcs diff in the current directory and create a patch.
  The PRCS_REPOSITORY
  environment variable should be set.

ddiff.py [options] convert [diff_filename]
 
  read the the unified diff diff_filename or from stdin and create a dpatch.

ddiff.py [options] create

  current directory is the root of a new project. The entire contents of
  the directory are added to the patch.

ddiff.py [options] privlink master_tree 

  current directory is assumed to be a tree of symlinks to a master tree,
  with some symlinks moved around and some replaced with local copies of
  files.

ddiff.py [options] privcopy master_tree

  current directory is assumed to contain a copy of the master tree, with some
  new files and some files modified. Note that no move commands can be
  generated in this mode. Files identical between the two trees will not
  be incorporated in to the new tree.

ddiff.py [options] deltas master_tree

  current directory is assumed to contain a copy of the master tree, perhaps
  with some new files. The patch emitted, however, only contain deltas; new
  files in the current tree are ignored.

ddiff.py [options] sparse master_tree

  current directory is assumed to contain a sparse copy of the master tree.
  Note again that moves cannot be created in this mode.


Options are:

 -B  batch mode; do not prompt for documentation, insert placeholders instead.

 -p  paranoid mode, where every file that is modified has an MD5 sum
     in the patch file

 -P  ultra paranoid mode, where every file in the master tree has an
     MD5 sum in the patch file

 -n  do not read the ~/.ddiffrc file.

 -b  do not surpress backup files (anything ending in ~) and python
     cached binaries (anything ending in .pyc)

 -c  escape non newline control codes

 -8  do not base64 strings with top bits set

 -v  produce verbose tracing on stderr

 -l  limit

     approximate maximum number of bytes to place in file
 
 -f filename

     create patch in file filename. If -l is specified as well, split in
     to several files with underscores and numbers added to end.

 -s pathcomponents

     in convert mode, strip pathcomponents number of leading path components
     in order to find filename.

"""


import sys, time, os, string, md5, base64

# the real start of the file
# globals

paranoia = 0
adduserrc = 1
surpressbackups = 1
eightencapsulate = 1
escapecontrol = 0
verbose = 0
limit = None
filename = None
outobj = sys.stdout
#exceptions
stringify_broken = 'stringfiy_broken'
fatal = 'fatal'
magic = '#end of ddiff command\n'
strip = 2
interactive = 1

def popen(command):
    sys.stderr.write('Executing '+command+' in to pipe\n')
    return os.popen(command)

def showusage():
    sys.stderr.write(usage)
    sys.exit(1)

def error(str):
    sys.stderr.write('ERROR: '+str+'\n')
    sys.exit(1)

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
            if self.surpressbackups and (name[-1] == '~' or name[-4:] in [ '.pyc', '.bak']) : continue
            if dirname:
                filename = dirname + '/' + name
            else:
                filename = name
            if os.path.isdir(filename): pass
            islink = os.path.islink(filename)
            isfile = os.path.isfile(filename)
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

def getdocstring(firstlen, maxlen):
    sys.stderr.write('Comment? REDO/ABORT/(ctrl D, \\n\\n or .)?\n')
    doc = ''
    firstline = 1
    lastempty = 0
    while 1:
        if firstline: maxlength = firstlen
        else: maxlength = maxlen
        sys.stderr.write('  |'+('-'*(maxlength-3))+'|\n')
        sys.stderr.write('> ')
        line = sys.stdin.readline()[:-1]
        if (line =='\n' and lastempty) or line == '.\n' or len(line) == 0:
            break
        if line == '\n' and not lastempty:
            lastempty = 1
            continue
        
        if line == 'REDO\n':
            sys.stderr.write('Redo typed! Start again\n')
            firstline = 1
            doc = ''
            continue
        if line == 'ABORT\n':
            sys.stderr.write('Abort typed!\n')
            sys.exit(1)
        if len(line)>1: lastempty = 0
        if len(line)>maxlength:
            sys.stderr.write('Line too long (maximum length '+`maxlength`+')\n')
            sys.stderr.write('Retype from start\n')
            firstline = 1
            doc = ''
            continue
        if len(doc)>0: doc = doc + '\n'
        if lastempty: doc = doc + '\n' # belatedly add newline
        doc = doc + line
        firstline = 0
    sys.stderr.write('Document string ['+doc+']\n')
    return doc

class Dpatch:
    def __init__(self):
        self.commands  = []
        self.author = None
        self.authoremail = None
        self.overview = None
        # format for commands is (command name, documentation, extra args tuple)*
        if adduserrc:
            filename = os.path.expanduser('~/.ddiffrc')
            if os.path.isfile(filename):
                o = open(filename, 'r')
                data = o.readline()
                o.close()
                try:
                    (self.author, self.authoremail) = eval(data)
                except:
                    sys.stderr.write('Could not parse ~/.ddiffrc\n')
    def command(self, command_name, extra_args_string):
        if interactive:
            if command_name == 'create':
                # we don't show the entire contents of a file in create
                # mode because it blows out the scrollback
                spl = string.split(extra_args_string, ',')
                sys.stderr.write('Create filename '+spl[0]+'\n\n')
            else:
                sys.stderr.write('Command '+command_name+' arguments '+extra_args_string+'\n\n')                
            if command_name in ['patch', 'create', 'mkdri', 'rmdir', 'replace']:
                doc = getdocstring(35, 78)
            else:
                doc = getdocstring(78, 78)
        else:
            doc = 'describe me'
        self.commands.append( (command_name, doc, extra_args_string) )
    def output(self, outobj):
        if interactive:
            if not self.author or not self.authoremail:
                sys.stderr.write("""
Hint: you can avoid having to type your name and email address by writing a
file called ~/.ddiffrc containing something like the single line:
("Dickon Reed", "dr10009@cam.ac.uk")
""")
            if not self.author:
                sys.stderr.write('Your name?\n')
                self.author = getdocstring(78,2)
            if not self.authoremail:
                sys.stderr.write('Your email address?\n')
                self.authoremail = getdocstring(78,2)
            if not self.overview:
                sys.stderr.write('An overview of this entire change?\n')
                self.overview = getdocstring(78, 78)
        else:
            if not self.author: self.author = 'Anonymous'
            if not self.authoremail: self.authoremail = 'no email'
            if not self.overview: self.overview = 'no overview'
        outobj.header('date("'+time.ctime(time.time())+'")\n')
        outobj.header('author_name("'+self.author+'")\n')
        outobj.header('author_email("'+self.authoremail+'")\n')
        outobj.header('overview('+stringify(self.overview)+')\n')
        for (command_name, doc, extra_args_str) in self.commands:
            #sys.stderr.write('Command '+`command_name`+' string '+`doc`+'\n')
            outobj.write(command_name+'('+stringify(doc)+','+extra_args_str+')\n')
            outobj.write(magic)
            outobj.write('######################################################################\n')
        
class LimitedStdoutOutput:
    def __init__(self, bytelimit):
        self.count = 0
        self.bytelimit = bytelimit
    def write(self, str):
        self.count = self.count + len(str)
        if self.bytelimit and self.count > self.bytelimit:
            sys.stderr.write('Patch too big!\n')
            raise fatal
        sys.stdout.write(str)
    def header(self, str):
        self.write(str)
    def finish(self):
        pass
    
class MultiFileOutput:
    def __init__(self, bytelimit, filename):
        self.count = 0
        self.bytelimit = bytelimit
        self.part = None
        self.data = ''
        self.filename = filename
        self.head = ''
    def fmap(self):
        return ('%s_%.3d' % (self.filename, self.part))
    def write(self, str, forcewrite = 0):
        self.count = self.count + len(str)
        if forcewrite or self.bytelimit and self.count > self.bytelimit:
            self.count = 0
            if not self.part: self.part = 0
            self.part = self.part + 1
            o = open(self.fmap(), 'w')
            o.write(self.head)
            o.write(self.data)
            o.close()
            sys.stderr.write('Wrote '+`self.fmap()`+' '+`len(self.data)`+'\n')
            self.data = ''
            self.count = len(self.head) + len(str)
            self.data = ''
        self.data = self.data + str
        #sys.stderr.write(`self.count`+',')
    def header(self, str):
        self.head = self.head + str
        self.count = self.count + len(str)
    def finish(self):
        self.write('', forcewrite=1)
            

def encapsulate_test(prev, char):
    return (
        ord(char)>=128 or
#        char == '\\' or
        (char == '0' and prev == '\\') or
        (escapecontrol and ord(char)<32  and ord(char) != 13)
    )


def stringify(str):
    # hack; if it is too big to read easily, use the built in stringify
    if len(str) > 16384: return `str`
    def insert(str):
        return '"""+'+str+'+r"""'

    if str == '': return "''"
    encapsulate = 0
    prev = None
    if eightencapsulate and 0:
        for char in str:
            if encapsulate_test(prev, char):
                #sys.stderr.write('Encapsulating due to characeter '+`ord(char)`+'\n')
                encapsulate = encapsulate + 1
            prev = char
    if encapsulate > 256: # XXX; hard coded
        sys.stderr.write(`encapsulate`+' special characters; base 64 encoding instead\n')
        return 'base64("""\n'+base64.encodestring(str)+'""")'
    if 0:
        # short string mode
        outstr = '("'

        for char in str:
            if char == '\\': char = r"""\\"""
            elif char == '"': char = '\\"'
            elif char == '\n': char = '"+'+"'\\n'"+'+\n"'
            elif char == '\t': char = '\\t'
            elif ord(char)<32 or ord(char)>127: char = '\\x'+hex(ord(char))[2:]
            outstr = outstr + char
        outstr = outstr + '")'
    elif 1:
        #sys.stderr.write('longstr ')
        
        # long string mode
        outstr = '"""'
        index = 0
        while index < len(str):
            char = str[index]
            if char == '\\': char = r"""\\"""
            elif char == '"': char = '\\"'
            elif char == '\t': pass #char = '\\t'
            elif char == '\n': pass
            elif (ord(char)<32 or ord(char)>127): char = `char`[1:-1]
            outstr = outstr + char
            index = index + 1
                
        outstr = outstr + '"""'
    else:
        # long restricted string mode
        outstr = 'r"""'
        index = 0
        while index < len(str):
            if str[index:index+3] == '"""':
                outstr = outstr + insert("'"+'"""'+"'")
                index = index + 3
            else:
                if index == 0: prev = None
                else: prev = str[index-1]
                if encapsulate_test(prev, str[index]):
                    substr = 'chr('+`ord(str[index])`+')'
                    sys.stderr.write(`(str[index],substr,insert(substr))`)
                    outstr = outstr + insert(substr)
                else:
                    outstr = outstr + str[index]
                index = index + 1
        outstr = outstr + '"""'
    #sys.stderr.write(outstr)
    #sys.stderr.write(' eval ')
    evalout = eval(outstr)
    if evalout != str:
        sys.stderr.write(`('versions differ',evalout,str)`)
        index = 0
        while index < len(str):
            if evalout[index] != str[index]:
                sys.stderr.write(`('differ at index',index,evalout[index],str[index])`)
            index = index + 1
        raise stringify_broken
    #sys.stderr.write('*'*80+'\n')
    #sys.stderr.write('done\n')
    return outstr

def strtohex(x):
    outstr = ''
    for byte in x:
        y = ord(byte)
        outstr = outstr + ("%.2x" % y)
    return outstr

def assertmd5(file, data):
    m = md5.new()
    m.update(data)
    digest = m.digest()
    digeststr = strtohex(digest)
    outobj.write('assert_md5("'+file+'", "'+digeststr+'")\n')
    

def prefix(string, x): return ( len(string) >= len(x) and string[:len(x)] == x)



    
args = sys.argv[1:]

while len(args)>0 and args[0][0] == '-':
    if args[0][1] == 'p': paranoia = 1
    elif args[0][1] == 'P': paranoia = 2
    elif args[0][1] == 'n': adduserrc = 0
    elif args[0][1] == 'b': surpressbackups = 0
    elif args[0][1] == 'B': interactive = 0
    elif args[0][1] == 'c': escapecontrol = 1
    elif args[0][1] == '8': eightencapsulate = 0
    elif args[0][1] == 'v': verbose = 1
    elif len(args)>=2 and args[0][1] == 'l':
        limit = eval(args[1])
        args = args[1:]
    elif len(args)>=2 and args[0][1] == 'f': 
        filename = args[1]
        args = args[1:]
    elif len(args)>=2 and args[0] == '-s':
        strip = eval(args[1])
        args = args[1:]
    else:
        showusage()
    args = args[1:]

#sys.stderr.write('After decoding options, arguments are '+`args`+'\n')
#sys.stderr.write('Patch size limit '+`limit`+' filename '+`filename`+'\n')

if filename:
    outobj = MultiFileOutput(limit, filename)
else:
    outobj = LimitedStdoutOutput(limit)


mode = None
original = None
movement = []
changed = []
newdirs = []

if len(args) == 0: showusage()


dpatch = Dpatch()

if args[0] in ['convert', 'prcsdiff']:
    if args[0] == 'prcsdiff':
        command = 'prcs diff '+string.join(args[1:])
        o = os.popen('prcs diff '+string.join(args[1:])+' 2>&1')
        lines = o.readlines()
        o.close()
    elif len(args) == 1:
        sys.stderr.write('Engaging non-interactive mode because patch piped in from stdin\n')
        interactive = 0
        lines = sys.stdin.readlines()
    elif len(args) == 2:
        o = open(args[1], 'r')
        lines = o.readlines()
        o.close()
    else:
        showusage()
    index = 0
    patches = []
    creates = []
    removes = []
    from_branch = None
    to_branch = None
    # states:
    #  0; not in the middle of any command
    #  1; in the middle of a patch
    
    state = 0
    start_of_patch = None
    patch_file = None
    patch_ignore = 0
    # parse the patch
    while index <= len(lines):
        if index < len(lines):
            line = lines[index]
        else:
            line = ''
        cmdtype = None
        #sys.stderr.write('Line: '+`line`+'\n')
        if len(line)>8 and line[:7] == 'Index: ':
            cmdtype = 1
            patch_ignore = 0
        if len(line)>9 and line[:8] == 'Only in ':
            if line[-3:-1] == ': ':
                line = line[:-1] + lines[index+1]
                index = index + 1
            cmdtype = 2
            commandstr = line
        if len(line)>28 and line[:27] == 'prcs: Producing diffs from ':
            cmdtype = 3
        path = None
        if cmdtype in [1,2]:
            colonloc = string.find(line, ':')
            path = line[colonloc+2:-1]
            # strip off the branch names that some versions of prcs diff
            # tack on to the start of things
            if from_branch and to_branch:
                for branchname in [from_branch, to_branch]:
                    if path[:len(branchname)+1] == branchname + '/':
                        path = path[len(branchname)+1:]
                        sys.stderr.write('Truncating path to %s\n' % (path) )
                        break
            spl = string.split(path, '/')
            if len(spl)>2 and spl[1] == 'master':
                path = string.join(spl[2:], '/')
                patch_ignore = 0
            elif not(len(path) > 7 and path[:7] == 'master/'):
                sys.stderr.write('Ignoring top level file '+path+'\n')
                patch_ignore = 1
            else:
                path = path[7:]
                patch_ignore = 0
                #sys.stderr.write('Path '+`path`+'\n')
                    
        if (state == 1 and (cmdtype or (index == len(lines)))):
            # reached end of patch
            # patch extends from start_of_patch+1 to index-1
            sys.stderr.write('patch: file %s length %d patch_ignore %d\n' % ( patch_file, len(lines), patch_ignore))
            if not patch_ignore: patches.append( (patch_file, lines[start_of_patch+1:index]) )
            state = 0
        if state == 0:
            start_of_patch = None
            # not in the middle of any command
            if cmdtype == 1 and not patch_ignore:
                state = 1
                start_of_patch = index
                patch_file = path
            elif cmdtype == 2:
                versnum = string.split(commandstr, ':')[0]
                #sys.stderr.write('\ncreate or remove version numb '+`versnum`+' command string '+`commandstr`+'\n')
                if versnum[-3:] == '(w)':

                    if not os.path.exists('master/'+path):
                        error('PRCS mentions file master/'+path+' that does not exist')
                    if os.path.isfile('master/'+path):
                        o = open('master/'+path, 'r')
                        data = o.read()
                        o.close()
                        creates.append( (path, data) )
                    else:
                        error('PRCS mentions file master/'+path+' that is actually a directory')
                else:
                    # only present upstream
                    removes.append( path )
                    if os.path.exists('master/'+path):
                        error('PRCS incorrectly states states that master/'+path+' does not exist')
            elif cmdtype == 3:
                spl = string.split(line[27:-2])
                if len(spl) != 3:
                    sys.stderr.write('Could not parse prcs: Producing diffs line\n')
                    raise fatal
                from_branch = spl[0]
                to_branch = spl[2]
                sys.stderr.write('PRCS diff from branch '+from_branch+' to branch '+to_branch+'\n')
            elif cmdtype == None and index < len(lines):
                sys.stderr.write('Ignoring garbage '+line)
        index = index + 1
        
        
    for (path, patch) in patches:
        dpatch.command('patch', ('"'+path+'", '+stringify(string.join(patch,''))))
    for (path, data) in creates:
        dpatch.command('create', ('"'+path+'", '+stringify(data)))

    for (path) in removes:
        dpatch.command('remove', ('"'+path+'"'))
else:

    finder = Finder(surpressbackups)

    if verbose: sys.stderr.write('finder files  '+`finder.files`+' directories '+`finder.directories`+'\n')

    # we treat privlink and sparse the same, because finder.files does not
    # contain symlinks (they are in finder.symlinks and finder.danglies)
    #
    # and because null patches get omitted below, we don't need to do anything
    # different for privcopy mode either
    #
    # XXX; do we really want to make them different options on the command line
    # privcopy could probably be optmised differently

    if len(args) == 2 and args[0] in ['privlink', 'sparse', 'privcopy', 'deltas']:
        original = args[1]
        mode = args[0]
        changed = finder.files.keys()
        newdirs = finder.newdirs(original)

    if len(args)==1 and args[0] == 'create':
        original = None
        mode = 'create'
        changed = finder.files.keys()
        newdirs = finder.directories.keys()
        newdirs.sort()

    if not mode: showusage()

    # at this stage we require:
    #  that the CWD is the root of the private tree
    #  that original is the path to the master tree
    #  that movement is a list of (from, to) pairs of moving files
    #  that paranoia is zero to indicate that no md5 sums are generated,
    #  1 to indicate that md5 sums for patched files should be generated and
    #  2 to indicate that md5 sums for the whole of the master tree should be
    #  generated
    #  that changed is a list of subpaths to modified files
    #  that newdirs is a list of directores to create

    # order is do paranoia level 2, make directories, move files, patch files

    #sys.stderr.write('Starting main diff; mode='+`mode`+' paranoia '+`paranoia`+' changed '+`changed`+' newdirs '+`newdirs`+' movement '+`movement`+'\n')

    changed.sort()
    # now do the paranoia level 2
    if paranoia == 2 and original:
        masterfinder = Finder(0, original)
        filenames = masterfinder.files.keys()
        filenames.sort()
        for file in filenames:
            o = open(original + '/' + file, 'r')
            data = o.read()
            o.close()
            assertmd5(file, data)
    sys.stderr.write('\n')
    # make directories
    if not mode == 'deltas':
        for dir in newdirs:
            dpatch.command('mkdir', '"'+dir+'"')

    for file in changed:
        sys.stderr.write('('+file+')\n')
        if paranoia == 1 and original:
            if os.path.isfile(original + '/'+ file):
                o = open(original+'/'+file, 'r')
                data = o.read()
                o.close()
                assertmd5(file, data)
        if original and os.path.isfile(original+'/'+file):
            o = popen('diff -u '+original+'/'+file+' '+file)
            patch = o.read()
            o.close()
            if len(patch)>0:
                dpatch.command('patch', '"'+file+'", '+stringify(patch))
        else:
            if mode != 'deltas':
                o = open(file, 'r')
                contents = o.read()
                o.close()
                try:
                    stri = stringify(contents)
                    if stri[:6] == 'base64':
                        sys.stderr.write('Encoding binary '+file+'\n')
                    dpatch.command('create', '"'+file+'", '+stri)
                except stringify_broken:
                    sys.stderr.write('Could not stringify '+file+'\n')
                    raise fatal

dpatch.output(outobj)
outobj.finish()

