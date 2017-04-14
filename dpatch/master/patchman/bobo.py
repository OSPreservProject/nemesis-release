#!/bin/sh
""":"
exec python $0 ${1+"$@"}
"""

#" Waaaa
#
# Copyright (c) 1996-1998, Digital Creations, Fredericksburg, VA, USA.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#   o Redistributions of source code must retain the above copyright
#     notice, this list of conditions, and the disclaimer that follows.
# 
#   o Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions, and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
# 
#   o All advertising materials mentioning features or use of this
#     software must display the following acknowledgement:
# 
#       This product includes software developed by Digital Creations
#       and its contributors.
# 
#   o Neither the name of Digital Creations nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
# 
# 
# THIS SOFTWARE IS PROVIDED BY DIGITAL CREATIONS AND CONTRIBUTORS *AS
# IS* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DIGITAL
# CREATIONS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
# USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.
#
#
# If you have questions regarding this software,
# contact:
#
#   Digital Creations, L.C.
#   910 Princess Ann Street
#   Fredericksburge, Virginia  22401
#
#   info@digicool.com
#
#   (540) 371-6909
#

__doc__='''Command-line Bobo

Usage

   bobo [options] module_path [path_info]

   where:

   module_path -- is a full path to a published module 

   path_info -- Is the information after the module name that would
         normally be specified in a GET URL, including a query string.

Description

   The command-line interface to Bobo provides a handy way to test,
   debug, and profile Bobo without a web server.  

Options

   -u username:password        -- Supply HTTP authorization information

   -e name=value               -- Supply environment variables.  Use a
                                  seperate -e option for each variable 
                                  specified.

   -p profiler_data_file       -- Run under profiler control,
                                  generating the profiler 
                                  data file, profiler_data_file.

   -t                          -- Compute the time required to
                                  complete a request, in 
                                  milliseconds.

   -r n                        -- Specify a repeat count for timing or
                                  profiling.

   -d                          -- Run in debug mode.  With this
                                  option, bobo will run under Python
                                  debugger control.  Two useful
                                  breakpoints are set.  The first is
                                  at the beginning of the module
                                  publishing code.  Steping through
                                  this code shows how bobo finds
                                  objects and obtains certain
                                  meta-data.  The second breakpoint is
                                  at the point just before the
                                  published object is called.  To jump
                                  to the second breakpoint, you must
                                  enter 's' followed by a carriage
                                  return to step into the module, then
                                  enter a 'c' followed by a carriage
                                  return to jump to the first
                                  breakpoint and then another 'c'
                                  followed by a carriage return to
                                  jump to the point where the object
                                  is called.  Finally, enter 's'
                                  followed a carriage return.

   -s                             Don\'t generate any output

Examples

   For example, to debug a published object (such as a method), spam,
   the following might be entered::

            bobo -d /prj/lib/python/mymod container/spam
            s
            c
            c
            s


$Id: bobo.py 1.1 Thu, 18 Feb 1999 14:18:34 +0000 dr10009 $
'''
__version__='$Revision: 1.1 $'[11:-2]

import sys, traceback, profile, os, getopt, string
from time import clock
repeat_count=100

def main():
    import sys, os, getopt, string
    global repeat_count

    try:
        optlist,args=getopt.getopt(sys.argv[1:], 'dtu:p:r:e:s')
        if len(args) > 2 or len(args) < 2: raise TypeError, None
        if len(args) == 2: path_info=args[1]
    except:
        sys.stderr.write(__doc__)
        sys.exit(-1)

    silent=profile=u=debug=timeit=None
    env={}
    for opt,val in optlist:
        if opt=='-d':
            debug=1
        if opt=='-s':
            silent=1
        if opt=='-t':
            timeit=1
        if opt=='-u':
            u=val
        elif opt=='-p':
            profile=val
        elif opt=='-r':
            repeat_count=string.atoi(val)
        elif opt=='-e':
            opt=string.find(val,'=')
            if opt <= 0: raise 'Invalid argument to -e', val
            env[val[:opt]]=val[opt+1:]

    if (debug or 0)+(timeit or 0)+(profile and 1 or 0) > 1:
        raise 'Invalid options', 'only one of -p, -t, and -d are allowed' 

    module=args[0]

    publish(module,path_info,u=u,p=profile,d=debug,t=timeit,e=env,s=silent)



def time(function,*args,**kwargs):
    repeat_range=range(repeat_count)
    apply(function,args,kwargs)
    t=clock()
    for i in repeat_range:
        apply(function,args,kwargs)
    t=(clock()-t)*1000.0

    return float(t)/len(repeat_range)



def run(statement, *args):
    import sys, profile, time

    prof = profile.Profile(time.time)
    try:
        prof = prof.run(statement)
    except SystemExit:
        pass
    if args:
        prof.dump_stats(args[0])
    else:
        return prof.print_stats()


