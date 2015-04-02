#include <sys/mount.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include "hfsp.h"

#ifndef _HFSP_BTREE_H_
#define _HFSP_BTREE_H_

MALLOC_DECLARE(M_HFSPBTREE);
MALLOC_DECLARE(M_HFSPNODE);

/* Btree held in memory */
struct hfsp_btree {
    struct hfsp_inode * hb_ip; /* The inode of the btree */

    u_int32_t           hb_rootNode;
    u_int16_t           hb_nodeSize;
    u_int16_t           hb_nodeShift;
    u_int16_t           hb_treeDepth;
    u_int32_t           hb_mapNode;
};

/* In memory node */
struct hfsp_node {
    struct buf *    hn_buffer;          /* Buffer containing the read data */
    u_int64_t       hn_offset;          /* Offset from the special file. */
    u_int32_t       hn_next;
    u_int32_t       hn_prev;
    u_int16_t       hn_numRecords;
    u_int16_t       hn_nodeSize;
    void *          hn_recordTable;     /* Jump table to recorde */
    __int8_t        hn_kind;
    u_int8_t        hn_height;
    bool            hn_inMemory;
};

int hfsp_btree_open(struct hfsp_inode * ip, struct hfsp_btree ** btreepp);
void hfsp_btree_close(struct hfsp_btree * btreep);
int hfsp_get_btnode(struct hfsp_btree * btreep, u_int32_t num);
#endif /* _HFSP_BTREE_H_ */
