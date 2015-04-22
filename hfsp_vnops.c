#include <sys/param.h>
#include <sys/types.h>
#include <sys/vnode.h>

#include "hfsp.h"

static vop_reclaim_t    hfsp_reclaim;

struct vop_vector hfsp_vnodeops = {
    .vop_default = &default_vnodeops,
    .vop_reclaim = hfsp_reclaim
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



