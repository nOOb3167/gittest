FUNCTION (GITTEST_AUX_SOURCES_APPEND_PLATFORM lump plat)
  # for lump=LIB , plat=WIN
  # append GITTEST_LIB_HEADERS_WIN (or SOURCES_WIN)
  # to     GITTEST_LIB_HEADERS     (or SOURCES)
  
  # helpers
  
  SET(varname_headers GITTEST_${lump}_HEADERS)
  SET(varname_sources GITTEST_${lump}_SOURCES)
  
  # append

  GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM(${lump} ${plat}
    ${varname_headers} ${varname_sources})

  # output
  
  SET("${varname_headers}" "${${varname_headers}}" PARENT_SCOPE)
  SET("${varname_sources}" "${${varname_sources}}" PARENT_SCOPE)
ENDFUNCTION ()


FUNCTION (GITTEST_AUX_SOURCES_APPEND_SEPARATE_PLATFORM lump plat
  output_varname_headers output_varname_sources
)
  # append

  LIST(APPEND ${output_varname_headers} ${GITTEST_${lump}_HEADERS_${plat}})
  LIST(APPEND ${output_varname_sources} ${GITTEST_${lump}_SOURCES_${plat}})
  
  # output
  
  SET(${output_varname_headers} "${${output_varname_headers}}" PARENT_SCOPE)
  SET(${output_varname_sources} "${${output_varname_sources}}" PARENT_SCOPE)  
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
  # imagine this setup:
  # around the top of main CMakeLists:
  #   SET(GITTEST_SOURCES_MAGIC_TARGET_LIST a b c)
  # some utility function:
  #   GITTEST_AUX_SOURCES_ENSURE_MAGIC_TARGET_LIST_HAVE(a b c)
  #   then performing target-specific work (ex TARGET_INCLUDE_DIRECTORIES).
  # if 'd' is added to the magic target list
  # but the utility function was not updated
  # an error will be thrown.
  # this define + check makes it easier to not forget
  # updating utility functions to account for newly added targets.

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
