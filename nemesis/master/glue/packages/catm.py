
############################################################################
# Package database for catm
#
# This may look like source code, but it is actually the build system
# database format. You may edit this file, but only semantic changes will
# be kept the next time this file is automatically regenerated.
#

from nemclasses import *

items = []
associated_cpp_name = 'associated_cpp_name'
contents = 'contents'
description = 'description'
env = 'env'
makefileflags = 'makefileflags'
package = 'package'
path = 'path'
qos = 'qos'
requires = 'requires'
section = 'section'
system_interfaces = 'system_interfaces'
tweakability = 'tweakability'
type = 'type'
value = 'value'



#######################################################################
# Net (2 items)


items = items + [
Program('nicstar', {
  associated_cpp_name : 'NICSTAR',
  contents : {'dev/pci/nicstar':['rbuf.h', 'nicstar_aal5.c', 'nicstar.h', 'nicstar.c'],},
  description : 'Nicstar device driver',
  env : {'contexts':32,'defstack':'2k','endpoints':512,'frames':64,'heap':'32k','kernel':1,'sysheap':'8k',},
  makefileflags : {'libio':1,},
  package : 'catm',
  path : 'dev/pci/nicstar',
  qos : {'extra':1,'latency':'10ms','period':'10ms','slice':'2ms',},
  requires : ['atm', 'pci'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 2,
}),

PosixProgram('rvideo', {
  associated_cpp_name : 'RVIDEO',
  description : 'Raw video player',
  env : {'contexts':32,'defstack':'2k','endpoints':128,'frames':64,'heap':'32k','kernel':0,'sysheap':'8k',},
  package : 'catm',
  path : 'app/rawvideo',
  qos : {'extra':1,'latency':'5ms','period':'5ms','slice':'200us',},
  requires : ['fbdev', 'atm', 'windowing_system'],
  section : 'Applications',
  type : 'quad',
  value : 2,
}),

Program('oppo', {
  associated_cpp_name : 'OPPO',
  contents : {'dev/pci/oppo':['suni.h', 'state.h', 'rbuf.h', 'otto_xilinx.c', 'otto_regs.h', 'otto_platform.h', 'otto_hw.h', 'otto_hw.c', 'otto_exec.h', 'otto_aal5.c', 'otto.h', 'otto.c', 'main.c', 'direct.c', 'DirectOppo.if'],},
  description : 'Oppo device driver',
  env : {'contexts':32,'defstack':'2k','endpoints':512,'frames':64,'heap':'32k','kernel':1,'sysheap':'8k',},
  makefileflags : {'libio':1,},
  package : 'catm',
  path : 'dev/pci/oppo',
  qos : {'extra':1,'latency':'3ms','period':'3ms','slice':'400us',},
  requires : ['atm', 'pci'],
  section : 'Net',
  system_interfaces : ['DirectOppo.if'],
  tweakability : 2,
  type : 'quad',
  value : 2,
}),

Program('spans', {
  associated_cpp_name : 'SPANS',
  contents : {'mod/net/spans':['spans.c', 'Spans.if', 'fore_msg.h', 'fore_xdr.h'],},
  description : 'SPANS ATM signalling protocol server',
  env : {'contexts':32,'defstack':'2k','endpoints':128,'frames':64,'heap':'32k','kernel':0,'sysheap':'8k',},
  package : 'catm',
  path : 'mod/net/spans',
  qos : {'extra':2, 'latency':'20ms','period':'20ms','slice':'500us',},
  requires : ['atm'],
  section : 'Net',
  system_interfaces : ['Spans.if'],
  tweakability : 2,
  type : 'quad',
  value : 0,
}),
  
]




#######################################################################
# unclassified (1 items)


items = items + [
InterfaceCollection('atm_interfaces', {
  associated_cpp_name : 'ATM_INTERFACES_ANON',
  contents : {'dev/atm/interfaces':['ATM.if', 'AALPod.if'],},
  package : 'catm',
  path : 'dev/atm/interfaces',
  requires : ['atm'],
  section : 'unclassified',
  system_interfaces : ['AALPod.if', 'ATM.if'],
  type : 'bool',
  value : 1,
}),

]


######################################################################
files = {
	'dev/atm/interfaces' : ['AALPod.if', 'ATM.if'],
	'dev/pci/nicstar' : ['nicstar.c', 'nicstar.h', 'nicstar_aal5.c', 'rbuf.h'],
	'dev/pci/oppo' : ['DirectOppo.if', 'direct.c', 'main.c', 'otto.c', 'otto.h', 'otto_aal5.c', 'otto_exec.h', 'otto_hw.c', 'otto_hw.h', 'otto_platform.h', 'otto_regs.h', 'otto_xilinx.c', 'rbuf.h', 'state.h', 'suni.h'],
}
