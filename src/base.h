#ifndef BASE_H
#define BASE_H

#include <errno.h>
#include <stdint.h>
#include <stddef.h>

#define FREE(p) do{                             \
        free(p);                                \
        p = NULL;                               \
    } while(0)

#define FCLOSE(fid) do {                        \
        fclose(fid);                            \
        fid = NULL;                             \
    } while(0)

/**
 * Prevent compilation if assertion is false or not a compile time constant.
 * Thanks to http://www.jaggersoft.com/pubs/CVu11_3.html
 */
#define assert_compilation(isTrue) \
    void assert_compilation(char x[1 - (!(isTrue))])

#endif /* BASE_H */
