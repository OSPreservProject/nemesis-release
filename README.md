The Nemesis Operating System
=========================
Welcome to Nemesis. This document describes the licensing of Nemesis and
the procedures for obtaining it, working with it and contributing back
to it that have been developed for the Nemesis release.

Look at the [Nemesis home page](http://www.cl.cam.ac.uk/Research/SRG/netos/nemesis/)
 for news, new versions and documentation.

## The Nemesis Free License

The intent of this document is to state the conditions under which a
Package may be copied, such that the Copyright Holder maintains some
semblance of artistic control over the development of the package, while
giving the users of the package the right to use and distribute the
Package in a more-or-less customary fashion, plus the right to make
reasonable modifications.

### Definitions

“Package”

:   refers to the collection of files distributed by the Copyright
    Holder, and derivatives of that collection of files created through
    textual modification.

“Standard Version”

:   refers to such a Package if it has not been modified, or has been
    modified in accordance with the wishes of the Copyright Holder as
    specified below.

“Copyright Holder”

:   is whoever is named in the copyright or copyrights for the package.

“You”

:   is you, if you’re thinking about copying or distributing
    this Package.

“Reasonable copying fee”

:   is whatever you can justify on the basis of media cost, duplication
    charges, time of people involved, and so on. (You will not be
    required to justify it to the Copyright Holder, but only to the
    computing community at large as a market that must bear the fee.)

“Freely Available”

:   means that no fee is charged for the item itself, though there may
    be fees involved in handling the item. It also means that recipients
    of the item may redistribute it under the same conditions they
    received it.

1.  You may make and give away verbatim copies of the source form of the
    Standard Version of this Package without restriction, provided that
    you duplicate all of the original copyright notices and
    associated disclaimers.

2.  You may apply bug fixes, portability fixes and other modifications
    derived from the Public Domain or from the Copyright Holder. A
    Package modified in such a way shall still be considered the
    Standard Version.

3.  You may otherwise modify your copy of this Package in any way,
    provided that you insert a prominent notice in each changed file
    stating how and when you changed that file, and provided that you do
    at least ONE of the following:

    1.  place your modifications in the Public Domain or otherwise make
        them Freely Available, such as by posting said modifications to
        Usenet or an equivalent medium, or placing the modifications on
        a major archive site such as uunet.uu.net, or by allowing the
        Copyright Holder to include your modifications in the Standard
        Version of the Package.

    2.  use the modified Package only within your corporation
        or organization.

    3.  rename any non-standard executables so the names do not conflict
        with standard executables, which must also be provided, and
        provide a separate manual page for each non-standard executable
        that clearly documents how it differs from the Standard Version.

    4.  make other distribution arrangements with the Copyright Holder.

4.  You may distribute the programs of this Package in object code or
    executable form, provided that you do at least ONE of the following:

    1.  distribute a Standard Version of the executables and library
        files, together with instructions (in the manual page
        or equivalent) on where to get the Standard Version.

    2.  accompany the distribution with the machine-readable source of
        the Package with your modifications.

    3.  give non-standard executables non-standard names, and clearly
        document the differences in manual pages (or equivalent),
        together with instructions on where to get the Standard Version.

    4.  make other distribution arrangements with the Copyright Holder.

5.  You may charge a reasonable copying fee for any distribution of
    this Package. You may charge any fee you choose for support of
    this Package. You may not charge a fee for this Package itself.
    However, you may distribute this Package in aggregate with other
    (possibly commercial) programs as part of a larger
    (possibly commercial) software distribution provided that you do not
    advertise this Package as a product of your own.

6.  Aggregation of this Package with a commercial distribution is always
    permitted provided that the use of this Package is embedded; that
    is, when no overt attempt is made to make this Package’s interfaces
    visible to the end user of the commercial distribution. Such use
    shall not be construed as a distribution of this Package.

7.  The name of the Copyright Holder may not be used to endorse or
    promote products derived from this software without specific prior
    written permission.

8.  This package is provided “as is” and without any express or implied
    warranties, including, without limitation, the implied warranties of
    merchantibility and fitness for a particular purpose.

Overview
========

Nemesis is managed as a set of packages. Each package consists of a set
of branches. Some branches of some packages are publicly available and
some have more restricted availabilities for a variety of reasons. Each
package, by convention, has a branch called live, where the main thrust
of active development takes place. Other branches may exist as bug fixes
to stable points in the live development, or when the direction a
package is taking branches.

Several versions of Nemesis are built daily and (if they compile!), then
tarballs are made available. The most common of these is available as
the quickstart tar ball; see the section below on Quickstart to see how
to obtain and use it.

We use a custom designed system called dpatch to make changes to
Nemesis. See the section on Contributing your work back to us for
details. We use PRCS as a back end for dpatch. You may wish to set
yourself up with a Nemesis source code repository; this will make it
possible for you to submit patches back to us easily, and will make it
easier for you to upgrade to new versions of the Nemesis packages when
you wish. Some sites may already have a Nemesis source code repository
set up, and if so then you may be able to use it saving the time of
setting up your own. See the instructions below on obtaining a source
code repository.

The standard, publicly available Nemesis packages are currently:

* *ccore*:   contains the Heart Of Nemesis; the low level code, IDC system, startup, build system and so on. You always have to have a version of this.

* *tgtx86*:   contains x86 support.

* *cnet*:   contains networking support.

* *cws*:   contains windowing system support.

* *catm*:   contains ATM support.

* *caudio*:   contains audio support.

* *dpatch*:   contains the source code management scripts we use for Nemesis.

* *nemtools*:   contains the new tools necessary to build Nemesis, such as fancy linkers and IDL compilers.

* *releasemisc*:   contains the skeleton of the quickstart tar ball.

The following packages may become available in the future:

* tgtarm:   contains ARM support. This package is not publically available yet,
    pending a copyright review.

* *tgtalpha*:   contains Alpha support. This package is not publically available
    yet, pending a copyright review.

* *snet*:   will contain extended networking support from SICS.

* *gnemx*:   will contain X11 support for the windowing system in cws from Glasgow..

* *gxopen*:   will contain X/Open support from Glasgow.

Quickstart
==========

We’ve put together a fast track mechanism for you to build and evaluate
Nemesis. It just involves downloading one file, using tar to extract it
and then building it. It includes everything you need to get started.

1.  Download the quickstart tar ball:

    located [here](http://www.cl.cam.ac.uk/Research/SRG/netos/nemesis/quickstart.tar.gz)

2.  Extract the quickstart tar ball in to a scratch directory. If you
    want to use the bootfloppy mechanism (recommended for
    first-time users) then you should ensure that the scratch directory
    is in an ext2 filesystem on a primary partition of an IDE disk. The
    quickstart tar ball contains many files. They are:

    master/

    :   which contains some READMEs (including a copy of this document
        in HTML), and a makefile and script to allow you to easily build
        things without becoming familiar with the details of our
        innovative build system.

    master/nemesis/master

    :   which contains the source code for Nemesis. You can edit it to
        modify Nemesis.

    master/nemesis

    :   which apart from the source code also contains logs of what has
        changed in the source code. Furthermore, if you type
        <span>`make`</span> in the top level directory, extra
        directories will appear which are called “build trees”. They
        contain symbolic links to the source code directory
        <span>[master/nemesis/master](master/nemesis/master)</span> and
        also Nemesis binaries, as they are built. The convetion is that
        a build tree for a particular Nemesis target like intel will
        apppear in
        <span>[master/nemesis/build\_intel](master/nemesis/build_intel)</span>.

    master/tools/source/master

    :   contains the source code for the tools necessary to
        build nemesis.

    master/tools/install

    :   is where the installed copies of the Nemesis tools will go when
        you build them. Nemesis developers will normally have these
        tools on their binary path.

    master/docs

    :   contains source code for various Nemesis documents.

    master/dpatch/master

    :   contains source code for <span>`dpatch`</span>, the source code
        management system developed for Nemesis. It isn’t part of
        Nemesis and is written in python so it doesn’t need to compile.
        You may like to copy
        <span>[master/dpatch/master/ddiff.py](master/dpatch/master/ddiff.py)</span>
        and
        <span>[master/dpatch/master/dcheckin.py](master/dpatch/master/dcheckin.py)</span>
        on to your path.

    master/misc

    :   contains things that don’t fit anywhere else.

    master/misc/scripts

    :   contains a few scripts that you may find useful when working
        with Nemesis.

    master/misc/syslinux

    :   contains a compressed intel boot floppy, used to build
        Nemesis bootloaders.

    master/misc/bootfloppy

    :   is used during the creation of Intel Nemesis bootloaders.

3.  Change in to that directory. If you are not running on a standard
    Linux system or do not wish to build an intel version of Nemesis,
    edit the start of the Makefile in the top level directory to change
    the options you want to build.

4.  Type <span>`make`</span> and take a break while the Nemesis tools
    and a Nemesis image are built for you.

5.  In the subdirectory:

        nemesis/build_intel/links

    you should find a Nemesis image and some support files.

6.  Format a standard HD floppy and leave it in
    <span>[/dev/fd0](/dev/fd0)</span>. Type
    <span>`make bootfloppy`</span>; a build tree for the Nemesis
    bootloader will be created, and the bootloader will be installed on
    the floppy.

7.  The file <span>[SYSLINUX.CFG](SYSLINUX.CFG)</span> on the floppy
    contains a line which tells the bootloader how to find the
    <span>[master/nemesis/build\_intel/links](master/nemesis/build_intel/links)</span> directory.
    If you move your build tree, or want to use a new one, your should
    edit this file appropriately; instructions for this are in
    <span>[help.txt](help.txt)</span> on the floppy.

8.  When the floppy is booted you will end up running the
    Nemesis bootloader. To boot the image generated when you typed
    <span>`make`</span>, enter the command <span>`boot`</span>.

9.  If you wish to continue working with Nemesis, you should arrange to
    place the Nemesis tools on your path. They have been built as part
    of the above process for you. You need to do something like:

        PATH=/local/scratch/dr10009/release/master/tools/install/ix86_linux_rh5.1/bin:$PATH
        export PATH

10. If you want to boot Nemesis using LILO or another bootloader, or
    mount something other than a local ext2fs at boot time, you should
    read the Nemesis tutorial.

11. Read the Nemesis documentation, available as postscript or HTML from
    our web site or in the quickstart tar ball.

12. When you want to configure Nemesis, look at (eg)

        nemesis/build_intel/choices 

    and modify that file.

13. When you want to modify Nemesis, you have two choices:

    1.  You may modify the contents of nemesis/source/master or
        tools/source/master directly. Don’t edit the build directories
        (<span>[nemesis/build\_\*](nemesis/build_*)</span>), apart from
        the choices file. The build trees consist of symlinks to the
        master trees.

    2.  You may use a sparse source tree. This means that files in a new
        tree you create will override files in the quickstart tar ball.
        It is called sparse because it does not need to contain every
        file; in fact, it is best if you just copy files in to it when
        you intend to change them. This is more complicated, but saves
        you having to back up your extracted quickstart tar balls. It is
        also much easier when you abandon the quickstart tarball and use
        checkoutandgo.py instead. checkoutandgo.py is located in
        <span>[misc/scripts](misc/scripts)</span> of the quickstart
        tarball and releasemisc package, along with some
        example scripts. The idea is that you add a line to a file
        called choices in each build tree. This tells the build system
        to, whenever creating symlinks, check another directory tree for
        files and symlink to those in preference to the ones in the
        master tree. For example, if you add to
        <span>[nemesis/build\_intel/choices](nemesis/build_intel/choices)</span>
        a line like:

            add_source_tree('/usr/groups/pegasus/users/dr10009/experimentalsparsetree')

        then type <span>`make grow`</span> in
        <span>[nemesis/build\_intel](nemesis/build_intel)</span>, all
        files in
        <span>[/usr/groups/pegasus/users/dr10009/experimentalsparsetree](/usr/groups/pegasus/users/dr10009/experimentalsparsetree)</span>
        will override the master tree files. You can then copy files
        there, modify them, and just back up the sparsetree directory
        instead of the whole of the quickstart tar ball.

Frequently asked questions about the quickstart process:

-   Why do your scripts invoke gnumake?

    Nemesis builds on multiple Unix systems that have GNU
    make installed. However, GNU make is sometimes installed as
    `gnumake` (for example on OSF) and sometimes as `make` (for example
    on Linux). Most of the time, we handle such issues with platform
    specific make files, but you need to arrange that invoking gnumake
    runs a proper GNU make. Any recent version of GNU make should do. We
    use version 3.76.1.

Obtaining a Nemesis source code repository
==========================================

The quickstart tar ball is usually updated every day, so you can just
download new versions of that. However, it may be much more convenient
to set yourself up with a local Nemesis source code repository. Then,
you will be able to work with whichever version of Nemesis you like, and
with whichever combination of packages you like. Furthermore, you will
be able to use PRCS to help manage your source code.

You may already have access to a Nemesis source code repository on your
site. In the Computer Laboratory of the University of Cambridge, where
Nemesis was developed, a PRCS archive is currently available at
<span>[/local/scratch/dr10009/nemesis/PRCS](/local/scratch/dr10009/nemesis/PRCS)</span>
on all normal build machines. If you already have access to such a PRCS,
skip this section and proceed to the next section on working with this
PRCS archive.

Obtaining a source code repository will let you work with custom
versions of Nemesis, and will save on download times. We recommend you
keep source code repositories on each machine you will build on; this
really speeds up creating new build trees.

A Nemesis source code repository consists of two parts; a set of patches
in a special format called the patch archive, and a PRCS archive to make
it easy and efficient to check out particular versions of Nemesis
packages. The PRCS repository just provides the data for an easy way to
check out particular version of Nemesis; it is generates from the patch
archive. The repository you will end up with on your filesystem will of
course just be copies of the offical patch archives in Cambridge and
other places. But it will enable you to retrieve any combination of any
version of any Nemesis package.

1.  Install PRCS if you do not already have it.

2.  You will find a script called
    <span>[dcheckin.py](dcheckin.py)</span> in directory
    <span>[master/dpatch/master](master/dpatch/master)</span> of any
    Nemesis release. It handles the Nemesis source code repository. As
    well as checking in as the names would suggest, it can also perform
    several other operations on a Nemesis patch archive. Make sure it is
    executable and available to you.

3.  Configure dcheckin.py by writing a
    <span>[.dcheckinrc](.dcheckinrc)</span> file in your home directory.
    Here is an example:

        {
          'patch tree' : '/home/fred/nemesis/patches',
          'prcs repository' : '/home/fred/PRCS',
          'description map' : {
             'project' : 'The Nemesis Project at OurSiteName'
          }
        }

    Change the directory names tagged patch tree and prcs repository to
    suit your needs. The prcs repository does not need to be backed up.
    The patch tree only needs to be backed up if you start exporting
    your own packages.

4.  Download the patch repository from Cambridge by executing:

        dcheckin.py webimport http://www.cl.cam.ac.uk/Research/SRG/netos/nemesis/patches.html

    (If you believe you have access to some of the non-public branches,
    you will need to find out the alternative command for you to obtain
    the extra material).

    You may like to run this command from a CRON job. It works
    incrementally; it will just download new patches.

    (At any stage, you may destroy your patch tree and PRCS repository,
    and start again from scratch. That is unless you are exporting a
    package in the way suggested below).

    Working with Nemesis via a PRCS archive
    =======================================

    You should by now have access to Nemesis in a PRCS archive. We only
    use prcs for checking versions of Nemesis out. To contribute back to
    Nemesis, see the section below.

    A tool called checkoutandgo.py provides a useful way to checkout
    multiple PRCS packages and build them. It is located in the
    quickstart tar ball as
    <span>[master/misc/scripts/checkoutandgo.py](master/misc/scripts/checkoutandgo.py)</span>
    but we suggest you copy it somewhere else and keep it on your path.
    It used by us to generate the quickstart tar ball, for instance. See
    the build system users guide for more details, or the start of the
    source code of <span>[checkoutandgo.py](checkoutandgo.py)</span>
    where you’ll find an explanation of what it does and how it works.

    For example, suppose you want to build Nemesis in a directory called
    <span>[/anfs/scratch/hornet/dr10009/autobuild/cuttingedge](/anfs/scratch/hornet/dr10009/autobuild/cuttingedge)</span>.
    You’ve got a PRCS repository in
    <span>[/local/scratch/dr10009/nemesis/PRCS](/local/scratch/dr10009/nemesis/PRCS)</span>
    and <span>`prcs`</span> is on your path, as are built Nemesis tools.
    You also don’t need ATM support. You’d write a configuration file
    called <span>[nemesis\_test.coag](nemesis_test.coag)</span>, in your
    home directory for instance, containing:

        {
          'packages' : [
             ('prcs', 'releasemisc:live.@', '/..'),
             ('prcs', 'ccore:live.@', '/nemesis'),
             ('prcs', 'cnet:live.@', '/nemesis'),
             ('prcs', 'cws:live.@', '/nemesis'),
             ('prcs', 'caudio:live.@', '/nemesis'),
             ('prcs', 'cfs:live.@', '/nemesis'),
             ('prcs', 'tgtx86:live.@', '/nemesis'),
           ],
          'basepath' : '/anfs/scratch/hornet/dr10009/autobuild/cuttingedge',
          'actions' : """
        python quickbuild.py establish-intel ix86_linux
        echo "include('/homes/dr10009/u/mychoicesfile')" >> nemesis/build_intel/choices
        cd nemesis/build_intel
        make
        """,
          'prcsrepo' : '/local/scratch/dr10009/nemesis/PRCS',
          'postfix' : 'master'
        }

    Then, invoke (from any directory):

        checkoutandgo.py ~/coag/nemesis_test.coag

    (specifying the path to your checkoutandgo script as the argument to
    <span>[checkoutandgo.py](checkoutandgo.py)</span>, of course, and
    the path to your local PRCS repository instead of the example given
    above in the line defining `prcsrepo`).

    <span>[checkoutandgo.py](checkoutandgo.py)</span> will empty your
    build tree (specified by the `basepath` line if it exists, create
    the directory if it does not already exist, and then check out the
    versions you asked for of the packages you want, and invoke the
    `actions` script. `releasemisc` forms the framework for your build
    tree so you will always need that. `core, cfs` and a target package
    such as `tgtx86` are usually required. All other packages are
    optional bolt on extras. In the example above, this then sets up a
    intel build tree, makes it use the <span>[choices](choices)</span>
    file in
    <span>[/homes/dr10009/u/mychoicesfile](/homes/dr10009/u/mychoicesfile)</span>
    and then builds the tree. See the build system users guide for an
    explanation of choices files. The idea is that you keep the
    <span>[choices](choices)</span> file, the checkoutandgo script and
    any of your own source code safe on backed up filespace. At any
    stage, you can repeat the
    <span>[checkoutandgo.py](checkoutandgo.py)</span> command to destroy
    the build tree and start again, should the non backed up filespace
    become damaged or the build tree become confused. The actions line
    contains the commands necessary to create the Nemesis build tree. We
    work by picking configuration files close to what we need, and
    modifying them for new build trees.

5.  You may use PRCS directly to checkout, merge or diff packages
    of course. See the PRCS documentation for details. If you want to
    see exactly what versions of files you need, then look at the
    <span>[CONTENTS](CONTENTS)</span> file in the directory created by
    <span>[checkoutandgo](checkoutandgo)</span>. It gives the PRCS
    versions of each file. You may want to inspect the contents of the
    patch repository to see what has changed; see the dpatch manual
    for details.

6.  When you wish to upgrade one of the packages in a checked out tree,
    you can use <span>[prcs merge](prcs merge)</span> or <span>[prcs
    checkout](prcs
    checkout)</span>. <span>[prcs checkout](prcs checkout)</span> is
    simpler; it will merely write the new package on top of what you
    have at that time. prcs merge will interactively reconcile your
    changes with what have gone before. Alternatively, you can just
    reinvoke `checkoutandgo.py` which is event easier but means the
    entire tree will be recreated which may take some time.

Contributing your work back to us
=================================

All changes to Nemesis must be submitted to us in dpatch format, using
the patchman web interface (see the Nemesis home page). There are many
ways to generate patch files. First of all, however, make sure you are
working with the latest versions of the Nemesis packages you want
changed. The more out of date your versions, the less likely it is your
patch will still be valid and will be accepted. Alternatively, you may
try to write your patches by hand.

See the dpatch manual for details of the dpatch format; it is present in
the Nemesis quickstart tar ball.

See the [dpatch](http://www.cl.cam.ac.uk/Research/SRG/netos/nemesis/dpatch/) manual for details.

If you are modifying the contents of a Nemesis quickstart tar ball
master directory directly:

1.  Use prcs populate to inform prcs of any new files you have created.
    Make sure they go in to the .prj file in the Nemesis directory,
    corresponding to the package they should end up in. Hint; tell PRCS
    explicitly what files to add to the prj file, or it may become
    confused and try to add the contents of all the other packages to
    your file.

2.  Invoke:

        ddiff.py prcsdiff packagename > mypatch_to_packagename

    in the nemesis directory, where packagename is the name of one of
    the .prj files in that directory.

    If you modify the nemtools or dpatch, you need to run it in the
    directory containing the relevant .prj file.

If you are working with a private tree (sparse or with symlinks), for
each package you want to change:

1.  In an empty directory, check out a fresh copy of the package.

2.  Copy the files from your sparse tree you want to go in to this
    package on top of your newly checked out copy of the file.

3.  If you have created new files in the previous step, run

        prcs populate

    to let PRCS update your .prj file.

4.  Invoke:

        ddiff.py prcsdiff > mypatch_to_packagename

See the dpatch manual for more ways to use ddiff.

Now, review your patch files and upload them to us using the web
interface.

Making a new package
====================

*Warning; I haven’t checked this section is up to date for a while; mail
me if you find any problems, and of course make sure you keep your own
copies of the patch files!*

If you have some code which is new, rather than an enhancement of an
existing package, or you wish to maintain it yourself, or you cannot
persuade us to take on your changes, then you should make a new package.
First perform step 1 of “Contributing your work back to us” to set up
your patch tree. Your package will go in the same patch tree as the
patches you import from us. Then:

1.  Read about the Nemesis build system.

2.  Decide on a name for your package. For example, I’ll assume it is
    called `gnemx`. (So where you see `gnemx` insert the name of your
    new package).

3.  Create a new directory, containing `glue/packages/gnemx.py`. This
    file should contain the blueprint items for your pacakge. You might
    want to look at the corresponding files in other packages
    for inspiration.

4.  Before you go any further, test your package. It should be the case
    that if you copy the entire contents of your package directory on
    top of a release tree, your package builds and works as you intend.

5.  Back in the directory containing just the contents of your new
    package, execute:

        ddiff.py create > ~/initial_gnemx_patch

6.  Now you need to create a package and branch in your patch tree. By
    convention, the main development and initial branch is called
    `live`. So execute:

        dcheckin.py create gnemx live

    You’ll need a <span>[.dcheckinrc](.dcheckinrc)</span> file in your
    home directory to let dcheckin know where to write to; an example is
    given above. Now you’ve got a new package, you should add a
    description of your package to your
    <span>[.dcheckinrc](.dcheckinrc)</span>. Mine looks like:

        {
            'patch tree' : '/usr/groups/pegasus/nemesis/patches',
            'prcs repository': '/usr/groups/pegasus/nemesis/PRCS',
            'description map': {
                'project' : 'The Nemesis Project\n',
                'ccore' : 'The Cambridge Nemesis Core\n',
                'dpatch' : 'The dpatch Project\n',
                'catm' : 'The Cambridge Nemesis ATM subsystem\n',
                'caudio' : 'The Cambridge Nemesis Audio subsystem\n',
                'cnet' : 'The Cambridge Nemesis Network subsystem\n',
                'cfs' : 'The Cambridge Nemesis filing subsystem\n',
                'cws' : 'The Cambridge Nemesis windowing subsystem\n',
                'nemtools' : 'The Nemesis build tools\n'
             }
        }

    At this point, you have created your package and branch, but there
    isn’t any code in the package yet.

7.  You should now checkin the patch you created above. Type:

        dcheckin.py commit gnemx live ~/initial_gnemx_patch

    Make sure you give an absolute path to the name of your patch.

    At this point, you have a package with one branch and one patch. You
    can check that everything is working by inspecting the files
    `metaupdates` and `allupdates` in the patch tree, or you can type:

        prcs info gnemx

    You should see that there has been one patch in the updates file,
    and there are now two PRCS versions of your packages (the first one
    is empty).

8.  Next you should publish your package. Choose a directory for your
    patches index HTML file to appear in, and build it by typing:

        dcheckin.py publish ~/public_html/nemesis/patches.html

    In order for the patch index to work, in the directory that you have
    built the HTML file you will need to create a symlink to your patch
    tree called <span>[patches](patches)</span>. You can test this by
    making sure that you can follow the link on your patch index web
    page to your patch, using HTTP.

    If you wish, mail me `Dickon.Reed@cl.cam.ac.uk` with the URL of your
    patch archive web index. I can then arrange for it to be indexed off
    the master patch index in Cambridge.

9.  Whenever people make changes to your package they wish your package
    repository to carry, they will need to have their patches applied.
    When a patch is submitted to you, check it for semantics, then apply
    it using:

        dcheckin.py commit gnemx live ~/another_patch

    The patch will first of all be tested, and if it cannot be applied
    to the package and branch you specify then you will be told and you
    will need to edit the patch or ask whoever wrote it to regenerate it
    against the latest release. If the patch has been accepted then
    remember to reexecute:

        dcheckin.py publish ~/public_html/nemesis/patches.html

    in order to expose your patch to the world.

See the dpatch manual for more details on how dpatch works.

Your patch tree contains the canonical copy of your package and its
branches. You should make sure that at least the subdirectory containing
your package in your patch tree is regularly backed up. The other
directories in your tree will continue to be filled from Cambridge every
time you run <span>`dcheckin webimport`</span>.

Known problems
==============

-   Need comments about booting

-   We still need to integrate documentation.

-   The other packages of Nemesis need to be mentioned somewhere, as
    they become available.

-   There aren’t many platform makefiles in tools/source/master/mk

-   tools/source/master/Makefile is problematic; not all targets have
    proper SUBDIRS line.

-   The platform makefiles in the main tree are not named the same as
    the ones in the tool trees. Workaround; specify a platform in the
    main tool tree as an extra argument to quickbuild.py


