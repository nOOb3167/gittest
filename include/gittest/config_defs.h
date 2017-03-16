/* These are meant to be set by the build system (CMake) */

#ifdef EXTERNAL_GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB
#  define GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB EXTERNAL_GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB
#endif /* EXTERNAL_GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB */

#define GS_CONFIG_DEFS_NONE 1
#define GS_CONFIG_DEFS_ASSERT 2
#define GS_CONFIG_DEFS_DEBUGBREAK 3

#if EXTERNAL_GS_CONFIG_DEFS_GLOBAL_CLEAN_HANDLING == GS_CONFIG_DEFS_NONE
#  define GS_CONFIG_DEFS_MISC_GS_GOTO_CLEAN_HANDLING {}
#elif EXTERNAL_GS_CONFIG_DEFS_GLOBAL_CLEAN_HANDLING == GS_CONFIG_DEFS_ASSERT
#  define GS_CONFIG_DEFS_MISC_GS_GOTO_CLEAN_HANDLING { GS_ASSERT(0); }
#elif EXTERNAL_GS_CONFIG_DEFS_GLOBAL_CLEAN_HANDLING == GS_CONFIG_DEFS_DEBUGBREAK
#  define GS_CONFIG_DEFS_MISC_GS_GOTO_CLEAN_HANDLING { DebugBreak(); }
#else
#  error EXTERNAL_GS_CONFIG_DEFS_GLOBAL_CLEAN_HANDLING definition broken
#endif /* EXTERNAL_GS_CONFIG_DEFS_GLOBAL_CLEAN_HANDLING */

#define GS_CONFIG_DEFS_WIN 32338437

#if EXTERNAL_GS_CONFIG_DEFS_GLOBAL_DEBUG_BREAK == GS_CONFIG_DEFS_WIN
#  define GS_CONFIG_DEFS_MISC_GS_DEBUG_BREAK
#else
#  error EXTERNAL_GS_CONFIG_DEFS_GLOBAL_DEBUG_BREAK definition broken
#endif /* EXTERNAL_GS_CONFIG_DEFS_GLOBAL_DEBUG_BREAK */

#ifdef EXTERNAL_GS_CONFIG_DEFS_GLOBAL_CONFIG_BUILTIN_HEXSTRING
#  define GS_CONFIG_DEFS_GLOBAL_CONFIG_BUILTIN_HEXSTRING EXTERNAL_GS_CONFIG_DEFS_GLOBAL_CONFIG_BUILTIN_HEXSTRING
#else
#  error EXTERNAL_GS_CONFIG_DEFS_GLOBAL_CONFIG_BUILTIN_HEXSTRING not defined
#endif /* EXTERNAL_GS_CONFIG_DEFS_GLOBAL_CONFIG_BUILTIN_HEXSTRING */
