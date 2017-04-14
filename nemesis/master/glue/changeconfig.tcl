#! /usr/bin/env wish

# (Is it my imagination, or does the line above look increasingly more scary
# every time I edit this file?)

#########################################################################
#                                                                       #
#  Copyright 1997, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################

# Nemesis configuration changing tool - changeconfig.tcl
#
# Dickon Reed, July 1997

# This program reads a configuration database, that contains data for each
# option. This is performed by the section "read configuration file" in the
# main body of the program.
#
# The configuration database is stored in the following global dictionaries,
# keyed by option name.
#
# 'requires' contains a list of dependencies for each option
# 'optiondata' contains the configuration database line for that option, as a
#              list
# 'poss' contains a list of possibilties for each option. A possiblitiy is a
#        3 tuple containing (availiblity flag, friendly name, value) where 
#        the availiblity is 1 if the dependancies on an option should be 
#        enabled
#        if the option is set to that. value is the value stored in the status
#        array and written in the configstatus file. friendly name is the name of 
#        the possibilty presented to the user.
# 'status' contains the value of the possibility selected for a particular
#           option
# 'avail' is the availiblity flag for that option. The values in this array 
#         should be the availility flag of the possibility selected by poss.
# 'fixed' is an array of booleans. If an option has it's fixed flag set to 
#         true, then it should not be editable. 
#
#
# In addition, the configuration database file itself is stored in configlist.

# It then reads, if it exists, an configstatus file which specifies a 
# configuration state. The configuration state may then be edited and the 
# configstatus file written out again. The procedures readconfig and writeconfig 
# perform this operation.


################################################################################
# Display help for str

proc helpstr {str} {
    global helpfilename
    set lookfor [join [list CONFIG_ $str] ""]
    set l {}
    set f [open $helpfilename r]
    set relevant 0
    while {[gets $f line] >= 0} {
	if {$relevant == 1} {
	    lappend l $line
	}
	if {$line == $lookfor} {
	    set relevant 1
	}
	if {$line == ""} {
	    set relevant 0
	}
    }	
    return [join $l "\n"]
}

proc helpcom {str} {
    destroy .help
    message .help -text [helpstr $str] -width 400

    pack .help -fill x

}

################################################################################
# Correspond; make avail reflect status for option
#

proc correspond {option} {
    global avail status poss
    
    set x $status($option)
    foreach choice $poss($option) {
	if {[lindex $choice 2] == $x} {
	    set avail($option) [lindex $choice 0]
	    #puts [list availability of $option is $avail($choice) value $status($option)]
	}
    }
}

###########################################################################
# invcorrespond; make status reflect avail for option
#

proc invcorrespond {option} {
    global avail status poss
    set x  $avail($option)
    #puts [list invcorrespond $option avail $x]

    if {[lsearch [array names poss] $option] < 0} {
	# option cannot be edited
	if {[lsearch [array names status] $option]<0} {
	    set avail($option) 0
	} else {
	    if {$status($option)=="y"} {
		set avail($option) 1
	    }
	}
	return
    }


    foreach choice $poss($option) {
	#puts $choice
	if {[lindex $choice 0] == $x} {
	    set status($option) [lindex $choice 2]
	    #puts [list availability of $option is $avail($option) value $status($option)]
	}
    }
    notify $option
}


################################################################################
# notify database that status of option has changed

proc notify {option} {
    global depend status requires avail
    #puts [list Notify $option]
    correspond $option
    if {$avail($option) > 0} {
	foreach req $requires($option) {
	    if {$avail($req) == 0} {
		set avail($option) 0
		invcorrespond $option
	    }
	}
    }
    if {$avail($option) == 0} {
	foreach dep $depend($option) {
	    set avail($dep) 0
	    invcorrespond $dep
	}
    }
}


################################################################################
# generate controls for a multi choice question

proc genradioset {widgetname option choices} {


} 

################################################################################
# generate set of controls for a group

