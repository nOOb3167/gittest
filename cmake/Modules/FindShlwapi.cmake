FIND_LIBRARY(SHLWAPI_LIBRARY NAMES Shlwapi)

SET(SHLWAPI_LIBRARIES ${SHLWAPI_LIBRARY})

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Shlwapi DEFAULT_MSG SHLWAPI_LIBRARY SHLWAPI_LIBRARIES)

MARK_AS_ADVANCED(SHLWAPI_LIBRARY SHLWAPI_LIBRARIES)
