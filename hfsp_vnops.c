#include <sys/param.h>
#include <sys/namei.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/dirent.h>

#include "hfsp.h"
#include "hfsp_btree.h"
#include "hfsp_debug.h"

static vop_reclaim_t    hfsp_reclaim;
static vop_readdir_t    hfsp_readdir;
static vop_getattr_t    hfsp_getattr;
static vop_access_t     hfsp_access;

struct vop_vector hfsp_vnodeops = {
    .vop_default = &default_vnodeops,
    .vop_reclaim = hfsp_reclaim,
    .vop_readdir = hfsp_readdir,
    .vop_getattr = hfsp_getattr,
    .vop_access = hfsp_access
};

/* Structure to syntesize the '.' and '..' entry */
struct hfsp_dot_direntry {
    u_int32_t   hd_fileNo;      // File number entry
    u_int16_t   hd_reclen;      // Length of the record. Should be constant.
    u_int8_t    hd_type;        // file type
    char        hd_name[3];      // String length
};

static enum vtype hfsp_record2vtype[] = {VNON, VDIR, VCHR, VNON, VNON};

int
hfsp_access(struct vop_access_args * ap)
{
    struct vnode * vp = ap->a_vp;
    struct hfsp_inode * ip = VTOI(vp);
    struct hfsp_record * rp = &ip->hi_record;
    accmode_t accmode = ap->a_accmode;

    // Disalow write access
    if (accmode & VWRITE) {
        return (EROFS);
    }

    return vaccess(vp->v_type, rp->hr_fileMode, rp->hr_ownerId, rp->hr_groupId, ap->a_accmode, ap->a_cred, NULL);

}

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
        vap->va_size = 2;//recp->hr_folder.hrfo_valence + 2;
        vap->va_nlink = recp->hr_linkCount;
        vap->va_bytes = (recp->hr_folder.hrfo_valence + 2) * HFS_AVERAGE_DIRENTRY_SIZE;
    }
    vap->va_mode = recp->hr_fileMode & (~S_IFMT);
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
    struct hfsp_dot_direntry dot[2];
    int error;

    uio = ap->a_uio;
    if (uio->uio_offset < 0)
        return EINVAL;

    vp = ap->a_vp;
    ip = VTOI(ap->a_vp);
    hmp = VFSTOHFSPMNT(vp->v_mount);
    rfp = &ip->hi_record.hr_folder;

    // We synthesize the '.' and '..'
    if (uio->uio_offset == 0)
    {
        dot[0].hd_fileNo = ip->hi_record.hr_cnid;
        dot[0].hd_type = DT_DIR;
        dot[0].hd_reclen = sizeof(struct hfsp_dot_direntry);
        dot[0].hd_name[0] = '.';
        dot[0].hd_name[1] = '\0';
        dot[0].hd_name[2] = '\0';


        dot[1].hd_fileNo = ip->hi_record.hr_parentCnid;
        dot[1].hd_type = DT_DIR;
        dot[1].hd_reclen = sizeof(struct hfsp_dot_direntry);
        dot[1].hd_name[0] = '.';
        dot[1].hd_name[1] = '.';
        dot[1].hd_name[2] = '\0';

        error = uiomove((caddr_t)dot, sizeof(dot), uio);
        if (error)
            return error;

        uprintf("Moving uio: %lu, offset: %ld\n", sizeof(dot), uio->uio_offset);
        udump((caddr_t)dot, sizeof(dot));
        return 0;

    }

    error = hfsp_get_btnode_from_offset(hmp->hm_catalog_bp, ip->hi_record.hr_nodeOffset, &np);
    if (error)
        return error;


    while (uio->uio_resid > 0 && uio->uio_offset < rfp->hrfo_valence)
    {
        break;
    }

    hfsp_release_btnode(np);
    return EBADF;
}

void
hfsp_vinit(struct vnode * vp, struct hfsp_inode * ip)
{
    // XXX: Need to discover if it is a FIFO etc.
    if (ip->hi_record.hr_type < sizeof(hfsp_record2vtype))
        vp->v_type = hfsp_record2vtype[ip->hi_record.hr_type];
    else
        vp->v_type = VBAD;

    if (ip->hi_cnid == HFSP_ROOT_FOLDER_CNID)
        vp->v_vflag |= VV_ROOT;
}
