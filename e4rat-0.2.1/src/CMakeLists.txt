cmake_minimum_required(VERSION 2.6)

# test weather variables from top level directories are missing
IF( NOT CMAKE_BUILD_TYPE )
  MESSAGE(FATAL_ERROR "Do not run cmake on subdirectories")
ENDIF()

if(CMAKE_SIZEOF_VOID_P MATCHES 4)
    SET(HAVE_64_BIT 0)
else(CMAKE_SIZEOF_VOID_P MATCHES 4)
    SET(HAVE_64_BIT 1)
endif(CMAKE_SIZEOF_VOID_P MATCHES 4)

add_definitions(-Wall)
add_definitions(-DPROGRAM_NAME=\"${PROJECT_NAME}\")
add_definitions(-DVERSION=\"${E4RAT_VERSION}\")
IF(CMAKE_BUILD_TYPE STREQUAL "release")
    add_definitions(-O2)
    add_definitions(-D_FORTIFY_SOURCE=2)
    add_definitions(-fstack-protector-all)
else(CMAKE_BUILD_TYPE STREQUAL "release")
    add_definitions(-ggdb -DDEBUG_ENABLED)
endif(CMAKE_BUILD_TYPE STREQUAL "release")

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)

##
# Set RPATH to find shared library
##
# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
    if(${CMAKE_INSTALL_PREFIX} STREQUAL "/")
        SET(RPATH "/lib")
    else(${CMAKE_INSTALL_PREFIX} STREQUAL "/")
        SET(RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    endif(${CMAKE_INSTALL_PREFIX} STREQUAL "/")
    SET(CMAKE_INSTALL_RPATH "${RPATH}")
ENDIF("${isSystemDir}" STREQUAL "-1")

##
# dependencies
##
FIND_PACKAGE(ext2fs REQUIRED)
set(${PROJECT_NAME}_LIBRARIES   ${${PROJECT_NAME}_LIBRARIES}
    ${EXT2FS_LIBRARY})

find_package(blkid REQUIRED)
set(${PROJECT_NAME}_LIBRARIES   ${${PROJECT_NAME}_LIBRARIES}
    ${BLKID_LIBRARY})

find_package(audit REQUIRED)
set(${PROJECT_NAME}_LIBRARIES   ${${PROJECT_NAME}_LIBRARIES}
    ${AUDIT_LIBRARY})

find_package(auparse REQUIRED)
set(${PROJECT_NAME}_LIBRARIES   ${${PROJECT_NAME}_LIBRARIES}
    ${AUPARSE_LIBRARY})

find_package(Threads REQUIRED)
set(${PROJECT_NAME}_LIBRARIES   ${${PROJECT_NAME}_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT})


###
# Building source code
###
set(${PROJECT_NAME}-core_SRC
        config.cc
        logging.cc
        common.cc
        fiemap.cc
        device.cc
)

ADD_EXECUTABLE(${PROJECT_NAME}-collect
        e4rat-collect.cc
        fileptr.cc
        listener.cc
        eventcatcher.cc
)

ADD_EXECUTABLE(${PROJECT_NAME}-preload
        e4rat-preload.cc
)

ADD_EXECUTABLE(${PROJECT_NAME}-realloc
        e4rat-realloc.cc
        defrag.cc
        buddycache.cc
)


IF(CMAKE_BUILD_TYPE STREQUAL "debug")
    ADD_EXECUTABLE(${PROJECT_NAME}-offsets
        e4rat-offsets.cc
    )
    TARGET_LINK_LIBRARIES(${PROJECT_NAME}-offsets
        ${PROJECT_NAME}-core
    )
ENDIF(CMAKE_BUILD_TYPE STREQUAL "debug")

# On 64 bit architecture you cannot link a static to a shared library
# Therefore -static-libstdc++ will fail.
if(HAVE_64_BIT)
    add_library(${PROJECT_NAME}-core STATIC
        ${${PROJECT_NAME}-core_SRC}
    )
else(HAVE_64_BIT)
    add_library(${PROJECT_NAME}-core SHARED
        ${${PROJECT_NAME}-core_SRC}
    )
    set_target_properties(${PROJECT_NAME}-core  PROPERTIES
        SOVERSION   0
        LINK_FLAGS  "-static-libgcc -static-libstdc++"
    )
endif(HAVE_64_BIT)

target_link_libraries(${PROJECT_NAME}-core
        ${${PROJECT_NAME}_LIBRARIES}
        ${${PROJECT_NAME}_STATIC_LIBRARIES}
)

foreach( EXE     ${PROJECT_NAME}-collect
                 ${PROJECT_NAME}-realloc 
                 ${PROJECT_NAME}-preload)
    TARGET_LINK_LIBRARIES(${EXE}
        ${PROJECT_NAME}-core
    )
    set_target_properties(${EXE}  PROPERTIES
        LINK_FLAGS  "-static-libgcc -static-libstdc++"
    )
endforeach( EXE )

###
# install project targets
###
set(${PROJECT_NAME}_TARGETS 
    ${PROJECT_NAME}-collect
    ${PROJECT_NAME}-preload
    ${PROJECT_NAME}-realloc
)

if(NOT HAVE_64_BIT)
    set(${PROJECT_NAME}_TARGETS 
        ${${PROJECT_NAME}_TARGETS}
        ${PROJECT_NAME}-core
    )
endif(NOT HAVE_64_BIT)

INSTALL(TARGETS ${${PROJECT_NAME}_TARGETS}
        RUNTIME DESTINATION "sbin"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
)

INSTALL(DIRECTORY DESTINATION "/var/lib/${PROJECT_NAME}")
