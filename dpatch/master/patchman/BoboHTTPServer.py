#!/bin/env python
"""Simple Bobo HTTP Server (based on CGIHTTPServer.py)

What's Bobo?

An open source collection of tools for developing
world-wide web applications in Python. Find out more at
http://www.digicool.com/releases/bobo/

What does BoboHTTPServer.py do?

It is a very simple web server that publishes a module 
with Bobo. BoboHTTPServer.py is probably the easiest way
to publish a module with Bobo. It does not require anything
besides Python and Bobo to be installed. It is fast because
it publishes a module as a long running process. However,
BoboHTTPServer.py still hasn't seen a lot of testing, so I
would not recommend it for production use just yet.

Why not use CGI or PCGI or Medusa?

BoboHTTPServer is much simpler to use than other Bobo publishers.
BoboHTTPServer does not require any configuration. It has a 
friendly license. It also offers excellent performance, threaded
publishing, and streaming response, not to mention all the Bobo
basics like authentication, control of response content-type,
file uploads, etc.

Features:

-Only publishes one module at a time
-Cannot reload a module
-Does not serve files or CGI programs, only Bobo
-Single-threaded or multi-threaded publishing
-Unbuffered output so response can stream
-Works under Python 1.5 and Python 1.4 (mostly) 
-At least it's easy to use

Usage:

BoboHTTPServer.py [-p <port>] [-t] <path to module to publish>

Starts a web server which publishes the specified module.

-p <port>: Run the server on the specified port. The default
port is 8080.

-t: use a multi-threaded HTTP server. The default is a
single-threaded server. Note, your platform must support
threads to use the multi-threaded server.

Known bugs:

REQUEST.write never closes the connection under Python 1.4

There is a problem with mutipart/form-data POST requests
under win32 and python 1.5.2a2. I think the problem is
with cgi.py

Under win32 interupted HTTP requests can raise exceptions,
but they don't seem to cause any problems.

Changes:

0.3 RESPONSE.write should stream now.
    Fixed SCRIPT_NAME, so base should be correct now.
    (Thanks to Steve Spicklemire and Jim Fulton)
    Overhauled POST handling.
	
0.2 Added multi-threaded server option.
    Fixed POST under win32, I think.
    Fixed muti-part/form-data encoding, I think.

0.1 Inital release. Kinda worked.

License:

Same terms as Bobo.

copyright 1998 Amos Latteier"""

__version__="0.3"
__author__="Amos Latteier amos@aracnet.com"

import SocketServer
import SimpleHTTPServer
import BaseHTTPServer
import cgi_module_publisher
import os
import sys
import urllib
import string
import time
import tempfile
try: from cStringIO import StringIO
except ImportError: from StringIO import StringIO


class ResponseWriter:
	"""Logs response and reorders it so status header is
	first. After status header has been written, the rest
	of the response can stream."""
	
	def __init__(self,handler):
		self.handler=handler
		self.data=""
		self.latch=None
		
	def write(self,data):
		if self.latch:
			self.handler.wfile.write(data)
		else:
			self.data=self.data+data
			start=string.find(self.data,"Status: ")
			if start:
				end=string.find(self.data,"\n",start)
				status=self.data[start+8:end]
				code, message=tuple(string.split(status," ",1))
				self.handler.send_response(string.atoi(code),message)
				self.handler.wfile.write(self.data[:start]+
					self.data[end+1:])
				self.latch=1

	def flush(self):
		pass
	

class BoboRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
	"""Handler for GET, HEAD and POST. Only publishes one
	Bobo module at a time. All URLs go to the published module.
	Does not serve files or CGI programs.
	"""
	
	server_version = "BoboHTTP/" + __version__
	buffer_size = 8192
	
	def setup(self):
		"""overrides defaults to set larger buffer for rfile
		otherwise large POST requests seem to take forever."""
		self.connection = self.request
		self.rfile = self.connection.makefile('rb', self.buffer_size)
		self.wfile = self.connection.makefile('wb', 0)

	def do_POST(self):
		#cgi.py doesn't like to parse file uploads unless we do this
		if string.find(self.headers.getheader('content-type'),
			'multipart/form-data') != -1:
			si_len=string.atoi(self.headers.getheader('content-length'))
			if si_len < 1048576:
				si=StringIO()
				si.write(self.rfile.read(si_len))
			else:
				bufsize=self.buffer_size
				si=tempfile.TemporaryFile()
				while si_len > 0:
					if si_len < bufsize:
						bufsize=si_len
					data=self.rfile.read(bufsize)
					si_len=si_len - len(data)
					si.write(data)
			si.seek(0)
			self.rfile=si
		self.publish_module()

	def do_GET(self):
		self.publish_module()

	def do_HEAD(self):
		#is this necessary?
		self.publish_module()
		
	def publish_module(self):
		cgi_module_publisher.publish_module(
			self.module_name,
			stdin=self.rfile,
			stdout=ResponseWriter(self),
			stderr=sys.stdout,
			environ=self.get_environment())	

	def get_environment(self):
		#Partially derived from CGIHTTPServer
		env={}
		env['SERVER_SOFTWARE'] = self.version_string()
		env['SERVER_NAME'] = self.server.server_name
		env['SERVER_PORT'] = str(self.server.server_port)
		env['SERVER_PROTOCOL'] = self.protocol_version
		env['GATEWAY_INTERFACE'] = 'CGI/1.1'
		env['SCRIPT_NAME'] = "" # i am everything and nothing
		dir=self.module_dir
		rest = self.path
		i = string.rfind(rest, '?')
		if i >= 0:
			rest, query = rest[:i], rest[i+1:]
		else:
			query = ''
		uqrest = urllib.unquote(rest)
		env['PATH_INFO'] = uqrest
		env['REQUEST_METHOD'] = self.command
		env['PATH_TRANSLATED'] = self.translate_path(uqrest)
		if query:
			env['QUERY_STRING'] = query
		host = self.address_string()
		if host != self.client_address[0]:
			env['REMOTE_HOST'] = host
		env['REMOTE_ADDR'] = self.client_address[0]
		env['CONTENT_TYPE'] = self.headers.getheader('content-type')
		length = self.headers.getheader('content-length')
		if length:
			env['CONTENT_LENGTH'] = length
		# handle the rest of the environment
		for k,v in self.headers.items():
			k=string.upper(string.join(string.split(k,"-"),"_"))
			if not env.has_key(k) and v:
				env['HTTP_'+k]=v
		return env


class ThreadingHTTPServer(SocketServer.ThreadingMixIn,
	BaseHTTPServer.HTTPServer):
	pass


def try_to_become_nobody():
	# from CGIHTTPServer
	try: import pwd
	except: return
	try:
		nobody = pwd.getpwnam('nobody')[2]
	except pwd.error:
		nobody = 1 + max(map(lambda x: x[2], pwd.getpwall()))
	try: os.setuid(nobody)
	except os.error: pass	

def set_published_module(file,klass):
	dir,file=os.path.split(file)
	name,ext=os.path.splitext(file)
	klass.module_name=name
	klass.module_dir=dir
	cdir=os.path.join(dir,'Components')
	sys.path[0:0]=[dir,cdir,os.path.join(cdir,sys.platform)]
	__import__(name) # to catch problem modules right away
	print "Publishing module %s" % name

class Log:
	"""Really simple log facility. An object of this class should
	replace sys.stdout and sys.stderr for all output of a program
	to be logged. """
	def __init__(self, logfilename, upstream):
		self.logfilename = logfilename
		self.logfile = open(logfilename, 'a')
		self.upstream = upstream
		self.upstream.write('Opened log '+logfilename+'\n')
	def write(self, str):
		str = time.ctime(time.time()) + ':'+ str
		if str[-1] != '\n': str = str + '\n'
		self.upstream.write('LOG: '+str)
		self.logfile.write(str)
		self.logfile.flush()

def start(module_file, port=8080, threading=None, logfilename=None):
	if logfilename: sys.stdout = sys.stderr = Log(logfilename, sys.stderr)
	print "Starting server"
	try_to_become_nobody()
	set_published_module(module_file,BoboRequestHandler)
	server_address = ('', port)
	if threading:
		try:
			import thread
			httpd = ThreadingHTTPServer(server_address,
				BoboRequestHandler)
			print "Using threading server"
		except ImportError:
			httpd = BaseHTTPServer.HTTPServer(server_address,
				BoboRequestHandler)
	else:
		httpd = BaseHTTPServer.HTTPServer(server_address,
			BoboRequestHandler)

	print "Serving HTTP on port "+` port`+ "..."
	httpd.serve_forever()


if __name__=="__main__":
	import getopt
	optlist, args=getopt.getopt(sys.argv[1:],"tp:l:")
	if len(optlist)>2 or len(args)!=1:
		print __doc__
		sys.exit(1)
	port=8080
	threading=None
	logfilename=None
	for k,v in optlist:
		if k=="-p":
			port=string.atoi(v)
		if k=="-l":
			logfilename = v
		# Threading seems unstable; disable it
		#if k=="-t":
		#	threading=1
	module_file=args[0]
	if logfilename:
		sys.stderr.write('Logging to '+logfilename+'\n')
	else:
		sys.stderr.write('Warning; no logging!')
	start(module_file,port,threading, logfilename)
	
