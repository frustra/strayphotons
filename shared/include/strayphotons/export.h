
#ifndef SP_EXPORT_H
#define SP_EXPORT_H

#ifndef SP_EXPORT
    // #ifdef sp_EXPORTS
    #ifdef SP_SHARED_INTERNAL
        /* We are building this library */
        #ifdef _WIN32
            #define SP_EXPORT __declspec(dllexport)
        #else
            #define SP_EXPORT __attribute__((__visibility__("default")))
        #endif
    #else
        /* We are using this library */
        #ifdef _WIN32
            #define SP_EXPORT __declspec(dllimport)
        #else
            #define SP_EXPORT
        #endif
    #endif
#endif

#endif /* SP_EXPORT_H */