proc showgroup {name} {
    global groups optiondata poss currentgroup
    global optiondata requires status avail
    set count 0
    if {[string length $currentgroup]>0} {
	destroy .details
    }
    if {$name == $currentgroup} {
	set currentgroup ""
	return
    }
    set currentgroup $name
    frame .details
    destroy .help
    frame .help
    pack .help -fill x
    foreach option $groups($name) {
	set widgetname [join [list .details . [string tolower $option]] ""]
	frame $widgetname -relief groove -borderwidth 4
	pack $widgetname -side top -fill x

	label $widgetname.label -text [lrange $optiondata($option) 5 end]

	
	frame $widgetname.buttons
	foreach l $poss($option) {
	    set num [lindex $l 2]
	    set desc [lindex $l 1]
	    set subwidgetname [ join [list $widgetname.buttons . [string tolower $num] subw ] "" ]
	    radiobutton $subwidgetname -text $desc  -variable status($option) -value $num -selectcolor blue -command [list notify $option] 
	    #puts [list Choice $num]
	    pack $subwidgetname -side left -fill x -expand 1

	}
	$widgetname.buttons configure -width 200
	
	set l {}
	
	button $widgetname.help -text ? -command [list helpcom $option] 
	pack $widgetname.help -side right
foreach r $requires($option) {
	    checkbutton $widgetname.deps$r -text $r -variable avail($r) -command [list invcorrespond $r] -selectcolor green
	    pack $widgetname.deps$r -side right
	}

	
	
	pack $widgetname.buttons -side left 
	
	pack $widgetname.label -fill both -expand 1  -side left

	set count [expr $count + 1]
    }
    pack .details 
    
}    

################################################################################
# read a configuration file

proc readconfig { f } {
    global status kind poss
    while {[gets $f line] >= 0} {
	if {[llength $line] == 2} {
	    #set option [join [list CONFIG_ [lindex $line 0]] ""]
	    set option [lindex $line 0]
	    set x [lindex $line 1]
	    if {[lsearch [array names poss] $option] >=0 } {
		foreach pos $poss($option) {
		    if {$x == [lindex $pos 2]} {
			set status($option) [lindex $pos 2]
			set avail($option) [lindex $pos 0]
			correspond $option
		    }
		}
	    }
	}
    }
}

################################################################################
# write a configuration file

proc writeconfig { f } {
    global status kind poss
    
    foreach option [array names status] {
	puts $f [list $option $status($option)]
    }
}

###########################################################################
# 
# check the configuration database is consistent
#
# if fixup is one, attempt to fix things

proc checkconfig { fixup } {
    global status poss avail fixed requires

    set changed 0

    foreach option [array names status] {

	# check status is in poss
	set found 0
	foreach p $poss($option) {
	    if { [lindex $p 2] == $status($option) } {
		set found 1
		set possibility $p
	    }
	}

	if {$found == 0} {
	    puts [list Inconsistency: option $option is set to $status($option) that is not present in $poss($option)]
	    if {$fixup} {
		puts [list Action: Setting status for $option to first possibility]
		set possibility [list index poss($option) 0]
		set changed 1
	    }
	}
		
	# check avail
	set possavail [lindex $possibility 0]
	#puts [list option $option status $status($option) avail $avail($option) poss $possibility]
	if {$avail($option) != $possavail} {
	    puts [list Inconsistency: availability array for $option gives $avail($option) whereas possibility databse gives $possavail]
	    if {$fixup} {
		puts [list Action: Setting avail array for $option]
		set avail($option) $possavail
		set changed 1
	    }
	}

	# check requires
	if {$avail($option) == 1} {
	    foreach requirement $requires($option) {
		if { $avail($requirement) == 0 } {
		    puts [list Inconsistency: requirement $requirement is not available for $option]
		    if {$fixup} {
			puts [list Action: Setting avail for $option to zero]
			set avail($option) 0
			invcorrespond $option
			set changed 1
		    }
		}
	    }
	}
    }
    if  {$changed} {
	puts "Warning; database changed"
    }

    return $changed
}
	
###########################################################################
# checkbutton and fixbutton
#

proc checkb {} {
    puts {Checking configuration}
    checkconfig 0
}

proc fixb {} {
    puts {Fixing configuration}
    checkconfig 1 
}



################################################################################
# command buttons

proc combutton { kind } {
    global processfilename

    if { $kind == "save" } {
	puts [list Saving $processfilename]
	set f [open $processfilename w]
	writeconfig $f
	close $f
    }

    if { $kind == "saveexit" || $kind == "exit"} {
	puts [list Saving $processfilename]
	set f [open $processfilename w]
	writeconfig $f
	close $f
	exit 0
    }

    if { $kind == "quit" } {
	exit 1
    }
    if { $kind == "dump" } {
	global status
	writeconfig stdout
	puts "\n\n\n"

    }
}

################################################################################
# Main

if {[llength $argv] != 4} {
    puts "Usage: config.tcl mode configfile processfile helpfile"
    exit 2
}

