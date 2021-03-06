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

enum {
    HFSP_NODE_INDEX     = 0,
    HFSP_NODE_HEADER    = 1,
    HFSP_NODE_MAP       = 2,
    HFSP_NODE_LEAF      = -1
};

struct hfsp_node;
struct hfsp_record;

/*
 * Operation that read a record depending on the node type.
 */
typedef int (*btree_record_read_t)(struct hfsp_node * bp, int recidx, struct hfsp_record ** recpp);

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
    u_int32_t           hb_totalNodes;
    u_int32_t           hb_freeNodes;
    u_int32_t           hb_leafRecords;
};

/* In memory node */
struct hfsp_node {
    struct hfsp_btree * hn_btreep;
    struct buf *        hn_buffer;          /* Buffer containing the read data */
    u_int64_t           hn_offset;          /* Offset from the special file. */
    u_int32_t           hn_next;
    u_int32_t           hn_prev;
    u_int16_t           hn_numRecords;
    u_int16_t           hn_nodeSize;
    u_int8_t *          hn_beginBuf;
    u_int16_t *         hn_recordTable;     /* Jump table to records */
    __int8_t            hn_kind;
    u_int8_t            hn_height;
    bool                hn_inMemory;
    btree_record_read_t hn_read;
};

int hfsp_btree_open(struct hfsp_inode * ip, struct hfsp_btree ** btreepp);
void hfsp_btree_close(struct hfsp_btree * btreep);
void hfsp_release_btnode(struct hfsp_node * np);
int hfsp_get_btnode_from_idx(struct hfsp_btree * btreep, u_int32_t num, struct hfsp_node ** npp);
int hfsp_get_btnode_from_offset(struct hfsp_btree * btreep, u_int64_t offset, struct hfsp_node ** npp);
int hfsp_brec_catalogue_read_key(struct hfsp_record * np, struct hfsp_record_key * rkp);

/*
 * Copy a hfsp_record_key to a other.
 * dstp: Pointer to the destination hfsp_record_key.
 * srcp: Pointer to the source hfsp_record_key.
 */
void hfsp_copy_record_key(struct hfsp_record_key * dstp, struct hfsp_record_key * srcp);

/*
 * Find the record for a given hfsp_record_key.
 * btree: The btree where to find the record.
 * kp: The hfsp_record_key structur to find.
 * recpp: Address to pointer to the hfsp_record that will be fill upon exit.
 */
int hfsp_btree_find(struct hfsp_btree * btreep, struct hfsp_record_key * kp, struct hfsp_record ** recpp);

/*
 * Find a record for a given cnid.
 * btree: The btree where to find the record.
 * cnid: The cnid to find.
 * recpp: Address to pointer to the hfsp_record that will be fill upon exit.
 */
int hfsp_btree_find_cnid(struct hfsp_btree * btreep, hfsp_cnid cnid, struct hfsp_record ** recpp);

/*
 * Compare record key.
 * XXX Todo the HFSX case sensitive comparaison.
 */
int hfsp_brec_key_cmp(struct hfsp_record_key * lkp, struct hfsp_record_key * rkp);

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
 * Return an address within a record.
 * rp: Pointer to a hfsp_record from which to read.
 * offset: Offset from the beginning of the record.
 */
caddr_t hfsp_brec_read_addr(struct hfsp_record * rp, u_int16_t offset);

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
 * Reads a record as a folder record.
 * Function used internally.
 * recp: Pointer to the hfsp_record that will be fill upon exit.
 * Return 0 on success.
 */
int hfsp_brec_catalogue_read_folder(struct hfsp_record * recp);

/*
 * Reads a record as a file record.
 * Function used internally.
 * recp: Pointer to the hfsp_record that will be fill upon exit.
 * Return 0 on success.
 */
int hfsp_brec_catalogue_read_file(struct hfsp_record * recp);

/*
 * Read a catalogue record entry from a node.
 * np: Pointer to a hfsp_node structure that contain the record. Node should be in memory.
 * recidx: Index of the record to read.
 * recpp: Address of a pointer to a hfsp_record. If it point to NULL the record will be allocated.
 * Otherwise it will assume that the structure is allocated.
 */
int hfsp_brec_catalogue_read(struct hfsp_node * np, int recidx, struct hfsp_record ** recpp);

/*
 * Same as hfsp_brec_catalogue_read but only reads the key part of the record.
 * This is used for lookup avoiding the overhead of reading the complete record.
 * np: Pointer to a hfsp_node structure that contain the record. Node should be in memory.
 * recidx: Index of the record to read.
 * recpp: Address of a pointer to a hfsp_record. If it point to NULL the record will be allocated.
 * Otherwise it will assume that the structure is allocated.
 */
int hfsp_brec_catalogue_lookup_read(struct hfsp_node * np, int recidx, struct hfsp_record ** recpp);

/*
 * Read a record as an index in the catalogue file.
 * np: Pointer to a hfsp_node structure that contain the record. Node should be in memory.
 * recidx: Index of the record to read.
 * recpp: Address of a pointer to a hfsp_record. If it point to NULL the record will be allocated.
 * Otherwise it will assume that the structure is allocated.
 */
int hfsp_brec_catalogue_index_read(struct hfsp_node * np, int recidx, struct hfsp_record ** recpp);

/*
 * Default read operation for reading a record.
 * XXX Manly used in development stage.
 */
int hfsp_brec_noops(struct hfsp_node * np, int recidx, struct hfsp_record ** recpp);

/*
 * Search for a record that best match the key within a node.
 * np: Pointer to a hfsp_node structure.
 * kp: Pointer to a hfsp_record_key structure.
 * recpp: Address of a pointer to a hfsp_record. If it point to NULL the record will be allocated.
 * Otherwise it will assume that the structure is allocated.
 */
int hfsp_brec_find(struct hfsp_node * np, struct hfsp_record_key * kp, struct hfsp_record ** recpp);

/*
 * Return a new structure that can hold record information.
 */
struct hfsp_record * hfsp_brec_alloc(void);

/*
 * Fetch the next record. On return the node is the one from where the record have been fetch.
 * npp: Node from where the fetch will happen. On exit the node can be updated.
 * recidx: starting index of the record.
 * next: number of record to go next. Can be negative.
 * recpp: Address of a pointer to a hfsp_record. If it point to NULL the record will be allocated.
 * Otherwise it will assume that the structure is allocated.
 */
int hfsp_brec_catalogue_read_next(struct hfsp_node ** npp, int recidx, int next, struct hfsp_record ** recpp);

/*
 * Release a hfsp_record structure.
 */
void hfsp_brec_release_record(struct hfsp_record ** rpp);

#define PBE16TOH(x) be16toh(*(u_int16_t*)(x))
#define PBE32TOH(x) be32toh(*(u_int32_t*)(x))

#endif /* _HFSP_BTREE_H_ */
