#ifndef __LIB_KERNEL_FIXPOINT_H
#define __LIB_KERNEL_FIXPOINT_H

#define FP_Q 14
#define FP_P 17
typedef int32_t fp;
#define FQ_F (1 << FP_Q)

fp to_fp(int32_t n);
int32_t to_int(fp n);
fp add_fp(fp a, fp b);
fp sub_fp(fp a, fp b);
fp add_int(fp a, int32_t b);
fp sub_int(fp a, int32_t b);
fp mul_int(fp a, int32_t b);
fp div_int(fp a, int32_t b);
fp mul_fp(fp a, fp b);
fp div_fp(fp a, fp b);

// inline fp to_fp(int32_t n) {
//     return n * FQ_F;
// }

// /** round to nearest*/
// inline int32_t to_int(fp n) {    
//     return n >= 0 ? (n + (FQ_F) / 2) / FQ_F : (n - (FQ_F) / 2) / FQ_F; 
// }


// inline fp add_fp(fp a, fp b) {
//     return a + b;
// }

// inline fp sub_fp(fp a, fp b) {
//     return a - b;
// }

// inline fp add_int(fp a, int32_t b) {
//     return a + b * FQ_F;
// }

// inline fp sub_int(fp a, int32_t b) {
//     return a - b * FQ_F;
// }

// inline fp mul_int(fp a, int32_t b) {
//     return a * b;
// }

// inline fp div_int(fp a, int32_t b) {
//     return a / b;
// }

// inline fp mul_fp(fp a, fp b) {
//     return ((int64_t) a) * b / FQ_F;
// }

// inline fp div_fp(fp a, fp b) {
//     return ((int64_t) a) * FQ_F / b;
// }


#endif