#ifndef _GITTEST_LOG_DEFS_H_
#define _GITTEST_LOG_DEFS_H_

#define GS_LOG_LEVEL_CLEAN 0
#define GS_LOG_LEVEL_INFO 1000
#define GS_LOG_LEVEL_I GS_LOG_LEVEL_INFO
#define GS_LOG_LEVEL_WARNING 800
#define GS_LOG_LEVEL_W GS_LOG_LEVEL_WARNING
#define GS_LOG_LEVEL_ERROR 600
#define GS_LOG_LEVEL_E GS_LOG_LEVEL_ERROR

#define GS_LOG(LEVEL, TT, ...) { GS_LOG_TT_ ## TT (__FILE__, __LINE__, GS_LOG_LEVEL_ ## LEVEL, __VA_ARGS__); }

#define GS_LOG_TT_SZ gs_log_tls_SZ
#define GS_LOG_TT_S  gs_log_tls_S
#define GS_LOG_TT_PF  gs_log_tls_PF

void gs_log_tls_SZ(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf, uint32_t MsgSize);
void gs_log_tls_S(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf);
void gs_log_tls_PF(const char *CppFile, int CppLine, uint32_t Level, const char *Format, ...);

#endif /* _GITTEST_LOG_DEFS_H_ */