def publish_module_pm(module_name,
                      stdin=sys.stdin, stdout=sys.stdout, stderr=sys.stderr,
                      environ=os.environ, debug=0):

    from CGIResponse import Response
    from cgi_module_publisher import ModulePublisher

    after_list=[None]
    request=None
    try:
        response=Response(stdout=stdout, stderr=stderr)
        publisher = ModulePublisher(stdin=stdin, stdout=stdout, stderr=stderr,
                                    environ=environ)
        response = publisher.response
        request=publisher.request
        response = publisher.publish(module_name,after_list,debug=debug)
        request.other={}
        response=str(response)
    finally:
        try: request.other={}
        except: pass
        if after_list[0] is not None: after_list[0]()

try: from codehack import getlineno
except:
    def getlineno(code):
        return code.co_firstlineno

def publish(script,path_info,u=None,p=None,d=None,t=None,e=None,s=None,pm=0):

    profile=p
    debug=d
    timeit=t
    silent=s
    if e is None: e={}

    if not script: script='+Main'
    if script[0]=='+': script='../../lib/python/'+script[1:]

    env=e
    env['SERVER_NAME']='bobo.server'
    env['SERVER_PORT']='80'
    env['REQUEST_METHOD']='GET'
    env['REMOTE_ADDR']='204.183.226.81 '
    env['REMOTE_HOST']='bobo.remote.host'
    env['HTTP_USER_AGENT']='Bobo/%s' % __version__
    env['HTTP_HOST']='127.0.0.1'
    env['SERVER_SOFTWARE']='Bobo/%s' % __version__
    env['SERVER_PROTOCOL']='HTTP/1.0 '
    env['HTTP_ACCEPT']='image/gif, image/x-xbitmap, image/jpeg, */* '
    env['SERVER_HOSTNAME']='bobo.server.host'
    env['GATEWAY_INTERFACE']='CGI/1.1 '
    env['SCRIPT_NAME']=script
    p=string.split(path_info,'?')
    if   len(p)==1: env['PATH_INFO'] = p[0]
    elif len(p)==2: [env['PATH_INFO'], env['QUERY_STRING']]=p
    else: raise TypeError, ''

    if u:
        import base64
        env['HTTP_AUTHORIZATION']="Basic %s" % base64.encodestring(u)

    dir,file=os.path.split(script)
    cdir=os.path.join(dir,'Components')
    sys.path[0:0]=[dir,cdir,os.path.join(cdir,sys.platform)]

    # We delay import to here, in case cgi_module_publisher is part of the
    # application distribution.
    from cgi_module_publisher import publish_module

    if profile:
        import __main__
        __main__.publish_module=publish_module
        __main__.file=file
        __main__.env=env
        print profile
        publish_module(file, environ=env, stdout=open('/dev/null','w'))
        c=("for i in range(%s): "
           "publish_module(file, environ=env, stdout=open('/dev/null','w'))"
           % repeat_count
           )
        if profile: run(c,profile)
        else: run(c)
    elif debug:
        import cgi_module_publisher
        from cgi_module_publisher import ModulePublisher
        import pdb

        class Pdb(pdb.Pdb):
            def do_pub(self,arg):
                if hasattr(self,'done_pub'):
                    print 'pub already done.'
                else:
                    self.do_s('')
                    self.do_s('')
                    self.do_c('')
                    self.done_pub=1
            def do_ob(self,arg):
                if hasattr(self,'done_ob'):
                    print 'ob already done.'
                else:
                    self.do_pub('')
                    self.do_c('')
                    self.done_ob=1

        db=Pdb()

        def fbreak(db,meth):
            try: meth=meth.im_func
            except AttributeError: pass
            code=meth.func_code
            lineno = getlineno(code)
            filename = code.co_filename
            db.set_break(filename,lineno)

        fbreak(db,ModulePublisher.publish)
        fbreak(db,ModulePublisher.call_object)
        #fbreak(db,cgi_module_publisher.new_find_object)
        #fbreak(db,cgi_module_publisher.old_find_object)

        dbdata={'breakpoints':(), 'env':env}
        b=''
        try: b=open('.bobodb','r').read()
        except: pass
        if b: exec b in dbdata

        for b in dbdata['breakpoints']:
            if type(b) is type(()):
                apply(db.set_break,b)
            else:
                fbreak(db,b)    

        db.prompt='pdb> '
        # db.set_continue()

        print (
        '* Type "s<cr>c<cr>" to jump to beginning of real publishing process.\n'
        '* Then type c<cr> to jump to the beginning of the URL traversal\n'
        '  algorithm.\n'
        '* Then type c<cr> to jump to published object call.'
        )
        db.run('publish_module(file,environ=env,debug=1)',
               cgi_module_publisher.__dict__,
               {'file':file, 'env':env})
    elif timeit:
        stdout=sys.stdout
        t= time(publish_module,file,
                stdout=open('/dev/null','w'), environ=env)
        stdout.write('%s milliseconds\n' % t)
    elif pm:
        stdout=sys.stdout
        publish_module_pm(file, environ=env, stdout=stdout)
        print '\n%s\n' % ('_'*60)
    else:
        if silent: stdout=open('/dev/null','w')
        else: stdout=sys.stdout
        publish_module(file, environ=env, stdout=stdout)
        print '\n%s\n' % ('_'*60)

if __name__ == "__main__": main()
