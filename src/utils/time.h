#ifndef TIME_H
#define TIME_H

#include <stdbool.h>
#include <time.h>

bool clock_res_is_millis();
long long now_millis();
bool now_sec(time_t *t);


#endif /* TIME_H */
