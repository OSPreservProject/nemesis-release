import BE
import sys
import time

class Interface(BE.Interface):
##################################################################
	# 
	# Emit template for C implementation of intf

	def template (self, f):
	  intfName = self.name
	  (year,_,_,_,_,_,_,_,_) = time.localtime(time.time())
	  boilerplate = \
	      '/*\n' + \
	      '*****************************************************************************\n' + \
	      '**                                                                          *\n' + \
	      '**  Copyright '+`year`+', University of Cambridge Computer Laboratory             *\n' + \
	      '**                                                                          *\n' + \
	      '**  All Rights Reserved.                                                    *\n' + \
	      '**                                                                          *\n' + \
	      '*****************************************************************************\n' + \
	      '**\n' + \
	      '** FILE:\n' + \
	      '**\n' + \
	      '**	\n' + \
	      '**\n' + \
	      '** FUNCTIONAL DESCRIPTION:\n' + \
	      '**\n' + \
	      '**	\n' + \
	      '**\n' + \
	      '** ENVIRONMENT: \n' + \
	      '**\n' + \
	      '**\n' + \
	      '**\n' + \
	      '** ID : $' + 'Id: $\n' + \
	      '**\n' + \
	      '**\n' + \
	      '*/\n' + \
	      '\n' + \
	      '#include <nemesis.h>\n' + \
	      '\n' + \
	      '#include <' + intfName + '.ih>\n' + \
	      '\n' + \
	      '#ifdef DEBUG\n' + \
	      '#define TRC(_x) _x\n' + \
	      '#else\n' + \
	      '#define TRC(_x)\n' + \
	      '#endif\n' + \
	      '\n' + \
	      '#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED\n", __FUNCTION__);\n' + \
	      '\n\n'

	  f.write (boilerplate)

	  for i in self.allOps():
	    i.prototype (f, intfName + '_', '_m')
	  f.write ('\n')

	  f.write ('const static' + BE.Tab + intfName + '_op' + BE.Tab + intfName+'_ms = {\n')
	  ops = self.allOps()
	  for i in ops:
	    # emit initialiser for method suite member
	    f.write ('  ' + intfName + '_' + i.name + '_m')
	    if i <> ops[-1]: f.write (',')
	    f.write ('\n')
	  f.write ('};\n\n')

	  f.write ('/* export a stateless closure */\n')
	  f.write ('const static' + BE.Tab + intfName + '_cl' + BE.Tab + \
	      'stateless_'+intfName+'_cl = { &'+intfName+'_ms, NULL };\n')
	  f.write ('CL_EXPORT (' + intfName + ', ' + intfName + \
	      ', statless_'+intfName+'_cl);\n\n\n')

	  f.write ('/*---------------------------------------------------- Entry Points ----*/\n\n')

	  for i in self.allOps():
	    i.template (f, self, intfName, '_m')

	  f.write ('/*\n * End \n */\n')


class Operation(BE.Operation):
	def template(self, f, intf, implName, suffix) :

	        f.write ('static ')
		self.fnDecl (implName, intf.clType, suffix, f, 0)
		f.write ('\n{\n')

		stType = implName + '_st'
		f.write ('  ' + stType + BE.Tab + '*st = self->st;\n\n')
		f.write ('  UNIMPLEMENTED;\n')
		f.write ('}\n\n')


def Go(kind, intflist, fp):
  intf = intflist[-1].obj
  sys.stdout.write('('+intf.name+'.c'+')')
  sys.stdout.flush()
  t_d  = open (intf.name + '.c', 'w')
  intf.template (t_d)
  return [intf.name + '.c']