set mode [string tolower [lindex $argv 0]]
set configfilename [lindex $argv 1]
set processfilename [lindex $argv 2]
set helpfilename [lindex $argv 3]
if {0} {
    puts [list Mode $mode]
    puts [list Config file $configfilename]
    puts [list Process file $processfilename]
    puts [list Help file $helpfilename]
}


################################################################################
# Setup basic UI

if {$mode == "tk"} {
    frame .topbar
    button .topbar.check -text "Check" -command checkb
    button .topbar.fix -text "Fix" -command fixb
    button .topbar.dump -text "Dump" -command {combutton dump}
    button .topbar.save -text "Save" -command {combutton save}
    button .topbar.saveexit -text "Save+Exit" -command {combutton saveexit}
    button .topbar.savequit -text "Quit" -command {combutton quit}
    pack .topbar.check .topbar.fix .topbar.dump .topbar.save .topbar.saveexit .topbar.savequit -side left -expand 1 -fill x
    
    set notsetup 1


    frame .help
    pack .topbar
    wm title . "Nemesis Configuration"
    set currentgroup ""
}

if {$mode == "tty"} {
    puts "Nemesis configuration"
    puts  ""
    puts "h for help"
    puts "(use gnumake xconfig for TK based graphical interface)"
    puts ""
}

################################################################################
# read configuration database
set f [open $configfilename r]
while {[gets $f linestr] >= 0} {
    set length [string length $linestr]
    set line $linestr
    set option invalid
    if {[llength $line]>5} {
	if {[string range $line 0 0] != "#" } {

	    set option [lindex $line 1]
	    set depends [split [lindex $line 2] ","]
	    foreach dep $depends {
		lappend depend($dep) $option
		set availset [array names avail]
		set posn [lsearch -exact $availset $dep]
		if {$posn == -1} {
		    set avail($dep) 0
		}

	    }
	    set requires($option) $depends
	    set optiondata($option) $line
	    set status($option) [lindex $line 3]
	    set fixed($option) 1
	    set kind($option) [lindex $line 0]
	    

	    #if {$kind($option) == "*bool"} {
	    #set poss($option) { { 0 N n } { 1 Y y } }
	    #}
	    #if {$kind($option) == "bool"} {
		#set poss($option) { { 0 N n } { 1 Y y } }
	    #}
	    #if {$kind($option) == "quad"} {
		#set poss($option) { { 0 N 0 } { 1 S 1 } { 1 M 2 } { 1 Y 3 } }
	    #}
	    set substr [string range [lindex $line 0] 0 5]
	    if {$substr == "choice"} {
		set sub [string range [lindex $line 0] 7 [expr [string length [lindex $line 0]] - 2]]
		set subl [split $sub ",{}"]
		 set l {}
		 set count 0
		 while {$count < [llength $subl]} {
		     lappend l [lrange $subl [expr $count+1] [expr $count+3]]
		     set count [expr $count+5]
		 }
		 set poss($option) $l

	     }

	    set substr [string range [lindex $line 0] 0 6]
	    if {$substr == "*choice"} {
		set sub [string range [lindex $line 0] 8 [expr [string length [lindex $line 0]] - 2]]
		set subl [split $sub ",{}"]
		 set l {}
		 set count 0
		 while {$count < [llength $subl]} {
		     lappend l [lrange $subl [expr $count+1] [expr $count+3]]
		     set count [expr $count+5]
		 }
		 set poss($option) $l

	     }
	     if {[string index $line 0] !="*"} {
		 lappend groups([lindex $line 4]) $option
		 lappend groups(Everything) $option
		 set fixed($option) 0
	     }
	     correspond $option
	}
    }
    lappend configlist $option $line
}

close $f

foreach option [array names optiondata] {
    if {[lsearch [array names depend] $option] == -1} {
	set depend($option) {}
    }
}



################################################################################
# read configuration

if { [file exists $processfilename] == 1} {
    set f [open $processfilename r]
    readconfig $f
    close $f
} else {
    puts "Configuration file doesn't exist yet"
}


################################################################################
# set up group buttons

if {$mode == "tk"} {
    set xtarget 40
    set count 0
    set ycount 0
    set current ""
    frame .groups -borderwidth 4 -relief ridge
    pack .groups -fill x
    foreach name [lsort [array names groups]] {
	if {$current == "" || $ccount > $xtarget} {
	    set current [join [list .groups.list $count] ""]
	    frame $current 
	    pack $current -expand 1 -fill x
	    #puts $current
	    set ccount 0

	}
	set buttonname [join [list $current .button $count ""] ""]
	set menuname [join [list $current .button $count ".menu"] ""]
	button $buttonname -text $name -relief raised -command [list showgroup $name]
	set ccount [expr $ccount + [string length $name]]
	#puts $ccount
	pack $buttonname -side left -expand 1 -fill x
	set count [expr $count+1]
	
    }
}

