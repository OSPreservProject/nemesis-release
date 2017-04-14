
"""

Multi user authentication system for Bobob/SimpleHTTPServer

Copyright 1999 Dickon Reed

In this system, email addresses are used as user IDs. The basic idea
is that accounts are created by email address, and then passwords are
mailed to people. We store (email, md5) pairs and export the MD5 sums as
the names to access accounts with.

A file stores (email, md5sum) pairs, represented in a stringified
python format.

See authtest.py for an example of using this object.

"""

import md5, whrandom, passwordgen, os, sys, string
from HTMLgen import SeriesDocument, Form, Href, Input, HR

def maptomd5(user, password):
    str = user + ':' + string.lower(password)
    m = md5.new(str)
    hash = 'user'
    for byte in m.digest():
        hash = "%s%.2x" % (hash, ord(byte))
    return hash

def genpassword():
    generator = whrandom.whrandom()
    password = 0
    for i in range(numpassfragments):
        password = password + passfragments[generator.randint(o, len(passfragments))]
    return password

def teleport(url, mesg= ''):
    return '<HTML><HEAD><META HTTP-EQUIV="refresh" CONTENT="1; url='+url+'"><TITLE> Redirection </TITLE></HEAD><BODY> '+mesg+'<p>If your browser supports meta tags, you will shortly be forwarded to the next page.<p> <A HREF="'+url+'> Otherwise go there manually </A></BODY></HTML>'    




PasswordFileBad = 'PasswordFileBad'


class Users:
    """ export me """
    def __init__(self):
        pass

class Authenticator:
    """ Class used to authenticate multi user accounts.

    users is a mapping of email to MD5 sums.


    """
    def __init__(self, passwdfile, docrc, accounthome, servicename, usersobj, accountfactory, payload):
        self.passwdfile = passwdfile
        self.accounthome = accounthome
        self.docrc = docrc
        self.servicename = servicename
        self.usersobj = usersobj
        self.accountfactory = accountfactory
        self.payload = payload
        try:
            o = open(self.passwdfile, 'r')
            data = o.read()
            o.close()
            self.users = eval(data)
            if type(self.users) != type({}):
                raise PasswordFileBad
        except:
            raise PasswordFileBad
        for user in self.users.keys():
            self.export(user, self.users[user])

    def export(self, user, md5sum):
        sys.stderr.write('Creating account for user '+user+'\n')
        exec 'self.usersobj.'+md5sum+' = self.accountfactory(user, md5sum, self.payload)'
        
    def writepasswords(self):
        o = open(self.passwdfile+'_new', 'w')
        o.write(`self.users`)
        o.close()
        os.unlink(self.passwdfile)
        os.system('mv '+self.passwdfile+'_new '+self.passwdfile)
        os.system('chmod 600 '+self.passwdfile)
        
    def index_html(self):
        """ login form """
        doc = SeriesDocument(self.docrc)
        doc.title = 'User login for '+self.servicename
        doc.append('If you have a '+self.servicename+' account and know the password for it, type in your email and address and '+self.servicename+' password here:')
        f = Form('login')
        f.append('Email address: ')
        f.append(Input(type='text', name='user'))
        f.append('Password: ')
        f.append(Input(type='password', name='password'))
        f.submit = Input(type='submit', name='login', value='login')
        doc.append(f)
        doc.append(HR())
        doc.append('If you do not already have an account, create one for yourself here and you will be mailed a password. Or, if you have forgotten your password, type just your email address in and you will be sent a new password.<P>')
        f = Form('createaccount')
        f.append('Email address: ')
        f.append(Input(type='text', name='user'))
        f.submit = Input(type='submit', name='create account', value='create account')
        doc.append(f)
        return str(doc)
    
    def login(self, user, password):
        """ the authentication routine """
        result = None
        # XXX: ought to store reverse mapping here to avoid the linear search

        md5sum = maptomd5(user, password)
        if not self.users.has_key(user):
            failure = 'No such user '
        elif self.users[user] != md5sum:
            failure = 'Bad password'
        else:
            failure = None
        if not failure:
            # XXX: is this the best way of forwading the user to their new page?
            return '<HTML><HEAD><META HTTP-EQUIV="refresh" CONTENT="1; url='+self.accounthome+'/'+md5sum+'"><TITLE> Redirection to account home page </TITLE></HEAD><BODY>If your browser supports meta tags, you will shortly be forwarded to your account home page. <A HREF="'+self.accounthome+'/'+md5sum+'> Otherwise go there manually </A></BODY></HTML>'
        doc = SeriesDocument(self.docrc)
        doc.title = failure and 'Login failed' or 'Login successful'
        doc.append(failure)
        return str(doc)

    def createaccount(self, user):
        """ recreate a new account with name user """
        password = passwordgen.genpassword()
        md5sum = maptomd5(user, password)
        self.users[user] = md5sum
        self.export(user, md5sum)
        self.writepasswords()
        serviceenvvar = 'AUTH_'+string.upper(self.servicename)
        url = self.accounthome+'/'+md5sum+'\n\n'+self.servicename
        o = os.popen('mail '+user, 'w')
        o.write('From: '+self.servicename+'\n\nYour (new) password for '+self.servicename+' is '+password+'.\nYou may login with this password or proceed directly to:\n'+url+'\n\n ps. Some scripts may require the environment variable '+serviceenvvar+' to be defined as:\nexport '+serviceenvvar+'='+url+'\n')
        o.close()
        sys.stderr.write('Password for '+user+' changed\n')
        doc = SeriesDocument(self.docrc)
        doc.title = 'Account created'
        doc.append('An account for '+user+' has been (re)created. The password will be forwarded by email.')

        return (str(doc))




