DEBUG=on
KMOD=hfsp
SRCS=hfsp.h hfsp_debug.c hfsp_debug.h hfsp_unicode.c hfsp_unicode.h hfsp_vfsops.c hfsp_vnops.c hfsp_inode.c hfsp_btree.h hfsp_btree.c vnode_if.h

.include <bsd.kmod.mk>
