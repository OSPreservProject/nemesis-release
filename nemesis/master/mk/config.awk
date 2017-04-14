
/^[^ ] DEFINES_(.*)[ :=]/ { gsub("^.*DEFINES_", "");gsub("[ :=].*$", ""); print; }
/^[^ ] CFLAGS_(.*)[ :=]/ { gsub("^.*CFLAGS_", "");gsub("[ :=].*$", ""); print; }
/^[^ ] ASFLAGS_(.*)[ :=]/ { gsub("^.*ASFLAGS_", "");gsub("[ :=].*$", ""); print; }

