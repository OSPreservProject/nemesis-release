#! /usr/bin/env python
import sys
print sys.path
from guicompiler import *

gui = Gui()

gui.prefix = 'buttons'
gui.fork_events_thread = 0
but = FixedWindow(gui)
but.name = 'main'
but.pos = (0,736)
but.size = (1024, 32)
but.background_ppm = 'buttons.ppm'
but.title = ''
but.handlers['mouse_click'].append( ('Left', (0,0),(1024,32),'test_invoke') )
gui.go()

