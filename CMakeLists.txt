cmake_minimum_required(VERSION 2.6)

set(CMAKE_VERBOSE_MAKEFILE              off)

PROJECT(e4rat-lite)

set(E4RAT_VERSION   "0.2.4-git")

IF( NOT CMAKE_BUILD_TYPE )
    SET( CMAKE_BUILD_TYPE "debug" )
ENDIF()

IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/" CACHE PATH "e4rat install prefix" FORCE)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

set(Boost_USE_STATIC_LIBS   ON)
set(Boost_USE_MULTITHREADED OFF)
add_definitions(-DBOOST_FILESYSTEM_VERSION=2)
find_package(Boost 1.41 COMPONENTS system filesystem regex REQUIRED)
set(${PROJECT_NAME}_LIBRARIES   ${${PROJECT_NAME}_LIBRARIES}
    ${Boost_LIBRARIES})
set(Boost_VERSION_STR  "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")

add_subdirectory(   ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_subdirectory(   ${CMAKE_CURRENT_SOURCE_DIR}/doc)


# to generate debian package run: make package
FIND_PROGRAM(DPKG_CMD dpkg)
if(DPKG_CMD)
    EXECUTE_PROCESS(COMMAND "${DPKG_CMD}" --print-architecture
        OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else(DPKG_CMD)
    MESSAGE(STATUS "CPACK: Can not find dpkg in your path, default to i386.")
    SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
endif(DPKG_CMD)

SET (CPACK_GENERATOR DEB)
SET(CPACK_PACKAGE_VERSION  ${E4RAT_VERSION})
SET(CPACK_PACKAGE_FILE_NAME
    "${PROJECT_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

IF(NOT CMAKE_BUILD_TYPE STREQUAL "release")
    SET(CPACK_PACKAGE_FILE_NAME
    	${CPACK_PACKAGE_FILE_NAME}_debug)
endif(NOT CMAKE_BUILD_TYPE STREQUAL "release")

SET (CPACK_INSTALL_PREFIX "/")

if(NOT CPACK_INSTALL_PREFIX STREQUAL CMAKE_INSTALL_PREFIX)
SET(CPACK_PACKAGE_FILE_NAME
    	${CPACK_PACKAGE_FILE_NAME}_invalid-prefix)
endif(NOT CPACK_INSTALL_PREFIX STREQUAL CMAKE_INSTALL_PREFIX)

SET (CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Reduce disk access time on ext4 filesystems through relevant file defragmentation.")
SET (CPACK_PACKAGE_CONTACT              "Andreas Rid <conso@users.sf.net>")
SET (CPACK_RESOURCE_FILE_LICENSE        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
SET (CPACK_DEBIAN_PACKAGE_SECTION       "extra")
SET (CPACK_DEBIAN_PACKAGE_MAINTAINER    "Andreas Rid")
set (CPACK_SET_DESTDIR                  "ON") # Necessary because of the absolute install paths

# extra install and uninstall create subfolders
# I don't know why cpack deb does not create or remove directories.
set( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_SOURCE_DIR}/debian/preinst"
        "${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm")

SET (CPACK_DEBIAN_PACKAGE_DEPENDS
        "e2fslibs (>=1.41)")

SET (CPACK_DEBIAN_PACKAGE_CONFLICTS
        "auditd, ureadahead")

INCLUDE(CPack)

add_custom_target(src
        COMMAND test -e ${PROJECT_NAME}-${E4RAT_VERSION} || mkdir ${PROJECT_NAME}-${E4RAT_VERSION}
        COMMAND rsync -p --relative `git ls-files` ${PROJECT_NAME}-${E4RAT_VERSION}
        COMMAND tar pczf ${PROJECT_NAME}_${E4RAT_VERSION}_src.tar.gz ${PROJECT_NAME}-${E4RAT_VERSION}
        )

INSTALL(FILES e4rat-lite.conf_pt_BR
              e4rat-lite.conf_en_US
        DESTINATION "/usr/share/e4rat-lite/conf"
        )
