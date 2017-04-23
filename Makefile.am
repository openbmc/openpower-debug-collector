AM_DEFAULT_SOURCE_EXT = .cpp
sbin_PROGRAMS = debug_data_collector

debug_data_collector_SOURCES = debug_data_collector.cpp
debug_data_collector_LDFLAGS = \
					$(SYSTEMD_LIBS)	\
					$(SDBUSPLUS_LIBS) \
                    $(PHOSPHOR_LOGGING_LIBS)

debug_data_collector_CXXFLAGS = $(SYSTEMD_CFLAGS) \
                        $(SDBUSPLUS_CFLAGS) \
                        $(PHOSPHOR_LOGGING_CFLAGS)
