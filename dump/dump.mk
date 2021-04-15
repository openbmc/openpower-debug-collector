if DUMP_COLLECTION
sbin_PROGRAMS += openpower-dump-manager
systemdsystemunit_DATA = \
	dump/org.open_power.Dump.Manager.service
	
openpower_dump_manager_SOURCES = dump/dump_manager.cpp \
				dump/dump_manager_main.cpp \
				dump/dump_utils.cpp

openpower_dump_manager_LDFLAGS  =  \
			$(PHOSPHOR_LOGGING_LIBS) \
			$(SDBUSPLUS_LIBS) \
			$(SDEVENTPLUS_LIBS) \
			-lpdbg

openpower_dump_manager_CXXFLAGS  = \
			$(PHOSPHOR_LOGGING_CFLAGS) \
			$(SDBUSPLUS_CFLAGS) \
			$(SDEVENTPLUS_CFLAGS)
endif
