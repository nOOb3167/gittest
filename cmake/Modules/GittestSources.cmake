MACRO (GITTEST_SOURCES_SET)
  GITTEST_SOURCES_SET_COMMON()
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(MISC)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(LIB)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(NET)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(SELFUPDATE)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(SERV)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(CLNT)
  GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED(REPO_SETUP)
  GITTEST_COMMON_ENSURE_PLATFORM()
  GITTEST_AUX_SOURCES_ADD_PLATFORM(MISC ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_ADD_PLATFORM(LIB ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_ADD_PLATFORM(NET ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_ADD_PLATFORM(SELFUPDATE ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_ADD_PLATFORM(SERV ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_ADD_PLATFORM(CLNT ${GITTEST_PLATFORM_PLAT})
  GITTEST_AUX_SOURCES_ADD_PLATFORM(REPO_SETUP ${GITTEST_PLATFORM_PLAT})  
ENDMACRO ()

MACRO (GITTEST_SOURCES_DEFINE_COSMETIC_VISUAL_STUDIO_STUFF)
  # SOURCE_GROUP is used by the Visual Studio CMake Generator
  # (just to make the generated vs project prettier)
  SOURCE_GROUP("Header Files" FILES ${GITTEST_LIB_HEADERS})
  SOURCE_GROUP("Source Files" FILES ${GITTEST_LIB_SOURCES})
  SOURCE_GROUP("Header Files" FILES ${GITTEST_NET_HEADERS})
  SOURCE_GROUP("Source Files" FILES ${GITTEST_NET_SOURCES})
  SOURCE_GROUP("Header Files" FILES ${GITTEST_SELFUPDATE_HEADERS})
  SOURCE_GROUP("Source Files" FILES ${GITTEST_SELFUPDATE_SOURCES})
  SOURCE_GROUP("Source Files" FILES ${GITTEST_SERV_HEADERS})
  SOURCE_GROUP("Source Files" FILES ${GITTEST_SERV_SOURCES})
  SOURCE_GROUP("Source Files" FILES ${GITTEST_CLNT_HEADERS})
  SOURCE_GROUP("Source Files" FILES ${GITTEST_CLNT_SOURCES})
  SOURCE_GROUP("Source Files" FILES ${GITTEST_REPO_SETUP_HEADERS})
  SOURCE_GROUP("Source Files" FILES ${GITTEST_REPO_SETUP_SOURCES})
  # FIXME: how does SET_DIRECTORY_PROPERTIES ACTUALLY work within macro?
  SET_DIRECTORY_PROPERTIES(PROPERTIES
  #    VS_STARTUP_PROJECT "gittest_selfupdate"
  )
ENDMACRO ()

FUNCTION (GITTEST_SOURCES_CREATE_TARGETS)
  GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(
    gittest_lib
    gittest_net
    gittest_selfupdate
    gittest_serv
    gittest_clnt
    gittest_clnt_clone
    gittest_repo_setup
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
    ${GITTEST_SERV_SOURCES}
  )

  ADD_EXECUTABLE(gittest_clnt
    ${GITTEST_CLNT_SOURCES}
  )

  ADD_EXECUTABLE(gittest_clnt_clone
    ${GITTEST_CLNT_SOURCES}
  )

  ADD_EXECUTABLE(gittest_repo_setup
    ${GITTEST_REPO_SETUP_SOURCES}
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
  )

  # target compile definitions
  
  FILE(READ "${CMAKE_SOURCE_DIR}/data/gittest_config_serv.conf" GS_CMAKE_CONFIG_BUILTIN_HEXSTRING HEX)
  
  ADD_DEFINITIONS(
    -DEXTERNAL_GS_CONFIG_DEFS_GLOBAL_CONFIG_BUILTIN_HEXSTRING="${GS_CMAKE_CONFIG_BUILTIN_HEXSTRING}"
  )

  ADD_DEFINITIONS(
    -DEXTERNAL_GS_CONFIG_DEFS_GLOBAL_DEBUG_BREAK=GS_CONFIG_DEFS_WIN
    -DEXTERNAL_GS_CONFIG_DEFS_GLOBAL_CLEAN_HANDLING=GS_CONFIG_DEFS_NONE
  )

  TARGET_COMPILE_DEFINITIONS(gittest_clnt PRIVATE GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB="versub_clnt")
  TARGET_COMPILE_DEFINITIONS(gittest_clnt_clone PRIVATE GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB="versub_clnt_clone")
  
  # target include dirs
  
  SET(GITTEST_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${GITTEST_DEP_INCLUDE_DIRS}
  )

  INCLUDE_DIRECTORIES(${GITTEST_INCLUDE_DIRS})

  # target libraries
  
  SET(GITTEST_LIBRARIES
    ${GITTEST_DEP_LIBRARIES}
  )
  
  TARGET_LINK_LIBRARIES(gittest_serv gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
  TARGET_LINK_LIBRARIES(gittest_clnt gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
  TARGET_LINK_LIBRARIES(gittest_clnt_clone gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
  TARGET_LINK_LIBRARIES(gittest_repo_setup gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
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
  )

  INSTALL(TARGETS gittest_serv gittest_clnt gittest_clnt_clone gittest_repo_setup
    LIBRARY DESTINATION "lib"
    RUNTIME DESTINATION "bin"
    ARCHIVE DESTINATION "lib"
  )

  # https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html
  #   $<TARGET_PDB_FILE:tgt>

  IF (MSVC)
      INSTALL(FILES
        "$<TARGET_PDB_FILE:gittest_serv>"
        "$<TARGET_PDB_FILE:gittest_clnt>"
        "$<TARGET_PDB_FILE:gittest_clnt_clone>"
        "$<TARGET_PDB_FILE:gittest_repo_setup>"
        DESTINATION "bin"
      )
  ENDIF ()

  INSTALL(DIRECTORY "data/"
      DESTINATION "data"
      PATTERN "marker.txt" EXCLUDE
  )
ENDFUNCTION ()

MACRO (GITTEST_SOURCES_SET_COMMON)

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
    include/gittest/log.h
    include/gittest/misc.h
  )
  SET(GITTEST_LIB_HEADERS_WIN
    include/gittest/misc_win.h
  )

  SET(GITTEST_LIB_SOURCES
    src/cbuf.cpp
    src/log.cpp
    src/misc.cpp
    src/main.cpp
  )
  SET(GITTEST_LIB_SOURCES_WIN
    src/log_win.cpp
    src/misc_win.cpp
  )
  SET(GITTEST_LIB_SOURCES_NIX
    src/misc_nix.cpp
  )
  
  # gittest_net

  SET(GITTEST_NET_HEADERS
    include/gittest/frame.h
    include/gittest/net.h
  )
  
  SET(GITTEST_NET_SOURCES
    src/frame.cpp
    src/net.cpp
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

ENDMACRO ()