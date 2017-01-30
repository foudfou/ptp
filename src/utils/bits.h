#ifndef BITS_H
#define BITS_H

#define BITS_SET(n, x) ((n) |=  (x))
#define BITS_CLR(n, x) ((n) &= ~(x))
#define BITS_TGL(n, x) ((n) ^=  (x))
#define BITS_CHK(n, x) ((n) &   (x))

#endif /* BITS_H */
