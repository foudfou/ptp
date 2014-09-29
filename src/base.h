#ifndef BASE_H
#define BASE_H

#include <errno.h>
#include <stddef.h>

#define FREE(p) do{                             \
        free(p);                                \
        p = NULL;                               \
    } while(0)

#define FCLOSE(fid) do {                        \
        fclose(fid);                            \
        fid = NULL;                             \
    } while(0)

#endif /* BASE_H */
