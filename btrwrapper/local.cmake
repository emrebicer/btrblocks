# ---------------------------------------------------------------------------
# BtrBlocks library wrapper for Rust FFI
# ---------------------------------------------------------------------------

set(BTR_CC_LINTER_IGNORE)
if (NOT UNIX)
    message(SEND_ERROR "unsupported platform")
endif ()

# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

file(GLOB_RECURSE BTRWRAPPER_HH btrwrapper/*.hpp btrwrapper/*.h)
file(GLOB_RECURSE BTRWRAPPER_CC btrwrapper/**/**.cpp btrwrapper/*.cpp btrwrapper/*.c)
set(BTRWRAPPER_SRC ${BTRWRAPPER_HH} ${BTRWRAPPER_CC})

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

if (${BUILD_SHARED_LIBRARY})
    add_library(btrwrapper SHARED ${BTRWRAPPER_CC})
else ()
    add_library(btrwrapper STATIC ${BTRWRAPPER_CC})
endif ()


if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_options(btrwrapper PUBLIC -g)
endif ()
target_compile_options(btrwrapper PUBLIC -Wno-unused-parameter)

target_link_libraries(btrblocks croaring tbb csv-parser)

set(BTRWRAPPER_PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR} ${BTR_INCLUDE_DIR} ${CROARING_INCLUDE_DIR} ${TBB_INCLUDE_DIR} ${CSV_INCLUDE_DIR})
set(BTRWRAPPER_PRIVATE_INCLUDE_DIR ${BTRWRAPPER_INCLUDE_DIR})
set(BTRWRAPPER_INCLUDE_DIR ${BTRWRAPPER_PUBLIC_INCLUDE_DIR} ${BTRWRAPPER_PRIVATE_INCLUDE_DIR})

target_include_directories(btrwrapper
    PUBLIC ${BTRWRAPPER_PUBLIC_INCLUDE_DIR}
    PRIVATE ${BTRWRAPPER_PRIVATE_INCLUDE_DIR})


# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

install(TARGETS btrwrapper
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

install(DIRECTORY ${BTRWRAPPER_PUBLIC_INCLUDE_DIR}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "btrblocks-wrapper.hpp")
