#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>

#include "hfsp.h"

/*
 * Given an inode we read from the disk the specified size.
 * Read happen at physical block size granularity.
 *
 * This function is helper for internal file system usage.
 **/
int
hfsp_bread_inode(struct hfsp_inode * ip, u_int64_t fileOffset, int size, struct buf ** bpp)
{
    struct vnode *                  vp;
    struct hfsp_fork *              fork;
    struct hfsp_extent_descriptor * ep;
    int i, found, blkOffsetFile, blkCount, blk, blkFactor, sizeBread;

    vp = ip->hi_vp;
    sizeBread = (max(1, size / ip->hi_mount->hm_physBlockSize)) * ip->hi_mount->hm_physBlockSize;

    blkOffsetFile = fileOffset / ip->hi_mount->hm_blockSize;

    blkFactor = ip->hi_mount->hm_blockSize / ip->hi_mount->hm_physBlockSize;

    found = 0;
    fork = &ip->hi_fork;

    if (fileOffset + size > fork->size)
        return (EBADF);

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

    return bread(vp, blk * blkFactor, sizeBread, NOCRED, bpp);
}

