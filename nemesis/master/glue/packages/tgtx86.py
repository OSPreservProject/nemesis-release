
############################################################################
# Package database for tgtx86
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
exports = 'exports'
growrecursive = 'growrecursive'
helptext = 'helptext'
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

items = [
PureConfiguration('ix86_arch', {
  associated_cpp_name : 'IX86_ARCH',
  description : 'Machine property ARM_ARCH',
  package : 'tgtx86',
  requires : [],
  section : 'Kernel',
  tweakability : 1,
  type : 'bool',
  value : 1,
}),

NTSC('ntsc_ix86', {
  associated_cpp_name : 'NTSC_IX86_ANON',
  binobject : 'NTSCix86',
  makefileflags : {'custom':1,},
  package : 'tgtix86',
  path : 'ntsc/ix86',
  requires : ['ix86_arch'],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),  

Directory('boot_images_intel', {
  associated_cpp_name : 'BOOT_IMAGES_INTEL_ANON',
  contents : {'boot/images/intel':['video.S', 'setup.S', 'nemesis.gdb', 'misc.c', 'linkage.h', 'io.h', 'inflate.c', 'head.S', 'bootsect.S', 'Makefile'],},
  description : 'boot image for intel',
  makefileflags : {'custom':1,},
  package : 'tgtx86',
  path : 'boot/images/intel',
  requires : ['build_image', 'intel_target'],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

AutoModule('NemesisVPix86', {
  associated_cpp_name : 'NEMESISVPIX86',
  contents : {'mod/nemesis/ix86':['VP.c', 'Time.c'],},
  description : 'NemesisVPix86',
  package : 'tgtx86',
  path : 'mod/nemesis/ix86',
  requires : ['ix86_arch'],
  section : 'Standard',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

Directory('h_intel', {
  associated_cpp_name : 'H_INTEL_ANON',
  contents : {'h/intel':['veneer_tgt.h', 'timer.h', 'syscall.h', 'pip.h', 'ntsc_tgt.h', 'nemesis_tgt.h', 'math_inline.h', 'kernel_tgt.h', 'kernel_st.h', 'kernel_config.h', 'jmp_buf.h', 'irq.h', 'io.h', 'dcb.off.h', 'dcb.h', 'VPMacros.h', 'Type.ih'],},
  description : 'intel header files',
  package : 'ccore',
  path : 'h/intel',
  requires : ['intel_target'],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

Directory('h_ix86', {
  associated_cpp_name : 'H_IX86_ANON',
  contents : {'h/ix86':['veneer.h', 'stdarg.h', 'processor.h', 'platform.h', 'multiboot.h', 'mmu_tgt.h', 'limits.h', 'interrupt.h', 'float.h', 'elf.h', 'context.h', 'bitops.h'],'h/ix86/asm':['stackframe.h', 'ptrace.h', 'elf.h'],},
  description : 'ix86 header files',
  package : 'ccore',
  path : 'h/ix86',
  requires : ['ix86_arch'],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

PureConfiguration('intel_smp_target', {
  associated_cpp_name : 'INTEL_SMP_TARGET',
  description : 'Machine property INTEL_SMP_TARGET',
  requires : [],
  section : 'Kernel',
  tweakability : 1,
  type : 'bool',
  value : 1,
}),

PureConfiguration('intel_target', {
  associated_cpp_name : 'INTEL_TARGET',
  description : 'Machine property INTEL_TARGET',
  requires : [],
  section : 'Kernel',
  tweakability : 1,
  type : 'bool',
  value : 1,
}),

PureConfiguration('ix86_linux_plat', {
  associated_cpp_name : 'IX86_LINUX_PLAT',
  description : 'Machine property IX86_LINUX_PLAT',
  requires : [],
  section : 'Kernel',
  tweakability : 1,
  type : 'bool',
  value : 1,
}),

Directory('h_intel_smp', {
  description : 'intel_smp header files',
  path : 'h/intel_smp',
  requires : ['intel_smp_target'],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

]

