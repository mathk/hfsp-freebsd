#include "hfsp.h"
#include "hfsp_btree.h"

#ifndef _HFSP_DEBUG_H_
#define _HFSP_DEBUG_H_

int hfsp_utf8_wctomb(char * sp, u_int16_t wc, int maxLen);
int hfsp_uni2asc(struct hfsp_unistr * ustrp, char * astrp, int len);
void udump(char * buff, int size);
void uprint_record(struct hfsp_record * rp);

#endif /* _HFSP_DEBUG_H_ */
