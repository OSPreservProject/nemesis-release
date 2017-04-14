#! /usr/bin/env python

######################################################################
# Choices editor GUI Copyright 1998 Dickon Reed
######################################################################


desc = """
I suppose I better document how this thing works, because I don't want
to have to reverse engineer it.

Essentially, there are four python objects; a Control object, that
encapsulates the entire session, a ChoicesEditor object that
encapsulates a choices file and a Text widget for editing it, a
NotebookItemSelector object that encapsulates a structured list of all
the items and lets you select them, and a item editor that
encapsulates one item at the time and includes widgets to edit each
other.

The control object contains references to the other three objects if
they are created, and each of the other three objects contains a
reference to the control object. 

The control object creates the other objects, and reacts to menu
commands.

The Selector and Editor objects get destroyed every now and then by
the Control object, whenever the choices file has changed. The control
objects then generates new ones and stitches the new objects back in to
the top level window. We rely on the python reference counter to kill
all the old objects that are no longer useful, and that GTK will
recursively kill objects when an object dies.
"""
import sys, os, posixpath, string
try:
    from Gtkinter import *
except:
    sys.stderr.write("""
Gtkinter is not available in this installation of python. Please pester your
sysadmins. 

This code was written against Python 1.5.1, gtk+-1.0.3 and pygtk-0.4.3. Earlier
versions of Python should work.

(In the CL, a Linux version of Python with GTK support is located at
  /anfs/scratch/mcconnell/dr10009/linux-install/python/bin/python

You will need
  /anfs/scratch/mcconnell/dr10009/linux-install/lib
on your LD_LIBRARY_PATH to find the GTK libraries.
)
""")

import GtkExtra, imp, customizer, time, buildutils

def convert_items_to_dict(items):
    dict = {}
    for item in items:
	if not item.options.has_key('section'): item.options['section'] = 'unspecified'
	section = item.options['section']
	if not dict.has_key(section):
	    dict[section] = {}
	dict[section][item.name] = item
    return dict


# mode can be 0 (beginner), 1 (expert) or 2 (deity)

