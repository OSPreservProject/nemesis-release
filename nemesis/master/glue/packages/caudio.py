
############################################################################
# Package database for caudio
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
helptext = 'helptext'
makefileflags = 'makefileflags'
package = 'package'
path = 'path'
qos = 'qos'
requires = 'requires'
section = 'section'
type = 'type'
value = 'value'



#######################################################################
# Audio (5 items)


items = items + [
PosixProgram('amp', {
  associated_cpp_name : 'AMP_ANON',
  contents : {'app/amp':['util.c', 'transform.h', 'transform.c', 'rtbuf.h', 'rtbuf.c', 'readppm.c', 'proto.h', 'position.h', 'position.c', 'nemstuff.c', 'misc2.h', 'layer3.h', 'layer3.c', 'layer2.h', 'misc2.c', 'layer2.c', 'huffman.h', 'huffman.c', 'guicontrol.h', 'guicontrol.c', 'getopt1.c', 'getopt.h', 'getopt.c', 'getdata.h', 'getdata.c', 'getbits.h', 'getbits.c', 'formats.h', 'formats.c', 'dump.h', 'dump.c', 'controldata.h', 'audioIO_aubuf.c', 'audioIO.h', 'audioIO.c', 'audio.h', 'audio.c', 'args.c', 'amp.h'],},
  description : 'Nemesis application amp',
  env : {'contexts':32,'defstack':'4k','endpoints':128,'frames':96,'heap':'64k','kernel':0,'sysheap':'16k',},
  makefileflags : {'install':['amp.ppm'],},
  package : 'caudio',
  path : 'app/amp',
  qos : {'extra':1,'latency':'20ms','period':'20ms','slice':'0ns',},
  requires : ['aubuf_audio'],
  section : 'Audio',
  type : 'quad',
  value : 2,
}),

PureConfiguration('aubuf_audio', {
  associated_cpp_name : 'AUBUF_AUDIO',
  description : 'AuBuf audio support',
  helptext : 'Enables aubuf_audio driver, modules and applications. Intel only, for the moment.',
  package : 'caudio',
  requires : ['intel_target'],
  section : 'Audio',
  type : 'bool',
  value : 0,
}),

AutoModule('aumod', {
  associated_cpp_name : 'AUMOD_ANON',
  contents : {'mod/au':['AuPlaybackWriter_st.h', 'AuPlaybackWriter.c', 'AuMod.c', 'AsyncAuPlaybackWriter.c'],},
  description : 'Nemesis application aumod',
  package : 'caudio',
  path : 'mod/au',
  requires : ['aubuf_audio'],
  section : 'Audio',
  type : 'quad',
  value : 3,
}),

PosixProgram('lksound', {
  associated_cpp_name : 'LKSOUND_ANON',
  contents : {'dev/isa/lksound':['vidc.h', 'v_midi.h', 'ulaw.h', 'tuning.h', 'soundvers.h', 'soundmodule.h', 'sound_firmware.h', 'sound_config.h', 'sound_calls.h', 'softoss.h', 'sb_mixer.h', 'sb_mixer.c', 'sb_midi.c', 'sb_common.c', 'sb_audio.c', 'sb.h', 'os.h', 'opl3.h', 'midi_synth.h', 'midi_ctrl.h', 'lksound.h', 'lksound.c', 'iwmem.h', 'gus_linearvol.h', 'gus_hw.h', 'finetune.h', 'dmasound.h', 'dev_table.h', 'dev_table.c', 'coproc.h', 'aubuf.h', 'ad1848_mixer.h'],},
  description : 'Nemesis application lksound',
  env : {'contexts':32,'defstack':'4k','endpoints':128,'frames':96,'heap':'64k','kernel':0,'sysheap':'16k',},
  package : 'caudio',
  path : 'dev/isa/lksound',
  qos : {'extra':1,'latency':'20ms','period':'20ms','slice':'0ns',},
  requires : ['aubuf_audio', 'isa'],
  section : 'Audio',
  type : 'quad',
  value : 2,
}),

PosixProgram('timidity', {
  associated_cpp_name : 'TIMIDITY_ANON',
  contents : {'app/timidity':['timidity.obj', 'timidity.c', 'tables.h', 'tables.c', 'resample.h', 'resample.c', 'readppm.c', 'readmidi.h', 'readmidi.c', 'playmidi.h', 'playmidi.c', 'output.h', 'output.c', 'nemmain.c', 'nemesis_c.c', 'nemesis_a.c', 'mix.h', 'mix.c', 'misc.tcl', 'instrum.h', 'instrum.c', 'filter.h', 'filter.c', 'controls.h', 'controls.c', 'config.h', 'common.h', 'common.c', 'README', 'Makefile', 'COPYING'],},
  description : 'Nemesis application timidity',
  env : {'contexts':32,'defstack':'16k','endpoints':128,'frames':96,'heap':'64k','kernel':0,'sysheap':'16k',},
  makefileflags : {'custom':1,},
  package : 'caudio',
  path : 'app/timidity',
  qos : {'extra':1,'latency':'2ms','period':'2ms','slice':'10us',},
  requires : ['aubuf_audio'],
  section : 'Audio',
  type : 'quad',
  value : 2,
}),

PosixProgram('aumixer', {
  associated_cpp_name : 'AUMIXER_ANON',
  description : 'Nemesis application aumixer',
  env : {'contexts':32,'defstack':'4k','endpoints':128,'frames':96,'heap':'64k',
'kernel':0,'sysheap':'16k',},
  path : 'app/aumixer',
  qos : {'extra':1,'latency':'20ms','period':'20ms','slice':'0ns',},
  requires : ['aubuf_audio'],
  section : 'Audio',
  system_interfaces : ['MixerRegister.if'],
  type : 'quad',
  value : 2,
}),


]


