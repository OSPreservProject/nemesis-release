# Useful gdb commands for debugging dynamic modules To set things in
# motion, type the name of the machine to which you wish to connect,
# or else type 'connect <host:port>'

# heap_of <object> works as the HEAP_OF() macro in C

define heap_of
	print (Heap_clp)(*(((word_t *)$arg0)-1))
end

# dumpmodules searches for dynamically loaded modules and prints the
# commands required to load them, which can then be selected and
# pasted back into gdb. This only works if the modules and the kernel
# were compiled with the CONFIG_MODULE_OFFSET_DATA option

define dumpmodules
	set $mod_data = $pip.mod_data
	if $mod_data != 0
		set $num_modules = $mod_data->num_entries
		set $mod_entry = $mod_data->entries
		if $num_modules > 0
			echo Dynamic modules: \n
			echo Select and paste following lines to load symbols\n
			echo \n
		end
		while $num_modules > 0
			printf "loadsymbols %s %p\n", \
				$mod_entry->name, $mod_entry->offset
			set $num_modules = $num_modules - 1
			set $mod_entry = $mod_entry + 1
		end
	
		printf "\n"
	end
end

define loadsymbols
	add-symbol-file $arg0 $arg1
end

define connect
	source nemesis.sym
	echo Connecting to $arg0\n
	tar rem $arg0		
	set $pip = *(struct pip *)0x1000
	set $__pvs = *(Pervasives_Rec *)($pip.pvsptr)

	dumpmodules
end

define blaster
	connect styx:9782
end

define grenade
	connect styx:9752
end

define shotgun 
	connect styx:9762
end

define candlestick 
	connect styx:9652
end

define rope
	connect styx:9662
end

define cannon
	connext styx:9672
end

define revolver
	connect styx:9682
end



set remotecache on
