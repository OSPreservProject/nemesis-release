
############################################################################
# Package database for cfs
#
# This may look like source code, but it is actually the build system
# database format. You may edit this file, but only semantic changes will
# be kept the next time this file is automatically regenerated.
#

from nemclasses import *

items = []
associated_cpp_name = 'associated_cpp_name'
binobject = 'binobject'
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



#######################################################################
# Disks (6 items)


items = items + [
Program('ide', {
  associated_cpp_name : 'IDE',
  contents : {'dev/isa/ide':['ideprivate.h', 'ide.h', 'ide.c'],},
  description : 'IDE disk driver for USD',
  env : {'contexts':32,'defstack':'2k','endpoints':512,'frames':64,'heap':'32k','kernel':1,'sysheap':'8k',},
  makefileflags : {'libio':1,},
  package : 'cfs',
  path : 'dev/isa/ide',
  qos : {'extra':1,'latency':'1ms','period':'1ms','slice':'5us',},
  requires : ['usd', 'isa'],
  section : 'Disks',
  tweakability : 2,
  type : 'quad',
  value : 3,
}),

Program('ncr', {
  associated_cpp_name : 'NCR53C8XX',
  binobject : 'ncr',
  contents : {'dev/scsi/ncr53c8xx':['sc.c', 'ncr53c8xx.h', 'ncr53c8xx.c', 'main.c', 'fake.h'],},
  description : 'NCR 53c8xx chipset SCSI driver for USD',
  env : {'contexts':32,'defstack':'2k','endpoints':512,'frames':64,'heap':'32k','kernel':1,'sysheap':'8k',},
  makefileflags : {'libio':1,},
  package : 'cfs',
  path : 'dev/scsi/ncr53c8xx',
  qos : {'extra':1,'latency':'1ms','period':'1ms','slice':'5us',},
  requires : ['pci', 'usd'],
  section : 'Disks',
  tweakability : 2,
  type : 'quad',
  value : 0,
}),

AutoModule('ramdisk', {
  associated_cpp_name : 'RAMDISK',
  contents : {'mod/fs/ramdisk':['ramdisk.c', 'Makefile'],},
  description : 'RAM Disk (no USD support)',
  env : {'contexts':32,'defstack':'2k','endpoints':512,'frames':64,'heap':'32k','kernel':1,'sysheap':'8k',},
  helptext : 'Provides a RAM disk exporting a BlockDev descended interface.',
  package : 'cfs',
  path : 'mod/fs/ramdisk',
  qos : {'extra':1,'latency':'1ms','period':'1ms','slice':'5us',},
  requires : [],
  section : 'Disks',
  tweakability : 2,
  type : 'quad',
  value : 0,
}),

PureConfiguration('scsi', {
  associated_cpp_name : 'SCSI',
  description : 'SCSI support',
  helptext : 'Support for the standard SCSI commands such as inquire, read and write (various sizes).',
  package : 'cfs',
  requires : ['usd'],
  section : 'Disks',
  tweakability : 1,
  type : 'bool',
  value : 1,
}),

PureConfiguration('scsi_disk', {
  associated_cpp_name : 'SCSI_DISK',
  description : 'Support for SCSI disks',
  helptext : 'Disk specific SCSI code.',
  package : 'cfs',
  requires : ['scsi'],
  section : 'Disks',
  tweakability : 1,
  type : 'bool',
  value : 1,
}),

AutoModule('usd', {
  associated_cpp_name : 'USD',
  contents : {'mod/fs/usd':['usd.h', 'usd.c', 'partition.c', 'io.c', 'disklabel.h'],},
  description : 'User safe disk support',
  helptext : 'Enable user safe disk support. This is mandatory if you want to do anything with user safe disks or local Nemesis filesystems.',
  package : 'cfs',
  path : 'mod/fs/usd',
  requires : [],
  section : 'Disks',
  tweakability : 2,
  type : 'quad',
  value : 3,
}),

]




#######################################################################
# Filesystems (11 items)


