# 3.4 for generator expressions ?
# 3.3+ for IF(.. IN_LIST ..)
CMAKE_MINIMUM_REQUIRED(VERSION 3.4)

PROJECT (gittest)

SET(GITTEST_COMMON_PREFIX ${CMAKE_SOURCE_DIR}/../gittest_common CACHE STRING ".")
SET(GITTEST_COMMON_CONFIG_GENERATION_FILEPATH ${CMAKE_SOURCE_DIR}/data/GsConfig.conf CACHE STRING ".")

LIST(APPEND CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/cmake/Modules
  ${GITTEST_COMMON_PREFIX}/cmake/Modules
)

INCLUDE(GittestCommon)

GITTEST_COMMON_SET_GENERATION()
GITTEST_COMMON_SET_LIB()

INCLUDE("GittestPlatform")
INCLUDE("GittestDeps")
INCLUDE("GittestAuxSources")
INCLUDE("GittestSources")

SET(GITTEST_SOURCES_MAGIC_TARGET_LIST
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

GITTEST_PLATFORM_SET_PLATFORM()
GITTEST_PLATFORM_SET_PLATFORM_PLAT()
GITTEST_PLATFORM_SET_COMPILER()
GITTEST_PLATFORM_SET_DEP_VARS()

GITTEST_SOURCES_SET_PREREQ_MISC()
GITTEST_SOURCES_SET()
GITTEST_SOURCES_CREATE_TARGETS()
GITTEST_SOURCES_CONFIGURE_TARGETS()
GITTEST_SOURCES_DEFINE_COSMETIC_VISUAL_STUDIO_STUFF()

GITTEST_SOURCES_INSTALL_TARGETS()

ADD_CUSTOM_TARGET(dummyprep
  COMMAND
    gittest_repo_setup
      --gsreposetup
      --xdummyprep
)
