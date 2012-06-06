cmake_minimum_required(VERSION 2.6)

set( _MAN_LANGS pt_BR en_US )

# test weather variables from top level directories are missing
IF( NOT E4RAT-LITE_VERSION )
    MESSAGE(FATAL_ERROR "Do not run cmake on subdirectories")
ENDIF()

ADD_CUSTOM_TARGET(ManPages ALL)

foreach( _langs ${_MAN_LANGS} )
	foreach( _man  e4rat-lite-collect e4rat-lite-realloc e4rat-lite-preload )
	    ADD_CUSTOM_COMMAND(
	        TARGET ManPages
	        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/${_man}.pod
	        COMMAND pod2man ARGS -u
	                        ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/${_man}.pod
	                        ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/${_man}.8
	                        --section 8
	                        --center "User Manuals"
	                        --release "e4rat-lite ${E4RAT-LITE_VERSION}"
	        OUTPUTS ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/${_man}.8
	    )
	endforeach( _man )
	
	ADD_CUSTOM_COMMAND(
	    TARGET ManPages
	    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite.conf.pod
	    COMMAND pod2man ARGS -u
	                         ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite.conf.pod
	                         ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite.conf.5
	                         --section 5
	                         --center "User Manuals"
	                         --release "e4rat-lite ${E4RAT-LITE_VERSION}"
	    OUTPUTS ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite.conf.5
	)
	
	ADD_CUSTOM_COMMAND(
	    TARGET ManPages
	    SOURCE ManPages
	    DEPENDS
	                ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite-collect.8
	                ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite-realloc.8
	                ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite-preload.8
	                ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite.conf.5
	)
	
	INSTALL(FILES
	                ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite-collect.8
	                ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite-realloc.8
	                ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite-preload.8
	    DESTINATION /usr/share/man/${_langs}/man8/)
	
	INSTALL(FILES
	                ${CMAKE_CURRENT_SOURCE_DIR}/${_langs}/e4rat-lite.conf.5
	    DESTINATION /usr/share/man/${_langs}/man5/)
endforeach( _langs )
