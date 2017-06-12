AM_DEFAULT_SOURCE_EXT = .cpp
sbin_PROGRAMS = checkstop_app \
                watchdog_timeout

nobase_nodist_include_HEADERS = \
            org/open_power/Host/error.hpp

BUILT_SOURCES = \
            org/open_power/Host/error.cpp \
            org/open_power/Host/error.hpp

CLEANFILES = ${BUILT_SOURCES}

checkstop_app_SOURCES = checkstop_app.cpp \
                        org/open_power/Host/error.cpp

watchdog_timeout_SOURCES = watchdog_timeout.cpp \
                           org/open_power/Host/error.cpp

generic_ldflags = \
                 $(PHOSPHOR_LOGGING_LIBS) \
                 $(SDBUSPLUS_LIBS)

generic_cxxflags = \
                  $(PHOSPHOR_LOGGING_CFLAGS) \
                  $(SDBUSPLUS_CFLAGS)

checkstop_app_LDFLAGS  = ${generic_ldflags}
watchdog_timeout_LDFLAGS  = ${generic_ldflags}

checkstop_app_CXXFLAGS  = ${generic_cxxflags}
watchdog_timeout_CXXFLAGS  = ${generic_cxxflags}

org/open_power/Host/error.hpp: ${top_srcdir}/org/open_power/Host.errors.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) error exception-header org.open_power.Host > $@

org/open_power/Host/error.cpp: ${top_srcdir}/org/open_power/Host.errors.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) error exception-cpp org.open_power.Host > $@

