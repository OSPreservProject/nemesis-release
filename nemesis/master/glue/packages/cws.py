
############################################################################
# Package database for cws
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



#######################################################################
# Applications (1 items)


items = items + [
PosixProgram('launcher', {
  associated_cpp_name : 'LAUNCHER',
  contents : {'app/launcher':['launcher.h', 'launcher.gui.py', 'launcher.c', 'buttons.xcf', 'buttons.ppm', 'Makefile'],},
  description : 'Button bar for demos',
  env : {'contexts':32,'defstack':'4k','endpoints':128,'frames':64,'heap':'32k','kernel':0,'actheap':'8k',},
  makefileflags : {'custom':'1','install':['buttons.ppm'],},
  package : 'cws',
  path : 'app/launcher',
  qos : {'extra':1,'latency':'20ms','period':'20ms','slice':'0ns',},
  requires : ['windowing_system'],
  section : 'Applications',
  tweakability : 2,
  type : 'quad',
  value : 2,
}),

PosixProgram('qosbars', {
  associated_cpp_name : 'QOSBARS',
  description : 'Nemesis application qosbars',
  env : {'contexts':32,'defstack':'4k','endpoints':128,'frames':64,'heap':'32k','kernel':1,'actheap':'8k', 'b_env>priv_root' : '<| bars=true, graph=true, dump=false, latency=20000000,  lead=5000000, winwidth=500, graphheight=100, barheight=8|>'},
  package : 'cws',
  path : 'app/qosbars',
  qos : {'extra':0,'latency':'10ms','period':'10ms','slice':'500us',},
  requires : ['windowing_system'],
  section : 'Applications',
  tweakability : 2,
  type : 'quad',
  value : 2,
}),

PosixProgram('carnage', {
  associated_cpp_name : 'CARNAGE',
  description : 'Nemesis application carnage',
  env : {'contexts':32,'defstack':'4k','endpoints':128,'frames':64,'heap':'32k','kernel':0,'actheap':'8k', 'b_env>priv_root' :' <| random=1, width=80, height= 80 |>' },
  makefileflags : {'libws':1,},
  package : 'cws',
  path : 'app/carnage',
  qos : {'extra':1,'latency':'20ms','period':'20ms','slice':'0ns',},
  requires : ['windowing_system'],
  section : 'Applications',
  tweakability : 2,
  type : 'quad',
  value : 2,
}),


PosixProgram('scrdump', {
  associated_cpp_name : 'SCRDUMP',
  contents : {'app/scrdump' : ['scrdump.c'],},
  description : 'Screen dump commandline utility',
  env : {'contexts':32,'defstack':'4k','endpoints':128,'frames':64,'heap':'32k','kernel':0,'actheap':'8k',},
  makefileflags : { },
  package : 'cws',
  path : 'app/scrdump',
  qos : {'extra':1,'latency':'20ms','period':'20ms','slice':'0ns',},
  requires : ['windowing_system'],
  section : 'Applications',
  tweakability : 2,
  type : 'quad',
  value : 2,
}),

PosixProgram('serialmouse', {
  associated_cpp_name : 'SERIALMOUSE',
  contents : {'app/serialmouse' : ['serialmouse.c'],},
  description : 'Serialmouse event translator',
  env : {'contexts':32,'defstack':'4k','endpoints':128,'frames':64,'heap':'32k','kernel':0,'actheap':'8k',},
  makefileflags : { },
  package : 'cws',
  path : 'app/serialmouse',
  qos : {'extra':1,'latency':'20ms','period':'20ms','slice':'0ns',},
  requires : ['windowing_system'],
  section : 'Applications',
  tweakability : 2,
  type : 'quad',
  value : 2,
}),

]




#######################################################################
# Framebuffers (3 items)


