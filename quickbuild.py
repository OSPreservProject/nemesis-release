#! /usr/bin/env python

usage = """
quickbuild.py [tools|docs|docps|dochtml] [bootloader-ARCH] [ARCH] [establish-ARCH] [platform]

Invoke this program in a Nemesis quickstart tar ball like this:

python quickbuild.py intel
  to create and build an intel tree.

python quickbuild.py establish-intel
  to create an intel tree only.

python quickbuild.py tools
  to set up and build a Nemesis tool tree (needed unless you have access to a
  ready built one).

python quickbuild.py docps
  builds postscript documentation in subdirectories of the docs directory

python quickbuild.py dochtml
  builds HTML documentation in subdirectories of subdirectories of the
  docs directory using latex2html

python quickbuild.py bootloader-intel
  builds an intel bootloader.

python quickbuild.py bootfloppy
  creates a Nemesis boot floppy; make sure you have a spare floppy in your
  first floppy drive and have write permissions on /dev/fd0.


You may optionally specify an explicit architecture as the optional second
argument; if you leave one out, quickbuild.py will use arch -binv to guess.

"""


import sys, os, string

def getplatform():
    if len(sys.argv) == 3: return sys.argv[2]
    o = os.popen('arch -binv')
    archbinv = o.read()
    o.close()
    print 'arch -binv claims '+archbinv
    mkfiles = os.listdir('tools/source/master/mk')
    platforms = []
    for file in mkfiles:
        if file[-3:] == '.mk':
            if not file[:-3] in ['platform','rules']:
                platforms.append(file[:-3])
    if archbinv in platforms:
        print 'Platform makefile found for '+archbinv
        return archbinv
    print 'Known platforms are:',platforms
    print '\nPlease select nearest one, or enter tools/source/master/mk and make up your\none.\n'
    sys.stderr.write('>')
    platform = sys.stdin.readline()[:-1]
    print 'Platform',platform
    if not platform in platforms:
        print 'Unknown platform!'
        sys.exit(2)
    else:
        return platform

def maybemkdir(dir):
    if not os.path.isdir(dir):
        print 'Creating '+dir
        os.system('mkdir -p '+dir)

def link(source, destination):
    if not os.path.exists(destination) and not os.path.islink(destination):
        print source,'->',destination
        if source[0] != '/':
            (subpath, _) = os.path.split(source)
            components = string.split(subpath, '/')
            source = ('../'*len(components)) + source
        os.symlink(source, destination)

def lndir(source, destination):
    class Linker:
        def __init__(self, source, destination):
            self.source = source
            self.destination = destination
            os.path.walk(source, self.visit, None)
        def visit(self, bogus, fulldirname, names):
            directory = fulldirname[len(self.source):]
            print 'Growing',directory
            maybemkdir(self.destination+directory)
            for name in names:
                if not os.path.isdir(self.source+directory+'/'+name):
                    link(self.source+directory+'/'+name,
                         self.destination+directory+'/'+name)
    Linker(source, destination)

def gnumake(directory, target = None):
    print 'Make in ',directory,
    if not os.path.isdir(directory):
        print 'No such directory'
        sys.exit(3)
        
    if target: print 'target',target
    else: print
    command = 'make'
    if target: command = command + ' '+target
    result = os.system('(cd '+directory+' && gnu'+command+')')
    if result == 0x7f00: # returned when running an unknown command
        result = os.system('(cd '+directory+' && '+command+')')        
    print 'Result code',result

def fatalsystem(command):
    print 'Shell: '+command
    r = os.system(command)
    print 'Result: ',r
    assert not r
    
def verify_home():
    # check out that the tarball directories appear to exist
    for directory in ['tools', 'tools/source', 'tools/source/master', 'nemesis', 'nemesis/master']:
        if not os.path.isdir(directory):
            print 'Error; directory '+directory+' not found'
            print 'This tool should be run in an extracted Nemesis quickstart tarball'



arches = ['intel', 'eb164', 'shark']
targets = ['tools', 'docs', 'docps', 'dochtml', 'docclean'] + arches

