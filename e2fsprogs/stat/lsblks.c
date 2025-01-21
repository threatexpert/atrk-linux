/*
 * debugfs.c --- a program which allows you to attach an ext2fs
 * filesystem and play with it.
 *
 * Copyright (C) 1993 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 *
 * Modifications by Robert Sanders <gt8134b@prism.gatech.edu>
 */

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "debugfs.h"

void com_err (const char *whoami,
	      errcode_t code,
	      const char *fmt, ...)
{
}

struct lsblk_context {
	int (*func)(blk64_t	blknum, void *priv_data);
	void *priv_data;
};

static int list_blocks_proc(ext2_filsys fs EXT2FS_ATTR((unused)),
			    blk64_t *blocknr, e2_blkcnt_t blockcnt,
			    blk64_t ref_block EXT2FS_ATTR((unused)),
			    int ref_offset EXT2FS_ATTR((unused)),
			    void *private)
{
	struct lsblk_context *lb = (struct lsblk_context *) private;

	if (*blocknr != 0 && blockcnt >= 0) {
		lb->func(*blocknr, lb->priv_data);
		return 0;
	}
	return 0;
}


static errcode_t dump_blocks(ext2_filsys current_fs, ext2_ino_t inode, 
	struct lsblk_context *lsblkc)
{
	return ext2fs_block_iterate3(current_fs, inode, BLOCK_FLAG_READ_ONLY, NULL,
			      list_blocks_proc, (void *)lsblkc);
}

#define DUMP_LEAF_EXTENTS	0x01
#define DUMP_NODE_EXTENTS	0x02

static errcode_t dump_extents(ext2_filsys current_fs, ext2_ino_t ino,
			 int flags, struct lsblk_context *lsblkc)
{
	ext2_extent_handle_t	handle;
	struct ext2fs_extent	extent;
	struct ext2_extent_info info;
	int			op = EXT2_EXTENT_ROOT;
	unsigned int		printed = 0;
	errcode_t 		errcode;
	__u32 i;

	errcode = ext2fs_extent_open(current_fs, ino, &handle);
	if (errcode)
		return errcode;

	while (1) {
		errcode = ext2fs_extent_get(handle, op, &extent);

		if (errcode)
			break;

		op = EXT2_EXTENT_NEXT;

		if (extent.e_flags & EXT2_EXTENT_FLAGS_SECOND_VISIT)
			continue;

		if (extent.e_flags & EXT2_EXTENT_FLAGS_LEAF) {
			if ((flags & DUMP_LEAF_EXTENTS) == 0)
				continue;
		} else {
			if ((flags & DUMP_NODE_EXTENTS) == 0)
				continue;
		}

		errcode = ext2fs_extent_get_info(handle, &info);
		if (errcode)
			continue;

		if (!(extent.e_flags & EXT2_EXTENT_FLAGS_LEAF)) {
			if (extent.e_flags & EXT2_EXTENT_FLAGS_SECOND_VISIT)
				continue;

			continue;
		}

		if (extent.e_len == 0)
			continue;
		else if (extent.e_len == 1)
			lsblkc->func(extent.e_pblk, lsblkc->priv_data);
		else {
			for (i=0; i<extent.e_len; i++) {
				lsblkc->func(extent.e_pblk + i, lsblkc->priv_data);
			}
		}
	}
	ext2fs_extent_free(handle);
	return 0;
}

int lsblks(const char *dev, ext2_ino_t inode_num, 
	int (*func)(blk64_t	blknum, void *priv_data),
	void *priv_data)
{
	int	retval;
	int		open_flags = EXT2_FLAG_SOFTSUPP_FEATURES |
				EXT2_FLAG_64BITS | EXT2_FLAG_THREADS | EXT2_FLAG_IGNORE_CSUM_ERRORS;
	blk64_t		superblock = 0;
	blk64_t		blocksize = 0;
	io_manager io_ptr = unix_io_manager;
 	struct ext2_inode inode;
	ext2_filsys	fs;
	struct lsblk_context lsblkc;

	lsblkc.func = func;
	lsblkc.priv_data = priv_data;

	retval = ext2fs_open(dev, open_flags, superblock, blocksize,
			     io_ptr, &fs);
	if (retval ) {
		return retval;
	}

	retval = ext2fs_read_inode(fs, inode_num, &inode);
	if (retval) {
		return retval;
	}

	if (inode.i_flags & EXT4_EXTENTS_FL) {
		dump_extents(fs, inode_num,
					DUMP_LEAF_EXTENTS|DUMP_NODE_EXTENTS,
					&lsblkc);
	}
	else if (inode.i_flags & EXT4_INLINE_DATA_FL) {
	}
	else 
	{
		dump_blocks(fs, inode_num, &lsblkc);
	}
	ext2fs_close_free(&fs);

	return 0;
}

#ifdef BUILD_lsblks

static
int lsb_cb(blk64_t	blknum, void *priv_data)
{
	printf("%llu ", (unsigned long long)blknum);
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf("%s <disk-device> <inode-number>\n", argv[0]);
		return 2;
	}
	int	retval;
	lsblks(argv[1], strtoul(argv[2], NULL, 0), lsb_cb, NULL);
	printf("\n");
	return 0;
}

#endif
