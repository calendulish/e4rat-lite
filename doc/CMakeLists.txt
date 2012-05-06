cmake_minimum_required(VERSION 2.6)

# test weather variables from top level directories are missing
IF( NOT E4RAT_VERSION )
    MESSAGE(FATAL_ERROR "Do not run cmake on subdirectories")
ENDIF()

ADD_CUSTOM_TARGET(ManPages ALL)

foreach( _man  e4rat-collect e4rat-realloc e4rat-preload )
    ADD_CUSTOM_COMMAND(
        TARGET ManPages
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${_man}.pod
        COMMAND pod2man ARGS ${CMAKE_CURRENT_SOURCE_DIR}/${_man}.pod
                        ${CMAKE_CURRENT_BINARY_DIR}/${_man}.8
                        --section 8
                        --center "User Manuals"
                        --release "e4rat ${E4RAT_VERSION}"
        OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/${_man}.8
    )
endforeach( _man )

ADD_CUSTOM_COMMAND(
    TARGET ManPages
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/e4rat.conf.pod
    COMMAND pod2man ARGS ${CMAKE_CURRENT_SOURCE_DIR}/e4rat.conf.pod
                         ${CMAKE_CURRENT_BINARY_DIR}/e4rat.conf.5
                         --section 5
                         --center "User Manuals"
                         --release "e4rat ${E4RAT_VERSION}"
    OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/e4rat.conf.5
)

ADD_CUSTOM_COMMAND(
    TARGET ManPages
    SOURCE ManPages
    DEPENDS
                ${CMAKE_CURRENT_BINARY_DIR}/e4rat-collect.8
                ${CMAKE_CURRENT_BINARY_DIR}/e4rat-realloc.8
                ${CMAKE_CURRENT_BINARY_DIR}/e4rat-preload.8
                ${CMAKE_CURRENT_BINARY_DIR}/e4rat.conf.5
)

if(CMAKE_INSTALL_PREFIX STREQUAL "/")
    SET(MAN_INSTALL_PREFIX "/usr")
else(CMAKE_INSTALL_PREFIX STREQUAL "/")
    SET(MAN_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/usr")
endif(CMAKE_INSTALL_PREFIX STREQUAL "/")

INSTALL(FILES
                ${CMAKE_CURRENT_BINARY_DIR}/e4rat-collect.8
                ${CMAKE_CURRENT_BINARY_DIR}/e4rat-realloc.8
                ${CMAKE_CURRENT_BINARY_DIR}/e4rat-preload.8
    DESTINATION ${MAN_INSTALL_PREFIX}/share/man/man8)

INSTALL(FILES
                ${CMAKE_CURRENT_BINARY_DIR}/e4rat.conf.5
    DESTINATION ${MAN_INSTALL_PREFIX}/share/man/man5)

