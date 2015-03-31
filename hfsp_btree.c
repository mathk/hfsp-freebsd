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

int
hfsp_bread_inode(struct vnode * devvp, struct hfsp_inode * ip, u_int64_t fileOffset, int size, struct buf ** bpp)
{
    struct hfsp_fork * fork;
    struct hfsp_extent_descriptor * ep;
    int i, found, blkOffsetFile, blkCount, blk, blkFactor, sizeBread;

    sizeBread = (max(1, size / ip->hi_mount->hm_physBlockSize)) * ip->hi_mount->hm_physBlockSize;

    blkOffsetFile = fileOffset / ip->hi_mount->hm_blockSize;

    blkFactor = ip->hi_mount->hm_blockSize / ip->hi_mount->hm_physBlockSize;

    found = 0;
    fork = &ip->hi_fork;

    if (fileOffset + size > fork->size)
        return (EBADF);

    uprintf("Read from extent:\n start block[0]: %d\n size[0]: %d\n", fork->first_extents[0].startBlock, fork->first_extents[0].blockCount);
    /* First try to find in the first extent */
    for (blkCount = 0, i = 0; i < HFSP_FIRSTEXTENT_SIZE; i++)
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

    uprintf("Read block: %d \nLogical block size: %d\nSize read %d\n", blk * blkFactor, ip->hi_mount->hm_blockSize, sizeBread);
    //return ENOMEM;

    return bread(devvp, blk * blkFactor, sizeBread, NOCRED, bpp);
}

int
hfsp_btree_open(struct hfspmount * hmp, struct hfsp_inode * ip, struct hfsp_btree ** btreepp)
{
    struct vnode * devvp;
    struct hfsp_btree * btreep;
    struct BTNodeDescriptor * btreeRaw;
    struct BTHeaderRec * btHeaderRaw;
    struct buf * bp;
    int error;

    devvp = hmp->hm_devvp;

    btreep = malloc(sizeof(*btreep), M_HFSPBTREE, M_WAITOK | M_ZERO);
    if (btreep == NULL)
    {
        *btreepp = NULL;
        return ENOMEM;
    }

    error = hfsp_bread_inode(devvp, ip, 0, sizeof(*btreeRaw) + sizeof(*btHeaderRaw), &bp);
    if (error)
    {
        return error;
    }

    btreeRaw = (struct BTNodeDescriptor*)bp->b_data;
    btHeaderRaw = (struct BTHeaderRec*)(bp->b_data + sizeof(*btreeRaw));

    uprintf("open: Buffer size %ld, Buffer offset %ld, First int %d\n", bp->b_bufsize, bp->b_offset, ((int*)bp->b_data)[2]);

    btreep->hb_nodeSize = be16toh(btHeaderRaw->nodeSize);
    btreep->hb_rootNode = be32toh(btHeaderRaw->rootNode);
    btreep->hb_ip = ip;

    brelse(bp);

    *btreepp = btreep;

    return error;
}

int
hfsp_btree_close(struct hfsp_btree * btreep)
{
    free(btreep, M_HFSPBTREE);
    return 0;
}
