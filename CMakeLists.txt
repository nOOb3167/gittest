# 3.4 for generator expressions ?
# 3.3+ for IF(.. IN_LIST ..)
CMAKE_MINIMUM_REQUIRED(VERSION 3.4)

PROJECT (gittest)

OPTION(GITTEST_DISABLE_CONFIG_GENERATION "Use an empty default config instead of generating one" OFF)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")

INCLUDE("GittestCommon")
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
  gittest_log_convert
  gittest_test_02
  gittest_ev2_test
)

GITTEST_COMMON_SET_PLATFORM()
GITTEST_COMMON_SET_PLATFORM_PLAT()
GITTEST_COMMON_SET_COMPILER()

GITTEST_COMMON_SET_DEP_VARS()
GITTEST_SOURCES_SET_PREREQ_MISC()
GITTEST_SOURCES_SET_GENERATION()
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
