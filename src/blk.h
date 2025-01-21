#ifndef ATRK_blk
#define ATRK_blk
#include "utils.h"


struct DeviceInfo
{
    char name[128];
    char dev[128];
	char md[128];
    char maj_min[32];
    char type[32];
    char fstype[32];
};


/**
 * sysfs_devname_sys_to_dev:
 * @name: devname to be converted in place
 *
 * Linux kernel linux/drivers/base/core.c: device_get_devnode()
 * defines a replacement of '!' in the /sys device name by '/' in the
 * /dev device name. This helper replaces all occurrences of '!' in
 * @name by '/' to convert from /sys to /dev.
 */
static inline void sysfs_devname_sys_to_dev(char *name)
{
	char *c;

	if (name)
		while ((c = strchr(name, '!')))
			c[0] = '/';
}

/**
 * sysfs_devname_dev_to_sys:
 * @name: devname to be converted in place
 *
 * See sysfs_devname_sys_to_dev().
 */
static inline void sysfs_devname_dev_to_sys(char *name)
{
	char *c;

	if (name)
		while ((c = strchr(name, '/')))
			c[0] = '!';
}

char *sysfs_blkdev_get_path(dev_t devno, char *buf, size_t bufsiz);
int sysfs_blkdev_from_path(const char *path, dev_t *devno);
int get_blkdev_fstype(dev_t devno, char *fstype, int fstsize);
int get_path_fstype(char *path, char *fstype, int fstsize);
int get_blkdev_type(dev_t devno, char *type, int tsize);
int get_blkdev_parent(dev_t devno, dev_t *parent, char *mdname, int mdnamelen);
int get_one_blk_info(dev_t devno, struct DeviceInfo *device);
int myLsblk(dev_t devno, struct DeviceInfo *device, struct DeviceInfo *parent);

// //依赖lsblk命令来获取设备信息和父设备，
// int scan_lsblk_val(const char *buf, const char *name, char *out);
// int parseLsblk_checkparent(const char *maj_min, const char *maj_min_parent);
// int parseLsblkOutput(dev_t devno, struct DeviceInfo *device, struct DeviceInfo *parent);

int lvm_parse_pvs_pestart(const char *dev, uint64_t *pe_start);
int lvm_parse_volume_range(const char *dev, const char *volume, uint64_t *pe_start, uint64_t *pe_end, uint64_t *pe_size);

int read_1st(const char *path, char *buf, int bufsize);

int ext_parse_volume_range(const char *dev, const char *volume, uint64_t *sector_start, uint64_t *sector_end);

int xfs_get_path_extents(const char *path, struct Vector* pblocks, size_t *pcount);

int ext_get_inode_datablocks(const char *dev, ino_t ino, struct Vector* pblocks, size_t *pcount);
int ext_get_path_datablocks(const char *dev, const char *path, struct Vector* pblocks, size_t *pcount);

#endif /* !ATRK_blk */
