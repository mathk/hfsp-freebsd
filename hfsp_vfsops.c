#include <sys/param.h>
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
#include <sys/kobj.h>
#include <sys/iconv.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include "hfsp.h"
#include "hfsp_btree.h"

MALLOC_DEFINE(M_HFSPMNT, "hfsp_mount", "HFS Plus mount structure");

static uma_zone_t       uma_inode;
uma_zone_t       uma_record_key;

static vfs_mount_t      hfsp_mount;
static vfs_unmount_t    hfsp_unmount;
static vfs_statfs_t     hfsp_statfs;
static vfs_init_t       hfsp_init;
static vfs_uninit_t     hfsp_uninit;

int hfsp_iget(struct hfspmount * mp, struct HFSPlusForkData * fork, struct hfsp_inode ** ipp);
void hfsp_freemnt(struct hfspmount * hmp);
int hfsp_vget(struct mount * mp, struct HFSPlusForkData * fork, struct vnode ** vpp);
int hfsp_mount_volume(struct vnode * devvp, struct hfspmount * hmp, struct HFSPlusVolumeHeader * hfsph);
void udump(char * buff, int size);

void uprint_record(struct hfsp_record * rp);

static struct vfsops hfsp_vfsops = {
//    .vfs_fhtovp =   NULL,
    .vfs_init =     hfsp_init,
    .vfs_uninit =   hfsp_uninit,
    .vfs_mount =    hfsp_mount,
//    .vfs_root = NULL,
    .vfs_statfs =   hfsp_statfs,
//    .vfs_sync =     NULL,
    .vfs_unmount =  hfsp_unmount,
//    .vfs_vget =     NULL
};
VFS_SET(hfsp_vfsops, hfsp, 0);

MODULE_DEPEND(hfsp_mod, libiconv, 2, 2, 2);

static int
hfsp_init(struct vfsconf * conf)
{
    uprintf("HFS+ module initialized\n");
    uma_inode = uma_zcreate("HFS+ inode", sizeof(struct hfsp_inode), NULL, NULL, NULL, NULL,
                            UMA_ALIGN_PTR, 0);

    uma_record_key = uma_zcreate("HFS+ key record", sizeof(struct hfsp_record_key), NULL, NULL, NULL, NULL,
                                 UMA_ALIGN_PTR, 0);

    hfsp_brec_catalogue_read_init();
    return 0;
}

static int
hfsp_uninit(struct vfsconf * conf)
{
    uma_zdestroy(uma_inode);
    uma_zdestroy(uma_record_key);
    uprintf("HFS+ module uninitialized\n");
    return 0;
}

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
    struct HFSPlusVolumeHeader hfsph;
    struct hfspmount *hmp = NULL;

    td = curthread;
    opts = mp->mnt_optnew;
    vnodecovered = mp->mnt_vnodecovered;

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

    bcopy(bp->b_data, &hfsph, sizeof(hfsph));
    hmp = malloc(sizeof(*hmp), M_HFSPMNT, M_WAITOK | M_ZERO);
    brelse(bp);
    bp = NULL;

    hmp->hm_blockSize = be32toh(hfsph.blockSize);
    hmp->hm_totalBlocks = be32toh(hfsph.totalBlocks);
    hmp->hm_freeBlocks = be32toh(hfsph.freeBlocks);
    hmp->hm_physBlockSize = cp->provider->sectorsize;
    hmp->hm_dev = devvp->v_rdev;
    hmp->hm_devvp = devvp;

    hfsp_mount_volume(devvp, hmp, &hfsph);

    mp->mnt_data = hmp;
    mp->mnt_stat.f_fsid.val[0] = dev2udev(devvp->v_rdev);
    mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
    hmp->hm_cp = cp;
    MNT_ILOCK(mp);
    mp->mnt_flag |= MNT_LOCAL;
    mp->mnt_kern_flag |= MNTK_LOOKUP_SHARED | MNTK_EXTENDED_SHARED;
    MNT_IUNLOCK(mp);

    vfs_mountedfrom(mp, fromPath);

    error = ENOMEM;
    goto out;
    return 0;