def dotarget(target):

    if target == 'tools':
        verify_home()
        print 'Building a tools tree'
        platform = getplatform()    
        maybemkdir('tools/install')
        maybemkdir('tools/install/'+platform)
        maybemkdir('tools/install/'+platform+'/bin')
        maybemkdir('tools/install/'+platform+'/python')
        maybemkdir('tools/build')
        maybemkdir('tools/build/'+platform)
        lndir(source='tools/source/master',destination='tools/build/'+platform)
        link('tools/build/'+platform+'/mk/'+platform+'.mk','tools/build/'+platform+'/mk/platform.mk')
        gnumake('tools/build/'+platform)
        gnumake('tools/build/'+platform, 'install')
        sys.exit(0)


    if (target in arches
        or (target[:10] == 'bootloader' and target[11:] in arches)
        or (target[:9 ] == 'establish' and target[10:] in arches)):
        verify_home()
        result = os.system('cl -v')
        platform = getplatform()
        bootloader = (target[:10] == 'bootloader')
        establishonly = (target[:9] == 'establish')
        if bootloader:
            target = target[11:]
            print 'Building bootloader for ',target
        if establishonly:
            target = target[10:]
            print 'Establishing tree for ',target
        if result == 0x7f00:
            print 'Tools not available'
            print 'Build them by typing python quickbuild.py tools'
            print 'And place the directory tools/install/'+platform+'/bin on your path'
            if not establishonly: sys.exit(4)
        builddir = 'nemesis/build_'+target+(bootloader and '_bootloader' or '')
        maybemkdir(builddir)

        choicesfile = builddir+'/choices'
        if not os.path.isfile(choicesfile):
            o = open(choicesfile, 'w')
            if bootloader:
                o.write("""
# choices file for local bootloader - SDE

set('netterm',3)
set('UDPSocket',3)
set('ugdb',0)
set('LMPF',3)
set('dns',3)
set('netif',3)
set('arp',3)
set('network_protocols',3)
set('nfs',3)
set('flowman',3)
set('resolver',3)
set('ethde4x5',3)
set('eth3c509',3)
set('bsd_sockets',3)
set('initmount_ext2fs_ide',1)
set('initmount_nfs',0)
set('ps2',3)
set('vgaconsole',3)
set('nashmod',3)
set('cline',3)
set('nashlogin',0)
set('bootloader',1)
    """)
            else:            
                o.write('# choices file\n')
            o.close()
        cwd = os.getcwd()
        fatalsystem('(cd nemesis/master/glue && python ./layfoundations.py +platform='+platform+' '+cwd+'/'+builddir+' '+cwd+'/nemesis/master '+target+' '+cwd+'/'+builddir+'/choices)')
        if not establishonly: gnumake(builddir)
        return

    if target in ['docs']:
        print 'Documentation not yet availabe.\nLook at http://www.cl.cam.ac.uk/Research/SRG/pegasus/nemesis/documentatin.html'
        return

    if target == 'bootfloppy':
        verify_home()
        print 'Building a boot floppy; I hope you have built an intel bootloader already or this will fail'

        o = os.popen('/bin/pwd', 'r')
        data = o.readline()[:-1]
        o.close()

        pwd = data

        o = os.popen('df .', 'r')
        data = o.readlines()
        o.close()

        details = data[1]

        spl = string.split(details)

        filesystem = spl[0]
        mountpoint = spl[5]

        if mountpoint == '/':
            subpath = pwd[1:]
        else:
            subpath = pwd[len(mountpoint)+1:]

        if filesystem[:6] == '/dev/h':
            rootd = filesystem[5:8]
            rootp = eval(filesystem[8])-1
            print 'IDE disk', rootd,'Nemesis partition number ',rootp, ' fs path ', subpath
        else:
            sys.stderr.write('This filesystem (%s) is not local; you will not be able to access it from Nemesis\n' % filesystem)
            sys.exit(1)
        o = open('misc/bootfloppy/SYSLINUX.CFG', 'w')
        o.write('DEFAULT nemesis\n')
        o.write('APPEND rootd=%s rootp=%d dir=%s' % (rootd, rootp, subpath))
        o.write("""
PROMPT 1
TIMEOUT 50
DISPLAY readme.txt
F1 help.txt
""")
        o.close()
        fatalsystem('cat misc/bootfloppy/SYSLINUX.CFG')
        gnumake('nemesis/build_intel_bootloader/boot/images/intel', 'bzimage')
        fatalsystem('cp nemesis/build_intel_bootloader/boot/images/intel/bzimage misc/bootfloppy/Nemesis')
        fatalsystem('(cd misc/syslinux && gzip -d -c syslinux1440.gz | dd of=/dev/fd0 bs=1024)')
        fatalsystem('(cd misc/bootfloppy && mcopy * a:)')
        fatalsystem('mdir a:')
        return

    if target[:3] == 'doc':
        def genps(filename):
            fatalsystem('(cd docs/'+filename+' && latex '+filename+')')
            fatalsystem('(cd docs/'+filename+' && latex '+filename+')')
            # we do it this way becasue most dvips's send docs to the
            # printer by default
            fatalsystem('(cd docs/'+filename+' && dvips -f < '+filename+'.dvi > '+filename+'.ps)')
        def genhtml(filename):
            fatalsystem('(cd docs/'+filename+' && latex2html -split 0 -toc_depth 3 '+filename+')')
        spl = string.split(target, '-')
        if len(spl)== 1:
            filenames = os.listdir('docs')
        elif len(spl) >= 2:
            filenames = [string.join(spl[1:],'-')]
        else:
            filenames = []
        print 'doc mode ', filenames
        for filename in filenames:
            print 'Looking for ',filename
            if os.path.isdir('docs/'+filename):
                print 'Processing document ',filename
                print target[:5], target[:7]
                if target == 'docs' or target[:5] == 'docps':
                    print 'Invoking latex'
                    genps(filename)
                if target == 'docs' or target[:7] == 'dochtml':
                    print 'Invoking latex2html'
                    genhtml(filename)
                if target == 'docclean':
                    fatalsystem('(cd docs/'+filename+' && rm -rf *.dvi *.ps *.log *.aux *.toc '+filename+'/ )')
        return

    # fall through to the usage message
    sys.stderr.write(usage)
    sys.exit(1)


    

if len(sys.argv) == 1:
    sys.stderr.write(usage)
    sys.exit(1)
dotarget(sys.argv[1])
