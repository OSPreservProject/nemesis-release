
"""

dpatch patch manager

The idea is to allow arbitary users to upload patches, to have the
system automatically try and apply them, let the users know if their
patch does not apply, and so on.

Basic idea; users submit a patch using a form where they specify a URL and
their email address, then patchman trots off and tries to apply it. If it
does apply, it sits there in an repository of incoming patches waiting
for the tsar to apply it or bounce it. If it doesn't apply, it gets
bounched automatically

We user multiuser.py to do the authentication for us.

There are four kinds of user:

The secretary general, who runs the whole system. (me!)
The secretaries, who appoints the Tsars.
The tsars, who are Tsar for a number of branches each. A branch Tsar
may fork the branch, admit patches to the branch and reject patches and move
patches to different branches.
Regular users may examine pending patches, upload new patches, withdraw their
patches.

patchman stores in package/branch directories:

- pending patches, as email:patch number
- tsar lists, in a file called Tsars
- readership lists, in a file called Members
- validiting list, in a file called Valid
format is lines of:
email:(patch number) (patchlevel) (1 or 0)
a 1 means that the patch applies okay against patchlevel, a 0 means it
fails
- validator process ID, in a faile called Validator


In the root of the patchman database, there is a file called Secretaries
storing the email addresses of Secretaris. Another file called SecretaryGeneral
stores the email address of the Secretary General.

TODO:

escape < and > as &lt; and &gt;
"""


from HTMLgen import *
import sys, urllib, thread, os,time, tempfile, multiuser
# XXX; make these come from somewhere sensible
docrc = 'patchman.rc'
patch_tree = '/usr/groups/pegasus/nemesis/patches'
prcs_repo = '/usr/groups/pegasus/nemesis/PRCS'
contrib_dir = '/usr/groups/pegasus/users/dr10009/patchman'
passwordfile = '/usr/groups/pegasus/users/dr10009/patchman/passwd'
bad_username = 'bad_username'
always_show_all_branches = 1

if not os.environ.has_key('PRCS_REPOSITORY'): os.environ['PRCS_REPOSITORY'] = prcs_repo


def normalize_string(notes):
    notes2 = ''
    if notes:
        for char in notes:
            if not char in string.letters+string.digits+'_@.':
                notes2 = notes2 + '_'
            else:
                notes2 = notes2 + char
    return notes2

def check_filename(filename):
    if type(filename) != type(''): return 0
    if '/' in filename: return 0
    for char in filename:
        if not char in string.digits + string.letters + '_': return 0
    return 1
            
def system(command):
    command = command + ' 2>&1'
    sys.stderr.write('Command: %s\n' % command)
    o = os.popen(command, 'r')
    output = o.read()
    result = o.close()
    sys.stderr.write(output+'\n')
    sys.stderr.write(`result`+'\n')
    return (result,output)

def assert_username(str):
    allowed = string.letters + string.digits + '_@.'
    for char in str:
        if not char in allowed:
            raise bad_username, (str, char)

def read_user_list(filename):

    o = open(filename, 'r')
    lines = o.readlines()
    o.close()
    users = []
    for line in lines:
        user = line[:-1]
        assert_username(user)
        users.append(user)
    #print 'Reading user list from '+filename,'users',users
    return users

def message_page(text, url, title='patchman error', outmsg='Back to patchman', instant = 0):
    if not instant:
        doc = SimpleDocument(docrc)
        doc.title = title
        doc.append(text)
        doc.append(HR())
        doc.append(Href(url, 'Back to patchman'))
        return str(doc)
    else:
        return multiuser.teleport(url, '<HR> '+text+'<P><HR>')

