#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/systm.h>

#include "hfsp_btree.h"

MALLOC_DEFINE(M_HFSPBTREE, "hfsp_btree", "HFS+ B-Tree");

int
hfsp_bread_inode(struct vnode * devvp, struct hfsp_inode * ip, u_int64_t fileOffset, int size, struct buf ** bpp)
{
    struct hfsp_fork * fork;
    struct hfsp_extent_descriptor * ep;
    int i, found, blkOffsetFile, blkCount, blk;

    blkOffsetFile = fileOffset / ip->hi_mount->hm_blockSize;

    blockCount = 0L;
    found = 0;
    fork = &ip->hi_fork;

    if (fileOffset + size > fork->size)
        return (EBADF);

    /* First try to find in the first extent */
    for (i = 0; i < HFSP_FIRSTEXTENT_SIZE; i++)
    {
        ep = fork->first_extents + i;
        if (blkCount + ep->blockCount > blkOffsetFile)
        {
            found = 1;
            blk = ep->startBlock + (blkOffsetFile - blkCount);
            break;
        }
        blkCount += ep->blockCount;
    }

    /* Todo suport search in file extent */
    if (!found)
    {
        return EINVAL;
    }

    return bread(devvp, blk, size, NOCRED, bpp);
}

int
hfsp_btree_open(struct mount * mp, struct hfsp_inode * ip, struct hfsp_btree ** btreepp)
{
    struct vnode * devvp;
    struct hfspmount * hmp;
    struct hfsp_btree * btreep;
    struct BTNodeDescriptor * btreeRaw;
    struct BTHeaderRec * btHeaderRaw;
    struct buf * bp;
    int error;

    hmp = VFSTOHFSPMNT(mp);
    devvp = hmp->hm_devvp;

    btree = malloc(sizeof(*btree), M_HFSPBTREE, M_WAITOK | M_ZERO);
    if (btree == NULL)
    {
        *btreepp = NULL;
        return ENOMEM;
    }

    error = hfsp_bread_inode(devvp, ip, 0i, sizeof(*btreeRaw) + sizeof(*btHeaderRaw), &bp);

    btreeRaw = (struct BTHeaderRec*)bp->b_data;
    btHeaderRaw = (struct BTHeaderRec*)(bp->b_data + sizeof(*btreeRaw));

    return error;
}


