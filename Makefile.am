AM_DEFAULT_SOURCE_EXT = .cpp
sbin_PROGRAMS = checkstop_ffdc_app

checkstop_ffdc_app_SOURCES = \
				checkstop_ffdc_app.cpp

BUILT_SOURCES = elog-errors.hpp

CLEANFILES = elog-errors.hpp

checkstop_app_LDFLAGS = \
					$(SYSTEMD_LIBS)	\
					$(SDBUSPLUS_LIBS) \
                    $(PHOSPHOR_LOGGING_LIBS)

checkstop_app_CXXFLAGS = $(SYSTEMD_CFLAGS) \
                        $(SDBUSPLUS_CFLAGS) \
                        $(PHOSPHOR_LOGGING_CFLAGS)

ELOG_MAKO ?= elog-gen-template.mako.hpp
META_MAKO ?= elog-process-metadata.mako.cpp
ELOG_TEMPLATE_DIR ?= ${abs_srcdir}/tools/templates/
REQ_FILES_TO_GEN ?= ${abs_srcdir}/tools/elog-gen.py\
	${ELOG_TEMPLATE_DIR}/$(ELOG_MAKO)\
	${ELOG_TEMPLATE_DIR}/$(META_MAKO)

# Be sure to package up the required script, yaml, and mako template to
# generate the header file
EXTRA_DIST = $(REQ_FILES_TO_GEN)

elog-errors.hpp: $(REQ_FILES_TO_GEN)
	@mkdir -p `dirname $@`
	$(AM_V_at)$(PYTHON) ${abs_srcdir}/tools/elog-gen.py -y ${YAML_DIR} -t ${ELOG_TEMPLATE_DIR} -m $(ELOG_MAKO) -o $@