class Control:
    def __init__(self, choices_filename, blueprint_filename):

	# store the filenames here; this object contains the only and 
	# canonical versions of these filenames

	self.choices_filename = choices_filename
	self.blueprint_filename = blueprint_filename
	# we set these to None here so that setup_itemeditor the first
	# time round doesn't raise an AttributeError
	self.ch = None
	self.ed = None 
	self.nb = None

	# we set these to None so that setmode the first time round can
	# detect that and not try to delete them
	self.vbox = None
	self.menubar = None
	self.hbox = None

	# we set this to None; it will be set to a section name when the user
	# first selects a current section
	self.current_section = None

	self.setmode(0)

    def setmode(self, mode):
	self.mode = mode
	if self.vbox and self.menubar and self.hbox:
	    #print '**************************************************regen'
	    self.vbox.remove(self.menubar)
	    self.vbox.remove(self.hbox)
	    self.menubar.destroy()
	    self.hbox.destroy()
	    self.ed = None
	    vbox  = self.vbox
	    win = self.win
	else:
	    #print '++++++++++++++++++++++++++++++++++++++++++++++++++  gen'
	    ## create the top level window
	    win = GtkWindow()
	    win.set_title("Nemesis build system configuration tool")
	    vbox = GtkVBox()
	    self.mainvbox = win
	    self.win = win
	    self.vbox = vbox
	    win.show()

	hbox = GtkHBox()
	self.hbox = hbox

	# set up Choices Editor
	if mode > 0:
	    ch = ChoicesEditor(self, invisible = 0)
	    framech = GtkFrame('Choices Editor')
	    framech.border_width(5)
	    framech.add(ch)
	    framech.show()
	    hbox.pack_end(framech)
	else:
	    ch = ChoicesEditor(self, invisible = 1)

	self.ch = ch

	# set up menu bar
	menubar = GtkExtra.MenuFactory()
	l = [
	    ('File/New Choices File', None, self.new_choices),
	    ('File/Open Choices File', None, self.open_choices),
	    ('File/<separator>', None, None),
	    ('File/Exit', None, self.exit),
	    ('Blueprint/Open Generic Blueprint', None, self.open_tree),
	    ('Mode/Beginner', None, self.mode_beginner),
	    ('Mode/Expert', None, self.mode_expert),
	    ('Mode/Deity', None, self.mode_deity),
	    ]

	if self.mode == 2:
	    l = l + [
	    ('Add/Program',None,self.new_program),
	    ('Add/Module',None,self.new_module),
	    ]

	l = l + [
	    ('Help/Manual', None, self.help),
	]
	menubar.add_entries(l)

	menubar.show()
	self.menubar = menubar
	print 'Setting up main window'
	# set up main window
	print 'packing'
	vbox.pack_start(menubar, fill=FALSE, expand=FALSE)
	vbox.pack_start(hbox)
	hbox.show()
	vbox.show()
	win.add(vbox)
	self.win = win
	# read the choices file
	frameitem = GtkFrame('Item Editor')
	frameitem.border_width(5)
	self.hbox2 = GtkHBox()

	# read the choices files, implicintly creating the choices editor
	self.ch.refresh_file()
	frameitem.add(self.hbox2)
	frameitem.show()
	self.hbox2.show()
	hbox.pack_start(frameitem)

	print 'Setting up item editor'
	self.setup_itemeditor()
	# the item editor must have been set up by this point, otherwise
	# reload blueprint will fail when it tries to point the Editor
	# at the new item
	self.reload_blueprint()

    # create a new Selector and Editor, destroying the old ones
    # if they exist. Get the new editor to point at what any old
    # editor would have been pointing at.
    def setup_itemeditor(self):
	if self.nb:
	    self.hbox2.remove(self.nb)
	    self.nb.destroy()
	olditem = None
	if self.ed:
	    self.hbox2.remove(self.ed)
	    if self.ed.haveitem:
		olditem = self.ed.item
	    self.ed.destroy()

	# set up Item Editor
	
	nb = NotebookItemSelector(self)
	ed = ItemEditor(self)
	self.hbox2.pack_start(nb)
	self.hbox2.pack_start(ed)

	# save the objects we created, so we can kill them next time round
	self.nb = nb
	self.ed = ed
	# get the editor to point at the same place it used to
	if olditem and olditem.options.has_key('section'):
	    self.ed.pointat(olditem.name)
	# XXX; it would be nice to get the selector to point at the same place as well

    def reload_blueprint(self):
	(pathname,filename) = posixpath.split(self.blueprint_filename)
	mainpart = string.split(filename, '.')[0]
	print 'Import ',[pathname], mainpart
	(file, pathname, desc) = imp.find_module(mainpart, [pathname])
	print 'Blueprint desc ',desc,'pathname',pathname
	if desc ==imp.PY_COMPILED:
	    print 'Loading compiled generic blueprint'
	    obj = imp.load_compiled('generic_blueprint', self.blueprint_filename, file)
	else:
	    print 'Loading uncompiled generic blueprint'
	    obj = imp.load_source('generic_blueprint', self.blueprint_filename, file)
	file.close()
	cust = customizer.Customizer(obj.items, donottouch = 1)
	cust.fatal = 0
	f = None
	cust.interpret(self.choices_filename)
	cust.donottouch = 0
	cust.makeconsistent(1)
	self.items = cust.getitems()
	self.dict = convert_items_to_dict(self.items)
	self.setup_itemeditor()

    def reload_choices(self):
	print 'Choices reloading'
	self.ch.refresh_file()

    # menu commands
    def save_tree(self, mi): pass
    def open_tree(self, mi): pass
    def new_choices(self, mi): pass
    def open_choices(self,mi): 
	filename = GtkExtra.file_open_box(modal = FALSE)
	self.choices_filename = filename
	self.reload_choices()
    def save_choices(self, mi): pass
    def help(self, mi): 
	print mi
    def exit(self, mi):
	sys.exit(0)
    def new_program(self, mi): pass
    def new_module(self, mi): pass

    def mode_beginner(self, mi): self.setmode(0)
    def mode_expert(self, mi): self.setmode(1)
    def mode_deity(self, mi): self.setmode(2)