class Account:
    """ patchman Account objects """
    def __init__(self, user, md5sum, patchman):
        self.guest = (user == 'guest')
        self.user = user
        self.patchman = patchman
        self.patchman.members[user] = self
        self.url = '/patchman/users/'+md5sum

    def issecretary(self):
        if self.user in (patchman.secs + patchman.secgens):
            return 1
        else:
            return 0

    def getinfo(self):
        # return:
        # changeables: a list of branches that can be chagned by this user
        # branches, a list of all branches visible to this user
        # under_validation, a list of branches under validation
        # branchcontents, a mapping of files in each branch
        # patchlevel, a mapping of branches to patchlevels
        changeables = self.branches_changeable()
        branches = self.branches_accessible()
        under_validation = []
        branchcontents = {}
        branchvalidations = {}
        patchlevel = {}
        branches.sort()
        o = os.popen('rlog -h '+patch_tree+'/metaupdates')
        lines = o.readlines()
        o.close()
        x = 0
        while x < len(lines) and lines[x] != 'locks: strict\n': x = x + 1
        
        if lines[x+1][0] in ' \t':
            dcheckin_active = 1
        else:
            dcheckin_active = 0
            
        for branch in branches:
            spl = string.split(branch, ':')
            # look through the dpatch archive to figure out patch level
            path = patch_tree+'/'+spl[0]+'/'+spl[1]
            patchmancontents = os.listdir(path)
            i = 1
            while `i` in patchmancontents: i = i + 1
            patchlevel[branch] = i
            validations = {}
            # look through the patchman repository to figure out patch level
            branchcontents[branch] = contents = os.listdir(contrib_dir+'/'+branch)
            if 'Validator' in contents: under_validation.append(branch)
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
            branchvalidations[branch] = validations

        return changeables, branches, under_validation, dcheckin_active, branchcontents, branchvalidations, patchlevel
    
    def index_html(self):
        """ invoke me """

        changeables, branches, under_validation, dcheckin_active, branchcontents, branchvalidations, patchlevel = self.getinfo()
        
        self.patchman.log( (self.user, 'visited patchman'))
        doc = SimpleDocument(docrc)
        doc.title = (self.issecretary() and 'Secretary ' or '') +  self.user + ' patchman account'
        doc.append('<H1> '+doc.title+' </H1>')
        doc.append("""Patchman is a forum for submitting new dpatch format patches (as generated by ddiff.py) to the packages listed below.""")
        doc.append(P())
        doc.append("""
To upload a new patch, point your browser at the file containing the patch, select the branch and hit submit. Optionally, you may briefly describe the patch.""")
        doc.append(P())
        doc.append(""" You should realise that the file you upload will be identified with you and is visible to the world, or in the case of a non-public branch to all developers of that branch. """)
        doc.append(HR())
        doc.append('<H2> Patchman News </H2>')
        doc.append(P())
        doc.append("""Patch validation service believed to be fixed. Please report any problems with patch validation to Dickon.Reed@cl.cam.ac.uk""")
        #doc.append(""" There is now an experimental patch validation service running. Within a few minutes (depending on load) of you uploading a patch, you should find a report of an attempt to verify the patch. Sometimes valid patches will be marked invalid. This is under investigation. """)
        doc.append("""You may use verifypatch to test whether Nemesis builds after a patch has been applied. Invoke pegasus/nemesis/misc/verifypatch with the filename of a patch as an argument. A file of the same name as the patch but with buildlog will be tacked on to the end of the file. Currently this only works on SRG linux boxes. If you have an account in Cambridge and are a member of group pegasus, you may invoke this on a patch in the patchman archive (/usr/groups/pegasus/nemesis/incoming), causing patchman to show everybody the buildlog.""")
        doc.append(HR())
        # XXX; multipart/form-data is required for file upload to work
        doc.append('<H2> Patch upload </H2>')
        f = Form('submit', enctype = 'multipart/form-data')
        f.append(Input(type='file', name='patchdata', llabel = 'Patch filename: '))
        f.append(Input(type='text', name='notes', llabel = 'Notes: '))
        f.append('Branch: ')
        f.append(Select(['select a branch']+branches, name='branch', size=1))
        f.submit = Input(type='submit', name='submit', value='submit')
        doc.append(f)
        doc.append(HR())
        doc.append('<H2> Archive status </H2>')
        if len(changeables)>0:
            doc.append('Welcome Tsar<p>')
            if dcheckin_active:
                doc.append(Strong('dpatch archive is in the process of being updated; checkins and forks are not possible right now'))

        for branch in branches:

            titlestr = '<H5> '+(self.patchman.acl[branch] and 'Restricted ' or 'Public ') + 'branch '+ branch + ' (patchlevel '+`patchlevel[branch]-1`+')'
            if branch in changeables:
                titlestr = titlestr + str(Href('start_validation?branch='+branch, ' kickstart ')) + '<P>'

            titlestr = titlestr + ' </H5> '
            
            if always_show_all_branches:
                doc.append(titlestr)
                donetitle = 1
            else:
                donetitle = 0
            if branch in under_validation:
                donetitle = 1
                doc.append('Patch validation is in progress<p>')
            patches = []
            validations = {}
            contents = branchcontents[branch]
            for patch in contents:
                spl = string.split(patch, ':')
                if len(spl) == 2:
                    if not donetitle:
                        doc.append(titlestr)
                        donetitle = 1
                    stuff = str(Href('view?branch='+branch+'&patch='+patch, patch))
                    validations = branchvalidations[branch]
                    if validations.has_key(patch):
                        levels = validations[patch].keys()
                        levels.sort()
                        for level in levels:
                            stuff = stuff + ' ' + (validations[patch][level] and 'valid' or 'invalid') + ' wrt '+level + ','
                    if patch + ':log' in contents:
                        stuff = stuff + ' ' + str(Href('view?branch='+branch+'&patch='+patch+':log', 'view patch application log'))
                    if patch + ':buildlog' in contents:
                        stuff = stuff + ' &nbsp ' + str(Href('view?branch='+branch+'&patch='+patch+':buildlog', 'view buildlog'))
                    if branch in changeables or spl[0] == self.user:
                        stuff = stuff + ' &nbsp ' + str(Href('withdraw?branch='+branch+'&patch='+patch, '(withdraw)'))
                    if branch in changeables and not dcheckin_active:
                        stuff = stuff + ' &nbsp ' + str(Href('commit?branch='+branch+'&patch='+patch, '(commit)'))
                    patches.append(stuff)
                    print 'Patch ',patch,'branch',branch
            if not patches == []:
                #doc.append('<H4> Pending patches: </H4>')
                doc.append(List(patches))
                

        if self.issecretary():
            doc.append(HR())
            doc.append('Welcome, Secretary. Our loyal subjects are: <P>')
            users = self.patchman.members.keys()
            users.sort()
            descriptions = []
            for user in users:
                description = [Href('mailto:'+user,user)]
                if user in self.patchman.secgens:
                    description.append('Secretary General')
                if user in self.patchman.secs:
                    description.append('Secretary')
                for branch in self.patchman.tsars.keys():
                    if user in self.patchman.tsars[branch]:
                        description.append('Tsar of '+branch)
                        # +' '+str(Href('revoke_tsarship?user='+user+'&branch='+branch,'(revoke)')))
                    if self.patchman.acl[branch] and user in self.patchman.acl[branch]:
                        description.append('Member of '+branch)
                descriptions.append(List(description))
            doc.append(List(descriptions))
        doc.append('<HR>')
        doc.append(patchman.render_log())
        return str(doc)



    def pending(self):
        """ export me """
        return `self.getinfo()`

        


    # find a list of branches which I can access
    def branches_accessible(self):
        self.patchman.read_db() # XXX reread the DB every time!
        acls = self.patchman.acl
        mybranches = []
        for branch in acls.keys():
            if self.issecretary() or (not acls[branch]) or (self.user in acls[branch]):
                mybranches.append(branch)
        return mybranches

    def branches_changeable(self):
        mybranches = []
        for branch in self.branches_accessible():
            if self.issecretary() or (self.user in self.patchman.tsars[branch]):
                mybranches.append(branch)
        return mybranches
    
    def assert_branch_existence(self, branch):
        if branch in self.branches_accessible(): return None
        return message_page('You are not authorised to use or know whether '+branch+' exists', title='patchman error')

    def assert_tsarship(self, branch):
        err = self.assert_branch_existence(branch)
        if err: return err
        if self.issecretary() or self.user in self.patchman.tsars[branch]:
            return None
        return message_page('You are not a Tsar of '+branch, title='patchman error')
    
    def start_validation(self, branch):
        """ invoke me """
        err = self.assert_tsarship(branch)
        if err: return err
        self.patchman.invokevalidator(branch)
        return multiuser.teleport(self.url)
    
    def view(self, branch, patch):
        """ invoke me """
        err = self.assert_branch_existence(branch)
        if err: return err
        if patch[0] in ['.', '/']:
            return message_page('Invalid patch name '+patch)
        print self.user,'viewing branch',branch,'patch',patch
        doc = SimpleDocument(docrc)
        doc.title = 'patchman view'
        doc.append(Href(self.url, "Back"))
        doc.append(HR())
        name = branch + '/'+patch
        doc.append('<H2>'+name+'</H2>\n')
        o = open(contrib_dir+'/'+name, 'r')
        data = o.read()
        o.close()
        
        doc.append(Pre(Text(data)))
        return str(doc)

    def submit(self, patchdata=None, branch=None, notes=None):
        """ invoke me """
        notes = normalize_string(notes)
        try:
            assert_username(notes)
        except:
            return message_page('Invalid notes string '+`notes`+'; notes strings may only contain alphanumerics and underscore chararcters', self.url)
        return self.patchman.submit(patchdata, branch=branch, user = self, notes =notes)

    def submitfilename(self, filename, branch, notes):
        """ invoke me """
        #self.patchman.log( (self.user, 'attempted indirect upload of ',filename))
        if not check_filename(filename):
            return message_page('Invalid filename '+`filename`, self.url)
        notes = normalize_string(notes)
        try:
            assert_username(notes)
        except:
            return message_page('Invalid notes string '+`notes`+'; notes strings may only contain alphanumerics and underscore chararcters', self.url)
        ffilename = '%s/Upload/%s' % (contrib_dir, filename)
        if not os.path.isfile(ffilename):
            return message_page('Could not find patch '+filename+' in upload directory', self.url)
        if os.path.islink(ffilename):
            return message_page('Filename '+filename+' in upload directory is symlink which is forbidden', self.url)            
        o = open(ffilename, 'r')
        data = o.read()
        o.close()
        os.unlink(ffilename)
        return self.patchman.submit_core(data, branch, self, notes)
    
    def withdraw(self, branch=None, patch=None):
        """ Withdraw a patch, if we are authorised to do so """
        print self.user,'Withdraw ',branch,patch
        spl = string.split(patch, ':')
        filename = contrib_dir + '/' + branch + '/'+ patch
        if not os.path.isfile(filename) or len(spl)!=2:
            return message_page('Patch '+patch+' of branch '+branch+' not found', self.url)
        if self.issecretary() or (self.user in self.patchman.tsars[branch]) or spl[0] == self.user:
            os.unlink(filename)
            # invoke the validator to update the valids file to reflect
            # that the patch doesn't exist anymore
            self.patchman.invokevalidator(branch)
            self.patchman.log( (self.user, 'withdraws',patch,'of branch',branch))
            return message_page('Patch '+patch+' of branch '+branch+' withdrawn', self.url, title='patchman message')
        else:
            self.patchman.log( (self.user,'illegally attempted to withdraw',patch,'of branch',branch))
            return message_page('You are not authorised to withdraw '+patch+' of branch '+branch, self.url)

    def commit(self, branch, patch, RESPONSE):
        """ invoke me """
        if not branch in self.branches_changeable(): return message_page('You are not authorised to commit patches to branch '+branch, self.url)
        spl = string.split(branch, ':')
        if not len(spl) == 2: return message_page('Invalid branch '+branch, self.url)
        print
        if not os.path.exists(contrib_dir+'/'+branch+'/'+patch): return message_page('No such patch '+patch+' in branch '+branch, self.url)
        RESPONSE.write('<HTML><HEAD><TITLE> dpatch commit output </TITLE></HEAD><BODY>'+'\n'+'Commiting branch '+branch+' patch '+patch+'<P><HR>')
        cmdline = 'dcheckin.py commit '+spl[0]+' '+spl[1]+' '+contrib_dir+'/'+branch+'/'+patch + ' 2>&1'+'\n'
        RESPONSE.write('Invoking '+cmdline+'\n<P><HR>\n')
        RESPONSE.flush()
        o = os.popen(cmdline, 'r')
        while 1:
            output = o.readline()
            if len(output) == 0: break
            RESPONSE.write(Pre(Text(output[:-1])).__str__()+'\n')
            RESPONSE.flush()
        result = o.close()
        RESPONSE.write('<P><HR>Result of dcheckin was '+`result`+'\n')
        RESPONSE.flush()
        if result in [ 0, None ]:
            # remove patch
            
            if not os.path.isdir(contrib_dir+'/Accepted'):
                os.mkdir(contrib_dir+'/Accepted')
            i = 0
            for filename in os.listdir(contrib_dir+'/Accepted'):
                spl = string.split(filename, ':')
                try:
                    x = eval(spl[0])
                    if type(x) == type(1) and x > i: i = x
                except: pass
            i = i + 1
         
            system('mv '+contrib_dir+'/'+branch+'/'+patch+' '+contrib_dir+'/Accepted/'+`i`+':' +branch+':'+patch)
            #print 'Invoking validator to clear up'
            #RESPONSE.flush()
            RESPONSE.write('<HR><P> Commit succeded; now clearing up\n')
            self.patchman.log( (self.user, 'committed', patch,'of', branch) )
            self.patchman.invokevalidator(branch) # to clear up
        else:
            RESPONSE.write('<HR><P> Commit failed\n')
            self.patchman.log( (self.user, 'attempted failed commit of', patch, 'to',branch) )
        RESPONSE.write('<HR><P> Checkin attempt finished\n')
        RESPONSE.write('<HR><A HREF="'+self.url+'">Back to patchman</A></BODY>')
        
        RESPONSE.flush()
        #return message_page( (result and ('Commit failed; error code '+`result`) or ('Commit succeeded')) + ' <p> Dcheckin command line is: <PRE>'+cmdline+'</PRE> <p> Ouptut of dcheckin is: <p> <PRE> '+output+' </PRE>', self.url, title = result and 'patchman error' or 'patchman message', instant = 0)
    
