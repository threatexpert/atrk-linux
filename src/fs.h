#ifndef ATRK_fs
#define ATRK_fs
#include "utils.h"

static inline uint16_t ext2fs_swab16(uint16_t val)
{
	return (val >> 8) | (uint16_t) (val << 8);
}

static inline  uint32_t ext2fs_swab32(uint32_t val)
{
	return ((val>>24) | ((val>>8)&0xFF00) |
		((val<<8)&0xFF0000) | (val<<24));
}

static uint64_t ext2fs_swab64(uint64_t val)
{
	return (ext2fs_swab32((uint32_t) (val >> 32)) |
		(((uint64_t)ext2fs_swab32(val & 0xFFFFFFFFUL)) << 32));
}

#define EXT2_NAME_LEN 255
struct ext2_dir_entry_2 {
	uint32_t	inode;			/* Inode number */
	uint16_t	rec_len;		/* Directory entry length */
	uint8_t	name_len;		/* Name length */
	uint8_t	file_type;
	char	name[EXT2_NAME_LEN];	/* File name */
};
struct my_dir_entry {
    uint64_t	inode;
    uint8_t	name_len;
    uint8_t	file_type;
    uint32_t	myflags;
    char	name[1];
};

#define EXT4_MAX_REC_LEN		((1<<16)-1)


typedef uint8_t  __u8;
typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;
typedef struct {
    uint32_t a,b,c,d;
}uuid_t;

#define	XFS_DIR2_DATA_FREE_TAG	0xffff
#define	XFS_DIR2_DATA_FD_COUNT	3

#define 	XFS_DIR2_BLOCK_MAGIC   0x58443242 /* XD2B: single block dirs */
#define 	XFS_DIR2_DATA_MAGIC   0x58443244 /* XD2D: multiblock dirs */
#define 	XFS_DIR2_FREE_MAGIC   0x58443246 /* XD2F: free index blocks */

#define XFS_DIR3_BLOCK_MAGIC    0x58444233      /* XDB3: single block dirs */
#define XFS_DIR3_DATA_MAGIC     0x58444433      /* XDD3: multiblock dirs */
#define XFS_DIR3_FREE_MAGIC     0x58444633      /* XDF3: free index blocks */
struct xfs_dir3_blk_hdr {
        __be32                  magic;  /* magic number */
        __be32                  crc;    /* CRC of block */
        __be64                  blkno;  /* first block of the buffer */
        __be64                  lsn;    /* sequence number of last write */
        uuid_t                  uuid;   /* filesystem we belong to */
        __be64                  owner;  /* inode that owns the block */
};
typedef struct xfs_dir2_data_free {
        __be16                  offset;         /* start of freespace */
        __be16                  length;         /* length of freespace */
} xfs_dir2_data_free_t;
struct xfs_dir3_data_hdr {
        struct xfs_dir3_blk_hdr hdr;
        xfs_dir2_data_free_t    best_free[XFS_DIR2_DATA_FD_COUNT];
        __be32                  pad;    /* 64 bit alignment */
};
typedef struct xfs_dir2_data_entry {
        __be64                  inumber;        /* inode number */
        __u8                    namelen;        /* name length */
        __u8                    name[];         /* name bytes, no null */
     /* __u8                    filetype; */    /* type of inode we point to */
     /* __be16                  tag; */         /* starting offset of us */
} xfs_dir2_data_entry_t;
#define XFS_DIR2_DATA_FREE_TAG  0xffff

/*
 * Unused entry in a data block.
 *
 * Aligned to 8 bytes.  Tag appears as the last 2 bytes and must be accessed
 * using xfs_dir2_data_unused_tag_p.
 */
typedef struct xfs_dir2_data_unused {
        __be16                  freetag;        /* XFS_DIR2_DATA_FREE_TAG */
        __be16                  length;         /* total free length */
                                                /* variable offset */
        __be16                  tag;            /* starting offset of us */
} xfs_dir2_data_unused_t;
typedef struct xfs_dir2_data_hdr {
    __be32          magic;      /* XFS_DIR2_DATA_MAGIC or */
                        /* XFS_DIR2_BLOCK_MAGIC */
    xfs_dir2_data_free_t    bestfree[XFS_DIR2_DATA_FD_COUNT];
} xfs_dir2_data_hdr_t;

static inline uint16_t from_be16(uint16_t val)
{
    if (isBigEndian())
        return val;
    else
    	return (val >> 8) | (uint16_t) (val << 8);
}
static inline uint32_t from_be32(uint32_t val)
{
    if (isBigEndian())
        return val;
    else
        return ((val>>24) | ((val>>8)&0xFF00) |
            ((val<<8)&0xFF0000) | (val<<24));
}
static inline uint64_t from_be64(uint64_t val)
{
    if (isBigEndian())
        return val;
    else
        return ext2fs_swab64(val);
}

int parse_ext2_dir(char *buf, uint32_t datalen, struct Vector* files);
int parse_xfs_dir(char *buf, uint32_t datalen, struct Vector* files);
int do_dir(const char *path, struct Vector* files);


#endif /* !ATRK_fs */