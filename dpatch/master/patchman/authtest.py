
"""
an example of using multiuser.py with Bobo.

"""


# passwdfile is a string passed in to Authenticator when it is constructed.
# The contents of the file map user email addresses to passwords.
# When you set up the system, make this file contain just:
# {}
# (ie a stringified empty dictionary. As new users register, it will contain
# their email addresses and the md5 sums of their email address and passwords.

passwdfile = '/local/scratch/dr10009/passwords'
import multiuser

# For each user, an account object will be created. All these accounts
# will be placed in to your Users object named as the concatenation of
# user and the md5sum of the (email,password) pair. The user email address
# and password is passed in to the structure. The account object should
# have an index_html method with returns the name of the index.

class Account:
    """ account test object """
    def __init__(self, user, md5sum, payload):
        print 'Account object for ',user,' being created'
        self.user = user
    def index_html(self):
        """ invoke me """
        return 'account of '+self.user

# docrc should be an HTMLgen .rc file defining the style used for
# the various authentication pages.

docrc = '/usr/groups/pegasus/nemesis/misc/nemdoc/weboccur/nemdoc.rc'

# this is the important bit; we construct a users object to hold the different
# users account. This object will be accessed by users using Bobo
users = multiuser.Users()
# ...and we construct an auth object. This will be accessed by users to log
# in or to create new accounts. The arguments are:
#  1. The name of the password file
#  2. The name of the style file
#  3. The URL prefix for the users object
#  4. The service name
#  5. The users object itself
#  6. The constructor for user accounts (normally just the class name)
#  7. A python object passed as a third argument to new accounts
auth = multiuser.Authenticator(passwdfile, docrc, 'http:/users', 'Authentication Test Service', users, Account, 'application-specific-data')



                    