# the choices editor has two modes; ro, where changes take effect immediately and the
# choices file cannot be directly edited, and rw, where changes are buffered up and the choices
# file can be directly edited. The variable self.mode is set to one of 'ro' and 'rw' accordingly.

class ChoicesEditor(GtkVBox):
    def __init__(self, control, invisible = 0):
	self.control = control
	self.invisible = invisible
	if not invisible:
	    GtkVBox.__init__(self)
	    self.fnamelabel = GtkLabel('foobar')
	    self.fnamelabel.show()
	    #self.pack_start(self.fnamelabel)
	    self.border_width(10)
	    self.edit = None
	    hbox = GtkHBox()
	    hbox.set_usize(200,20)	

	    #fnameentry = GtkEntry()
	    #fnameentry.set_text(self.control.choices_filename)
	    #fnameentry.show()
	    #hbox.pack_start(fnameentry)

	    # these buttons get relabelled when we switch in to and out of rw mode
	    self.button = GtkButton()
	    self.buttonlabel = GtkLabel('Edit')
	    self.buttonlabel.show()
	    self.button.add(self.buttonlabel)
	    self.button.show()
	    self.button.connect('clicked', self.buttonpress)

	    self.button2 = GtkButton()
	    self.button2label = GtkLabel('Reload')
	    self.button2label.show()
	    self.button2.add(self.button2label)
	    self.button2.connect('clicked', self.button2press)
	    self.button2.show()

	    self.mode = 'ro'
	    hbox.pack_end(self.button2)
	    hbox.pack_end(self.button)

	    hbox.show()
	    self.pack_start(hbox, expand=FALSE)
	    self.show()
    def buttonpress(self, obj):
	if self.mode == 'ro':
	    self.mode = 'rw'
	    self.buttonlabel.set('Commit')
	    self.button2label.set('Revert')
	    self.edit.set_editable(1)
	else:
	    point  = self.edit.get_point()
	    self.edit.set_point(0)
	    self.edit.freeze()

	    text = self.edit.get_chars(0, self.edit.get_length())
	    print 'Writing ',len(text),' bytes to ',self.control.choices_filename
	    o = open(self.control.choices_filename, 'w')
	    o.write(text)
	    o.close()
	    print 'saved'
	    self.remove(self.edit)
	    self.edit.hide()
	    self.edit.destroy()
	    self.edit = None
	    self.mode = 'ro'
	    self.buttonlabel.set('Edit')
	    self.button2label.set('Reload')
	    self.refresh_file()

    # click on the reload/revert button
    def button2press(self, obj):
	self.refresh_file()
	if self.mode == 'rw':	
	    self.button2label.set('Reload')
	    self.buttonlabel.set('Edit')
	self.mode = 'ro'

    def refresh_file(self):
	#self.control.mainvbox.unmap()
	if not self.invisible:
	    self.fnamelabel.set(self.control.choices_filename)
	    self.fnamelabel.show()
	    if not self.edit:
		self.edit = GtkText()
		self.edit.show()
		self.edit.set_usize(300,200)
		self.pack_start(self.edit)
	    self.edit.freeze()
	    self.edit.realize()
	    self.edit.set_point(0)
	    self.edit.forward_delete(self.edit.get_length())
	    self.edit.set_editable(0)
	    try:
		o = open(self.control.choices_filename, 'r')

		while 1:
		    line = o.readline()
		    if line == "": break
		    self.edit.insert_defaults(line)
		o.close()
	    except IOError:
		print 'Choices file',filename,'does not exist or could not be read'
	    self.edit.thaw()
	self.control.reload_blueprint()
	#self.control.mainvbox.map()
    def add_line(self, newline, forbid = None):
	print 'New line ',newline[:-1], 'forbid',forbid
	if self.control.mode == 0 or self.mode == 'ro':
	    try:
		# read only mode, so add to end of file
		o = None
		try:
		    o = open(self.control.choices_filename, 'r')
		except:
		    print 'Could not read', self.control.choices_filename
		if o:
		    lines = o.readlines()
		    o.close()
		else:
		    lines = []
		filename = self.control.choices_filename+'_'+`os.getpid()`

		o = open(self.control.choices_filename, 'w')
		for line in lines:
		    if line[0:len(forbid)] != forbid:
			o.write(line)
		o.write(newline)
		o.close()
		self.refresh_file()
	    except:
		print '***** Error while writing choices file; losing last change'
	else:
	    # read write mode, so just add to the end of the buffer
	    self.edit.set_point(self.edit.get_length())
	    self.edit.insert_defaults(newline)