items = items + [
PureConfiguration('fbdev', {
  associated_cpp_name : 'FBDEV',
  description : 'Framebuffer support',
  helptext : 'Select this is if you want to have framebuffer support. If not, turning this off will automatically disable all the framebuffer dependent stuff in one sweep.',
  package : 'cws',
  requires : [],
  section : 'Framebuffers',
  tweakability : 2,
  type : 'bool',
  value : 1,
}),

Program('fbmga', {
  associated_cpp_name : 'MGA',
  contents : {'dev/pci/mga':['stub.S', 'main.c', 'cursor32.xpm.h', 'blit_16.c', 'Makefile', 'FB_st.h'],'dev/pci/mga/common_hw':['xf86.h', 'vgaHW.c', 'vga.h', 'pci_stuff.h', 'pci_stuff.c', 'Makefile'],'dev/pci/mga/mga_hw':['vgatables.c', 'modes.c', 'mga_reg.h', 'mga_dac3026.c', 'mga_bios.h', 'mga.h', 'mga.c', 'Makefile'],},
  description : 'Matrox MGA Millenium based framebuffer device driver',
  env : {'contexts':32,'defstack':'4k','endpoints':1024,'frames':64,'heap':'128k','kernel':1, 'actheap' : '32k', 'qos>cpu>neps' : 512 },
  makefileflags : {'custom':1,},
  package : 'cws',
  path : 'dev/pci/mga',
  qos : {'extra':1,'latency':'10ms','period':'10ms','slice':'10us',},
  requires : ['fbdev', 'pci'],
  section : 'Framebuffers',
  tweakability : 2,
  type : 'quad',
  value : 2,
}),

Program('fbs3', {
  associated_cpp_name : 'S3',
  contents : {'dev/pci/s3':['stub.S', 's3.h', 's3.c', 'blit_16.c', 'Makefile', 'FB_st.h'],'dev/pci/s3/xrip':['xripmisc.c', 'xripio.c', 'xrip.c', 'vgaHW.c', 'Makefile'],'dev/pci/s3/xrip/commonhw':['xf86_PCI.h', 'xf86_PCI.c', 'xf86_HWlib.h', 'Ti302X.h', 'Ti3026clk.c', 'Ti3025clk.c', 'SlowBcopy.c', 'SlowBcopy.S', 'STG1703clk.c', 'SC11412.h', 'SC11412.c', 'S3gendac.h', 'S3gendac.c', 'Makefile', 'IODelay.c', 'IODelay.S', 'ICS2595.h', 'ICS2595.c', 'ICD2061A.h', 'IBMRGB.h', 'IBMRGB.c', 'I2061Aset.c', 'I2061Acal.c', 'I2061Aalt.c', 'CirrusClk.h', 'CirrusClk.c', 'Ch8391clk.c', 'BUSmemcpy.c', 'BUSmemcpy.S', 'ATTDac.c'],'dev/pci/s3/xrip/h':['xrip.h', 'xf86_Option.h', 'xf86_OSlib.h', 'xf86_Config.h', 'compiler.h', 'assyntax.h', 'Ti302X.h', 'Makefile', 'IBMRGB.h'],'dev/pci/s3/xrip/hc':['pegcursor2.xpm.c', 'Ti3026Curs.c', 'Makefile', 'IBMRGBCurs.c'],'dev/pci/s3/xrip/hnull':['xf86Procs.h', 'vga.h', 'site.h', 'servermd.h', 'scrnintstr.h', 's3linear.h', 'pixmapstr.h', 'misc.h', 'input.h', 'fontstruct.h', 'cfb32.h', 'cfb16.h', 'cfb.h', 'Xmd.h', 'X.h', 'Makefile'],'dev/pci/s3/xrip/s3':['s3misc.c', 's3linear.h', 's3init.c', 's3im.c', 's3consts.h', 's3_generic.c', 's3ELSA.h', 's3ELSA.c', 's3Conf.c', 's3Bt485.h', 's3.h', 's3.c', 'regs3.h', 'mmio_928.c', 'XF86_S3.c', 'Makefile'],},
  description : 'S3 968 based framebuffer device driver',
  env : {'contexts':32,'defstack':'4k','endpoints':1024,'frames':64,'heap':'32k','kernel':1,'actheap':'32k','qos>cpu>neps' : 512 },
  makefileflags : {'custom':1,},
  package : 'cws',
  path : 'dev/pci/s3',
  qos : {'extra':1,'latency':'10ms','period':'10ms','slice':'10us',},
  requires : ['fbdev', 'pci'],
  section : 'Framebuffers',
  tweakability : 2,
  type : 'quad',
  value : 2,
}),

]




#######################################################################
# Services (7 items)


