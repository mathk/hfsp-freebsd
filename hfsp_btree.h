#include <sys/mount.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include "hfsp.h"

#ifndef _HFSP_BTREE_H_
#define _HFSP_BTREE_H_

MALLOC_DECLARE(M_HFSPBTREE);
MALLOC_DECLARE(M_HFSPNODE);
MALLOC_DECLARE(M_HFSPREC);

enum {
    HFSP_FOLDER_RECORD          = 0x1,
    HFSP_FILE_RECORD            = 0x2,
    HFSP_FOLDER_THREAD_RECORD   = 0x3,
    HFSP_FILE_THREAD_RECORD     = 0x4
};

#define RECORD_TYPE_COUNT HFSP_FILE_THREAD_RECORD

/* Btree held in memory */
struct hfsp_btree {
    struct hfsp_inode * hb_ip; /* The inode of the btree */

    u_int32_t           hb_rootNode;
    u_int16_t           hb_nodeSize;
    u_int16_t           hb_nodeShift;
    u_int16_t           hb_treeDepth;
    u_int32_t           hb_mapNode;
    u_int32_t           hb_firstLeafNode;
};

/* In memory node */
struct hfsp_node {
    struct buf *    hn_buffer;          /* Buffer containing the read data */
    u_int64_t       hn_offset;          /* Offset from the special file. */
    u_int32_t       hn_next;
    u_int32_t       hn_prev;
    u_int16_t       hn_numRecords;
    u_int16_t       hn_nodeSize;
    u_int8_t *      hn_beginBuf;
    u_int16_t *     hn_recordTable;     /* Jump table to records */
    __int8_t        hn_kind;
    u_int8_t        hn_height;
    bool            hn_inMemory;
};

struct hfsp_record_thread {
    __int16_t           hrt_recordType;
    hfsp_cnid           hrt_parentCnid;
    struct hfsp_unistr  hrt_name;
};

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

struct hfsp_record {
    struct hfsp_record_key  hr_key;
    struct hfsp_node *      hr_node;
    u_int16_t               hr_offset;  /*Offset in the b-tree node. */
    u_int16_t               hr_dataOffset; /* Offset in the b-tree of the start of the data. */
    union {
        struct hfsp_record_common common;
        struct hfsp_record_thread thread;
    } hr_data;
};

#define hr_type     hr_data.common.hrc_recordType
#define hr_thread   hr_data.thread

int hfsp_btree_open(struct hfsp_inode * ip, struct hfsp_btree ** btreepp);
void hfsp_btree_close(struct hfsp_btree * btreep);
void hfsp_release_btnode(struct hfsp_node * np);
int hfsp_init_find_info(struct hfsp_find_info * fip);
void hfsp_destroy_find_info(struct hfsp_find_info * fip);
int hfsp_get_btnode(struct hfsp_btree * btreep, u_int32_t num, struct hfsp_node ** npp);
int hfsp_brec_catalogue_read_key(struct hfsp_record * np, struct hfsp_record_key * rkp);

/*
 * Initialize read routine for catalogue record.
 */
void hfsp_brec_catalogue_read_init(void);

/*
 * Read a 16 bit data from a record.
 * rp: Pointer to a hfsp_record from which to read.
 * offset: Offset from the beginning of the record.
 */
u_int16_t hfsp_brec_read_u16(struct hfsp_record * rp, u_int16_t offset);

/*
 * Read a 32 bit data from a record.
 * rp: Pointer to a hfsp_record from which to read.
 * offset: Offset from the beginning of the record.
 */
u_int32_t hfsp_brec_read_u32(struct hfsp_record * rp, u_int16_t offset);

/*
 * Bcopy data from a record.
 * rp: Pointer to an hfsp_record from which to bcopy.
 * offset: Offset from the beginning of the record.
 * buf: Buffer that will hold the data on exit.
 * len: Length of the data to bcopy.
 * Return 0 on success.
 */
void hfsp_brec_bcopy(struct hfsp_record * rp, u_int16_t offset, void * buf, size_t len);

/*
 * Read a unicode string within a record.
 * rp: Pointer to an hfsp_record from which to bcopy.
 * offset: Offset from the beginning of the record.
 * strp: Pointer to a hfsp_unistr structure receiving the string.
 */
int hfsp_brec_read_unistr(struct hfsp_record * rp, u_int16_t offset, struct hfsp_unistr * strp);

/*
 * This function read a record as catalogue thread record.
 * Function should only be used internally.
 * recp: Pointer to the hfsp_record that will be fill upon exit.
 * Return 0 on success.
 */
int hfsp_brec_catalogue_read_thread(struct hfsp_record * recp);

/*
 * Read a catalogue record entry from a node.
 * np: Pointer to a hfsp_node structure that contain the record. Node should be in memory.
 * recidx: Index of the record to read.
 * recpp: Address of a pointer to a hfsp_record that will be fill upon exit.
 */
int hfsp_brec_catalogue_read(struct hfsp_node * np, int recidx, struct hfsp_record ** recpp);

/*
 * Release a hfsp_record structure.
 */
void hfsp_brec_release_record(struct hfsp_record * rp);

#endif /* _HFSP_BTREE_H_ */
