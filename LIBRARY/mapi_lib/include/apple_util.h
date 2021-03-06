#ifndef _H_APPLE_UTIL_
#define _H_APPLE_UTIL_
#include "binhex.h"
#include "macbinary.h"
#include "applefile.h"

#ifdef __cplusplus
extern "C" {
#endif

BINARY* apple_util_binhex_to_appledouble(const BINHEX *pbinhex);

BINARY* apple_util_macbinary_to_appledouble(const MACBINARY *pmacbin);

BINARY* apple_util_appledouble_to_macbinary(const APPLEFILE *papplefile,
	const void *pdata, uint32_t data_len);

BINARY* apple_util_applesingle_to_macbinary(const APPLEFILE *papplefile);

BINARY* apple_util_binhex_to_macbinary(const BINHEX *pbinhex);

BINARY* apple_util_applesingle_to_appledouble(const APPLEFILE *papplefile);


#ifdef __cplusplus
}
#endif

#endif /* _H_APPLE_UTIL_ */
