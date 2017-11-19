#define TRACE_MODULE
#ifdef TRACE_MODULE
    #define TRACE( expr ) (expr)
#else
    #define TRACE( expr ) ((void)0)
#endif