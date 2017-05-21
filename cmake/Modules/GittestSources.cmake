MACRO (GITTEST_SOURCES_SET)
  GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(
    gittest_lib
    gittest_net
    gittest_selfupdate
    gittest_serv
    gittest_clnt
    gittest_clnt_clone
    gittest_repo_setup
    gittest_log_convert
  )
  GITTEST_SOURCES_SET_COMMON()
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(MISC)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(LIB)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(NET)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(SELFUPDATE)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(SERV)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(CLNT)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(REPO_SETUP)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(LOG_CONVERT)
  GITTEST_COMMON_ENSURE_PLATFORM()
  GITTEST_AUX_SOURCES_APPEND_PLATFORM(MISC ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_APPEND_PLATFORM(LIB ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_APPEND_PLATFORM(NET ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_APPEND_PLATFORM(SELFUPDATE ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_APPEND_PLATFORM(SERV ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_APPEND_PLATFORM(CLNT ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_APPEND_PLATFORM(REPO_SETUP ${GITTEST_PLATFORM_PLAT})  
  GITTEST_AUX_SOURCES_APPEND_PLATFORM(LOG_CONVERT ${GITTEST_PLATFORM_PLAT})  
ENDMACRO ()


FUNCTION (GITTEST_SOURCES_DEFINE_COSMETIC_VISUAL_STUDIO_STUFF)
  GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(
    gittest_lib
    gittest_net
    gittest_selfupdate
    gittest_serv
    gittest_clnt
    gittest_clnt_clone
    gittest_repo_setup
    gittest_log_convert
  )
  GITTEST_COMMON_ENSURE_PLATFORM()

  IF (GITTEST_PLATFORM_WIN)

    # FIXME: how does SET_DIRECTORY_PROPERTIES ACTUALLY work within function?
    SET_DIRECTORY_PROPERTIES(PROPERTIES
    #    VS_STARTUP_PROJECT "gittest_selfupdate"
    )
  
    # SOURCE_GROUP is used by the Visual Studio CMake Generator
    # (just to make the generated vs project prettier)
    SOURCE_GROUP("Header Files" FILES ${GITTEST_LIB_HEADERS})
    SOURCE_GROUP("Source Files" FILES ${GITTEST_LIB_SOURCES})
    SOURCE_GROUP("Header Files" FILES ${GITTEST_NET_HEADERS})
    SOURCE_GROUP("Source Files" FILES ${GITTEST_NET_SOURCES})
    SOURCE_GROUP("Header Files" FILES ${GITTEST_SELFUPDATE_HEADERS})
    SOURCE_GROUP("Source Files" FILES ${GITTEST_SELFUPDATE_SOURCES})
    SOURCE_GROUP("Header Files" FILES ${GITTEST_SERV_HEADERS})
    SOURCE_GROUP("Source Files" FILES ${GITTEST_SERV_SOURCES})
    SOURCE_GROUP("Header Files" FILES ${GITTEST_CLNT_HEADERS})
    SOURCE_GROUP("Source Files" FILES ${GITTEST_CLNT_SOURCES})
    SOURCE_GROUP("Header Files" FILES ${GITTEST_REPO_SETUP_HEADERS})
    SOURCE_GROUP("Source Files" FILES ${GITTEST_REPO_SETUP_SOURCES})
    SOURCE_GROUP("Header Files" FILES ${GITTEST_LOG_CONVERT_HEADERS})
    SOURCE_GROUP("Source Files" FILES ${GITTEST_LOG_CONVERT_SOURCES})
  
    # add NIX sources unconditionally for SOURCE_GROUP calls.
    
    # http://stackoverflow.com/a/31653555
    #   need to actually be used by a target for SOURCE_GROUP to work.
    # therefore create a dummy target (static library).
    # so that the files are not compiled, set HEADER_FILE_ONLY source property.

    SET(TMPLIST ";")
    GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM(MISC NIX TMPLIST TMPLIST)
    GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM(LIB NIX TMPLIST TMPLIST)
    GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM(NET NIX TMPLIST TMPLIST)
    GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM(SELFUPDATE NIX TMPLIST TMPLIST)
    GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM(SERV NIX TMPLIST TMPLIST)
    GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM(CLNT NIX TMPLIST TMPLIST)
    GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM(REPO_SETUP NIX TMPLIST TMPLIST)
    GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM(LOG_CONVERT NIX TMPLIST TMPLIST)
    
    SOURCE_GROUP("Platform Files" FILES ${TMPLIST})
    
    SET_SOURCE_FILES_PROPERTIES(${TMPLIST} PROPERTIES HEADER_FILE_ONLY TRUE)
    
    ADD_LIBRARY(dummy_lib STATIC ${TMPLIST})
    # FIXME: grrr, magic (duplicate of GITTEST_SOURCES_CONFIGURE_TARGETS)
    TARGET_INCLUDE_DIRECTORIES(dummy_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/)
  ENDIF ()

ENDFUNCTION ()


FUNCTION (GITTEST_SOURCES_CREATE_TARGETS)
  GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(
    gittest_lib
    gittest_net
    gittest_selfupdate
    gittest_serv
    gittest_clnt
    gittest_clnt_clone
    gittest_repo_setup
    gittest_log_convert
  )

  ADD_LIBRARY(gittest_lib STATIC
    ${GITTEST_MISC_HEADERS}
    ${GITTEST_LIB_HEADERS}
    ${GITTEST_LIB_SOURCES}
  )

  ADD_LIBRARY(gittest_net STATIC
    ${GITTEST_NET_HEADERS}
    ${GITTEST_NET_SOURCES}
  )

  ADD_LIBRARY(gittest_selfupdate STATIC
    ${GITTEST_SELFUPDATE_HEADERS}
    ${GITTEST_SELFUPDATE_SOURCES}
  )

  ADD_EXECUTABLE(gittest_serv
    ${GITTEST_SERV_HEADERS}
    ${GITTEST_SERV_SOURCES}
  )

  ADD_EXECUTABLE(gittest_clnt
    ${GITTEST_CLNT_HEADERS}
    ${GITTEST_CLNT_SOURCES}
  )

  ADD_EXECUTABLE(gittest_clnt_clone
    ${GITTEST_CLNT_HEADERS}
    ${GITTEST_CLNT_SOURCES}
  )

  ADD_EXECUTABLE(gittest_repo_setup
    ${GITTEST_REPO_SETUP_HEADERS}
    ${GITTEST_REPO_SETUP_SOURCES}
  )
  
  ADD_EXECUTABLE(gittest_log_convert
    ${GITTEST_LOG_CONVERT_HEADERS}
    ${GITTEST_LOG_CONVERT_SOURCES}
  )
ENDFUNCTION ()


FUNCTION (GITTEST_SOURCES_CONFIGURE_TARGETS)
  GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(
    gittest_lib
    gittest_net
    gittest_selfupdate
    gittest_serv
    gittest_clnt
    gittest_clnt_clone
    gittest_repo_setup
    gittest_log_convert
  )

  # explicit suffix definition for executables (prefer .exe even on nix)
  
  SET_PROPERTY(TARGET gittest_serv       PROPERTY SUFFIX ".exe")
  SET_PROPERTY(TARGET gittest_clnt       PROPERTY SUFFIX ".exe")
  SET_PROPERTY(TARGET gittest_clnt_clone PROPERTY SUFFIX ".exe")
  SET_PROPERTY(TARGET gittest_repo_setup PROPERTY SUFFIX ".exe")
  SET_PROPERTY(TARGET gittest_log_convert PROPERTY SUFFIX ".exe")
  
  # target compile definitions

  FILE(READ "${CMAKE_SOURCE_DIR}/data/gittest_config_serv.conf" GS_CMAKE_CONFIG_BUILTIN_HEXSTRING HEX)
  
  SET(GITTEST_DEFINITIONS
    -DEXTERNAL_GS_CONFIG_DEFS_GLOBAL_CONFIG_BUILTIN_HEXSTRING="${GS_CMAKE_CONFIG_BUILTIN_HEXSTRING}"
    # FIXME: WIN hardcoded
    -DEXTERNAL_GS_CONFIG_DEFS_GLOBAL_DEBUG_BREAK=GS_CONFIG_DEFS_WIN
    -DEXTERNAL_GS_CONFIG_DEFS_GLOBAL_CLEAN_HANDLING=GS_CONFIG_DEFS_NONE
  )
  
  SET(GITTEST_DEFINITIONS_EXTRA_CLNT
    GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB="versub_clnt"
  )
  
  SET(GITTEST_DEFINITIONS_EXTRA_CLNT_CLONE
    GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB="versub_clnt_clone"
  )
  
  TARGET_COMPILE_DEFINITIONS(gittest_lib PRIVATE ${GITTEST_DEFINITIONS})
  TARGET_COMPILE_DEFINITIONS(gittest_net PRIVATE ${GITTEST_DEFINITIONS})
  TARGET_COMPILE_DEFINITIONS(gittest_selfupdate PRIVATE ${GITTEST_DEFINITIONS})
  TARGET_COMPILE_DEFINITIONS(gittest_serv PRIVATE ${GITTEST_DEFINITIONS})
  TARGET_COMPILE_DEFINITIONS(gittest_clnt PRIVATE
    ${GITTEST_DEFINITIONS}
    ${GITTEST_DEFINITIONS_EXTRA_CLNT}
  )
  TARGET_COMPILE_DEFINITIONS(gittest_clnt_clone PRIVATE
    ${GITTEST_DEFINITIONS}
    ${GITTEST_DEFINITIONS_EXTRA_CLNT_CLONE}
  )
  TARGET_COMPILE_DEFINITIONS(gittest_repo_setup PRIVATE ${GITTEST_DEFINITIONS})
  TARGET_COMPILE_DEFINITIONS(gittest_log_convert PRIVATE ${GITTEST_DEFINITIONS})
  
  # target include dirs
  
  SET(GITTEST_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${GITTEST_DEP_INCLUDE_DIRS}
  )

  TARGET_INCLUDE_DIRECTORIES(gittest_lib PRIVATE ${GITTEST_INCLUDE_DIRS})
  TARGET_INCLUDE_DIRECTORIES(gittest_net PRIVATE ${GITTEST_INCLUDE_DIRS})
  TARGET_INCLUDE_DIRECTORIES(gittest_selfupdate PRIVATE ${GITTEST_INCLUDE_DIRS})
  TARGET_INCLUDE_DIRECTORIES(gittest_serv PRIVATE ${GITTEST_INCLUDE_DIRS})
  TARGET_INCLUDE_DIRECTORIES(gittest_clnt PRIVATE ${GITTEST_INCLUDE_DIRS})
  TARGET_INCLUDE_DIRECTORIES(gittest_clnt_clone PRIVATE ${GITTEST_INCLUDE_DIRS})
  TARGET_INCLUDE_DIRECTORIES(gittest_repo_setup PRIVATE ${GITTEST_INCLUDE_DIRS})
  TARGET_INCLUDE_DIRECTORIES(gittest_log_convert PRIVATE ${GITTEST_INCLUDE_DIRS})
  
  # target libraries
  
  SET(GITTEST_LIBRARIES
    ${GITTEST_DEP_LIBRARIES}
  )
  
  # http://stackoverflow.com/questions/14468678/cmake-link-a-library-to-library/14480396#14480396
  #   linking static libraries to static libraries not needed? list them just for the executables.
  TARGET_LINK_LIBRARIES(gittest_lib)
  TARGET_LINK_LIBRARIES(gittest_net)
  TARGET_LINK_LIBRARIES(gittest_selfupdate)
  TARGET_LINK_LIBRARIES(gittest_serv gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
  TARGET_LINK_LIBRARIES(gittest_clnt gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
  TARGET_LINK_LIBRARIES(gittest_clnt_clone gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
  TARGET_LINK_LIBRARIES(gittest_repo_setup gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
  TARGET_LINK_LIBRARIES(gittest_log_convert gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
ENDFUNCTION ()


FUNCTION (GITTEST_SOURCES_INSTALL_TARGETS)
  GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(
    gittest_lib
    gittest_net
    gittest_selfupdate
    gittest_serv
    gittest_clnt
    gittest_clnt_clone
    gittest_repo_setup
    gittest_log_convert
  )
  GITTEST_COMMON_ENSURE_COMPILER()

  INSTALL(TARGETS gittest_serv gittest_clnt gittest_clnt_clone gittest_repo_setup gittest_log_convert
    LIBRARY DESTINATION "lib"
    RUNTIME DESTINATION "bin"
    ARCHIVE DESTINATION "lib"
  )

  # https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html
  #   $<TARGET_PDB_FILE:tgt>

  IF (GITTEST_COMPILER_MSVC)
      INSTALL(FILES
        "$<TARGET_PDB_FILE:gittest_serv>"
        "$<TARGET_PDB_FILE:gittest_clnt>"
        "$<TARGET_PDB_FILE:gittest_clnt_clone>"
        "$<TARGET_PDB_FILE:gittest_repo_setup>"
        "$<TARGET_PDB_FILE:gittest_log_convert>"
        DESTINATION "bin"
      )
  ENDIF ()

  INSTALL(DIRECTORY "data/"
      DESTINATION "data"
      PATTERN "marker.txt" EXCLUDE
  )
ENDFUNCTION ()


MACRO (GITTEST_SOURCES_SET_COMMON)
  GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(
    gittest_lib
    gittest_net
    gittest_selfupdate
    gittest_serv
    gittest_clnt
    gittest_clnt_clone
    gittest_repo_setup
    gittest_log_convert
  )
  
  # misc

  SET(GITTEST_MISC_HEADERS
    include/gittest/config_defs.h
    include/gittest/log_defs.h
  )
  GITTEST_AUX_SOURCES_SET_MAGIC_EMPTY(GITTEST_MISC_SOURCES)
  
  # gittest_lib

  SET(GITTEST_LIB_HEADERS
    include/gittest/gittest.h
    include/gittest/cbuf.h
    include/gittest/config.h
    include/gittest/log.h
    include/gittest/misc.h
    include/gittest/bypart.h
  )
  SET(GITTEST_LIB_HEADERS_NIX
    include/gittest/misc_nix.h
  )

  SET(GITTEST_LIB_SOURCES
    src/cbuf.cpp
    src/config.cpp
    src/log.cpp
    src/log_unified.cpp
    src/misc.cpp
    src/main.cpp
    src/bypart.cpp
  )
  SET(GITTEST_LIB_SOURCES_WIN
    src/log_win.cpp
    src/misc_win.cpp
  )
  SET(GITTEST_LIB_SOURCES_NIX
    src/log_nix.cpp
    src/misc_nix.cpp
  )
  
  # gittest_net

  SET(GITTEST_NET_HEADERS
    include/gittest/frame.h
    include/gittest/net2.h
    include/gittest/net2_fwd.h
    include/gittest/net2_surrogate.h
    include/gittest/net2_request.h
    include/gittest/net2_crankdata.h
    include/gittest/net2_affinity.h
    include/gittest/net2_aux.h
    include/gittest/crank_clnt.h
    include/gittest/crank_serv.h
    include/gittest/crank_selfupdate_basic.h
    include/gittest/crank_test.h
  )
  
  SET(GITTEST_NET_SOURCES
    src/frame.cpp
    src/net2.cpp
    src/net2_surrogate.cpp
    src/net2_request.cpp
    src/net2_crankdata.cpp
    src/net2_affinity.cpp
    src/net2_aux.cpp
    src/net2_test.cpp
    src/crank_clnt.cpp
    src/crank_serv.cpp
    src/crank_selfupdate_basic.cpp
    src/crank_test.cpp
  )
  
  # gittest_selfupdate
  
  SET(GITTEST_SELFUPDATE_HEADERS
    include/gittest/gittest_selfupdate.h
  )
  
  SET(GITTEST_SELFUPDATE_SOURCES
    src/gittest_selfupdate.cpp
  )
  SET(GITTEST_SELFUPDATE_SOURCES_WIN
    src/gittest_selfupdate_win.cpp
  )
  SET(GITTEST_SELFUPDATE_SOURCES_NIX
    src/gittest_selfupdate_nix.cpp
  )
  
  # gittest_serv
  
  GITTEST_AUX_SOURCES_SET_MAGIC_EMPTY(GITTEST_SERV_HEADERS)
  
  SET(GITTEST_SERV_SOURCES
    src/gittest_serv.cpp
  )
  
  # gittest_clnt
  
  GITTEST_AUX_SOURCES_SET_MAGIC_EMPTY(GITTEST_CLNT_HEADERS)

  SET(GITTEST_CLNT_SOURCES
    src/gittest_clnt.cpp
  )
  
  # gittest_repo_setup
  
  GITTEST_AUX_SOURCES_SET_MAGIC_EMPTY(GITTEST_REPO_SETUP_HEADERS)

  SET(GITTEST_REPO_SETUP_SOURCES
    src/gittest_repo_setup.cpp
  )
  
  # gittest_log_convert
  
  GITTEST_AUX_SOURCES_SET_MAGIC_EMPTY(GITTEST_LOG_CONVERT_HEADERS)
  
  SET(GITTEST_LOG_CONVERT_SOURCES
    src/gittest_log_convert.cpp
  )

ENDMACRO ()
