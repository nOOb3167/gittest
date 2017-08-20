MACRO(GITTEST_SOURCES_SET_PREREQ_MISC)
  GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(
    gittest_lib
    gittest_net
    gittest_selfupdate
    gittest_serv
    gittest_clnt
    gittest_clnt_clone
    gittest_repo_setup
    gittest_test_02
    gittest_ev2_test
    gittest_ev2_serv
    gittest_ev2_selfupdate
    gittest_ev2_selfupdate_clone
  )

  SET(GITTEST_SOURCES_TARGETS
    gittest_lib
    gittest_net
    gittest_selfupdate
    gittest_serv
    gittest_clnt
    gittest_clnt_clone
    gittest_repo_setup
    gittest_test_02
    gittest_ev2_test
    gittest_ev2_serv
    gittest_ev2_selfupdate
    gittest_ev2_selfupdate_clone
  )
  SET(GITTEST_SOURCES_TARGETS_EXE
    gittest_serv
    gittest_clnt
    gittest_clnt_clone
    gittest_repo_setup
    gittest_test_02
    gittest_ev2_test
    gittest_ev2_serv
    gittest_ev2_selfupdate
    gittest_ev2_selfupdate_clone
  )
  FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS)
    STRING(REPLACE "gittest_" "" VV "${VV}")
    STRING(TOUPPER "${VV}" VV)
    LIST(APPEND GITTEST_SOURCES_TARGETS_LUMP_UPPERS "${VV}")
  ENDFOREACH()
  FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS_EXE)
    STRING(REPLACE "gittest_" "" VV "${VV}")
    STRING(TOUPPER "${VV}" VV)
    LIST(APPEND GITTEST_SOURCES_TARGETS_LUMP_UPPERS_EXE "${VV}")
  ENDFOREACH()
ENDMACRO()


MACRO (GITTEST_SOURCES_SET)
  GITTEST_SOURCES_SET_COMMON()
  FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS_LUMP_UPPERS)
    GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED("${VV}")
  ENDFOREACH()
  GITTEST_PLATFORM_ENSURE_PLATFORM()
  FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS_LUMP_UPPERS)
    GITTEST_AUX_SOURCES_APPEND_PLATFORM("${VV}" ${GITTEST_PLATFORM_PLAT})
  ENDFOREACH()
ENDMACRO ()


FUNCTION (GITTEST_SOURCES_DEFINE_COSMETIC_VISUAL_STUDIO_STUFF)
  GITTEST_PLATFORM_ENSURE_PLATFORM()

  IF (GITTEST_PLATFORM_WIN)

    # SET_DIRECTORY_PROPERTIES(PROPERTIES VS_STARTUP_PROJECT "gittest_selfupdate")
  
    # SOURCE_GROUP is used by the Visual Studio CMake Generator
    # (just to make the generated vs project prettier)
    FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS_LUMP_UPPERS)
      SOURCE_GROUP("Header Files" FILES ${GITTEST_${VV}_HEADERS})
      SOURCE_GROUP("Source Files" FILES ${GITTEST_${VV}_SOURCES})
    ENDFOREACH()
  
    # add NIX sources unconditionally for SOURCE_GROUP calls.
    
    # http://stackoverflow.com/a/31653555
    #   need to actually be used by a target for SOURCE_GROUP to work.
    # therefore create a dummy target (static library).
    # so that the files are not compiled, set HEADER_FILE_ONLY source property.

    SET(TMPLIST ";")
    FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS_LUMP_UPPERS)
      GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM("${VV}" NIX TMPLIST TMPLIST)
    ENDFOREACH()
    
    SOURCE_GROUP("Platform Files" FILES ${TMPLIST})
    
    SET_SOURCE_FILES_PROPERTIES(${TMPLIST} PROPERTIES HEADER_FILE_ONLY TRUE)
    
    ADD_LIBRARY(dummy_lib STATIC ${TMPLIST})
    # FIXME: grrr, magic (duplicate of GITTEST_SOURCES_CONFIGURE_TARGETS)
    TARGET_INCLUDE_DIRECTORIES(dummy_lib PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${GITTEST_COMMON_PREFIX}/include    # for Gittest Common
    )
  ENDIF ()