class NotebookItemSelector(GtkNotebook):
    def __init__(self, control):
	self.control = control
	GtkNotebook.__init__(self)
	self.border_width(5)
	self.dict = control.dict
	section_names = self.dict.keys()
	section_names.sort()
	self.section_name = section_names
	self.clists = []
	self.sections = []
	self.notebookcontents = []
	index = 0
	mapping = {}
	for section in section_names:
## 	    if section == 'all':
## 		item_names = []
## 		for sectionprime in self.dict.keys():
## 		    if control.mode == 2:
## 			item_names = item_names + self.dict[sectionprime].keys()
## 		    else:
## 			for item in self.dict[sectionprime].values():
## 			    if item.options.has_key('tweakability'):
## 				if ((self.control.mode == 0 and item.options['tweakability']==2) or
## 				    (self.control.mode == 1 and item.options['tweakability']>=1)):
## 				    item_names = item_names + [item.name]
				   
## 		self.allitems = item_names
## 	    else:
	    item_names = []
	    if control.mode == 2:
		item_names = item_names + self.dict[section].keys()
	    else:
		for item in self.dict[section].values():
		    if item.options.has_key('tweakability'):
			if ((self.control.mode == 0 and item.options['tweakability']==2) or
			    (self.control.mode == 1 and item.options['tweakability']>=1)):
			    item_names = item_names + [item.name]
	    if len(item_names) == 0: continue
	    item_names.sort()
	    clist = GtkCList(3, ['Name','value','overrides'])
	    clist.set_usize(320,200)
	    clist.column_titles_hide()
	    clist.set_column_width(0, 130)
	    clist.set_column_width(1, 10)
	    clist.set_column_width(2, 30)
	    for item_name in item_names:
		item = None
		for sectionprime in self.dict.keys():
		    if self.dict[sectionprime].has_key(item_name):
			item = self.dict[sectionprime][item_name]
		value = `item.options['value']`
		clist.append([item_name, value, `item.originals.keys()`])
	    self.notebookcontents.append( (section, item_names))
	    self.clists.append(clist)
	    clist.show()
	    label = GtkLabel(section)
	    self.append_page(clist, label)
	    self.set_tab_pos(0)
	    clist.connect("select_row", self.select_row)
	    self.sections = self.sections + [section]
	    mapping[section] = index
	    index = index + 1
	self.section_names = section_names
	if self.control.current_section:
	    if mapping.has_key(self.control.current_section):
		self.set_page(mapping[self.control.current_section])
	self.show()

    def select_row(self, _, r, c, event):
	secs = self.notebookcontents
	(sec, names) = secs[self.current_page()]
	self.control.current_section = sec
	self.control.current_item = names
	self.control.ed.pointat(names[r])