######################################################################
files = {
	'app/amp' : ['amp.h', 'args.c', 'audio.c', 'audio.h', 'audioIO.c', 'audioIO.h', 'audioIO_aubuf.c', 'controldata.h', 'dump.c', 'dump.h', 'formats.c', 'formats.h', 'getbits.c', 'getbits.h', 'getdata.c', 'getdata.h', 'getopt.c', 'getopt.h', 'getopt1.c', 'guicontrol.c', 'guicontrol.h', 'huffman.c', 'huffman.h', 'layer2.c', 'layer2.h', 'layer3.c', 'layer3.h', 'misc2.c', 'misc2.h', 'nemstuff.c', 'position.c', 'position.h', 'proto.h', 'readppm.c', 'rtbuf.c', 'rtbuf.h', 'transform.c', 'transform.h', 'util.c'],
	'app/timidity' : ['COPYING', 'Makefile', 'README', 'common.c', 'common.h', 'config.h', 'controls.c', 'controls.h', 'filter.c', 'filter.h', 'instrum.c', 'instrum.h', 'misc.tcl', 'mix.c', 'mix.h', 'nemesis_a.c', 'nemesis_c.c', 'nemmain.c', 'output.c', 'output.h', 'playmidi.c', 'playmidi.h', 'readmidi.c', 'readmidi.h', 'readppm.c', 'resample.c', 'resample.h', 'tables.c', 'tables.h', 'timidity.c', 'timidity.obj'],
	'dev/isa/lksound' : ['ad1848_mixer.h', 'aubuf.h', 'coproc.h', 'dev_table.c', 'dev_table.h', 'dmasound.h', 'finetune.h', 'gus_hw.h', 'gus_linearvol.h', 'iwmem.h', 'lksound.c', 'lksound.h', 'midi_ctrl.h', 'midi_synth.h', 'opl3.h', 'os.h', 'sb.h', 'sb_audio.c', 'sb_common.c', 'sb_midi.c', 'sb_mixer.c', 'sb_mixer.h', 'softoss.h', 'sound_calls.h', 'sound_config.h', 'sound_firmware.h', 'soundmodule.h', 'soundvers.h', 'tuning.h', 'ulaw.h', 'v_midi.h', 'vidc.h'],
	'mod/au' : ['AsyncAuPlaybackWriter.c', 'AuMod.c', 'AuPlaybackWriter.c', 'AuPlaybackWriter_st.h'],
}
