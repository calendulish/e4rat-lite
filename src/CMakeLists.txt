cmake_minimum_required(VERSION 2.6)

SET (CMAKE_C_FLAGS_RELEASE "-std=c99" )

# test weather variables from top level directories are missing
IF( NOT CMAKE_BUILD_TYPE )
  MESSAGE(FATAL_ERROR "Do not run cmake on subdirectories")
ENDIF()

if(NOT BUILD_CORE_LIBRARY_STATIC)
  if(CMAKE_SIZEOF_VOID_P MATCHES 8)
     SET(BUILD_CORE_LIBRARY_STATIC 1)
  endif()
endif()

#TODO permit link dependencies not static
#set(LINK_DEPENDENIES "static")

if(CMAKE_COMPILER_IS_GNUCXX)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                OUTPUT_VARIABLE GCC_VERSION)

    if(GCC_VERSION VERSION_LESS 4.5)
        message(WARNING "GCC <4.5 does not support -static-libstdc++")
	execute_process(COMMAND stat -c %d /
                        OUTPUT_VARIABLE DEV_ROOT_DIR)
        execute_process(COMMAND stat -c %d /usr
                        OUTPUT_VARIABLE DEV_USR_DIR)
        if(NOT ${DEV_ROOT_DIR} STREQUAL ${DEV_USR_DIR})
	    message(FATAL_ERROR "Your /usr directory is not part of the root device. Upgrade to gcc >=4.5.")
	endif(NOT ${DEV_ROOT_DIR} STREQUAL ${DEV_USR_DIR})
    endif(GCC_VERSION VERSION_LESS 4.5)
endif(CMAKE_COMPILER_IS_GNUCXX)

add_definitions(-Wall)
add_definitions(-DPROGRAM_NAME=\"${PROJECT_NAME}\")
add_definitions(-DVERSION=\"${E4RAT-LITE_VERSION}\")
IF(CMAKE_BUILD_TYPE STREQUAL "release")
    add_definitions(-O2)
    add_definitions(-D_FORTIFY_SOURCE=2)
    add_definitions(-fstack-protector-all)
else(CMAKE_BUILD_TYPE STREQUAL "release")
    add_definitions(-ggdb -DDEBUG_ENABLED)
endif(CMAKE_BUILD_TYPE STREQUAL "release")
if(MOVE_EXT_RDONLY_FLAG)
    add_definitions(-DMOVE_EXT_RDONLY_FLAG)
endif(MOVE_EXT_RDONLY_FLAG)

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
        config.c
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
        e4rat-preload.c
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
if(BUILD_CORE_LIBRARY_STATIC)
    add_library(${PROJECT_NAME}-core STATIC
        ${${PROJECT_NAME}-core_SRC}
    )
else(BUILD_CORE_LIBRARY_STATIC)
    add_library(${PROJECT_NAME}-core SHARED
        ${${PROJECT_NAME}-core_SRC}
    )
    set_target_properties(${PROJECT_NAME}-core  PROPERTIES
        SOVERSION   0
        LINK_FLAGS  "-static-libgcc -static-libstdc++"
    )
endif(BUILD_CORE_LIBRARY_STATIC)

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

if(NOT BUILD_CORE_LIBRARY_STATIC)
    set(${PROJECT_NAME}_TARGETS
        ${${PROJECT_NAME}_TARGETS}
        ${PROJECT_NAME}-core
    )
endif(NOT BUILD_CORE_LIBRARY_STATIC)

INSTALL(TARGETS ${${PROJECT_NAME}_TARGETS}
        RUNTIME DESTINATION "sbin"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
)

INSTALL(DIRECTORY DESTINATION "/var/lib/${PROJECT_NAME}")