out:
    if (bp)
        brelse(bp);
    if (hmp)
        hfsp_freemnt(hmp);
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
hfsp_iget(struct hfspmount * hmp, struct HFSPlusForkData * fork, struct hfsp_inode ** ipp)
{
    struct hfsp_inode * ip;
    int i;

    ip = uma_zalloc(uma_inode, M_WAITOK | M_ZERO);
    if (ip == NULL)
    {
        *ipp = NULL;
        return ENOMEM;
    }

    ip->hi_fork.size = be64toh(fork->logicalSize);
    ip->hi_fork.totalBlocks = be32toh(fork->totalBlocks);
    ip->hi_mount = hmp;

    for (i = 0; i < HFSP_FIRSTEXTENT_SIZE; i++)
    {
        ip->hi_fork.first_extents[i].startBlock = be32toh(fork->extents[i].startBlock);
        ip->hi_fork.first_extents[i].blockCount = be32toh(fork->extents[i].blockCount);
    }

    *ipp = ip;
    return 0;

}

int
hfsp_vget(struct mount * mp, struct HFSPlusForkData * fork, struct vnode ** vpp)
{
    struct hfsp_inode * ip;
    struct hfspmount * hmp;
    struct vnode * vp;
    int error;

    hmp = VFSTOHFSPMNT(mp);
    error = hfsp_iget(hmp, fork, &ip);
    if (error)
    {
        return error;
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
    struct hfsp_inode * ip;
    struct hfsp_btree * btreep;
    struct hfsp_node * np;
    struct hfsp_record * rp;
    int error, i;

    error = hfsp_iget(hmp, &(hfsph->extentsFile), &ip);
    if (error)
    {
        return error;
    }
    // Special file use the device vnode
    ip->hi_vp = hmp->hm_devvp;

    /* We first open the extent special file*/
    error = hfsp_btree_open(ip, &hmp->hm_extent_bp);
    if (error)
    {
        return error;
    }


    btreep = hmp->hm_extent_bp;

    uprintf("Extent btree open\n Node size: %d(<< %d)\n Root node %d\n Map node %d\n Tree depth %d\n", 
            btreep->hb_nodeSize, btreep->hb_nodeShift, btreep->hb_rootNode, btreep->hb_mapNode, btreep->hb_treeDepth);

    error = hfsp_iget(hmp, &(hfsph->catalogFile), &ip);
    if (error)
    {
        return error;
    }
    ip->hi_vp = hmp->hm_devvp;

    error = hfsp_btree_open(ip, &hmp->hm_catalog_bp);

    btreep = hmp->hm_catalog_bp;

    uprintf("Catalog btree open\n Node size: %d (<< %d) \n Root node %d\n Map node %d\n Tree depth %d\n", 
            btreep->hb_nodeSize, btreep->hb_nodeShift, btreep->hb_rootNode, btreep->hb_mapNode, btreep->hb_treeDepth);

    uprintf("Read Node number: %d\n", btreep->hb_firstLeafNode);
    error = hfsp_get_btnode(btreep, btreep->hb_rootNode, &np);
    if (error)
        return error;
    uprintf("Openning first leaf node. Record nbrs: %d, kind %d\n, Buffer data %lx\nFirst record table offset %d\n", np->hn_numRecords, np->hn_kind, (u_int64_t)np->hn_beginBuf, be16toh(*(np->hn_recordTable - 1)));

    for (i = 0;  i < np->hn_numRecords; i++)
    {
        uprintf("Reading record %d\n", i);
        error = hfsp_brec_catalogue_read(np, i, &rp);
        if (!error)
        {
            uprint_record(rp);
            hfsp_brec_release_record(rp);
        }
    }

    hfsp_release_btnode(np);
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

void
hfsp_irelease(struct hfsp_inode * ip)
{
    if (ip != NULL)
        uma_zfree(uma_inode, ip);
}

void
hfsp_freemnt(struct hfspmount * hmp)
{
    hfsp_btree_close(hmp->hm_extent_bp);
    hfsp_btree_close(hmp->hm_catalog_bp);
    free(hmp, M_HFSPMNT);
}

static int
hfsp_unmount(struct mount *mp, int mntflags)
{
    struct hfspmount * hmp;
    struct g_consumer * cp;
    struct vnode * devvp;
    uprintf("Unmounted device.\n");

    hmp = VFSTOHFSPMNT(mp);
    cp = hmp->hm_cp;
    devvp = hmp->hm_devvp;
    hfsp_freemnt(hmp);

    DROP_GIANT();
    g_topology_lock();
    g_vfs_close(hmp->hm_cp);
    g_topology_unlock();
    PICKUP_GIANT();
    vrele(hmp->hm_devvp);

    mp->mnt_data = NULL;
    MNT_ILOCK(mp);
    mp->mnt_flag &= ~MNT_LOCAL;
    MNT_IUNLOCK(mp);
    return 0;
}

