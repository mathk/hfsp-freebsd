#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/endian.h>

#include "hfsp_btree.h"

MALLOC_DEFINE(M_HFSPBTREE, "hfsp_btree", "HFS+ B-Tree");
MALLOC_DEFINE(M_HFSPNODE, "hfsp_node", "HFS+ B-Tree node");

int
hfsp_btree_open(struct hfsp_inode * ip, struct hfsp_btree ** btreepp)
{
    struct vnode * vp;
    struct hfsp_btree * btreep;
    struct BTNodeDescriptor * btreeRaw;
    struct BTHeaderRec * btHeaderRaw;
    struct buf * bp;
    int error;

    vp = ip->hi_vp;

    btreep = malloc(sizeof(*btreep), M_HFSPBTREE, M_WAITOK | M_ZERO);
    if (btreep == NULL)
    {
        *btreepp = NULL;
        return ENOMEM;
    }

    error = hfsp_bread_inode(ip, 0, sizeof(*btreeRaw) + sizeof(*btHeaderRaw), &bp);
    if (error)
    {
        return error;
    }

    btreeRaw = (struct BTNodeDescriptor*)bp->b_data;
    btHeaderRaw = (struct BTHeaderRec*)(bp->b_data + sizeof(*btreeRaw));

    uprintf("open: Buffer size %ld, Buffer offset %ld\nNumber of records in header node: %d\n", 
            bp->b_bufsize, bp->b_offset, be16toh(btreeRaw->numRecords));

    btreep->hb_mapNode  = be32toh(btreeRaw->fLink);
    btreep->hb_nodeSize = be16toh(btHeaderRaw->nodeSize);
    btreep->hb_rootNode = be32toh(btHeaderRaw->rootNode);
    btreep->hb_treeDepth = be16toh(btHeaderRaw->treeDepth);
    btreep->hb_firstLeafNode = be32toh(btHeaderRaw->firstLeafNode);
    btreep->hb_ip = ip;
    btreep->hb_nodeShift = ffs(btreep->hb_nodeSize) - 1;

    brelse(bp);

    *btreepp = btreep;

    return error;
}

int
hfsp_get_btnode(struct hfsp_btree * btreep, u_int32_t num, struct hfsp_node ** npp)
{
    struct buf * bp;
    struct hfsp_node * np;
    struct BTNodeDescriptor * ndp;
    u_int64_t  blockOffset;
    int error;

    blockOffset = (u_int64_t)num << btreep->hb_nodeShift;
    error = hfsp_bread_inode(btreep->hb_ip, blockOffset, btreep->hb_nodeSize, &bp);
    if (error)
    {
        return error;
    }
    uprintf("Bcount %ld, Bufsize %ld, Bresid %ld\n", bp->b_bcount, bp->b_bufsize, bp->b_resid);

    np = malloc(sizeof(*np), M_HFSPNODE, M_WAITOK | M_ZERO);
    if (np == NULL)
        return ENOMEM;

    ndp = (struct BTNodeDescriptor*)bp->b_data;

    np->hn_buffer = bp;
    np->hn_kind = ndp->kind;
    np->hn_height = ndp->height;
    np->hn_nodeSize = btreep->hb_nodeSize;
    np->hn_numRecords = be16toh(ndp->numRecords);
    np->hn_prev = be32toh(ndp->bLink);
    np->hn_next = be32toh(ndp->fLink);
    np->hn_offset = blockOffset;
    np->hn_beginBuf = (u_int8_t *)ndp;
    np->hn_recordTable = (u_int16_t*)(np->hn_beginBuf + np->hn_nodeSize);
    np->hn_inMemory = true;

    *npp = np;

    return 0;
}

void
hfsp_release_btnode(struct hfsp_node * np)
{
    if (np->hn_inMemory)
    {
        brelse(np->hn_buffer);
        np->hn_inMemory = false;
    }

    free(np, M_HFSPNODE);
}

void
hfsp_btree_close(struct hfsp_btree * btreep)
{
    if (btreep == NULL)
        return;

    hfsp_irelease(btreep->hb_ip);
    free(btreep, M_HFSPBTREE);
}

int
hfsp_init_find_info(struct hfsp_find_info * fip)
{
    fip->hf_search_keyp = uma_zalloc(uma_record_key, M_WAITOK | M_ZERO);
    if (fip->hf_search_keyp == NULL)
    {
        return ENOMEM;
    }

    fip->hf_current_keyp = uma_zalloc(uma_record_key, M_WAITOK | M_ZERO);
    if (fip->hf_current_keyp == NULL)
    {
        uma_zfree(uma_record_key, fip->hf_search_keyp);
        return ENOMEM;
    }

    return 0;
}

void
hfsp_destroy_find_info(struct hfsp_find_info * fip)
{
    uma_zfree(uma_record_key, fip->hf_search_keyp);
    uma_zfree(uma_record_key, fip->hf_current_keyp);
}

#define PBE16TOH(x) be16toh(*(u_int16_t*)(x))
#define PBE32TOH(x) be32toh(*(u_int32_t*)(x))

int
hfsp_bnode_read_catalogue_key(struct hfsp_node * np, int recidx, struct hfsp_record_key * rkp)
{
    u_int16_t off;
    //int i;
    if (!np->hn_inMemory)
    {
        return EINVAL;
    }

    off = be16toh(*(np->hn_recordTable - (1 + recidx)));

    rkp->hk_len = PBE16TOH(np->hn_beginBuf + off);
    rkp->hk_cnid = PBE32TOH(np->hn_beginBuf + off + sizeof(rkp->hk_len));
    rkp->hk_name.hu_len = PBE16TOH(np->hn_beginBuf + off + sizeof(rkp->hk_len) + sizeof(rkp->hk_cnid));
    bcopy(np->hn_beginBuf + off + sizeof(rkp->hk_len) + sizeof(rkp->hk_cnid) + sizeof(rkp->hk_name.hu_len), 
          rkp->hk_name.hu_str,
          max(sizeof(rkp->hk_name.hu_str), rkp->hk_name.hu_len * 2));

    return 0;
}