items = items + [
AutoModule('ClientRender', {
  associated_cpp_name : 'CREND',
  contents : {'mod/ws/crend':['pixmap.c', 'fontindex.c', 'fontdata.c', 'displaymod.c', 'display.c', 'crend.c', 'CRend_st.h', 'CRendDisplay_st.h', 'CRendDisplayMod_st.h'],},
  description : 'ClientRender',
  package : 'cws',
  path : 'mod/ws/crend',
  requires : ['windowing_system'],
  section : 'Services',
  tweakability : 1,
  type : 'quad',
  value : 2,
}),

AutoModule('wssvr', {
  associated_cpp_name : 'WSSVR',
  description : 'Windowing system server module',
  package : 'cws',
  path : 'mod/ws/wssvr',
  requires : ['windowing_system'],
  section : 'Services',
  tweakability : 1,
  type : 'quad',
  value : 2
}),

AutoModule('FBBlitMod', {
  associated_cpp_name : 'FBBLIT',
  contents : {'mod/fb/fbblit':['FBBlitMod.c'],},
  description : 'FBBlitMod',
  package : 'cws',
  path : 'mod/fb/fbblit',
  requires : ['windowing_system'],
  section : 'Services',
  tweakability : 1,
  type : 'quad',
  value : 2,
}),

AutoModule('Region', {
  associated_cpp_name : 'REGION',
  contents : {'mod/ws/region':['Region.c'],},
  description : 'Region',
  package : 'cws',
  path : 'mod/ws/region',
  requires : ['windowing_system'],
  section : 'Services',
  tweakability : 1,
  type : 'quad',
  value : 2,
}),

AutoModule('SWM', {
  associated_cpp_name : 'SWM',
  contents : {'mod/ws/savewm':['savewm.c'],},
  description : 'SWM',
  package : 'cws',
  path : 'mod/ws/savewm',
  requires : ['windowing_system'],
  section : 'Services',
  tweakability : 2,
  type : 'quad',
  value : 2,
}),

PureConfiguration('backdrop', {
  associated_cpp_name : 'BACKDROP',
  description : 'Backdrop code for the wssvr',
  helptext : 'Enable this to cause a backdrop to be displayed in the windowing system',
  package : 'cws',
  requires : [],
  section : 'Services',
  tweakability : 2,
  type : 'bool',
  value : 1,
}),

PureConfiguration('windowing_system', {
  associated_cpp_name : 'WS',
  description : 'Windowing system support',
  helptext : 'Disable this to disable the windowing system.',
  package : 'cws',
  requires : ['fbdev'],
  section : 'Services',
  tweakability : 2,
  type : 'quad',
  value : 2,
}),

AutoModule('wsterm', {
  associated_cpp_name : 'WSTERM',
  contents : {'mod/ws/wsterm':['wsterm.c', 'windows.c', 'strings.c', 'rectangles.c', 'points.c', 'lines.c', 'font.h', 'events.c', 'display.c', 'default8x16.c', 'WSprivate.h'],},
  description : 'wsterm',
  makefileflags : {'libws':1,},
  package : 'cws',
  path : 'mod/ws/wsterm',
  requires : ['windowing_system'],
  section : 'Services',
  tweakability : 1,
  type : 'quad',
  value : 2,
}),

]




#######################################################################
# WS (1 items)


items = items + [
PureConfiguration('mesa', {
  associated_cpp_name : 'MESA',
  description : 'Static stateful Mesa OpenGL library',
  helptext : 'Slow to build. Required for a few applications only.',
  package : 'cws',
  requires : ['windowing_system'],
  section : 'WS',
  tweakability : 1,
  type : 'bool',
  value : 0,
}),

]




#######################################################################
# unclassified (4 items)


