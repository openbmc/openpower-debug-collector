AM_DEFAULT_SOURCE_EXT = .cpp

if !INSTALL_ERROR_YAML
sbin_PROGRAMS = checkstop_app \
                watchdog_timeout

nobase_nodist_include_HEADERS = \
            org/open_power/Host/Boot/error.hpp

if GEN_ERRORS
nobase_nodist_include_HEADERS += \
            phosphor-logging/elog-errors.hpp
endif

BUILT_SOURCES = \
            org/open_power/Host/Boot/error.cpp \
            org/open_power/Host/Boot/error.hpp

if GEN_ERRORS
BUILT_SOURCES += phosphor-logging/elog-errors.hpp
endif

CLEANFILES = ${BUILT_SOURCES}

checkstop_app_SOURCES = checkstop_app.cpp \
                        org/open_power/Host/Boot/error.cpp

watchdog_timeout_SOURCES = watchdog_timeout.cpp \
                           org/open_power/Host/Boot/error.cpp

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

include dump/dump.mk

org/open_power/Host/Boot/error.hpp: ${top_srcdir}/org/open_power/Host/Boot.errors.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) error exception-header org.open_power.Host.Boot> $@

org/open_power/Host/Boot/error.cpp: ${top_srcdir}/org/open_power/Host/Boot.errors.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) error exception-cpp org.open_power.Host.Boot> $@

endif
# Export error YAML to shared location
yamldir = ${datadir}/phosphor-dbus-yaml/yaml
nobase_yaml_DATA = \
	org/open_power/Host/Boot.errors.yaml

# Generate phosphor-logging/elog-errors.hpp
if GEN_ERRORS
ELOG_MAKO ?= elog-gen-template.mako.hpp
ELOG_DIR ?= ${OECORE_NATIVE_SYSROOT}${datadir}/phosphor-logging/elog
ELOG_GEN_DIR ?= ${ELOG_DIR}/tools/
ELOG_MAKO_DIR ?= ${ELOG_DIR}/tools/phosphor-logging/templates/
YAML_DIR ?= ${OECORE_NATIVE_SYSROOT}${datadir}/phosphor-dbus-yaml/yaml
phosphor-logging/elog-errors.hpp:
	@mkdir -p ${YAML_DIR}/org/open_power/Host
	@cp ${top_srcdir}/org/open_power/Host/Boot.errors.yaml ${YAML_DIR}/org/open_power/Host/Boot.errors.yaml
	@mkdir -p `dirname $@`
	@chmod 777 $(ELOG_GEN_DIR)/elog-gen.py
	$(AM_V_at)$(PYTHON) $(ELOG_GEN_DIR)/elog-gen.py -y ${YAML_DIR} -t ${ELOG_MAKO_DIR} -m ${ELOG_MAKO} -o $@
endif
