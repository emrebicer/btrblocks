# ---------------------------------------------------------------------------
# BtrBlocks library wrapper for Rust FFI
# ---------------------------------------------------------------------------

if (NOT UNIX)
    message(FATAL_ERROR "Unsupported platform: Only UNIX systems are supported")
endif()

# ---------------------------------------------------------------------------
# Source Files
# ---------------------------------------------------------------------------

file(GLOB_RECURSE BTRWRAPPER_HH btrwrapper/*.hpp btrwrapper/*.h)
file(GLOB_RECURSE BTRWRAPPER_CC btrwrapper/**/*.cpp btrwrapper/*.cpp btrwrapper/*.c)
set(BTRWRAPPER_SRC ${BTRWRAPPER_HH} ${BTRWRAPPER_CC})

# ---------------------------------------------------------------------------
# Library Target
# ---------------------------------------------------------------------------

# Create the library
if (BUILD_SHARED_LIBRARY)
    add_library(btrwrapper SHARED ${BTRWRAPPER_SRC})
else()
    add_library(btrwrapper STATIC ${BTRWRAPPER_SRC})
endif()

# Add dependencies to ensure build order
add_dependencies(btrwrapper btrblocks croaring tbb csv-parser)

# Compiler options
if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_options(btrwrapper PUBLIC -g)
endif()
target_compile_options(btrwrapper PUBLIC -Wno-unused-parameter)

# Link the dependent libraries
target_link_libraries(btrwrapper
    PUBLIC btrblocks
    PUBLIC croaring
    PUBLIC tbb
    PUBLIC csv-parser
)

target_include_directories(btrwrapper
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

install(TARGETS btrwrapper
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

# Export the public include directory for external projects
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/btrwrapper
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