ENDFUNCTION ()


FUNCTION (GITTEST_SOURCES_CREATE_TARGETS)
  GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(
    gittest_lib
    gittest_net
    gittest_selfupdate
  )

  ADD_LIBRARY(gittest_lib STATIC
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

  FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS_EXE)
    STRING(REPLACE "gittest_" "" VVLUMPUPPER "${VV}")
    STRING(TOUPPER "${VVLUMPUPPER}" VVLUMPUPPER)
    ADD_EXECUTABLE("${VV}"
      ${GITTEST_${VVLUMPUPPER}_HEADERS}
      ${GITTEST_${VVLUMPUPPER}_SOURCES}
    )    
  ENDFOREACH()
  
ENDFUNCTION ()


FUNCTION (GITTEST_SOURCES_CONFIGURE_TARGETS)
  GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(
    gittest_lib         # special link
    gittest_net         # special link
    gittest_selfupdate  # special link
    gittest_clnt        # special defs
    gittest_clnt_clone  # special defs
    gittest_ev2_selfupdate       # special defs
    gittest_ev2_selfupdate_clone # special defs
  )

  # explicit suffix definition for executables (prefer .exe even on nix)
  
  FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS_EXE)
    SET_PROPERTY(TARGET "${VV}" PROPERTY SUFFIX ".exe")
  ENDFOREACH()
    
  # target compile definitions
  
  TARGET_COMPILE_DEFINITIONS(gittest_clnt       PRIVATE GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB="versub_clnt")
  TARGET_COMPILE_DEFINITIONS(gittest_clnt_clone PRIVATE GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB="versub_clnt_clone")

  TARGET_COMPILE_DEFINITIONS(gittest_ev2_selfupdate       PRIVATE GS_CONFIG_DEFS_GITTEST_EV2_SELFUPDATE_VERSUB="versub_ev2_selfupdate")
  TARGET_COMPILE_DEFINITIONS(gittest_ev2_selfupdate_clone PRIVATE GS_CONFIG_DEFS_GITTEST_EV2_SELFUPDATE_VERSUB="versub_ev2_selfupdate_clone")
  
  # target include dirs
  
  FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS)
    TARGET_INCLUDE_DIRECTORIES("${VV}"
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${GITTEST_COMMON_PREFIX}/include    # for Gittest Common
        ${GITTEST_DEP_INCLUDE_DIRS}
    )
  ENDFOREACH()
  
  # target libraries
  
  # http://stackoverflow.com/questions/14468678/cmake-link-a-library-to-library/14480396#14480396
  #   linking static libraries to static libraries not needed? list them just for the executables.
  #   it is however used to 'inherit' TARGET_COMPILE_DEFINITIONS (PUBLIC and INTERFACE keywords).
  #   ie if libA defines -DENABLE_FOO linking libA into libB causes libB to also be compiled with -DENABLE_FOO.
  TARGET_LINK_LIBRARIES(gittest_lib gittest_common)
  TARGET_LINK_LIBRARIES(gittest_net gittest_common)
  TARGET_LINK_LIBRARIES(gittest_selfupdate gittest_common)
  
  FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS_EXE)
    TARGET_LINK_LIBRARIES("${VV}"
      gittest_selfupdate
      gittest_net
      gittest_lib
      gittest_common    # for Gittest Common
      ${GITTEST_DEP_LIBRARIES})
  ENDFOREACH()  
ENDFUNCTION ()


