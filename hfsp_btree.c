#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/systm.h>

#include "hfsp_btree.h"

MALLOC_DEFINE(M_HFSPBTREE, "hfsp_btree", "HFS+ B-Tree");

int hfsp_bread_inode(struct vnode * devvp, struct hfsp_inode * ip, u_int64_t fileOffset, int size, struct buf ** bpp)
{

}

int
hfsp_btree_open(struct mount * mp, struct hfsp_inode * ip, struct hfsp_btree ** btreepp)
{
    struct vnode * devvp;
    struct hfspmount * hmp;
    struct hfsp_btree * btreep;
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

    error = bread(devvp, ip>hi_fork.first_extents[0].startBlock, sizeof(*btreepp), NOCRED, &bp);

    return error;
}