items = items + [
AutoModule('ext2', {
  associated_cpp_name : 'EXT2FS',
  binobject : 'ext2',
  contents : {'mod/fs/ext2fs':['uuid.h', 'unparse.c', 'inode.c', 'futils.c', 'fsclient.c', 'ext2mod.h', 'ext2mod.c', 'ext2fs.h', 'ext2.c', 'bcache.c', 'README', 'Ext2.if'],},
  description : 'ext2 filesystem server',
  package : 'cfs',
  path : 'mod/fs/ext2fs',
  requires : ['filesystem_interfaces', 'usd', 'CSIDCTransport'],
  section : 'Filesystems',
  system_interfaces : ['Ext2.if'],
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

InterfaceCollection('filesystem_interfaces', {
  associated_cpp_name : 'FS',
  contents : {'mod/fs/interfaces':['RamDiskMod.if', 'RamDisk.if', 'MountRemote.if', 'MountLocal.if', 'Filesystem.if', 'FSUtil.if', 'FSTypes.if', 'FSDir.if', 'FSClient.if', 'Disk.if', 'BlockDev.if'],},
  description : 'Filesystem interfaces',
  helptext : 'This enables filesystem support (both new style and old style).',
  package : 'cfs',
  path : 'mod/fs/interfaces',
  requires : [],
  section : 'Filesystems',
  system_interfaces : ['Disk.if', 'BlockDev.if', 'RamDisk.if', 'RamDiskMod.if', 'FSClient.if', 'FSTypes.if', 'FSTypes.if', 'FSDir.if', 'MountLocal.if', 'MountRemote.if', 'FSUtil.if'],
  tweakability : 2,
  type : 'bool',
  value : 1,
}),

PureConfiguration('initmount_ext2fs_ide', {
  associated_cpp_name : 'INITMOUNT_EXT2FS_IDE',
  description : 'Enables init.rc code to mount a ext2 filesystem from IDE and necessary infrastructure',
  package : 'cfs',
  requires : ['ide', 'ext2'],
  section : 'Filesystems',
  tweakability : 2,
  type : 'bool',
  value : 0,
}),

PureConfiguration('initmount_ext2fs_scsi', {
  associated_cpp_name : 'INITMOUNT_EXT2FS_SCSI',
  description : 'Enables init.rc code to mount a ext2 filesystem from SCSI and necessary infrastructure',
  package : 'cfs',
  requires : ['scsi', 'ext2'],
  section : 'Filesystems',
  tweakability : 2,
  type : 'bool',
  value : 0,
}),

PureConfiguration('initmount_nfs', {
  associated_cpp_name : 'INITMOUNT_NFS',
  description : 'Enables init.rc code to mount a NFS filesystem and necessary infrastructure',
  package : 'cfs',
  requires : ['nfs'],
  section : 'Filesystems',
  tweakability : 2,
  type : 'bool',
  value : 1,
}),

PureConfiguration('initmount_swapfs_ide', {
  associated_cpp_name : 'INITMOUNT_SWAPFS_IDE',
  description : 'Enables init.rc code to mount a "swap" filesystem from IDE and necessary infrastructure',
  package : 'cfs',
  requires : ['ide', 'swapfs'],
  section : 'Filesystems',
  tweakability : 2,
  type : 'bool',
  value : 0,
}),

PureConfiguration('initmount_swapfs_scsi', {
  associated_cpp_name : 'INITMOUNT_SWAPFS_SCSI',
  description : 'Enables init.rc code to mount a "swap" filesystem from SCSI and necessary infrastructure',
  package : 'cfs',
  requires : ['scsi', 'swapfs'],
  section : 'Filesystems',
  tweakability : 2,
  type : 'bool',
  value : 0,
}),

PureConfiguration('initmount_tmpfs', {
  associated_cpp_name : 'INITMOUNT_TMPFS',
  description : 'Enables init.rc code to mount a NFS filesystem read write and necessary infrastructure',
  package : 'cfs',
  requires : ['nfs'],
  section : 'Filesystems',
  tweakability : 2,
  type : 'bool',
  value : 0,
}),

AutoModule('nfs', {
  associated_cpp_name : 'NFS',
  contents : {'mod/fs/nfs':['nfs_xdr.c', 'nfs_st.h', 'nfs_prot.x', 'nfs_prot.h', 'nfs.c', 'mount_xdr.c', 'mount.x', 'mount.h', 'mount.c', 'fsoffer.h', 'fsclient.c', 'NFS.if'],},
  description : 'NFS filesystem code',
  makefileflags : {'libsocket':1,'libsunrpc':1,},
  package : 'cfs',
  path : 'mod/fs/nfs',
  requires : ['sunrpc', 'filesystem_interfaces'],
  section : 'Filesystems',
  system_interfaces : ['NFS.if'],
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

PureConfiguration('rootfs', {
  associated_cpp_name : 'ROOTFS',
  description : 'Choice of root filesystem',
  helptext : 'Selects root filesystem; used by init.rc.py autogenerator.',
  package : 'cfs',
  requires : [],
  section : 'Filesystems',
  tweakability : 2,
  type : [(1, 'NFS', 'NFS'), (1, 'ext2_ide', 'EXT2FS_IDE'), (1, 'tmpfs', 'TMPFS'), (1, 'ext2_scsi', 'EXT2FS_SCSI')],
  value : 0,
}),

AutoModule('swapfs', {
  associated_cpp_name : 'SWAPFS',
  binobject : 'swapfs',
  contents : {'mod/fs/swapfs':['swapfs.c', 'SwapFS.if', 'Swap.if'],},
  description : 'swap space "filesystem" server',
  package : 'cfs',
  path : 'mod/fs/swapfs',
  requires : ['filesystem_interfaces', 'usd'],
  section : 'Filesystems',
  system_interfaces : ['SwapFS.if', 'Swap.if'],
  tweakability : 1,
  type : 'quad',
  value : 0,
}),

]




#######################################################################
# Standard (1 items)


items = items + [
AutoModule('sd', {
  associated_cpp_name : 'SD',
  contents : {'dev/scsi/disk':['sd.c'],},
  description : 'sd',
  package : 'cfs',
  path : 'dev/scsi/disk',
  requires : ['scsi_disk'],
  section : 'Standard',
  tweakability : 1,
  type : 'quad',
  value : 0,
}),

]




#######################################################################
# unclassified (4 items)


items = items + [
Directory('dev_scsi', {
  associated_cpp_name : 'DEV_SCSI_ANON',
  contents : {'dev/scsi':[],},
  description : 'SCSI device drivers',
  growrecursive : 0,
  package : 'cfs',
  path : 'dev/scsi',
  requires : ['scsi'],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

AutoModule('fsutil', {
  associated_cpp_name : 'FSUTIL_ANON',
  binobject : 'fsutils',
  contents : {'mod/fs/util':['fsutils.c'],},
  description : 'Filesystem utilities: readers and writers',
  package : 'cfs',
  path : 'mod/fs/util',
  requires : ['filesystem_interfaces'],
  section : 'unclassified',
  type : 'quad',
  value : 3,
}),

InterfaceCollection('scsi_disk_interfaces', {
  associated_cpp_name : 'SCSI_DISK_INTERFACES_ANON',
  contents : {'dev/scsi/interfaces':['SCSIDisk.if', 'SCSIController.if', 'SCSICallback.if', 'SCSI.if'],},
  description : 'SCSI disk interfaces',
  package : 'cfs',
  path : 'dev/scsi/interfaces',
  requires : [],
  section : 'unclassified',
  system_interfaces : ['SCSIDisk.if', 'SCSIController.if', 'SCSICallback.if', 'SCSI.if'],
  type : 'bool',
  value : 1,
}),

InterfaceCollection('usd_interfaces', {
  associated_cpp_name : 'USD_INTERFACES_ANON',
  contents : {'sys/interfaces/usd':['USDMod.if', 'USDDrive.if', 'USDCtl.if', 'USDCallback.if', 'USD.if'],},
  package : 'cfs',
  path : 'sys/interfaces/usd',
  requires : ['usd'],
  section : 'unclassified',
  system_interfaces : ['USD.if', 'USDCallback.if', 'USDCtl.if', 'USDDrive.if', 'USDMod.if'],
  type : 'bool',
  value : 1,
}),

]


######################################################################
files = {
	'dev/isa/ide' : ['ide.c', 'ide.h', 'ideprivate.h'],
	'dev/scsi' : [],
	'dev/scsi/disk' : ['sd.c'],
	'dev/scsi/interfaces' : ['SCSI.if', 'SCSICallback.if', 'SCSIController.if', 'SCSIDisk.if'],
	'dev/scsi/ncr53c8xx' : ['fake.h', 'main.c', 'ncr53c8xx.c', 'ncr53c8xx.h', 'sc.c'],
	'mod/fs/ext2fs' : ['Ext2.if', 'README', 'bcache.c', 'ext2.c', 'ext2fs.h', 'ext2mod.c', 'ext2mod.h', 'fsclient.c', 'futils.c', 'inode.c', 'unparse.c', 'uuid.h'],
	'mod/fs/interfaces' : ['BlockDev.if', 'Disk.if', 'FSClient.if', 'FSDir.if', 'FSTypes.if', 'FSUtil.if', 'Filesystem.if', 'MountLocal.if', 'MountRemote.if', 'RamDisk.if', 'RamDiskMod.if'],
	'mod/fs/nfs' : ['NFS.if', 'fsclient.c', 'fsoffer.h', 'mount.c', 'mount.h', 'mount.x', 'mount_xdr.c', 'nfs.c', 'nfs_prot.h', 'nfs_prot.x', 'nfs_st.h', 'nfs_xdr.c'],
	'mod/fs/ramdisk' : ['Makefile', 'ramdisk.c'],
	'mod/fs/swapfs' : ['Swap.if', 'SwapFS.if', 'swapfs.c'],
	'mod/fs/usd' : ['disklabel.h', 'io.c', 'partition.c', 'usd.c', 'usd.h'],
	'mod/fs/util' : ['fsutils.c'],
	'sys/interfaces/usd' : ['USD.if', 'USDCallback.if', 'USDCtl.if', 'USDDrive.if', 'USDMod.if'],
}
