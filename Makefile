KMOD=hfsp
SRCS=hfsp.h hfsp_vfsops.c hfsp_vnops.c hfsp_inode.c hfsp_btree.h hfsp_btree.c vnode_if.h iconv_converter_if.h

.include <bsd.kmod.mk>
