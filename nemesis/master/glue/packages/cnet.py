
############################################################################
# Package database for cnet
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
# Net (15 items)


items = items + [
AutoModule('arp', {
  associated_cpp_name : 'PROTO_ARP',
  contents : {'mod/net/arp':['ARP.c'],},
  description : 'ARP protocol code',
  helptext : 'Address Resolution Protocol module implementation. Essential for ethernet networking.',
  package : 'cnet',
  path : 'mod/net/arp',
  requires : ['networking'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

PureConfiguration('atm', {
  associated_cpp_name : 'ATM',
  description : 'ATM support',
  helptext : 'Disabling this should disable all standard ATM support.',
  package : 'cnet',
  requires : [],
  section : 'Net',
  tweakability : 1,
  type : 'bool',
  value : 1,
}),

StaticLibrary('bsd_sockets', {
  associated_cpp_name : 'SOCKET',
  contents : {'lib/static/socket':['socket.c', 'Makefile'],},
  description : 'BSD sockets support',
  helptext : 'BSD style sockets over the native network stack. Used by the SUNRPC code and TFTP code. It provides both TCP and UDP, but only UDP has had much testing.  It is a stateless static library that is linked   with each application that uses it at run time.',
  makefileflags : {'custom':1,},
  package : 'cnet',
  path : 'lib/static/socket',
  requires : ['proto_common'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

AutoModule('dns', {
  associated_cpp_name : 'NET_DNS',
  contents : {'mod/net/DNS':['DNSMod.if', 'DNS.if', 'DNS.c'],},
  description : 'Internet DNS query code',
  helptext : 'Provides a module containg code to query DNS servers.',
  makefileflags : {'libsocket':1,},
  package : 'cnet',
  path : 'mod/net/DNS',
  requires : [],
  section : 'Net',
  system_interfaces : ['DNS.if', 'DNSMod.if'],
  tweakability : 2,
  type : 'quad',
  value : 3,
}),

Program('eth3c509', {
  associated_cpp_name : '3C509',
  contents : {'dev/isa/3c509':['3c509_st.h', '3c509.h', '3c509.c'],},
  description : '3c509 ethernet driver',
  env : {'contexts':32,'defstack':'2k','endpoints':512,'frames':64,'heap':'32k','kernel':1,'sysheap':'8k',},
  makefileflags : {'libio':1,},
  package : 'cnet',
  path : 'dev/isa/3c509',
  qos : {'extra':1,'latency':'20ms','period':'20ms','slice':'200us',},
  requires : ['networking', 'isa'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

Program('ethde4x5', {
  associated_cpp_name : 'DE4X5',
  contents : {'dev/pci/de4x5':['fake.h', 'de4x5.h', 'de4x5.c'],},
  description : 'DE4x5 ethernet driver',
  env : {'contexts':32,'defstack':'2k','endpoints':512,'frames':64,'heap':'32k','kernel':1,'sysheap':'8k',},
  makefileflags : {'libio':1,},
  package : 'cnet',
  path : 'dev/pci/de4x5',
  qos : {'extra':1,'latency':'10ms','period':'10ms','slice':'200us',},
  requires : ['networking', 'pci'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

PureConfiguration('net_console', {
  associated_cpp_name : 'NET_CONSOLE',
  description : 'UDP network console support',
  helptext : 'UDP Network console. Equips each domain with a thread and redirectable input and output streams. Then, by prodding the machine with a UDP packet that domain will switch its input and output to  UDP packets.',
  package : 'cnet',
  requires : ['networking'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 0,
}),

AutoModule('netterm', {
  associated_cpp_name : 'NET_TERM',
  contents : {'mod/net/term':['netterm.c'],},
  description : 'UDP network terminal support',
  helptext : 'Enables an insecure, UDP based network terminal system, used by nash.',
  makefileflags : {'libsocket':1,},
  package : 'cnet',
  path : 'mod/net/term',
  requires : ['networking'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

Directory('networking', {
  associated_cpp_name : 'NET',
  contents : {'mod/net':[],},
  description : 'Networking support',
  growrecursive : 0,
  helptext : 'Disabling this should disable all standard IP networking support.',
  package : 'cnet',
  path : 'mod/net',
  requires : [],
  section : 'Net',
  tweakability : 2,
  type : 'quad',
  value : 3,
}),

PureConfiguration('proto_common', {
  associated_cpp_name : 'PROTO_COMMON',
  description : 'common (ethernet, IP, UDP) protocol code',
  helptext : 'Common networking protocols almost everyone will want. It includes Ethernet framing protocol module implementation, IP module, implementing the IP protocol (including ICMP) and  UDP module implementation.',
  package : 'cnet',
  requires : ['networking'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

PureConfiguration('proto_ipv6', {
  associated_cpp_name : 'PROTO_IPV6',
  description : 'Placeholder support for IPv6',
  helptext : 'Placeholder support for IPv6.',
  package : 'cnet',
  requires : ['networking'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 0,
}),

PureConfiguration('proto_tcp', {
  associated_cpp_name : 'PROTO_TCP',
  description : 'TCP protocol code (experimental)',
  helptext : 'Enables experimental TCP support.',
  package : 'cnet',
  requires : ['networking'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

PureConfiguration('proto_tftp', {
  associated_cpp_name : 'PROTO_TFTP',
  description : 'TFTP protocol code',
  helptext : 'Enables TFTP supoprt. This is largely not necessary any more.',
  package : 'cnet',
  requires : ['networking'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 0,
}),

AutoModule('resolver', {
  associated_cpp_name : 'NET_RESOLVER',
  contents : {'mod/net/resolver':['resolver.c', 'ResolverMod.if', 'Resolver.if'],},
  description : 'Internet DNS resolution code',
  helptext : 'Provides a module containg code to do fully qualified domain naming.',
  makefileflags : {'libsocket':1,},
  package : 'cnet',
  path : 'mod/net/resolver',
  requires : ['dns'],
  section : 'Net',
  system_interfaces : ['Resolver.if', 'ResolverMod.if'],
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

StaticLibrary('sunrpc', {
  associated_cpp_name : 'SUNRPC',
  contents : {'lib/static/sunrpc':['xdr_reference.c', 'xdr_mem.c', 'xdr_array.c', 'xdr.c', 'svc_udp.c', 'svc_auth.c', 'svc.c', 'rpc_prot.c', 'rpc_commondata.c', 'rpc_callmsg.c', 'pmap_prot.c', 'pmap_getport.c', 'pmap_clnt.c', 'clnt_udp.c', 'clnt_tcp.c', 'clnt_perror.c', 'authunix_prot.c', 'auth_none.c', 'Makefile'],'lib/static/sunrpc/h':['Makefile'],'lib/static/sunrpc/h/rpc':['xdr.h', 'types.h', 'svc_auth.h', 'svc.h', 'rpc_msg.h', 'rpc.h', 'pmap_prot.h', 'pmap_clnt.h', 'clnt.h', 'auth_unix.h', 'auth.h', 'Makefile'],},
  description : 'Sun RPC system',
  helptext : 'Suns Remote Procedure Call library.  Used by a few things, such as the NFS code. It is a stateless static library that is linked with each application that uses it at run time.',
  makefileflags : {'custom':1,},
  package : 'cnet',
  path : 'lib/static/sunrpc',
  requires : ['bsd_sockets'],
  section : 'Net',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

]




#######################################################################
# Networking (3 items)


items = items + [
AutoModule('bpf', {
  associated_cpp_name : 'BPF',
  contents : {'mod/net/bpf':['BPF.c'],},
  description : 'BPF module, mostly obsolete',
  makefileflags : {'custom':1,},
  package : 'cnet',
  path : 'mod/net/bpf',
  requires : ['networking'],
  section : 'Networking',
  tweakability : 1,
  type : 'quad',
  value : 0,
}),

AutoModule('netif', {
  associated_cpp_name : 'NETIF',
  contents : {'mod/net/netif':['pool.h', 'pool.c', 'netif.c', 'Makefile', 'FragPool.if'],},
  description : 'netif',
  makefileflags : {'custom':1,},
  package : 'cnet',
  path : 'mod/net/netif',
  requires : ['networking'],
  section : 'Networking',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

AutoModule('tftp', {
  associated_cpp_name : 'TFTP',
  contents : {'mod/net/tftp':['tftp.c', 'Makefile'],},
  description : 'TFTP module, mostly obsolete',
  makefileflags : {'custom':1,},
  package : 'cnet',
  path : 'mod/net/tftp',
  requires : ['networking'],
  section : 'Networking',
  tweakability : 1,
  type : 'quad',
  value : 0,
}),

]




#######################################################################
# Standard (5 items)


items = items + [
AutoModule('LMPF', {
  associated_cpp_name : 'LMPF',
  contents : {'mod/net/lmpf':['pf.c'],},
  description : 'LMPF',
  package : 'cnet',
  path : 'mod/net/lmpf',
  requires : ['networking'],
  section : 'Standard',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

AutoModule('TCPSocket', {
  associated_cpp_name : 'TCPSOCKET',
  contents : {'mod/net/socket/tcp':['tcpu_xsum.c', 'tcpu_var.h', 'tcpu_usrreq.c', 'tcpu_timer.h', 'tcpu_timer.c', 'tcpu_subr.c', 'tcpu_seq.h', 'tcpu_output.c', 'tcpu_input.c', 'tcpu.h', 'tcpip.h', 'tcp_fsm.h', 'ip_var.h', 'ip.h', 'inet.h', 'event.h'],},
  description : 'TCPSocket',
  makefileflags : {'libsocket':1,},
  package : 'cnet',
  path : 'mod/net/socket/tcp',
  requires : ['proto_tcp'],
  section : 'Standard',
  tweakability : 1,
  type : 'quad',
  value : 2,
}),

AutoModule('UDPSocket', {
  associated_cpp_name : 'UDPSOCKET',
  contents : {'mod/net/socket/udp':['udp_socket.h', 'udp_socket.c'],},
  description : 'UDPSocket',
  package : 'cnet',
  path : 'mod/net/socket/udp',
  requires : ['networking'],
  section : 'Standard',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

AutoModule('flowman', {
  associated_cpp_name : 'FLOWMAN',
  contents : {'mod/net/flowman':['udp_var.h', 'udp_input.c', 'udp.h', 'tcpip.h', 'tcp_var.h', 'tcp_input.c', 'tcp.h', 'proto_ip.h', 'proto_ip.c', 'prot_stat.h', 'kill.h', 'kill.c', 'ipconf.c', 'ip_var.h', 'ip_output.c', 'ip_input.c', 'ip_icmp.h', 'ip_icmp.c', 'ip.h', 'in.h', 'if_ether.h', 'icmp_var.h', 'icmp.c', 'hosts.c', 'fm_constants.h', 'flowman_st.h', 'flowman.c', 'ethernet.h', 'ether_output.c', 'ether_input.c', 'bootp.c'],},
  description : 'flowman',
  makefileflags : {'donotinstall':1,'localrules':1,},
  package : 'cnet',
  path : 'mod/net/flowman',
  requires : ['networking'],
  section : 'Standard',
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

AutoModule('netcons', {
  associated_cpp_name : 'NETCONS',
  contents : {'mod/nemesis/netcons':['netcons.c', 'WrRedirMod.if', 'WrRedir.if', 'RdRedirMod.if', 'RdRedir.if'],},
  description : 'netcons',
  makefileflags : {'libsocket':1,},
  package : 'cnet',
  path : 'mod/nemesis/netcons',
  requires : ['net_console'],
  section : 'Standard',
  system_interfaces : ['WrRedir.if', 'WrRedirMod.if', 'RdRedir.if', 'RdRedirMod.if'],
  tweakability : 1,
  type : 'quad',
  value : 3,
}),

]




#######################################################################
# unclassified (9 items)


items = items + [
Directory('dev_atm', {
  associated_cpp_name : 'DEV_ATM_ANON',
  contents : {'dev/atm':[],},
  growrecursive : 0,
  package : 'cnet',
  path : 'dev/atm',
  requires : [],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

Directory('mod_net', {
  associated_cpp_name : 'MOD_NET_ANON',
  contents : {'mod/net':[],},
  description : 'Networking modules',
  growrecursive : 0,
  package : 'cnet',
  path : 'mod/net',
  requires : [],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

Directory('mod_net_socket', {
  associated_cpp_name : 'MOD_NET_SOCKET_ANON',
  contents : {'mod/net/socket':[],},
  description : 'Networking socket modules',
  growrecursive : 0,
  package : 'cnet',
  path : 'mod/net/socket',
  requires : [],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

AutoModule('network_protocols', {
  associated_cpp_name : 'NETWORK_PROTOCOLS_ANON',
  binobject : 'Protos',
  contents : {'mod/net/protos':['queuedIO.c', 'fakeIO.h', 'fakeIO.c', 'Makefile'],'mod/net/protos/ether':['ether.c', 'Makefile'],'mod/net/protos/ip':['ip.c', 'Makefile'],'mod/net/protos/ipv6':['ipv6.c', 'Makefile'],'mod/net/protos/udp':['udp.c', 'Makefile'],},
  makefileflags : {'custom':1,},
  package : 'cnet',
  path : 'mod/net/protos',
  requires : ['networking'],
  section : 'unclassified',
  type : 'quad',
  value : 3,
}),

InterfaceCollection('networking central interfaces', {
  associated_cpp_name : 'NETWORKING_CENTRAL_INTERFACES_ANON',
  contents : {'mod/net/interfaces':['UDPMod.if', 'UDP.if', 'TFTPMod.if', 'TFTP.if', 'SimpleIO.if', 'Protocol.if', 'PF.if', 'NetifMod.if', 'Netif.if', 'Netcard.if', 'Net.if', 'LMPFMod.if', 'LMPFCtl.if', 'InetCtl.if', 'IPMod.if', 'IPConf.if', 'IP.if', 'ICMPMod.if', 'ICMP.if', 'FlowMan.if', 'EtherMod.if', 'Ether.if', 'BaseProtocol.if', 'ARPMod.if'],},
  package : 'cnet',
  path : 'mod/net/interfaces',
  requires : ['networking'],
  section : 'unclassified',
  system_interfaces : ['BaseProtocol.if', 'FlowMan.if', 'IPConf.if', 'InetCtl.if', 'Net.if', 'Netcard.if', 'Netif.if', 'PF.if', 'LMPFCtl.if', 'LMPFMod.if', 'Protocol.if', 'SimpleIO.if', 'NetifMod.if', 'Netcard.if', 'Netif.if', 'ICMPMod.if', 'UDP.if', 'UDPMod.if', 'TFTPMod.if', 'TFTP.if', 'ARPMod.if', 'IP.if', 'IPMod.if', 'ICMP.if', 'ICMPMod.if', 'Ether.if', 'EtherMod.if'],
  type : 'bool',
  value : 1,
}),

Directory('protos_ether', {
  associated_cpp_name : 'PROTOS_ETHER_ANON',
  contents : {'mod/net/protos/ether':['ether.c', 'Makefile'],},
  makefileflags : {'custom':1,},
  package : 'cnet',
  path : 'mod/net/protos/ether',
  requires : ['proto_common'],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

Directory('protos_ip', {
  associated_cpp_name : 'PROTOS_IP_ANON',
  contents : {'mod/net/protos/ip':['ip.c', 'Makefile'],},
  makefileflags : {'custom':1,},
  package : 'cnet',
  path : 'mod/net/protos/ip',
  requires : ['proto_common'],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

Directory('protos_ipv6', {
  associated_cpp_name : 'PROTOS_IPV6_ANON',
  contents : {'mod/net/protos/ipv6':['ipv6.c', 'Makefile'],},
  makefileflags : {'custom':1,},
  package : 'cnet',
  path : 'mod/net/protos/ipv6',
  requires : ['proto_ipv6'],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

Directory('protos_udp', {
  associated_cpp_name : 'PROTOS_UDP_ANON',
  contents : {'mod/net/protos/udp':['udp.c', 'Makefile'],},
  makefileflags : {'custom':1,},
  package : 'cnet',
  path : 'mod/net/protos/udp',
  requires : ['proto_common'],
  section : 'unclassified',
  type : 'bool',
  value : 1,
}),

]


######################################################################
files = {
	'dev/atm' : [],
	'dev/isa/3c509' : ['3c509.c', '3c509.h', '3c509_st.h'],
	'dev/pci/de4x5' : ['de4x5.c', 'de4x5.h', 'fake.h'],
	'lib/static/socket' : ['Makefile', 'socket.c'],
	'lib/static/sunrpc' : ['Makefile', 'auth_none.c', 'authunix_prot.c', 'clnt_perror.c', 'clnt_tcp.c', 'clnt_udp.c', 'pmap_clnt.c', 'pmap_getport.c', 'pmap_prot.c', 'rpc_callmsg.c', 'rpc_commondata.c', 'rpc_prot.c', 'svc.c', 'svc_auth.c', 'svc_udp.c', 'xdr.c', 'xdr_array.c', 'xdr_mem.c', 'xdr_reference.c'],
	'lib/static/sunrpc/h' : ['Makefile'],
	'lib/static/sunrpc/h/rpc' : ['Makefile', 'auth.h', 'auth_unix.h', 'clnt.h', 'pmap_clnt.h', 'pmap_prot.h', 'rpc.h', 'rpc_msg.h', 'svc.h', 'svc_auth.h', 'types.h', 'xdr.h'],
	'mod/nemesis/netcons' : ['RdRedir.if', 'RdRedirMod.if', 'WrRedir.if', 'WrRedirMod.if', 'netcons.c'],
	'mod/net' : [],
	'mod/net/DNS' : ['DNS.c', 'DNS.if', 'DNSMod.if'],
	'mod/net/arp' : ['ARP.c'],
	'mod/net/bpf' : ['BPF.c'],
	'mod/net/flowman' : ['bootp.c', 'ether_input.c', 'ether_output.c', 'ethernet.h', 'flowman.c', 'flowman_st.h', 'fm_constants.h', 'hosts.c', 'icmp.c', 'icmp_var.h', 'if_ether.h', 'in.h', 'ip.h', 'ip_icmp.c', 'ip_icmp.h', 'ip_input.c', 'ip_output.c', 'ip_var.h', 'ipconf.c', 'kill.c', 'kill.h', 'prot_stat.h', 'proto_ip.c', 'proto_ip.h', 'tcp.h', 'tcp_input.c', 'tcp_var.h', 'tcpip.h', 'udp.h', 'udp_input.c', 'udp_var.h'],
	'mod/net/interfaces' : ['ARPMod.if', 'BaseProtocol.if', 'Ether.if', 'EtherMod.if', 'FlowMan.if', 'ICMP.if', 'ICMPMod.if', 'IP.if', 'IPConf.if', 'IPMod.if', 'InetCtl.if', 'LMPFCtl.if', 'LMPFMod.if', 'Net.if', 'Netcard.if', 'Netif.if', 'NetifMod.if', 'PF.if', 'Protocol.if', 'SimpleIO.if', 'TFTP.if', 'TFTPMod.if', 'UDP.if', 'UDPMod.if'],
	'mod/net/lmpf' : ['pf.c'],
	'mod/net/netif' : ['FragPool.if', 'Makefile', 'netif.c', 'pool.c', 'pool.h'],
	'mod/net/protos' : ['Makefile', 'fakeIO.c', 'fakeIO.h', 'queuedIO.c'],
	'mod/net/protos/ether' : ['Makefile', 'ether.c'],
	'mod/net/protos/ip' : ['Makefile', 'ip.c'],
	'mod/net/protos/ipv6' : ['Makefile', 'ipv6.c'],
	'mod/net/protos/udp' : ['Makefile', 'udp.c'],
	'mod/net/resolver' : ['Resolver.if', 'ResolverMod.if', 'resolver.c'],
	'mod/net/socket' : [],
	'mod/net/socket/tcp' : ['event.h', 'inet.h', 'ip.h', 'ip_var.h', 'tcp_fsm.h', 'tcpip.h', 'tcpu.h', 'tcpu_input.c', 'tcpu_output.c', 'tcpu_seq.h', 'tcpu_subr.c', 'tcpu_timer.c', 'tcpu_timer.h', 'tcpu_usrreq.c', 'tcpu_var.h', 'tcpu_xsum.c'],
	'mod/net/socket/udp' : ['udp_socket.c', 'udp_socket.h'],
	'mod/net/term' : ['netterm.c'],
	'mod/net/tftp' : ['Makefile', 'tftp.c'],
}
