#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/kernel.h> /* types used in module initialization */
#include <sys/mount.h>
#include <sys/fcntl.h>

//#include <geom/geom.h>
//#include <geom/geom_vfs.h>

static vfs_mount_t	hfsp_mount;
static vfs_unmount_t 	hfsp_unmount;

static struct vfsops hfsp_vfsops = {
	.vfs_fhtovp =	NULL,
	.vfs_mount = 	hfsp_mount,
	.vfs_root =	NULL,
	.vfs_statfs = 	NULL,
	.vfs_sync = 	NULL,
	.vfs_unmount = 	hfsp_unmount,
	.vfs_vget = 	NULL
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

	vput(devvp);
	return EINVAL;
}

static int
hfsp_unmount(struct mount *mp, int mntflags)
{
	uprintf("Unmounted device.\n");
	return 0;
}

