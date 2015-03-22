#include <sys/param.h>
#include <sys/types.h>
#include <sys/vnode.h>

struct vop_vector hfsp_vnodeops = {
    .vop_default = &default_vnodeops
};


