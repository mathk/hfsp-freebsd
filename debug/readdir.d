
fbt::hfsp_*:entry
/execname == "ls"/
{
}

/*
 * Readdir parameter read
 * */

fbt::hfsp_readdir:entry
{
    printf("uio resid: %d, uio_offset: %d", ((struct vop_readdir_args *)arg0)->a_uio->uio_resid, ((struct vop_readdir_args *)arg0)->a_uio->uio_offset);
    stack();
}

