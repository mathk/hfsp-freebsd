KMOD=hfsp
SRCS=hfsp.h hfsp_vfsops.c hfsp_vnops.c hfsp_btree.h hfsp_btree.c vnode_if.h

.include <bsd.kmod.mk>
