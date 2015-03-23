
struct BTNodeDescriptor {
    u_int32_t   fLink;          /* next node at this level*/
    u_int32_t   bLink;          /* previous node at this level*/
    __int8_t    kind;           /* kind of node (leaf, index, header, map)*/
    u_int8_t    height;         /* zero for header, map; child is one more than parent*/
    u_int16_t   numRecords;     /* number of records in this node*/
    u_int16_t   reserved;       /* reserved - initialized as zero */
} __attribute__((aligned(2), packed));

struct BTHeaderRec {
    u_int16_t   treeDepth;      /* maximum height (usually leaf nodes) */
    u_int32_t   rootNode;       /* node number of root node */
    u_int32_t   leafRecords;    /* number of leaf records in all leaf nodes */
    u_int32_t   firstLeafNode;  /* node number of first leaf node */
    u_int32_t   lastLeafNode;   /* node number of last leaf node */
    u_int16_t   nodeSize;       /* size of a node, in bytes */
    u_int16_t   maxKeyLength;   /* reserved */
    u_int32_t   totalNodes;     /* total number of nodes in tree */
    u_int32_t   freeNodes;      /* number of unused (free) nodes in tree */
    u_int16_t   reserved1;      /* unused */
    u_int32_t   clumpSize;      /* reserved */
    u_int8_t    btreeType;      /* reserved */
    u_int8_t    keyCompareType; /* Key string Comparison Type */
    u_int32_t   attributes;     /* persistent attributes about the tree */
    u_int32_t   reserved3[16];  /* reserved */
} __attribute__((aligned(2), packed));

