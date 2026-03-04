# SPDX-License-Identifier: Apache-2.0

phal_backend = get_option('phal_backend')

phal_deps = []
phal_src  = []

# phosphor-logging is always needed (lg2 used in all backends)
phal_deps += [dependency('phosphor-logging',
    fallback: ['phosphor-logging', 'phosphor_logging_dep'])]

if phal_backend == 'next'
  add_project_arguments('-DUSE_PHAL_NEXT', language: 'cpp')
  phal_deps += [
    dependency('hostfw_phal'),
    dependency('sdbusplus'),  # Required for D-Bus PEL creation in error_next.cpp
  ]
  phal_src  += files(
    'targeting_next.cpp',
    'chipop_next.cpp',
    'error_next.cpp'
  )
else
  add_project_arguments('-DUSE_PHAL_OLD', language: 'cpp')
  phal_deps += [
    cxx.find_library('pdbg'),
    cxx.find_library('libdt-api'),
    cxx.find_library('phal'),
    dependency('sdbusplus'),
  ]
  phal_src  += files(
    'targeting_old.cpp',
    'chipop_old.cpp',
    'error_old.cpp'
  )
endif

# Build as a static library - internal use only, not installed as a .so
# Using shared library causes BitBake QA errors:
#   - libphal_abstraction.so ends up in -dev package (dev-elf)
#   - main package rdepends on -dev package (dev-deps)
phal_abstraction_lib = static_library(
    'phal_abstraction',
    phal_src,
    dependencies: phal_deps,
    include_directories: include_directories('.'),
)

# Declare dependency for other modules to use
phal_abstraction_dep = declare_dependency(
    link_with: phal_abstraction_lib,
    include_directories: include_directories('.'),
    dependencies: phal_deps,
)