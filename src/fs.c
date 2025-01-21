#include "pred.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include "fs.h"
#include "utils.h"

int parse_ext2_dir(char *buf, uint32_t datalen, struct Vector* files)
{
	char *p, *end, *cp;
	struct ext2_dir_entry_2 *entry;
    struct my_dir_entry *entrycp;
	unsigned int rec_len;
    uint32_t inode;
	int id;
    //int len;

	end = buf + datalen;
	for (p = buf; p < end-8; p += rec_len) {
		entry = (struct ext2_dir_entry_2 *) p;
		rec_len = entry->rec_len;
        inode = entry->inode;
        if (isBigEndian()) {
            rec_len = ext2fs_swab16(rec_len);
            inode = ext2fs_swab32(inode);
        }

		if (rec_len == EXT4_MAX_REC_LEN || rec_len == 0)
			break;
		else 
			rec_len = (rec_len & 65532) | ((rec_len & 3) << 16);

		if (rec_len < 8 || (rec_len % 4) || (p+rec_len > end)) {
			break;
		}
		if (entry->name_len + 8U > rec_len) {
			break;
		}
        if (entry->inode == 0) {
            continue;
        }
		cp = p+8;
		if (entry->name_len==1 && cp[0] == '.')
			continue;
		if (entry->name_len==2 && cp[0] == '.' && cp[1] == '.')
			continue;
        if (entry->name_len==0)
            continue;
        entrycp = (struct my_dir_entry *) malloc(sizeof(struct my_dir_entry)+entry->name_len);
        entrycp->inode = inode;
        entrycp->file_type = entry->file_type;
        entrycp->name_len = entry->name_len;
        memcpy(entrycp->name, entry->name, entry->name_len);
        entrycp->name[entry->name_len] = '\0';
        entrycp->myflags = 0;
        vector_pushBackp(files, entrycp);
	}
    return 0;
}


int parse_xfs_dir(char *buf, uint32_t datalen, struct Vector* files)
{
    char *p, *end, *cp;
    struct xfs_dir3_data_hdr *hdr = (struct xfs_dir3_data_hdr *)buf;
    xfs_dir2_data_entry_t *entry;
    xfs_dir2_data_unused_t *unused;
    struct my_dir_entry *entrycp;
    unsigned int rec_len;
    __be64 inode;
    __be16 unused_length;
    __be32 magic = from_be32(hdr->hdr.magic);
    int hdrlen;
    int multiblock = 0;

    switch (magic) {
        case XFS_DIR3_BLOCK_MAGIC:
            hdrlen = 0x40;
            break;
        case XFS_DIR3_DATA_MAGIC:
            hdrlen = 0x40;
            multiblock = 1;
            break;
        case XFS_DIR2_BLOCK_MAGIC:
            hdrlen = 0x10;
            break;
        case XFS_DIR2_DATA_MAGIC:
            hdrlen = 0x10;
            multiblock = 1;
            break;
        default:
        return 1;
    }

    end = buf + datalen;
    for (p = buf+hdrlen; p < end-9; p += rec_len) {
        hdr = (struct xfs_dir3_data_hdr *) p;
        magic = from_be32(hdr->hdr.magic);
        entry = (xfs_dir2_data_entry_t *) p;
        unused = (xfs_dir2_data_unused_t *) p;
        if (multiblock && magic == XFS_DIR2_DATA_MAGIC) {
            rec_len = 0x10;
            continue;
        }
        else if (multiblock && magic == XFS_DIR3_DATA_MAGIC) {
            rec_len = 0x40;
            continue;
        }
        else if (unused->freetag == XFS_DIR2_DATA_FREE_TAG) {
            rec_len = from_be16(unused->length);
            continue;
        }else{
            inode = from_be64(entry->inumber);
            rec_len = 8 + 1 + entry->namelen + 3;
            rec_len = (rec_len+7)&(~7);
        }
        cp = p+9;
        if (entry->namelen==1 && cp[0] == '.')
            continue;
        if (entry->namelen==2 && cp[0] == '.' && cp[1] == '.')
            continue;
        if (entry->namelen==0)
            break;
        entrycp = (struct my_dir_entry *) malloc(sizeof(struct my_dir_entry)+entry->namelen);
        entrycp->inode = inode;
        entrycp->file_type = entry->name[entry->namelen];
        entrycp->name_len = entry->namelen;
        memcpy(entrycp->name, entry->name, entry->namelen);
        entrycp->name[entry->namelen] = '\0';
        entrycp->myflags = 0;
        vector_pushBackp(files, entrycp);
    }
    return 0;
}

int do_dir(const char *path, struct Vector* files)
{
    struct my_dir_entry *entrycp;
    size_t name_len;
    DIR* directory = opendir(path);
    if (directory == NULL) {
        return 1;
    }
    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        name_len = strlen(entry->d_name);
        entrycp = (struct my_dir_entry *) malloc(sizeof(struct my_dir_entry)+name_len);
        entrycp->inode = entry->d_ino;
        entrycp->file_type = entry->d_type;
        entrycp->name_len = (uint8_t)name_len;
        memcpy(entrycp->name, entry->d_name, name_len);
        entrycp->name[name_len] = '\0';
        entrycp->myflags = 0;
        vector_pushBackp(files, entrycp);
    }
    closedir(directory);
    return 0;
}