class Patchman:
    """ export me """
    def __init__(self):
        sys.stdout.write('Hello; patchman starting\n')
        self.branches = {}
        self.members = {}
        self.read_db()
        self.users = multiuser.Users()
        o = open(contrib_dir+'/Hostname', 'r')
        data = o.readline()
        o.close()
        hostname = data[:-1]
        self.auth = multiuser.Authenticator(passwordfile, docrc, 'http://'+hostname+'/patchman/users', 'Patchman', self.users, Account, self)
        self.logdata = []
        self.log( ('patchman started in working directory', os.getcwd()) )

    def log(self, data):
        timestamp = time.ctime(time.time())
        self.logdata.append( ( (timestamp, data ) ) )
        print 'LOG: %s: %s' % (timestamp, `data`)
        if len(self.logdata)>30: self.logdata = self.logdata[-30:]

    def render_log(self):
        str = '<H2> Log </H2>\n<UL>'
        for (date, data) in self.logdata:
            datastr = ''
            for item in data:
                if type(item) == type(''): datastr = datastr + ' ' + item
                else: datastr = datastr + ' ' + `item`
            str = str + ('<LI> %s:%s\n' % (date, datastr))
        return str + '</UL>'
    def read_db(self):
        self.acl = {}
        self.tsars = {}
        self.secgens = read_user_list(contrib_dir+'/SecretaryGeneral')
        #print 'Secretary Generals ',self.secgens
        self.secs = read_user_list(contrib_dir+'/Secretaries')
        #print 'Secretaries ',self.secs
        for branch in os.listdir(contrib_dir):
            if os.path.isdir(contrib_dir+'/'+branch) and len(string.split(branch, ':')) == 2:
                self.tsars[branch] = read_user_list(contrib_dir+'/'+branch+'/Tsars')
                (packagename, branchname) = string.split(branch, ':')
                if not os.path.exists(patch_tree+'/'+packagename+'/'+branchname):
                    raise branch_missing_from_patch_tree, branch
                self.branches[branch] = {}
                try:
                    self.acl[branch] = read_user_list(contirb_dir+'/'+branch+'/Members')
                except:
                    self.acl[branch] = None
    def index_html(self):
        """ invoke me """
        doc = SimpleDocument(docrc)
        doc.title = 'Welcome to patchman'
        doc.append('Patchman is a web service for submitting dpatch files. You need an account to do so. To use it you must ')
        doc.append(Href('/patchman/auth','login or create yourself a new account'))
        doc.append('<p> Your account details will be visible to the patchman secretaries but not to regular users.')
        return str(doc)

    def allocate_filename(self, branch, email, notes):
        filenames = os.listdir(contrib_dir+'/'+branch)
        level = 1
        while email + ':' + notes + `level` in filenames: level = level + 1
        return branch + '/'+email + ':' + notes+ `level`
            
    def invokevalidator(self, branch):
        if not os.path.isfile(contrib_dir+'/'+branch+'/Validator'):        
            os.system('validator.py %s %s %s &' % (contrib_dir, patch_tree, branch))
        
    def submit(self, patchdata=None, branch=None, user=None, notes=None):
        print 'Patch client hostname ',patchdata.filename
        print 'Branch',branch
        print 'User',user.user

        try:
            o = patchdata
            patch = o.read()
            o.close()
        except:
            self.log ( (user.user, 'invalid file upload to ', branch))
            return message_page('Patchman tried to download but was not successful.', user.url)
        return self.submit_core(patch, branch, user, notes)

    def submit_core(self, patch, branch, user, notes):
        if branch == None:
            return message_page('Patchman; branch must be specified')
        if len(patch) == 0:
            return message_page('Patchman: patch was zero bytes long and is therefore invalid; you probably typed a bad filename.', user.url)
        
        try:
            filename = self.allocate_filename(branch, user.user, notes)
        except:
            self.log ( (user.user, 'upload to invalid branch', branch))
            return message_page('Patchman could not find branch '+branch, user.url)

        print 'Submit core; patch length',len(patch),'branch',branch,'user',user,'Notes',notes
        o = open(contrib_dir+'/'+filename, 'w')
        o.write(patch)
        o.close()
        self.invokevalidator(branch)
        self.log( (user.user, 'uploaded to', branch,'notes', notes, 'length', len(patch), 'filename',filename ) ) 
        return message_page('Your patch against '+branch+' downloaded okay. Patch length '+`len(patch)`+' bytes.<p>\nYour patch is queued for approval by the Tsars. Please check back later to see the progress of your patch; it has the working filename '+filename+'\n', user.url, title='patchman message')
       


patchman = Patchman()


