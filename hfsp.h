#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <vm/uma.h>

#ifndef _HFSP_H_
#define _HFSP_H_
/*
 * Description of all the format for HFS Plus
 * Most of it is taken from the XNU kernel
 */


struct hfspmount;

MALLOC_DECLARE(M_HFSPMNT);
MALLOC_DECLARE(M_HFSPKEYSEARCH);
MALLOC_DECLARE(M_HFSPKEY);

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

typedef u_int16_t hfsp_unichar;

typedef u_int32_t   hfsp_cnid;

/*
 * Structure representing a HFS+ unicode string.
 * The hu_str member is expecting to be a copy of what is found
 * on disk (BE unichar) whereas the hu_len is convert to host endianness.
 */
struct hfsp_unistr {
    u_int16_t       hu_len;
    hfsp_unichar    hu_str[255];
};

/* HFS Plus extent descriptor */
struct HFSPlusExtentDescriptor {
    u_int32_t   startBlock;     /* first allocation block */
    u_int32_t   blockCount;     /* number of allocation blocks */
} __attribute__((aligned(2), packed));

typedef struct HFSPlusExtentDescriptor HFSPlusExtentRecord[8];

/* HFS Plus Fork data info - 80 bytes */
struct HFSPlusForkData {
    u_int64_t           logicalSize;    /* fork's logical size in bytes */
    u_int32_t           clumpSize;      /* fork's clump size in bytes */
    u_int32_t           totalBlocks;    /* total blocks used by this fork */
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


/* BTNodeDescriptor -- Every B-tree node starts with these fields. */
struct BTNodeDescriptor {
    u_int32_t   fLink;          /* next node at this level*/
    u_int32_t   bLink;          /* previous node at this level*/
    __int8_t    kind;           /* kind of node (leaf, index, header, map)*/
    u_int8_t    height;         /* zero for header, map; child is one more than parent*/
    u_int16_t   numRecords;     /* number of records in this node*/
    u_int16_t   reserved;       /* reserved - initialized as zero */
} __attribute__((aligned(2), packed));
typedef struct BTNodeDescriptor BTNodeDescriptor;

/* BTHeaderRec -- The first record of a B-tree header node */
struct BTHeaderRec {
    u_int16_t   treeDepth;          /* maximum height (usually leaf nodes) */
    u_int32_t   rootNode;           /* node number of root node */
    u_int32_t   leafRecords;        /* number of leaf records in all leaf nodes */
    u_int32_t   firstLeafNode;      /* node number of first leaf node */
    u_int32_t   lastLeafNode;       /* node number of last leaf node */
    u_int16_t   nodeSize;           /* size of a node, in bytes */
    u_int16_t   maxKeyLength;       /* reserved */
    u_int32_t   totalNodes;         /* total number of nodes in tree */
    u_int32_t   freeNodes;          /* number of unused (free) nodes in tree */
    u_int16_t   reserved1;          /* unused */
    u_int32_t   clumpSize;          /* reserved */
    u_int8_t    btreeType;          /* reserved */
    u_int8_t    keyCompareType;     /* Key string Comparison Type */
    u_int32_t   attributes;         /* persistent attributes about the tree */
    u_int32_t   reserved3[16];      /* reserved */
} __attribute__((aligned(2), packed));
typedef struct BTHeaderRec BTHeaderRec;

struct hfsp_extent_descriptor {
    u_int32_t   startBlock;     /* first allocation block */
    u_int32_t   blockCount;     /* number of allocation blocks */
};

struct hfsp_fork {
    u_int64_t   size;
    u_int32_t   totalBlocks;
    struct hfsp_extent_descriptor first_extents[8];
};

/* In memory content of a thread record */
struct hfsp_record_thread {
    __int16_t           hrt_recordType;
    hfsp_cnid           hrt_parentCnid;
    struct hfsp_unistr  hrt_name;
};

/* In memory content of a folder record */
struct hfsp_record_folder {
    __int16_t           hrfo_recordType;
    u_int16_t           hrfo_flags;
    u_int32_t           hrfo_valance;
    hfsp_cnid           hrfo_folderCnid;
    u_int32_t           hrfo_createDate;
    // XXX: To continue
};

struct hfsp_record_common {
    __int16_t           hrc_recordType;
};

/* Key of a record */
struct hfsp_record_key {
    u_int16_t   hk_len;
    hfsp_cnid   hk_cnid;
    struct hfsp_unistr hk_name;
};

/* Record structure */
struct hfsp_record {
    struct hfsp_record_key  hr_key;
    struct hfsp_node *      hr_node;
    u_int16_t               hr_offset;  /*Offset in the b-tree node. */
    u_int16_t               hr_dataOffset; /* Offset in the b-tree of the start of the data. */
    union {
        struct hfsp_record_common common;
        struct hfsp_record_thread thread;
        struct hfsp_record_folder folder;
        u_int32_t   index;
    } hr_data;
};

#define hr_type     hr_data.common.hrc_recordType
#define hr_thread   hr_data.thread
#define hr_folder   hr_data.folder
#define hr_index    hr_data.index

struct hfsp_inode {
    struct vnode *          hi_vp;
    struct hfspmount *      hi_mount;
    struct hfsp_record      hi_record;
    // XXX Can be remove. File record must contain the correct fork
    union {
        struct hfsp_fork    fork;
    } hi_data;
};

#define hi_fork     hi_data.fork

struct hfspmount {
    u_int16_t                   hm_signature;  /* ==kHFSPlusSigWord */
    u_int16_t                   hm_version;    /* ==kHFSPlusVersion */
    u_int32_t                   hm_blockSize;  /* Size in byte of allocation block */
    u_int32_t                   hm_totalBlocks;
    u_int32_t                   hm_freeBlocks;
    u_int32_t                   hm_fileCount;
    u_int32_t                   hm_physBlockSize;
    struct cdev *               hm_dev;
    struct vnode *              hm_devvp;
    struct hfsp_btree *         hm_extent_bp;
    struct hfsp_btree *         hm_catalog_bp;
    struct g_consumer *         hm_cp;
};
int hfsp_bread_inode(struct hfsp_inode * ip, u_int64_t fileOffset, int size, struct buf ** bpp);
void hfsp_irelease(struct hfsp_inode * ip);

#define VFSTOHFSPMNT(mp)        ((struct hfspmount *)((mp)->mnt_data))
#define VTOI(vp)                ((struct hfsp_inode *)((vp)->v_data))
#define HFSP_FIRSTEXTENT_SIZE   8
extern struct vop_vector hfsp_vnodeops;
extern uma_zone_t   uma_record_key;

#endif /* !_HFSP_H_ */
