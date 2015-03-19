
/* 
 * Description of all the format for HFS Plus
 * Most of it is taken from the XNU kernel
 */


MALLOC_DECLARE(M_HFSPMNT);

/* Signatures used to differentiate between HFS and HFS Plus volumes */
enum {
    kHFSSigWord             = 0x4244,   /* 'BD' in ASCII */
    kHFSPlusSigWord         = 0x482B,   /* 'H+' in ASCII */
    kHFSXSigWord            = 0x4858,   /* 'HX' in ASCII */

    kHFSPlusVersion         = 0x0004,   /* 'H+' volumes are version 4 only */
    kHFSXVersion            = 0x0005,   /* 'HX' volumes start with version 5 */

    kHFSPlusMountVersion    = 0x31302E30,   /* '10.0' for Mac OS X */
    kHFSJMountVersion       = 0x4846534a,   /* 'HFSJ' for journaled HFS+ on OS X */
    kFSKMountVersion        = 0x46534b21    /* 'FSK!' for failed journal replay */
};

/* HFS Plus extent descriptor */
struct HFSPlusExtentDescriptor {
    __int32_t   startBlock;     /* first allocation block */
    __int32_t   blockCount;     /* number of allocation blocks */
} __attribute__((aligned(2), packed));

typedef struct HFSPlusExtentDescriptor HFSPlusExtentRecord[8];

/* HFS Plus Fork data info - 80 bytes */
struct HFSPlusForkData {
    __int64_t           logicalSize;    /* fork's logical size in bytes */
    __int32_t           clumpSize;      /* fork's clump size in bytes */
    __int32_t           totalBlocks;    /* total blocks used by this fork */
    HFSPlusExtentRecord extents;        /* initial set of extents */
} __attribute__((aligned(2), packed));


struct HFSPlusVolumeHeader {
    __int16_t   signature;      /* == kHFSPlusSigWord */
    __int16_t   version;        /* == kHFSPlusVersion */
    __int32_t   attributes;     /* volume attributes */
    __int32_t   lastMountedVersion; /* implementation version which last mounted volume */
    __int32_t   journalInfoBlock;   /* block addr of journal info (if volume is journaled, zero otherwise) */

    __int32_t   createDate;     /* date and time of volume creation */
    __int32_t   modifyDate;     /* date and time of last modification */
    __int32_t   backupDate;     /* date and time of last backup */
    __int32_t   checkedDate;        /* date and time of last disk check */

    __int32_t   fileCount;      /* number of files in volume */
    __int32_t   folderCount;        /* number of directories in volume */

    __int32_t   blockSize;      /* size (in bytes) of allocation blocks */
    __int32_t   totalBlocks;        /* number of allocation blocks in volume (includes this header and VBM*/
    __int32_t   freeBlocks;     /* number of unused allocation blocks */

    __int32_t   nextAllocation;     /* start of next allocation search */
    __int32_t   rsrcClumpSize;      /* default resource fork clump size */
    __int32_t   dataClumpSize;      /* default data fork clump size */
    __int32_t   nextCatalogID;      /* next unused catalog node ID */

    __int32_t   writeCount;     /* volume write count */
    __int64_t   encodingsBitmap;    /* which encodings have been use  on this volume */

    __int8_t    finderInfo[32];     /* information used by the Finder */

    struct HFSPlusForkData  allocationFile;    /* allocation bitmap file */
    struct HFSPlusForkData  extentsFile;       /* extents B-tree file */
    struct HFSPlusForkData  catalogFile;       /* catalog B-tree file */
    struct HFSPlusForkData  attributesFile;    /* extended attributes B-tree file */
    struct HFSPlusForkData  startupFile;       /* boot file (secondary loader) */
} __attribute__((aligned(2), packed));

struct hfspmount {
    __int16_t   signature;  /* ==kHFSPlusSigWord */
    __int16_t   version;    /* ==kHFSPlusVersion */
    __int32_t   blockSize;  /* Size in byte of allocation block */
    __int32_t   totalBlocks;
    __int32_t   freeBlocks;
    struct cdev * hm_dev;
    struct vnode * hm_devvp;
    struct g_consumer * hm_cp;
};


