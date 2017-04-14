
import sys

def Go(kind, intflist, fp):
  intf = intflist[-1]
  sys.stdout.write('('+intf.obj.name+'.tsi'+')')
  sys.stdout.flush()
  t  = open (intf.obj.name + '.tsi', 'w')
  t.write(intf.ast)
  l = len(intf.intfgraph.values())
  t.write(`l`+'\n')
  for key in intf.intfgraph.keys():
      t.write(key)
      t.write(',')
      t.write(intf.intfgraph[key].fp)
      t.write('\n')
  t.close()
  return [intf.obj.name+'.tsi']
