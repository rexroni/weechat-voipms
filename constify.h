#ifndef CONSTIFY_H
#define CONSTIFY_H

#include <pjlib.h>

/* this needs to be compiled with -Wno-discarded-qualifiers, due to the age-old
   issue of "const" not propagating into strut members */
const pj_str_t constify(const char* ptr, size_t len);

#endif // CONSTIFY_H
