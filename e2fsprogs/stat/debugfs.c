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

struct list_blocks_struct {
	FILE		*f;
	e2_blkcnt_t	total;
	blk64_t		first_block, last_block;
	e2_blkcnt_t	first_bcnt, last_bcnt;
	e2_blkcnt_t	first;
};

static void finish_range(struct list_blocks_struct *lb)
{
	if (lb->first_block == 0)
		return;
	if (lb->first)
		lb->first = 0;
	else
		fprintf(lb->f, ", ");
	if (lb->first_block == lb->last_block)
		fprintf(lb->f, "(%lld):%llu",
			(long long)lb->first_bcnt,
			(unsigned long long) lb->first_block);
	else
		fprintf(lb->f, "(%lld-%lld):%llu-%llu",
			(long long)lb->first_bcnt, (long long)lb->last_bcnt,
			(unsigned long long) lb->first_block,
			(unsigned long long) lb->last_block);
	lb->first_block = 0;
}

static int list_blocks_proc(ext2_filsys fs EXT2FS_ATTR((unused)),
			    blk64_t *blocknr, e2_blkcnt_t blockcnt,
			    blk64_t ref_block EXT2FS_ATTR((unused)),
			    int ref_offset EXT2FS_ATTR((unused)),
			    void *private)
{
	struct list_blocks_struct *lb = (struct list_blocks_struct *) private;

	lb->total++;
	if (blockcnt >= 0) {
		/*
		 * See if we can add on to the existing range (if it exists)
		 */
		if (lb->first_block &&
		    (lb->last_block+1 == *blocknr) &&
		    (lb->last_bcnt+1 == blockcnt)) {
			lb->last_block = *blocknr;
			lb->last_bcnt = blockcnt;
			return 0;
		}
		/*
		 * Start a new range.
		 */
		finish_range(lb);
		lb->first_block = lb->last_block = *blocknr;
		lb->first_bcnt = lb->last_bcnt = blockcnt;
		return 0;
	}
	/*
	 * Not a normal block.  Always force a new range.
	 */
	finish_range(lb);
	if (lb->first)
		lb->first = 0;
	else
		fprintf(lb->f, ", ");
	if (blockcnt == -1)
		fprintf(lb->f, "(IND):%llu", (unsigned long long) *blocknr);
	else if (blockcnt == -2)
		fprintf(lb->f, "(DIND):%llu", (unsigned long long) *blocknr);
	else if (blockcnt == -3)
		fprintf(lb->f, "(TIND):%llu", (unsigned long long) *blocknr);
	return 0;
}


static void dump_blocks(ext2_filsys current_fs, FILE *f, const char *prefix, ext2_ino_t inode)
{
	struct list_blocks_struct lb;

	fprintf(f, "%sBLOCKS:\n%s", prefix, prefix);
	lb.total = 0;
	lb.first_block = 0;
	lb.f = f;
	lb.first = 1;
	ext2fs_block_iterate3(current_fs, inode, BLOCK_FLAG_READ_ONLY, NULL,
			      list_blocks_proc, (void *)&lb);
	finish_range(&lb);
	if (lb.total)
		fprintf(f, "\n%sTOTAL: %lld\n", prefix, (long long)lb.total);
	fprintf(f,"\n");
}

#define DUMP_LEAF_EXTENTS	0x01
#define DUMP_NODE_EXTENTS	0x02
#define DUMP_EXTENT_TABLE	0x04