###########################################################################
# command line mode


if {$mode == "tty"} {
    set pos 0
    set done 0
    set lastvalid 0
    while {$done == 0} {
	set option [lindex $configlist $pos]
	set requirements 1
	set valid 1
	if {$option == "invalid"} {
	    set valid 0
	}
	if {$option == ""} {
	    set valid 0
	}

	if {$valid == 0} {
	    set str [lindex $configlist [expr $pos + 1]]
#puts [string range $str 0 1]
	    if { [string range $str 0 1] != "##" }  {
		puts [lindex $configlist [expr $pos + 1 ]]
	    }
	    set requirements 0
	} else {
	    foreach req $requires($option) {
#		puts [list $option requires $req]
		if {$avail($req) == 0} {
		    set requirements 0
		    puts [list $option not selectable because $req is not available]
		}
	    }
	    if { $fixed($option) == 1 } {
		puts [list $option is fixed]
		set requirements 0
	    }
	}
	if { $requirements == 1} {
	    set prompt [list $option " " ( " "]
	    foreach choice $poss($option) {
		lappend prompt [lindex $choice 2]
		lappend prompt " "
	    }
	    lappend prompt )
	    lappend prompt " "
	    lappend prompt $status($option)
	    lappend prompt >
	    lappend prompt " "
	    puts -nonewline [join $prompt ""]
	    set lastvalid $pos
	    flush stdout
	    gets stdin command
	    set command [string tolower $command]
	    foreach choice $poss($option) {
		if {[string tolower [lindex $choice 2]] == $command} {
		    set status($option) [string toupper $command]
		    notify $option
		    set pos [expr $pos + 2]
		    
		}
	    }
	    if {[lsearch {save saveexit exit quit dump} $command] >= 0} {
		combutton $command
	    }
	    if {$command == "info" || $command == "i"} {
		puts {}
		puts [list $option is depended on by $depend($option)]
		puts [list $option requires $requires($option)]
		foreach group [array names groups] {
		    
		    if { [lsearch $groups($group) $option] >= 0} {
			puts [list $option is a member of group $group]
		    }
		}
		
	

		puts [helpstr $option]
		puts {}
	    }
	    if {$command == "prev" && $pos>0} {
		set pos [expr $pos - 2]
	    }
	    if {$command == "next" || $command == ""} {
		set pos [expr $pos + 2]
	    }
	    if {$command == "restart" } {
		set pos 0
	    }
	    if {$command == "check" } {
		checkb
	    }
	    if {$command == "fix" } {
		fixb
	    }
	    if {[lindex [split $command] 0] == "jump"} {
		set target [lindex [split $command] 1]
		set tcount 0
		set found 0
		while {$tcount < [llength $configlist]} {
		    if { [string tolower [lindex $configlist $tcount]] == $target } {
			set found 1
			if {$fixed([lindex $configlist $tcount]) == 1} {
			    puts [list Option [lindex $configlist $tcount] is fixed and so cannot be changed]
			    break
			} else {
			    set pos $tcount
			}
		    }
		    set tcount [ expr $tcount + 2]
		}
		if {$found == 0 } {
		    puts [list Option $target not found]
		}
	    }
	    if {$command == "h" || $command == "?" || $command == "help"} {
		puts "One of the choices in brackets to set current option"
		puts "save to save and continue editing"
		puts "saveexit or exit to save and stop editing"
		puts "quit to quit without saving changes"
		puts "dump to show option status database"
		puts "prev to go to the previous option"
		puts "next or carriage return to go to the next option"
		puts "restart to go back to the start"
		puts "jump followed by option name to jump to that option"
		puts "info or i to show information about current option"
		puts "check to check the configuration database"
		puts "fix to fix the configuration database"
	    }
	} else {
	    set pos [expr $pos +2]
	}
	if {$pos >= [llength $configlist]} {
	    puts [list At end]
	    puts [list Type exit to save and exit]
	    puts [list Type restart to go back to start]
	    set pos $lastvalid
	}

    }

}

if {$mode == "genconfigstatus"} {
    while {[checkconfig 1] == 1} {
	puts {Refixing configstatus}
    }
    combutton saveexit
}

if {$mode == "fixup"} {
    puts {Fixing configstatus}
    while {[checkconfig 1] == 1} {
	puts {Refixing configstatus}
    }
    puts {configstatus is stable}
    combutton saveexit
}