FUNCTION (GITTEST_SOURCES_INSTALL_TARGETS)
  GITTEST_PLATFORM_ENSURE_COMPILER()

  INSTALL(TARGETS ${GITTEST_SOURCES_TARGETS_EXE}
    LIBRARY DESTINATION "lib"
    RUNTIME DESTINATION "bin"
    ARCHIVE DESTINATION "lib"
  )
  
  # https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html
  #   $<TARGET_PDB_FILE:tgt>

  IF (GITTEST_COMPILER_MSVC)
      FOREACH(VV IN LISTS GITTEST_SOURCES_TARGETS_EXE)
        INSTALL(FILES
          "$<TARGET_PDB_FILE:${VV}>"
          DESTINATION "bin"
        )
      ENDFOREACH()  
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
    gittest_test_02
    gittest_ev2_test
    gittest_ev2_serv
    gittest_ev2_selfupdate
    gittest_ev2_selfupdate_clone
  )
  
  # gittest_lib

  SET(GITTEST_LIB_HEADERS
    include/gittest/gittest.h
    include/gittest/bypart_git.h
  )

  SET(GITTEST_LIB_SOURCES
    src/main.cpp
    src/bypart_git.cpp
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
    src/crank_clnt.cpp
    src/crank_serv.cpp
    src/crank_selfupdate_basic.cpp
    src/crank_test.cpp
    src/net3.cpp
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
  
  # gittest_clnt, gittest_clnt_clone
  
  GITTEST_AUX_SOURCES_SET_MAGIC_EMPTY(GITTEST_CLNT_HEADERS)
  GITTEST_AUX_SOURCES_SET_MAGIC_EMPTY(GITTEST_CLNT_CLONE_HEADERS)

  SET(GITTEST_CLNT_SOURCES
    src/gittest_clnt.cpp
  )
  SET(GITTEST_CLNT_CLONE_SOURCES
    src/gittest_clnt.cpp
  )
  
  # gittest_repo_setup
  
  GITTEST_AUX_SOURCES_SET_MAGIC_EMPTY(GITTEST_REPO_SETUP_HEADERS)

  SET(GITTEST_REPO_SETUP_SOURCES
    src/gittest_repo_setup.cpp
  )
  
  # gittest_test_02
  
  GITTEST_AUX_SOURCES_SET_MAGIC_EMPTY(GITTEST_TEST_02_HEADERS)
  
  SET(GITTEST_TEST_02_SOURCES
    src/net2_test.cpp
    src/net2_test_02.cpp
  )
  
  # gittest_ev2_test

  SET(GITTEST_EV2_TEST_HEADERS
    include/gittest/gittest_ev2_test.h
  )
  
  SET(GITTEST_EV2_TEST_SOURCES
    src/gittest_ev2_test.cpp
    src/gittest_ev2_test_c.cpp
    src/gittest_ev2_test_su.cpp
    src/gittest_ev2_test_s.cpp
    src/gittest_ev2_common.cpp
  )
  
  # gittest_ev2_serv
  
  SET(GITTEST_EV2_SERV_HEADERS
    include/gittest/gittest_ev2_test.h
  )
  
  SET(GITTEST_EV2_SERV_SOURCES
    src/gittest_ev2_serv.cpp
    src/gittest_ev2_test_s.cpp
    src/gittest_ev2_common.cpp
  )
  
  # gittest_ev2_selfupdate, gittest_ev2_selfupdate_clone
  
  SET(GITTEST_EV2_SELFUPDATE_HEADERS
    include/gittest/gittest_ev2_test.h
  )
  SET(GITTEST_EV2_SELFUPDATE_CLONE_HEADERS
    include/gittest/gittest_ev2_test.h
  )
  
  SET(GITTEST_EV2_SELFUPDATE_SOURCES
    src/gittest_ev2_selfupdate.cpp
    src/gittest_ev2_test_c.cpp
    src/gittest_ev2_test_su.cpp
    src/gittest_ev2_common.cpp
  )

  SET(GITTEST_EV2_SELFUPDATE_CLONE_SOURCES
    src/gittest_ev2_selfupdate.cpp
    src/gittest_ev2_test_c.cpp
    src/gittest_ev2_test_su.cpp
    src/gittest_ev2_common.cpp
  )
  
ENDMACRO ()