static void dump_extents(ext2_filsys current_fs, FILE *f, const char *prefix, ext2_ino_t ino,
			 int flags, int logical_width, int physical_width)
{
	ext2_extent_handle_t	handle;
	struct ext2fs_extent	extent;
	struct ext2_extent_info info;
	int			op = EXT2_EXTENT_ROOT;
	unsigned int		printed = 0;
	errcode_t 		errcode;

	errcode = ext2fs_extent_open(current_fs, ino, &handle);
	if (errcode)
		return;

	if (flags & DUMP_EXTENT_TABLE)
		fprintf(f, "Level Entries %*s %*s Length Flags\n",
			(logical_width*2)+3, "Logical",
			(physical_width*2)+3, "Physical");
	else
		fprintf(f, "%sEXTENTS:\n%s", prefix, prefix);

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

			if (flags & DUMP_EXTENT_TABLE) {
				fprintf(f, "%2d/%2d %3d/%3d %*llu - %*llu "
					"%*llu%*s %6u\n",
					info.curr_level, info.max_depth,
					info.curr_entry, info.num_entries,
					logical_width,
					(unsigned long long) extent.e_lblk,
					logical_width,
					(unsigned long long) extent.e_lblk + (extent.e_len - 1),
					physical_width,
					(unsigned long long) extent.e_pblk,
					physical_width+3, "", extent.e_len);
				continue;
			}

			fprintf(f, "%s(ETB%d):%llu",
				printed ? ", " : "", info.curr_level,
				(unsigned long long) extent.e_pblk);
			printed = 1;
			continue;
		}

		if (flags & DUMP_EXTENT_TABLE) {
			fprintf(f, "%2d/%2d %3d/%3d %*llu - %*llu "
				"%*llu - %*llu %6u %s\n",
				info.curr_level, info.max_depth,
				info.curr_entry, info.num_entries,
				logical_width,
				(unsigned long long) extent.e_lblk,
				logical_width,
				(unsigned long long) extent.e_lblk + (extent.e_len - 1),
				physical_width,
				(unsigned long long) extent.e_pblk,
				physical_width,
				(unsigned long long) extent.e_pblk + (extent.e_len - 1),
				extent.e_len,
				extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT ?
					"Uninit" : "");
			continue;
		}

		if (extent.e_len == 0)
			continue;
		else if (extent.e_len == 1)
			fprintf(f,
				"%s(%lld%s):%lld",
				printed ? ", " : "",
				(unsigned long long) extent.e_lblk,
				extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT ?
				"[u]" : "",
				(unsigned long long) extent.e_pblk);
		else
			fprintf(f,
				"%s(%lld-%lld%s):%lld-%lld",
				printed ? ", " : "",
				(unsigned long long) extent.e_lblk,
				(unsigned long long) extent.e_lblk + (extent.e_len - 1),
				extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT ?
					"[u]" : "",
				(unsigned long long) extent.e_pblk,
				(unsigned long long) extent.e_pblk + (extent.e_len - 1));
		printed = 1;
	}
	if (printed)
		fprintf(f, "\n");
	ext2fs_extent_free(handle);
}

static void dump_inline_data(ext2_filsys current_fs, FILE *out, const char *prefix, ext2_ino_t inode_num)
{
	errcode_t retval;
	size_t size;

	retval = ext2fs_inline_data_size(current_fs, inode_num, &size);
	if (!retval)
		fprintf(out, "%sSize of inline data: %zu\n", prefix, size);
}


void main(int argc, char *argv[])
{
	int	retval;
	int		open_flags = EXT2_FLAG_SOFTSUPP_FEATURES |
				EXT2_FLAG_64BITS | EXT2_FLAG_THREADS | EXT2_FLAG_IGNORE_CSUM_ERRORS;
	blk64_t		superblock = 0;
	blk64_t		blocksize = 0;
	io_manager io_ptr = unix_io_manager;
	ext2_ino_t	inode_num;
 	struct ext2_inode inode;
	ext2_filsys	fs;
	if (argc != 3) {
		printf("%s <disk-device> <inode-number>\n", argv[0]);
		return;
	}
	retval = ext2fs_open(argv[1], open_flags, superblock, blocksize,
			     io_ptr, &fs);
	if (retval ) {
		fprintf(stderr, "ext2fs_open: %s: error: 0x%.8X\n", argv[1], retval);
		return;
	}

	inode_num = strtoul(argv[2], NULL, 0);
	retval = ext2fs_read_inode(fs, inode_num, &inode);
	if (retval) {
		fprintf(stderr, "ext2fs_read_inode: %s: error: 0x%.8X\n", argv[1], retval);
		return;
	}

	FILE *out = stdout;
	const char *prefix = "";

	if (inode.i_flags & EXT4_EXTENTS_FL)
		dump_extents(fs, out, prefix, inode_num,
					DUMP_LEAF_EXTENTS|DUMP_NODE_EXTENTS, 0, 0);
	else if (inode.i_flags & EXT4_INLINE_DATA_FL)
		dump_inline_data(fs, out, prefix, inode_num);
	else 
	{
		dump_blocks(fs, out, prefix, inode_num);
	}
	ext2fs_close_free(&fs);
}