class Tweaker:
    def __init__(self, item, option, control):
	self.item = item
	self.option = option
	self.control = control
    def invoke(self, _):
	dialog = GtkDialog()
	self.dialog = dialog
	dialog.set_title('Tweak '+self.item.name+' '+self.option)
	button = GtkButton("OK")
	button.connect("clicked", self.accept)
	dialog.action_area.pack_start(button)
	button.show()
	button = GtkButton("Cancel")
	button.connect("clicked", self.cancel)
	dialog.action_area.pack_start(button)
	button.show()

	newtextstr = ''
	if self.item.options.has_key(self.option):
	    newtextstr = `self.item.options[self.option]`
	    if 1:
		label = GtkLabel("Current Value")
		label.show()
		dialog.vbox.pack_start(label)
		oldtext = GtkText()
		dialog.vbox.pack_start(oldtext)
		oldtext.show()
		oldtext.freeze()
		oldtext.realize()
		oldtext.insert_defaults(newtextstr)
		oldtext.thaw()
	label = GtkLabel("New Value")
	label.show()
	dialog.vbox.pack_start(label)
	newtext = GtkText()
	dialog.vbox.pack_start(newtext)
	newtext.show()	
	newtext.freeze()
	newtext.realize()
	newtext.insert_defaults(newtextstr)
	newtext.thaw()
	newtext.set_editable(1)
	self.edit = newtext
	print 'Invoked tweaker on ',self.item.name,'option',self.option
	dialog.show()
    def apply(self, _):
	self.edit.set_point(0)
	self.edit.freeze()
	newv = self.edit.get_chars(0, self.edit.get_length())
	self.edit.thaw()
	line = "tweak('"+self.item.name+"','"+self.option+"',"+newv+")\n"
	self.control.ch.add_line(line, "tweak('"+self.item.name+"','"+self.option+"',")
    def accept(self, _):
	self.apply(0)
	self.cancel(0)
    def cancel(self, _):
	self.dialog.hide()
	self.dialog.destroy()
