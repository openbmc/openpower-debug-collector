AM_DEFAULT_SOURCE_EXT = .cpp
sbin_PROGRAMS = checkstop_app 

checkstop_app_SOURCES = checkstop_app.cpp
checkstop_app_LDFLAGS = \
					$(SYSTEMD_LIBS)	\
					$(SDBUSPLUS_LIBS) \
                    $(PHOSPHOR_LOGGING_LIBS)

checkstop_app_CXXFLAGS = $(SYSTEMD_CFLAGS) \
                        $(SDBUSPLUS_CFLAGS) \
                        $(PHOSPHOR_LOGGING_CFLAGS)
