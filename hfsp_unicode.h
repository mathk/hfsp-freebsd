#include <sys/param.h>

#include "hfsp.h"

#ifndef _HFSP_UNICODE_H_
#define _HFSP_UNICODE_H_

int hfsp_unicode_cmp(struct hfsp_unistr * lstrp, struct hfsp_unistr * rstrp);

/*
 * Fold  case folding of unicode char.
 * ch: The unicode char to fold.
 * return: The value folded.
 */
u_int16_t hfsp_foldcase(u_int16_t ch);

/*
 * Copy a hfsp_unistr from one to an other.
 * This function assumed that the unistr are allocated.
 * srcp: Source hfsp_unistr from where to copy.
 * dstp: Destination hfsp_unistr to copy to.
 */
void hfsp_unicode_copy(struct hfsp_unistr * srcp, struct hfsp_unistr * dstp);

#endif /* _HFSP_UNICODE_H_ */
