#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/module.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kernel.h> /* types used in module initialization */
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/endian.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include "hfsp.h"

MALLOC_DEFINE(M_HFSPMNT, "hfsp_mount", "HFS Plus mount structure");

static uma_zone_t       uma_inode;

static vfs_mount_t      hfsp_mount;
static vfs_unmount_t    hfsp_unmount;
static vfs_statfs_t     hfsp_statfs;

int hfsp_vget_fork(struct mount * mp, struct HFSPlusForkData * fork, struct vnode ** vpp);
int hfsp_mount_volume(struct vnode * devvp, struct hfspmount * hmp, struct HFSPlusVolumeHeader * hfsph);

static struct vfsops hfsp_vfsops = {
//    .vfs_fhtovp =   NULL,
    .vfs_mount =    hfsp_mount,
//    .vfs_root = NULL,
    .vfs_statfs =   hfsp_statfs,
//    .vfs_sync =     NULL,
    .vfs_unmount =  hfsp_unmount,
//    .vfs_vget =     NULL
};
VFS_SET(hfsp_vfsops, hfsp, 0);

static int
hfsp_mount(struct mount *mp)
{
    struct vfsoptlist * opts;
    struct thread *td;
    struct vnode *devvp, *vnodecovered;
    struct nameidata nd, *ndp = &nd;
    struct vfsopt *opt;
    char * fromPath;
    int error, len;
    struct buf *bp = NULL;
    struct g_consumer *cp = NULL;
    struct HFSPlusVolumeHeader *hfsph;
    struct hfspmount *hmp;

    td = curthread;
    opts = mp->mnt_optnew;
    vnodecovered = mp->mnt_vnodecovered;

    if (uma_inode == NULL)
    {
        uma_inode = uma_zcreate("HFS+ inode", sizeof(struct hfsp_inode), NULL, NULL, NULL, NULL,
                                UMA_ALIGN_PTR, 0);
    }

    vfs_getopt(opts, "from", (void **)&fromPath, &len);

    uprintf("Mounting device\n");
    if (vnodecovered != NULL)
    {
        uprintf("Vnodecovered type: %s\n", vnodecovered->v_tag);
    }

    TAILQ_FOREACH(opt, opts, link) {
        if (opt->len != 0)
            uprintf("Option %s value: %s\n", opt->name, opt->value);
        else
            uprintf("Empty option %s\n", opt->name);
    }

    NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, fromPath, td);
    if ((error = namei(ndp)) != 0)
        return error;

    NDFREE(ndp, NDF_ONLY_PNBUF);
    devvp = ndp->ni_vp;

    if (!vn_isdisk(devvp, &error)) {
        vput(devvp);
        return error;
    }
    else
    {
        uprintf("Disk device detected\n");
    }

    DROP_GIANT();
    g_topology_lock();
    error = g_vfs_open(devvp, &cp, "hfsp",  0);
    g_topology_unlock();
    PICKUP_GIANT();
    VOP_UNLOCK(devvp, 0);

    if (error) {
        vrele(devvp);
        return error;
    }

    uprintf ("Sector size of %s: %d\nProvider name: %s\nMedia size: %ld\n", fromPath, cp->provider->sectorsize, cp->provider->name, cp->provider->mediasize);

    if ((error = bread(devvp, 2, 512, NOCRED, &bp)) != 0)
        goto out;

    hfsph = (struct HFSPlusVolumeHeader *)bp->b_data;
    hmp = malloc(sizeof(*hmp), M_HFSPMNT, M_WAITOK | M_ZERO);

    hmp->hm_blockSize = be32toh(hfsph->blockSize);
    hmp->hm_totalBlocks = be32toh(hfsph->totalBlocks);
    hmp->hm_freeBlocks = be32toh(hfsph->freeBlocks);

    brelse(bp);

    mp->mnt_data = hmp;
    mp->mnt_stat.f_fsid.val[0] = dev2udev(devvp->v_rdev);
    mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
    hmp->hm_dev = devvp->v_rdev;
    hmp->hm_devvp = devvp;
    hmp->hm_cp = cp;
    MNT_ILOCK(mp);
    mp->mnt_flag |= MNT_LOCAL;
    mp->mnt_kern_flag |= MNTK_LOOKUP_SHARED | MNTK_EXTENDED_SHARED;
    MNT_IUNLOCK(mp);
    vfs_mountedfrom(mp, fromPath);
    return 0;
out:
    if (bp)
        brelse(bp);
    if (cp != NULL) {
        DROP_GIANT();
        g_topology_lock();
        g_vfs_close(cp);
        g_topology_unlock();
        PICKUP_GIANT();
    }
    vrele(devvp);
    return error;
}

int
hfsp_vget_fork(struct mount * mp, struct HFSPlusForkData * fork, struct vnode ** vpp)
{
    struct hfspmount * hmp;
    struct hfsp_inode * ip;
    struct vnode * vp;
    int i, error; 
    ip = uma_zalloc(uma_inode, M_WAITOK | M_ZERO);
    hmp = VFSTOHFSPMNT(mp);

    ip->hi_fork.size = be64toh(fork->logicalSize);
    ip->hi_fork.totalBlocks = be32toh(fork->totalBlocks);
    for (i = 0; i < 8; i++)
    {
        ip->hi_fork.first_extents[i].startBlock = be32toh(fork->extents[i].startBlock);
        ip->hi_fork.first_extents[i].blockCount = be32toh(fork->extents[i].blockCount);
    }

    error = getnewvnode("hfsp", mp, &hfsp_vnodeops, &vp);

    if (error)
    {
        *vpp = NULL;
        uma_zfree(uma_inode, ip);
        return (error);
    }

    vp->v_data = ip;

    error = insmntque(vp, mp);
    if (error)
    {
        *vpp = NULL;
        uma_zfree(uma_inode, ip);
        return (error);
    }

    *vpp = vp;
    return (0);

}

int
hfsp_mount_volume(struct vnode * devvp, struct hfspmount * hmp, struct HFSPlusVolumeHeader * hfsph)
{
    
    return 0;
}


int
hfsp_statfs(struct mount *mp, struct statfs *sbp)
{
    struct hfspmount * hfsmp;
    uprintf("Statfs called.");

    hfsmp = VFSTOHFSPMNT(mp);

    sbp->f_bsize = hfsmp->hm_blockSize;
    sbp->f_blocks = hfsmp->hm_totalBlocks;
    sbp->f_bfree = hfsmp->hm_freeBlocks;
    sbp->f_files = hfsmp->hm_fileCount;
    
    return 0;
}

static int
hfsp_unmount(struct mount *mp, int mntflags)
{
    struct hfspmount * hmp;
    uprintf("Unmounted device.\n");

    hmp = VFSTOHFSPMNT(mp);

    DROP_GIANT();
    g_topology_lock();
    g_vfs_close(hmp->hm_cp);
    g_topology_unlock();
    PICKUP_GIANT();
    vrele(hmp->hm_devvp);

    free(hmp, M_HFSPMNT);
    mp->mnt_data = NULL;
    MNT_ILOCK(mp);
    mp->mnt_flag &= ~MNT_LOCAL;
    MNT_IUNLOCK(mp);
    return 0;
}

