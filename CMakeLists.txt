cmake_minimum_required(VERSION 2.6)

PROJECT(e4rat-lite)
set(E4RAT-LITE_VERSION "2.7-git")
set(CMAKE_VERBOSE_MAKEFILE off)
set(LOCALE_INSTALL_DIR "/usr/share/locale")

IF( NOT CMAKE_BUILD_TYPE )
    SET( CMAKE_BUILD_TYPE "debug" )
ENDIF()

IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/" CACHE PATH "e4rat install prefix" FORCE)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

set(Boost_USE_STATIC_LIBS   OFF)
set(Boost_USE_MULTITHREADED OFF)
find_package(Boost 1.41 COMPONENTS system filesystem regex REQUIRED)
set(${PROJECT_NAME}_LIBRARIES   ${${PROJECT_NAME}_LIBRARIES}
    ${Boost_LIBRARIES})
set(Boost_VERSION_STR  "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")

add_subdirectory(   ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_subdirectory(   ${CMAKE_CURRENT_SOURCE_DIR}/doc)
add_subdirectory(   ${CMAKE_CURRENT_SOURCE_DIR}/po)

add_custom_target(src
        COMMAND test -e ${PROJECT_NAME}-${E4RAT-LITE_VERSION} || mkdir ${PROJECT_NAME}-${E4RAT-LITE_VERSION}
        COMMAND rsync -p --relative `git ls-files` ${PROJECT_NAME}-${E4RAT-LITE_VERSION}
        COMMAND tar pczf ${PROJECT_NAME}_${E4RAT-LITE_VERSION}_src.tar.gz ${PROJECT_NAME}-${E4RAT-LITE_VERSION}
        )

INSTALL(FILES conf/e4rat-lite.conf_pt_BR
              conf/e4rat-lite.conf_en_US
        DESTINATION "/usr/share/e4rat-lite/conf"
        )
