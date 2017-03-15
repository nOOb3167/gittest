FUNCTION (GITTEST_DEPS_SET_DEP_VARS_WIN)
  GITTEST_COMMON_ENSURE_PLATFORM(WIN)
  
  # search for all needed packages
  
    FIND_PACKAGE(LibGit2 REQUIRED)
  ## ZLIB is a dependency of LibGit2
  ##   if LibGit2 does not find ZLIB it will use bundled (happens on windows/MSVC)
  IF (NOT GITTEST_COMPILER_MSVC)
    FIND_PACKAGE(ZLIB REQUIRED)
  ENDIF ()
  FIND_PACKAGE(ENet REQUIRED)
  FIND_PACKAGE(Shlwapi REQUIRED)
  
  # set output variables
  
  SET(GITTEST_DEP_INCLUDE_DIRS
    ${LIBGIT2_INCLUDE_DIR}
    ${ENET_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
  PARENT_SCOPE)
  SET (GITTEST_DEP_LIBRARIES
    ${LIBGIT2_LIBRARIES}
    ${ENET_LIBRARIES}
    # ZLIB must be on the link list AFTER LibGit2 to resolve symbols
    ${ZLIB_LIBRARIES}
    ${SHLWAPI_LIBRARIES}
  PARENT_SCOPE)
ENDFUNCTION ()

FUNCTION (GITTEST_DEPS_SET_DEP_VARS_NIX)
  GITTEST_COMMON_ENSURE_PLATFORM(NIX)
  
  # search for all needed packages
  
  FIND_PACKAGE(LibGit2 REQUIRED)
  ## ZLIB is a dependency of LibGit2
  FIND_PACKAGE(ZLIB REQUIRED)
  FIND_PACKAGE(ENet REQUIRED)
  
  ## http://stackoverflow.com/questions/1620918/cmake-and-libpthread/29871891#29871891
  ## https://cmake.org/cmake/help/v3.6/module/FindThreads.html
  ##   extra magic for gcc linking with pthreads (-pthread)
  
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  FIND_PACKAGE(Threads REQUIRED)
  
  # set output variables
  
  SET(GITTEST_DEP_INCLUDE_DIRS
    ${LIBGIT2_INCLUDE_DIR}
    ${ENET_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
  PARENT_SCOPE)
  SET (GITTEST_DEP_LIBRARIES
    ${LIBGIT2_LIBRARIES}
    ${ENET_LIBRARIES}
    # ZLIB must be on the link list AFTER LibGit2 to resolve symbols
    ${ZLIB_LIBRARIES}
    Threads::Threads
  PARENT_SCOPE)
ENDFUNCTION ()
