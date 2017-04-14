#
#	ModBE.py
#	--------
#
# $Id: ModBE.py 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
#
# Backend to generate Nemesis module headers, stubs and type info
#

import sys
import os

def preserve (fn):
  if os.path.isfile (fn):
    rc = os.system ('rm -f ' + fn + '.old')
    os.rename (fn, fn + '.old')

def GenerateTypes (intf, fp) :
  t_ih = open (intf.name + '.ih', 'w')
  intf.typeHeader (t_ih, fp)

def GenerateRep (intf, fp) :
  t_d = open (intf.name + '.def.c', 'w' )
  intf.representation(t_d)

class IDCFile:
    # A file and some string constants which vary between IDC styles
    def init (self, f) :
      self.f      = f
      self.idc    = 'idc'
      self.m      = 'm'
      self.u      = 'u'
      self.cl     = 'IDCControl'
      self.clp    = 'ctrl'
      self.bd     = ''
      self.idc_m  = 'idc_m'
      self.idc_u  = 'idc_u'
      self.idc_f  = 'idc_f'
      self.true   = 'TRUE'
      self.false  = 'FALSE'
      return self

    def malloc (self, clp) :
      return 'Malloc(' + clp + '->idc_heap, '

    def free (self, clp) :
      return 'FREE ('

    def write (self, str) :
      self.f.write (str)

class IDC1File:
    # A file and some string constants which vary between IDC styles
    def init (self, f) :
      self.f     = f
      self.idc   = 'IDC'
      self.m     = 'M'
      self.u     = 'U'
      self.cl    = 'IDC_cl'
      self.clp   = '_idc'
      self.bd    = ',_bd'
      self.idc_m = 'IDC_M'
      self.idc_u = 'IDC_U'
      self.idc_f = 'IDC_F'
      self.true  = 'True'
      self.false = 'False'
      return self

    def malloc (self, clp) :
      return 'Heap$Malloc(_bd->heap, '

    def free (self, clp) :
      return 'FREE ('

    def heap (self, clp) :
      return '(_bd->heap)'

    def write (self, str) :
      self.f.write (str)

def GenerateIDC1Marshalling(intf, fp):
    # Create DME-style Local IDC Marshaling code
    
    #intf.assignCodes(sys.stdout, fp)

    IDC_M_h = IDC1File().init(open ('IDC_M_' + intf.name + '.h' , 'w'))
    intf.IDCMarshalHeader( IDC_M_h )

    IDC_M_c = IDC1File().init(open ('IDC_M_' + intf.name + '.c' , 'w'))
    if len (intf.types) != 0 :
	intf.IDCMarshalCode( IDC_M_c )
    else :
	blank( IDC_M_c )


def GenerateIDC1Stubs(intf):
    # Create DME-style Local IDC Stub code

    IDC_S_c = IDC1File().init(open ('IDC_S_' + intf.name + '.c' , 'w'))

    if len (intf.ops) != 0 :
	intf.IDCServerStubs( IDC_S_c )
	intf.IDCClientStubs( IDC_S_c )
    else :
	blank(IDC_S_c)
	
def blank(f):
    # Some files must exist because the Makefile cannot know whether they
    # really will exist or not and must always attempt to compile them
    f.write( '/* This file intentionally left blank */\n' )


def GenerateTmpl (intf):
  intf.template (sys.stdout)


