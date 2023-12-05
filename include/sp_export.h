
#ifndef SP_EXPORT_H
#define SP_EXPORT_H

#ifdef SP_SHARED_BUILD
    #ifndef SP_EXPORT
        #ifdef sp_EXPORTS
            /* We are building this library */
            #define SP_EXPORT __declspec(dllexport)
        #else
            /* We are using this library */
            #define SP_EXPORT __declspec(dllimport)
        #endif
    #endif
#else
    #define SP_EXPORT
#endif

#endif /* SP_EXPORT_H */
