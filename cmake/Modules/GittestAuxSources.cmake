FUNCTION (GITTEST_AUX_SOURCES_ADD_PLATFORM lump plat)
  # for lump=LIB , plat=WIN
  # append GITTEST_LIB_HEADERS_WIN (or SOURCES_WIN)
  # to     GITTEST_LIB_HEADERS     (or SOURCES)
  
  # helpers
  
  SET(HEADERSVAR GITTEST_${lump}_HEADERS)
  SET(SOURCESVAR GITTEST_${lump}_SOURCES)
  SET(HEADERSPLATVAR GITTEST_${lump}_HEADERS_${plat})
  SET(SOURCESPLATVAR GITTEST_${lump}_SOURCES_${plat})
  
  # append
  
  IF (DEFINED ${HEADERSVAR})
    LIST(APPEND ${HEADERSVAR} ${${HEADERSPLATVAR}})
  ENDIF ()
  
  IF (DEFINED SOURCESVAR)
    LIST(APPEND ${SOURCESVAR} ${${SOURCESPLATVAR}})
  ENDIF ()
  
  # output
  
  SET("${HEADERSVAR}" "${${HEADERSVAR}}" PARENT_SCOPE)
  SET("${SOURCESVAR}" "${${SOURCESVAR}}" PARENT_SCOPE)
ENDFUNCTION ()


FUNCTION (GITTEST_AUX_SOURCES_ENSURE_HEADERS_SOURCES_DEFINED lump)
  # for lump=LIB
  # check that GITTEST_LIB_HEADERS (and SOURCES) are defined

  IF ((NOT (DEFINED GITTEST_${lump}_HEADERS AND
           (DEFINED GITTEST_${lump}_SOURCES))))
    MESSAGE(FATAL_ERROR "GITTEST_[${lump}]_ HEADERS or SOURCES not defined?")
  ENDIF()
ENDFUNCTION ()


FUNCTION (GITTEST_AUX_SOURCES_SET_MAGIC_EMPTY varname)
  # invocation: FUNC (SOMELISTVARNAME)
  #   sets SOMELISTVARNAME to semicolon.
  #   semicolons are list separators in cmake.
  #   basically set SOMELISTVARNAME such that
  #   it counts as nonempty in value tests
  #   but will not interfere ex with
  #   SET(A b c d ${SOMELISTVARNAME})

  SET("${varname}" ";" PARENT_SCOPE)
ENDFUNCTION ()


FUNCTION (GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE)  # varargs
  # imagine building an additional executable is wanted.
  # it would have to be specified / referenced in multiple places,
  # which is error prone.
  # the idea is that somewhere early on GITTEST_SOURCES_MAGIC_TARGET_LIST
  # is initialized to the list of all wanted executables to build.
  # places that perform some work for every built executable,
  # (work such as target definitions using ADD_EXECUTABLE,
  #  TARGET_INCLUDE_DIRECTORIES compile definitions and similar)
  # call this function, passing every executable they currently
  # perform work on.
  # when a new wanted executable is added to GITTEST_SOURCES_MAGIC_TARGET_LIST,
  # but callsites of this function have not been edited, an error will be thrown.

  # early exit on missing required (global) definition
  
  IF (NOT (DEFINED GITTEST_SOURCES_MAGIC_TARGET_LIST))
    MESSAGE(FATAL_ERROR "GITTEST_SOURCES_MAGIC_TARGET_LIST definition missing?")
  ENDIF ()
  
  # compare the contents
  
  FOREACH (TMPTARGET IN LISTS ARGN) # varargs ARGN
    IF (NOT ("${TMPTARGET}" IN_LIST GITTEST_SOURCES_MAGIC_TARGET_LIST))
      MESSAGE(FATAL_ERROR "target list mismatch (elt)")
    ENDIF ()
  ENDFOREACH ()  
ENDFUNCTION ()
