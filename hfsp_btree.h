#include <sys/mount.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include "hfsp.h"

#ifndef _HFSP_BTREE_H_
#define _HFSP_BTREE_H_

MALLOC_DECLARE(M_HFSPBTREE);

/* Btree held in memory */
struct hfsp_btree {
    struct hfsp_inode * hb_ip; /* The inode of the btree */

    u_int32_t           hb_rootNode;
    u_int16_t           hb_nodeSize;
};


int hfsp_btree_open(struct hfspmount * hmp, struct hfsp_inode * ip, struct hfsp_btree ** btreepp);
int hfsp_bread_inode(struct vnode * devvp, struct hfsp_inode * ip, u_int64_t fileOffset, int size, struct buf ** bpp);
int hfsp_btree_close(struct hfsp_btree * btreep);
#endif /* _HFSP_BTREE_H_ */
