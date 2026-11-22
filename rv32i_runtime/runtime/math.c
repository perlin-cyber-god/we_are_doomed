#include "runtime.h"
#include <limits.h>
/* Checked Q16.16 helpers. These are not drop-in copies of Doom's overflow
 * policy: invalid/overflow returns false, leaving *out unchanged. */
bool rt_fixed_mul(int32_t a,int32_t b,int32_t *out)
{
    const int64_t product=(int64_t)a*(int64_t)b;
    int64_t q=product/65536;
    /* Match arithmetic-right-shift rounding for negative fractional products,
     * without relying on a host/compiler's signed right shift behaviour. */
    if (product<0 && ((uint64_t)product & 65535u)!=0u) --q;
    if (q<INT32_MIN || q>INT32_MAX) return false;
    *out=(int32_t)q; return true;
}
bool rt_fixed_div(int32_t a,int32_t b,int32_t *out)
{
    if (b==0) return false;
    const int64_t q=((int64_t)a*65536)/b;
    if (q<INT32_MIN || q>INT32_MAX) return false;
    *out=(int32_t)q; return true;
}
