AM_DEFAULT_SOURCE_EXT = .cpp
sbin_PROGRAMS = checkstop_ffdc_app

checkstop_ffdc_app_SOURCES = \
				checkstop_ffdc_app.cpp

checkstop_ffdc_app_LDFLAGS = \
					$(SYSTEMD_LIBS)	\
					$(SDBUSPLUS_LIBS) \
                    $(PHOSPHOR_LOGGING_LIBS)

checkstop_ffdc_app_CXXFLAGS = $(SYSTEMD_CFLAGS) \
                        $(SDBUSPLUS_CFLAGS) \
                        $(PHOSPHOR_LOGGING_CFLAGS)
