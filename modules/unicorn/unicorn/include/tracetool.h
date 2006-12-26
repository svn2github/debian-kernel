#define PRINT_DETAIL PRINT_INFO
#define PRINTT_RMESSAGE PRINT_INFO
#define PRINTT_CMESSAGE PRINT_INFO

#ifdef DEBUG

#ifdef __cplusplus
extern "C" {
#endif

extern int PRINT_INFO(const char *format, ...);
extern int PRINT_WARNING(const char *format, ...);
extern int PRINT_ERROR(const char *format, ...);

#ifdef __cplusplus
}
#endif

#else

#define PRINT_INFO(fmt,arg...) do {} while (0)
#define	PRINT_WARNING(fmt,arg...) do {} while (0)	
#define	PRINT_ERROR(fmt,arg...) do {} while (0)

#endif
