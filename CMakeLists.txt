# 3.4 for generator expressions ?
CMAKE_MINIMUM_REQUIRED(VERSION 3.4)

PROJECT (gittest)

SET_DIRECTORY_PROPERTIES(PROPERTIES
#    VS_STARTUP_PROJECT "gittest_selfupdate"
)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")

FIND_PACKAGE(LibGit2 REQUIRED)
# dependency of LibGit2
#   if LibGit2 does not find ZLIB it will use bundled (happens on windows/MSVC)
IF (NOT MSVC)
  FIND_PACKAGE(ZLIB REQUIRED)
ENDIF ()

FIND_PACKAGE(ENet REQUIRED)

IF (WIN32)
  FIND_LIBRARY(SHLWAPI Shlwapi)
  if (SHLWAPI STREQUAL "SHLWAPI-NOTFOUND")
    MESSAGE(FATAL_ERROR "cannot find Shlwapi ?")
  ENDIF ()
ENDIF ()

SET(GITTEST_INCLUDE_DIRS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  ${LIBGIT2_INCLUDE_DIR}
  ${ENET_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
)

SET(GITTEST_LIBRARIES
  ${LIBGIT2_LIBRARIES}
  ${ENET_LIBRARIES}
# ZLIB must be on the link list AFTER LibGit2 to resolve symbols
  ${ZLIB_LIBRARIES}
  ${SHLWAPI}
)

SET(GITTEST_MISC_HEADERS
  include/gittest/config_defs.h
  include/gittest/log_defs.h
)

SET(GITTEST_LIB_HEADERS
  include/gittest/gittest.h
  include/gittest/cbuf.h
  include/gittest/log.h
  include/gittest/misc.h
)
SET(GITTEST_LIB_SOURCES
  src/cbuf.cpp
  src/log.cpp
  src/misc.cpp
  src/main.cpp
)
SET(GITTEST_LIB_HEADERS_WIN
  include/gittest/misc_win.h
)
SET(GITTEST_LIB_SOURCES_WIN
  src/log_win.cpp
  src/misc_win.cpp
)

SET(GITTEST_NET_HEADERS
  include/gittest/frame.h
  include/gittest/net.h
)
SET(GITTEST_NET_SOURCES
  src/frame.cpp
  src/net.cpp
)

SET(GITTEST_SELFUPDATE_HEADERS
  include/gittest/gittest_selfupdate.h
)
SET(GITTEST_SELFUPDATE_SOURCES
  src/gittest_selfupdate.cpp
)
SET(GITTEST_SELFUPDATE_SOURCES_WIN
  src/gittest_selfupdate_win.cpp
)

SET(GITTEST_SERV_SOURCES
  src/gittest_serv.cpp
)

SET(GITTEST_CLNT_SOURCES
  src/gittest_clnt.cpp
)

ADD_LIBRARY(gittest_lib STATIC
  ${GITTEST_MISC_HEADERS}
  ${GITTEST_LIB_HEADERS}
  ${GITTEST_LIB_HEADERS_WIN}
  ${GITTEST_LIB_SOURCES}
  ${GITTEST_LIB_SOURCES_WIN}
)

ADD_LIBRARY(gittest_net STATIC
  ${GITTEST_NET_HEADERS}
  ${GITTEST_NET_SOURCES}
)

ADD_LIBRARY(gittest_selfupdate STATIC
  ${GITTEST_SELFUPDATE_HEADERS}
  ${GITTEST_SELFUPDATE_SOURCES}
  ${GITTEST_SELFUPDATE_SOURCES_WIN}
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
  src/gittest_repo_setup.cpp
)

SOURCE_GROUP("Header Files" FILES ${GITTEST_LIB_HEADERS})
SOURCE_GROUP("Source Files" FILES ${GITTEST_LIB_SOURCES} ${GITTEST_LIB_SOURCES_WIN})
SOURCE_GROUP("Header Files" FILES ${GITTEST_NET_HEADERS})
SOURCE_GROUP("Source Files" FILES ${GITTEST_NET_SOURCES})
SOURCE_GROUP("Header Files" FILES ${GITTEST_SELFUPDATE_HEADERS})
SOURCE_GROUP("Source Files" FILES ${GITTEST_SELFUPDATE_SOURCES} ${GITTEST_SELFUPDATE_SOURCES_WIN})
SOURCE_GROUP("Source Files" FILES ${GITTEST_SERV_SOURCES})
SOURCE_GROUP("Source Files" FILES ${GITTEST_CLNT_SOURCES})

INCLUDE_DIRECTORIES(${GITTEST_INCLUDE_DIRS})

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

TARGET_LINK_LIBRARIES(gittest_serv gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
TARGET_LINK_LIBRARIES(gittest_clnt gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
TARGET_LINK_LIBRARIES(gittest_clnt_clone gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})
TARGET_LINK_LIBRARIES(gittest_repo_setup gittest_selfupdate gittest_net gittest_lib ${GITTEST_LIBRARIES})

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