items = items + [
InterfaceCollection('framebuffer_interfaces', {
  associated_cpp_name : 'FRAMEBUFFER_INTERFACES_ANON',
  contents : {'mod/fb/interfaces':['Region.if', 'HWCur.if', 'FBBlitMod.if', 'FBBlit.if', 'FB.if'],},
  description : 'Framebuffer interfaces',
  package : 'cws',
  path : 'mod/fb/interfaces',
  requires : [],
  section : 'unclassified',
  system_interfaces : ['FB.if', 'FBBlit.if', 'FBBlitMod.if', 'HWCur.if', 'Region.if'],
  type : 'bool',
  value : 1,
}),

Directory('mod_fb', {
  associated_cpp_name : 'MOD_FB_ANON',
  contents : {'mod/fb':[],},
  growrecursive : 0,
  package : 'cws',
  path : 'mod/fb',
  requires : [],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

Directory('mod_ws', {
  associated_cpp_name : 'MOD_WS_ANON',
  contents : {'mod/ws':['README'],},
  description : 'Windowing system modules',
  growrecursive : 0,
  package : 'cws',
  path : 'mod/ws',
  requires : [],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

InterfaceCollection('windowing_system_interfaces', {
  associated_cpp_name : 'WINDOWING_SYSTEM_INTERFACES_ANON',
  contents : {'mod/ws/interfaces':['WSF.if', 'WS.if', 'WMMod.if', 'WM.if', 'CRendPixmap.if', 'CRendDisplayMod.if', 'CRendDisplay.if', 'CRend.if'],},
  description : 'Windowing System',
  package : 'cws',
  path : 'mod/ws/interfaces',
  requires : ['windowing_system'],
  section : 'unclassified',
  system_interfaces : ['WS.if', 'WM.if', 'WMMod.if', 'CRend.if', 'CRendPixmap.if', 'CRendDisplay.if', 'CRendDisplayMod.if', 'WSF.if'],
  type : 'bool',
  value : 1,
}),

]


######################################################################
files = {
	'app/launcher' : ['Makefile', 'buttons.ppm', 'buttons.xcf', 'launcher.c', 'launcher.gui.py', 'launcher.h'],
	'dev/pci/mga' : ['FB_st.h', 'Makefile', 'blit_16.c', 'cursor32.xpm.h', 'main.c', 'stub.S'],
	'dev/pci/mga/common_hw' : ['Makefile', 'pci_stuff.c', 'pci_stuff.h', 'vga.h', 'vgaHW.c', 'xf86.h'],
	'dev/pci/mga/mga_hw' : ['Makefile', 'mga.c', 'mga.h', 'mga_bios.h', 'mga_dac3026.c', 'mga_reg.h', 'modes.c', 'vgatables.c'],
	'dev/pci/s3' : ['FB_st.h', 'Makefile', 'blit_16.c', 's3.c', 's3.h', 'stub.S'],
	'dev/pci/s3/xrip' : ['Makefile', 'vgaHW.c', 'xrip.c', 'xripio.c', 'xripmisc.c'],
	'dev/pci/s3/xrip/commonhw' : ['ATTDac.c', 'BUSmemcpy.S', 'BUSmemcpy.c', 'Ch8391clk.c', 'CirrusClk.c', 'CirrusClk.h', 'I2061Aalt.c', 'I2061Acal.c', 'I2061Aset.c', 'IBMRGB.c', 'IBMRGB.h', 'ICD2061A.h', 'ICS2595.c', 'ICS2595.h', 'IODelay.S', 'IODelay.c', 'Makefile', 'S3gendac.c', 'S3gendac.h', 'SC11412.c', 'SC11412.h', 'STG1703clk.c', 'SlowBcopy.S', 'SlowBcopy.c', 'Ti3025clk.c', 'Ti3026clk.c', 'Ti302X.h', 'xf86_HWlib.h', 'xf86_PCI.c', 'xf86_PCI.h'],
	'dev/pci/s3/xrip/h' : ['IBMRGB.h', 'Makefile', 'Ti302X.h', 'assyntax.h', 'compiler.h', 'xf86_Config.h', 'xf86_OSlib.h', 'xf86_Option.h', 'xrip.h'],
	'dev/pci/s3/xrip/hc' : ['IBMRGBCurs.c', 'Makefile', 'Ti3026Curs.c', 'pegcursor2.xpm.c'],
	'dev/pci/s3/xrip/hnull' : ['Makefile', 'X.h', 'Xmd.h', 'cfb.h', 'cfb16.h', 'cfb32.h', 'fontstruct.h', 'input.h', 'misc.h', 'pixmapstr.h', 's3linear.h', 'scrnintstr.h', 'servermd.h', 'site.h', 'vga.h', 'xf86Procs.h'],
	'dev/pci/s3/xrip/s3' : ['Makefile', 'XF86_S3.c', 'mmio_928.c', 'regs3.h', 's3.c', 's3.h', 's3Bt485.h', 's3Conf.c', 's3ELSA.c', 's3ELSA.h', 's3_generic.c', 's3consts.h', 's3im.c', 's3init.c', 's3linear.h', 's3misc.c'],
	'mod/fb' : [],
	'mod/fb/fbblit' : ['FBBlitMod.c'],
	'mod/fb/interfaces' : ['FB.if', 'FBBlit.if', 'FBBlitMod.if', 'HWCur.if', 'Region.if'],
	'mod/ws' : ['README'],
	'mod/ws/crend' : ['CRendDisplayMod_st.h', 'CRendDisplay_st.h', 'CRend_st.h', 'crend.c', 'display.c', 'displaymod.c', 'fontdata.c', 'fontindex.c', 'pixmap.c'],
	'mod/ws/interfaces' : ['CRend.if', 'CRendDisplay.if', 'CRendDisplayMod.if', 'CRendPixmap.if', 'WM.if', 'WMMod.if', 'WS.if', 'WSF.if'],
	'mod/ws/region' : ['Region.c'],
	'mod/ws/savewm' : ['savewm.c'],
	'mod/ws/wsterm' : ['WSprivate.h', 'default8x16.c', 'display.c', 'events.c', 'font.h', 'lines.c', 'points.c', 'rectangles.c', 'strings.c', 'windows.c', 'wsterm.c'],
}
