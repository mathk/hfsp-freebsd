#include <sys/param.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include "hfsp.h"
#include "hfsp_btree.h"

static vop_reclaim_t    hfsp_reclaim;
static vop_readdir_t    hfsp_readdir;

struct vop_vector hfsp_vnodeops = {
    .vop_default = &default_vnodeops,
    .vop_reclaim = hfsp_reclaim,
    .vop_readdir = hfsp_readdir
};


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

    while (uio->uio_resid > 0 && uio->uio_offset < rfp->hrfo_valance)
    {
        
    }
    return 0;
}

