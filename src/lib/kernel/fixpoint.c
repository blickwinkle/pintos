#include <inttypes.h>
#include "fixpoint.h"

inline fp to_fp(int32_t n) {
    return n * FQ_F;
}

/** round to nearest*/
inline int32_t to_int(fp n) {    
    return n >= 0 ? (n + (FQ_F) / 2) / FQ_F : (n - (FQ_F) / 2) / FQ_F; 
}


inline fp add_fp(fp a, fp b) {
    return a + b;
}

inline fp sub_fp(fp a, fp b) {
    return a - b;
}

inline fp add_int(fp a, int32_t b) {
    return a + b * FQ_F;
}

inline fp sub_int(fp a, int32_t b) {
    return a - b * FQ_F;
}

inline fp mul_int(fp a, int32_t b) {
    return a * b;
}

inline fp div_int(fp a, int32_t b) {
    return a / b;
}

inline fp mul_fp(fp a, fp b) {
    return ((int64_t) a) * b / FQ_F;
}

inline fp div_fp(fp a, fp b) {
    return ((int64_t) a) * FQ_F / b;
}




