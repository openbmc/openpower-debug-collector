AM_DEFAULT_SOURCE_EXT = .cpp

# For non native recipe build
if WANT_DEBUG_COLLECTOR
sbin_PROGRAMS = checkstop_app \
                watchdog_timeout

nobase_nodist_include_HEADERS = \
            org/open_power/Host/error.hpp


BUILT_SOURCES = \
            org/open_power/Host/error.cpp \
            org/open_power/Host/error.hpp

if WANT_ELOG_HEADER
BUILT_SOURCES += phosphor-logging/elog-errors.hpp
endif

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

# Generate elog-errors.hpp only for local build
if WANT_ELOG_HEADER
ELOG_MAKO ?= elog-gen-template.mako.hpp
ELOG_DIR ?= ${OECORE_NATIVE_SYSROOT}/usr/share/phosphor-logging/elog
ELOG_GEN_DIR ?= ${ELOG_DIR}/tools/
ELOG_MAKO_DIR ?= ${ELOG_DIR}/tools/phosphor-logging/templates/
YAML_DIR ?= ${OECORE_NATIVE_SYSROOT}/usr/share/phosphor-dbus-yaml/yaml
phosphor-logging/elog-errors.hpp:
	@mkdir -p ${YAML_DIR}/org/open_power/
	@cp ${top_srcdir}/org/open_power/Host.errors.yaml ${YAML_DIR}/org/open_power/Host.errors.yaml
	@chmod 777 $(ELOG_GEN_DIR)/elog-gen.py
	@mkdir -p `dirname $@`
	$(AM_V_at)$(PYTHON) $(ELOG_GEN_DIR)/elog-gen.py -y ${YAML_DIR} -t ${ELOG_MAKO_DIR} -m ${ELOG_MAKO} -o $@
endif #WANT_ELOG_HEADER
else
# For native build
# Export error YAML to shared location
yamldir = ${datadir}/phosphor-dbus-yaml/yaml
nobase_yaml_DATA = \
	org/open_power/Host.errors.yaml
endif #WANT_DEBUG_COLLECTOR
