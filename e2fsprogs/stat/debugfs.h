/*
 * debugfs.h --- header file for the debugfs program
 */

#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"

#ifdef __STDC__
#define NOARGS void
#else
#define NOARGS
#define const
#endif


int lsblks(const char *dev, ext2_ino_t inode_num, 
	int (*func)(blk64_t	blknum, void *priv_data),
	void *priv_data);
