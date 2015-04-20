#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/endian.h>

#include "hfsp_btree.h"
#include "hfsp_unicode.h"

MALLOC_DEFINE(M_HFSPBTREE, "hfsp_btree", "HFS+ B-Tree");
MALLOC_DEFINE(M_HFSPNODE, "hfsp_node", "HFS+ B-Tree node");
MALLOC_DEFINE(M_HFSPREC, "hfsp_record", "HFS+ B-Tree record");

typedef int (*record_read_t)(struct hfsp_record * recp);

static record_read_t brec_read_op[RECORD_TYPE_COUNT];

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

    uprintf("hfsp_btree_open: Buffer size %ld, Buffer offset %ld, Number of records in header node: %d\n", 
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
    uprintf("hfsp_get_btnode: Bcount %ld, Bufsize %ld, Bresid %ld\n", bp->b_bcount, bp->b_bufsize, bp->b_resid);

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
    switch (np->hn_kind)
    {
        case HFSP_NODE_INDEX:
            np->hn_read = hfsp_brec_catalogue_index_read;
            break;
        case HFSP_NODE_LEAF:
            np->hn_read = hfsp_brec_catalogue_read;
            break;
        default:
            np->hn_read = hfsp_brec_noops;
    }

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
hfsp_btree_find(struct hfsp_btree * btreep, struct hfsp_record_key * kp, struct hfsp_record ** recpp)
{
    int error, level;
    struct hfsp_node * np;
    hfsp_cnid currentCnid;
    struct hfsp_record * recp;

    level = btreep->hb_treeDepth;

    // We start from the root node.
    currentCnid = btreep->hb_rootNode;
    while (1)
    {
        error = hfsp_get_btnode(btreep, currentCnid, &np);
        if (error)
        {
            uprintf("hfsp_btree_find: Getting error reading btnode.");
            return error;
        }

        if (level == 1 && np->hn_kind == HFSP_NODE_LEAF)
        {
            hfsp_brec_find(np, kp, recpp);
            error = 0;
            break;
        }
        else if (level > 1 && np->hn_kind == HFSP_NODE_INDEX)
        {
            hfsp_brec_find(np, kp, recpp);
            recp = *recpp;
            currentCnid = recp->hr_index;
        }
        else
        {
            uprintf("hfap_btree_find: Invalid node type, level: %d,%d\n", np->hn_kind, level);
            error = EINVAL;
            break;
        }


        hfsp_release_btnode(np);
        level--;
    }

    hfsp_release_btnode(np);
    return error;
}

#define PBE16TOH(x) be16toh(*(u_int16_t*)(x))
#define PBE32TOH(x) be32toh(*(u_int32_t*)(x))

int
hfsp_brec_key_cmp(struct hfsp_record_key * lkp, struct hfsp_record_key * rkp)
{
    if (lkp->hk_cnid < rkp->hk_cnid)
        return -1;
    if (lkp->hk_cnid > rkp->hk_cnid)
        return 1;

    return hfsp_unicode_cmp(&lkp->hk_name, &rkp->hk_name);

}

u_int16_t
hfsp_brec_read_u16(struct hfsp_record * rp, u_int16_t offset)
{
    return PBE16TOH(rp->hr_node->hn_beginBuf + rp->hr_offset + offset);
}


u_int32_t
hfsp_brec_read_u32(struct hfsp_record * rp, u_int16_t offset)
{
    return PBE32TOH(rp->hr_node->hn_beginBuf + rp->hr_offset + offset);
}

void
hfsp_brec_bcopy(struct hfsp_record * rp, u_int16_t offset, void * buf, size_t len)
{
    bcopy(rp->hr_node->hn_beginBuf + rp->hr_offset + offset, buf, len);
}

int
hfsp_brec_read_unistr(struct hfsp_record * rp, u_int16_t offset, struct hfsp_unistr * strp)
{
    strp->hu_len = hfsp_brec_read_u16(rp, offset);
    if (strp->hu_len * 2 > sizeof(strp->hu_str) - 2)
    {
        return EINVAL;
    }

    hfsp_brec_bcopy(rp, offset + sizeof(strp->hu_len), strp->hu_str, strp->hu_len * 2);
    return 0;
}

void 
hfsp_brec_catalogue_read_init()
{
    brec_read_op[HFSP_FOLDER_RECORD - 1] = hfsp_brec_catalogue_read_folder;
    brec_read_op[HFSP_FILE_RECORD - 1] = hfsp_brec_catalogue_read_file;
    brec_read_op[HFSP_FOLDER_THREAD_RECORD - 1] = hfsp_brec_catalogue_read_thread;
    brec_read_op[HFSP_FILE_THREAD_RECORD - 1] = hfsp_brec_catalogue_read_thread;
}

int
hfsp_brec_catalogue_read_key(struct hfsp_record * rp, struct hfsp_record_key * rkp)
{

    rkp->hk_len = hfsp_brec_read_u16(rp, 0);
    rkp->hk_cnid = hfsp_brec_read_u16(rp, sizeof(rkp->hk_len));
    return hfsp_brec_read_unistr(rp, sizeof(rkp->hk_len) + sizeof(rkp->hk_cnid), &rkp->hk_name);
}

int
hfsp_brec_catalogue_lookup_read(struct hfsp_node * np, int recidx, struct hfsp_record ** recpp)
{
    struct hfsp_record * recp;
    int error;

    if (*recpp != NULL)
        recp = *recpp;
    else
    {
        recp = hfsp_brec_alloc();
        if (!recp)
        {
            return ENOMEM;
        }
    }

    recp->hr_node = np;
    recp->hr_offset = be16toh(*(np->hn_recordTable - (1 + recidx)));

    error = hfsp_brec_catalogue_read_key(recp, &recp->hr_key);
    if (error)
        return error;

    recp->hr_dataOffset = recp->hr_key.hk_len + sizeof(recp->hr_key.hk_len);

    *recpp = recp;
    return 0;
}

int
hfsp_brec_find(struct hfsp_node * np, struct hfsp_record_key * kp, struct hfsp_record ** recpp)
{
    int begin, end, rec, res, error;
    struct hfsp_record_key * curKeyp;

    begin = 0;
    end = np->hn_numRecords - 1;

    do {
        rec = (begin + end) >> 1;
        error = hfsp_brec_catalogue_lookup_read(np, rec, recpp);
        if (error)
            return error;

        curKeyp = &(*recpp)->hr_key;

        res = hfsp_brec_key_cmp(curKeyp, kp);
        if (res == 0)
        {
            np->hn_read(np, rec, recpp);
            goto done;
        }
        if (res < 0)
            begin = rec + 1;
        else
            end = rec - 1;


    } while (begin <= end);

    if (rec != end && end >= 0)
    {
        np->hn_read(np, end, recpp);
    }
    else
    {
        np->hn_read(np, rec, recpp);
    }
done:
    return 0;
}

int
hfsp_brec_noops(struct hfsp_node * np, int recidx, struct hfsp_record ** recpp)
{
    // The least we can do is to read the key.
    return hfsp_brec_catalogue_lookup_read(np, recidx, recpp);
}

int
hfsp_brec_catalogue_index_read(struct hfsp_node * np, int recidx, struct hfsp_record ** recpp)
{
    int error;
    struct hfsp_record * recp;

    error = hfsp_brec_catalogue_lookup_read(np, recidx, recpp);
    if (error)
        return error;
    recp = *recpp;

    recp->hr_index = hfsp_brec_read_u16(recp, recp->hr_dataOffset);
    return 0;
}

int
hfsp_brec_catalogue_read(struct hfsp_node * np, int recidx, struct hfsp_record ** recpp)
{
    int error;
    struct hfsp_record * recp;
    error = hfsp_brec_catalogue_lookup_read(np, recidx, recpp);
    if (error)
        return error;

    recp = *recpp;

    recp->hr_type = hfsp_brec_read_u16(recp, recp->hr_dataOffset);
    if (recp->hr_type  <= 0 || recp->hr_type > RECORD_TYPE_COUNT)
    {
        return EINVAL;
    }

    error = (brec_read_op[recp->hr_type - 1])(recp);
    if (error)
        return error;

    *recpp = recp;
    return 0;
}

int
hfsp_brec_catalogue_read_thread(struct hfsp_record * recp)
{
    int curOffset;

    curOffset = recp->hr_dataOffset + (2 * sizeof(recp->hr_type));
    recp->hr_thread.hrt_parentCnid = hfsp_brec_read_u32(recp, curOffset);

    curOffset = curOffset + sizeof(recp->hr_thread.hrt_parentCnid);
    return hfsp_brec_read_unistr(recp, curOffset, &recp->hr_thread.hrt_name);
}

int
hfsp_brec_catalogue_read_folder(struct hfsp_record * recp)
{
    int curOffset;

    curOffset = recp->hr_dataOffset + sizeof(recp->hr_type);
    recp->hr_folder.hrfo_flags = hfsp_brec_read_u16(recp, curOffset);
    curOffset += sizeof(recp->hr_folder.hrfo_flags);
    recp->hr_folder.hrfo_valance = hfsp_brec_read_u32(recp, curOffset);
    curOffset += sizeof(recp->hr_folder.hrfo_valance);
    recp->hr_folder.hrfo_folderCnid = hfsp_brec_read_u32(recp, curOffset);
    curOffset += sizeof(recp->hr_folder.hrfo_folderCnid);
    recp->hr_folder.hrfo_createDate = hfsp_brec_read_u32(recp, curOffset);
    return 0;
}

int
hfsp_brec_catalogue_read_file(struct hfsp_record * recp)
{
    return 0;
}

struct hfsp_record *
hfsp_brec_alloc()
{
    return malloc(sizeof(struct hfsp_record), M_HFSPREC, M_WAITOK | M_ZERO);
}

void
hfsp_brec_release_record(struct hfsp_record ** rpp)
{
    free(*rpp, M_HFSPREC);
    *rpp = NULL;
}
