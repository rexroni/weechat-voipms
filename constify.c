#include "stddef.h"
#include "constify.h"

const pj_str_t constify(const char* ptr, size_t len){
    return (pj_str_t){.ptr=ptr, .slen=len};
}
