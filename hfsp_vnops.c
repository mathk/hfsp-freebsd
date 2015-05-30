#include <sys/param.h>
#include <sys/namei.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include "hfsp.h"
#include "hfsp_btree.h"

static vop_reclaim_t    hfsp_reclaim;
static vop_readdir_t    hfsp_readdir;
static vop_getattr_t    hfsp_getattr;

struct vop_vector hfsp_vnodeops = {
    .vop_default = &default_vnodeops,
    .vop_reclaim = hfsp_reclaim,
    .vop_readdir = hfsp_readdir,
    .vop_getattr = hfsp_getattr
};

static enum vtype hfsp_record2vtype[] = {VNON, VDIR, VCHR, VNON, VNON};

int
hfsp_getattr(struct vop_getattr_args *ap)
{
    struct vattr *vap = ap->a_vap;
    struct vnode *vp = ap->a_vp;
    struct hfsp_inode * ip = VTOI(vp);
    struct hfsp_record  * recp = &ip->hi_record;

    vap->va_fsid = dev2udev(ip->hi_mount->hm_dev);
    vap->va_fileid = recp->hr_cnid;
    if (recp->hr_type == HFSP_FOLDER_RECORD)
    {
        vap->va_atime.tv_sec = recp->hr_folder.hrfo_lstAccessDate;
        vap->va_atime.tv_nsec = 0;
        vap->va_ctime.tv_sec = recp->hr_folder.hrfo_lstChangeTime;
        vap->va_ctime.tv_nsec = 0;
        vap->va_mtime.tv_sec = recp->hr_folder.hrfo_lstModifyDate;
        vap->va_mtime.tv_nsec = 0;
        vap->va_size = recp->hr_folder.hrfo_valence + 2;
    }
    vap->va_type = vp->v_type;
    return 0;
}

int
hfsp_reclaim(struct vop_reclaim_args * ap)
{
    struct hfsp_inode * ip;
    struct vnode * vp;

    vp = ap->a_vp;
    ip = VTOI(vp);

    hfsp_irelease(ip);
    vp->v_data = NULL;
    vnode_destroy_vobject(vp);
    return 0;
}

int
hfsp_readdir(struct vop_readdir_args /* */ *ap)
{
    struct hfsp_inode * ip;
    struct vnode * vp;
    struct hfspmount * hmp;
    struct uio * uio;
    struct hfsp_record_folder * rfp;
    struct hfsp_node * np;
    int error;

    uio = ap->a_uio;
    if (uio->uio_offset < 0)
        return EINVAL;

    vp = ap->a_vp;
    ip = VTOI(ap->a_vp);
    hmp = VFSTOHFSPMNT(vp->v_mount);
    rfp = &ip->hi_record.hr_folder;
    error = hfsp_get_btnode_from_offset(hmp->hm_catalog_bp, ip->hi_record.hr_nodeOffset, &np);
    if (error)
        return error;

    while (uio->uio_resid > 0 && uio->uio_offset < rfp->hrfo_valence)
    {
        
    }

    hfsp_release_btnode(np);
    return 0;
}

void
hfsp_vinit(struct vnode * vp, struct hfsp_inode * ip)
{
    if (ip->hi_record.hr_type < sizeof(hfsp_record2vtype))
        vp->v_type = hfsp_record2vtype[ip->hi_record.hr_type];
    else
        vp->v_type = VBAD;

    if (ip->hi_cnid == HFSP_ROOT_FOLDER_CNID)
        vp->v_vflag |= VV_ROOT;
}
