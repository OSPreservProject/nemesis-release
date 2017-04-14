#
# ModBEImports.py
#
# This file contains a Python class definition for MIDDL imported interfaces
#

import sys
import string

class Import:

  def typeHdr( self, f ) :
    f.write( '#include "' + self.name + '.ih"\n' )
    
  def idcMarshalHdr( self, f ) :
    f.write( '#include "idc_m_' + self.name + '.h"\n' )
      
  def IDCMarshalHdr( self, f ) :
    f.write( '#include "IDC_M_' + self.name + '.h"\n' )

  def repHdr( self, f ) :
    f.write( '#include "' + self.name + '.def"\n' )