class ItemEditor(GtkScrolledWindow):
    def __init__(self, control):
	GtkScrolledWindow.__init__(self)
	self.mainwidget = None
	self.control = control
	self.show()
	self.set_usize(400,200)
	self.haveitem = 0

    # make the editor edit a particular item
    # XXX: it would be nice if this got the notebook selector
    # to select that item as well, but that might be dangerous.
    def pointat(self, itemname):
	self.control.currentitem = itemname
	labstart = 0
	valstart = 10
	b0start = 35
	b1start = 40
	edgestart = 45
	def kindlen(x):
	    return [0,1,5,5,1,1][x]
	#print 'Point at ',itemname
	item = None
	for itemprime in self.control.items:
	    if itemprime.name == itemname: item = itemprime
	if not item:
	    print 'Could not locate ',itemname
	self.item = item
	self.haveitem = 1
	self.menumap = {}
	if self.mainwidget: self.mainwidget.destroy()
	# type 1 is an entry field with an edit button
	# type 2 is a text field with an edit button
	# type 3 is a list or dictionary with an edit button
	# type 4 is a label with a pick button
	# type 5 is a popup menu
	fields = [('name',1), ('class', 4), ('value', 5), ('type',4), ('section', 1), ('associated_cpp_name', 1), 
		     ('description',2), ('helptext',2), ('path', 1), ('makefileflags', 3), ('requires', 2), ('tweakability',1)]
	cls = item.get_class()
	if cls in ['program', 'posixprogram']:
	    fields = fields + [('qos', 3), ('env', 3)]

	table = GtkTable(edgestart, 0, FALSE)
	table.border_width(10)
	self.add(table)
	table.set_row_spacings(5)
	table.set_col_spacings(5)
	table.show()
	self.mainwidget = table
	count = 0
	for (key,kind) in fields:
	    compulsory = 0
	    fixed = 0
	    if key == 'name':
		value = item.name
		compulsory = 1
		fixed = 1
	    elif key == 'class':
		value = cls
		compulsory = 1
		fixed = 1
	    else:
		value = None
		if item.options.has_key(key):
		    value = item.options[key]
	    if key in ['qos', 'env', 'path']: compulsory = 1
	    if key == 'associated_cpp_name':
		label = GtkLabel('cpp_name')
	    else:
		label = GtkLabel(key)
	    #label.set_usize(100,24)
	    label.show()
	    table.attach(label, labstart,valstart,count,count+kindlen(kind), yoptions=FILL)
	    if value != None:
		valuestr = value
		if type(valuestr) != type(''):
		    valuestr = `value`
		    if type(value) == type([]) and key == 'type': valuestr = 'choice'

		if kind == 1:
		    entry = GtkLabel(valuestr)
		    #entry.set_usize(100,20)
		    entry.show()
		    table.attach(entry, valstart,b0start,count,count+1)

		if kind == 2:
		    text = GtkText()
		    table.attach(text, valstart,b0start, count, count+kindlen(kind), xoptions=FILL)
		    text.show()
		    text.freeze()
		    text.realize()
		    text.insert_defaults(valuestr)
		    text.thaw()


		if kind == 3:
		    text = GtkText()
		    table.attach(text, valstart,b0start, count, count+kindlen(kind), xoptions=FILL)
		    text.show()
		    text.freeze()
		    text.realize()
		    if type(value) == type([]):
			for datum in value:
			    text.insert_defaults(`datum`+'\n')
		    else:
			for datum in value.keys():
			    text.insert_defaults(datum + ' : ' + `value[datum]`+'\n')
		    text.thaw()

		if kind == 4:
		    label = GtkLabel(valuestr)
		    table.attach(label, valstart,b0start, count, count+kindlen(kind))
		    label.show()

		if kind == 5:
		    optionmenu = GtkOptionMenu()
		    menu = GtkMenu()
		    group = None
		    t = item.options['type']
		    if t == 'bool':
			typechoices = ['false', 'true']
		    elif t == 'quad':
			typechoices = ['none', 'support', 'build', 'image']
		    elif type(t) == type([]):
			typechoices = map(lambda x : x[1], t)
		    else:
			raise InvalidType
		    count2 = 0
		    menuitems = []
		    for choice in typechoices:
			menuitem = GtkRadioMenuItem(group, choice)
			group = menuitem
			menu.append(menuitem)
			menuitem.show()
			menuitem.item = item
			menuitem.value = count2
			self.menumap[`menuitem._o`] = count2
			menuitems = menuitems + [menuitem]
			if count2 == value:
			    menuitem.set_state(1)
			count2 = count2 + 1
		    optionmenu.set_menu(menu)
		    optionmenu.set_history(value)
		    table.attach(optionmenu, valstart, b0start, count, count+kindlen(kind))
		    optionmenu.show()
		    for menuitem in menuitems:
			menuitem.connect('toggled', self.value_choice_selected)

		if kind in [1,2,3,4] and not fixed and self.control.mode == 2:
			editbutton = GtkButton("edit")
			editbutton.connect('clicked', Tweaker(self.item, key, self.control).invoke)
			editbutton.show()
			table.attach(editbutton, b0start, b1start, count, count+kindlen(kind))

		if not compulsory and self.control.mode == 2 and 0: #XXX sort this one out
		    delbutton = GtkButton("del")
		    delbutton.show()
		    table.attach(delbutton, b1start,edgestart, count, count+kindlen(kind), xoptions=FILL)
	    else:
		if self.control.mode == 2:
		    addbutton = GtkButton("add")
		    addbutton.show()
		    table.attach(addbutton,b0start,b1start, count, count+kindlen(kind), xoptions=FILL)
	    count = count + kindlen(kind)
    def value_choice_selected(self, mi):
	index = self.menumap[`mi._o`]
	if index == self.item.options['value']: return
	line = "set('"+self.item.name+"',"+`index`+")\n"
	self.control.ch.add_line(line, "set('"+self.item.name+"',")

if len(sys.argv) != 3:
    sys.stderr.write("""
Usage: choicesgui.py absolute_path_to_choices_file path_to_generic_blueprint

""")
    sys.exit(1)

filename = sys.argv[1]

if filename[0] != '/':
    import os
    filename = os.getcwd() + '/' + filename
filename = buildutils.canonicalize('', filename)
bpfilename = sys.argv[2]
print 'Choices file',filename
print 'Blueprint',bpfilename
control = Control(filename, bpfilename)
mainloop()
