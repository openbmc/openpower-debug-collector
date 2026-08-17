# SPDX-License-Identifier: Apache-2.0

# Note: phal_backend variable is set in parent meson.build
phal_deps = []
phal_src = []

# phosphor-logging is always needed (lg2 used in all backends)
phal_deps += [dependency('phosphor-logging')]

# sdbusplus is needed for both backends (D-Bus PEL creation)
phal_deps += [dependency('sdbusplus')]

if phal_backend == 'next'
    error('The phal-next implementation is provided by a follow-up change')
elif phal_backend == 'legacy'
    add_project_arguments('-DLEGACY_PHAL', language: 'cpp')
    phal_deps += [
        cxx.find_library('pdbg'),
        cxx.find_library('libdt-api'),
        cxx.find_library('phal'),
    ]
    phal_src += files(
        'chipop_legacy.cpp',
        'error_legacy.cpp',
        'targeting_legacy.cpp',
    )
endif

# Build as a static library
phal_abstraction_lib = static_library(
    'phal_abstraction',
    phal_src,
    dependencies: phal_deps,
    include_directories: [
        include_directories('.'),
        include_directories('../dump'),  # For create_pel.hpp in error_legacy.cpp
    ],
)

# Declare dependency for other modules to use
phal_abstraction_dep = declare_dependency(
    link_with: phal_abstraction_lib,
    include_directories: include_directories('.'),
    dependencies: phal_deps,
)
