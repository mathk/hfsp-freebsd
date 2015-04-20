#include <sys/param.h>

#include "hfsp.h"

#ifndef _HFSP_UNICODE_H_
#define _HFSP_UNICODE_H_

int hfsp_unicode_cmp(struct hfsp_unistr * lstrp, struct hfsp_unistr * rstrp);
u_int16_t hfsp_foldcase(u_int16_t ch);

#endif /* _HFSP_UNICODE_H_ */
