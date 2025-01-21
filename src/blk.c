#include "pred.h"
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/vfs.h>
#include <dirent.h>
#include "blk.h"
#include "utils.h"
#ifdef USE_e2fsprogs
#include "../e2fsprogs/stat/debugfs.h"
#endif

int access_file(const char *dir, const char *name)
{
    char path[256];
    snprintf(path, sizeof(path)-1, "%s/%s", dir, name);

    return access(path, F_OK);
}

static
int ingore_dev_dir(const char *d_name) {
    if (strcmp(d_name, "subsystem") == 0
        || strcmp(d_name, "holders") == 0
        || strcmp(d_name, "slaves") == 0
        || strcmp(d_name, "integrity") == 0
        || strcmp(d_name, "mq") == 0
        || strcmp(d_name, "power") == 0
        || strcmp(d_name, "queue") == 0
        || strcmp(d_name, "trace") == 0
    ) {
        return 1;
    }
    return 0;
}
static
int searchDevDirs(const char *majmin, char *foundpath, const char *basePath, int followlink) {
    DIR *dir;
    struct dirent *entry;
    char path[256];
    int found = 0;
    //printf("searchDevDirs(%d): %s\n", followlink, basePath);
    if ((dir = opendir(basePath)) == NULL) {
        //perror("opendir");
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        //snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name);
        path_join(basePath, entry->d_name, path, sizeof(path));

        struct stat statbuf;
        if (lstat(path, &statbuf) == -1) {
            //perror("lstat");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (ingore_dev_dir(entry->d_name)) {
                continue;
            }
            if (searchDevDirs(majmin, foundpath, path, followlink)) {
                found = 1;
                break;
            }
        } else if (S_ISLNK(statbuf.st_mode) && followlink) {
            if (ingore_dev_dir(entry->d_name)) {
                continue;
            }            
            if (searchDevDirs(majmin, foundpath, path, 0)) {
                found = 1;
                break;
            }
        } else if (S_ISREG(statbuf.st_mode)) {
            if (strcmp(entry->d_name, "dev") == 0) {
                //fprintf(stderr, "DEV: %s\n", path);
                FILE *file = fopen(path, "r");
                if (file != NULL) {
                    char content[128];
                    char *text;
                    if (fgets(content, sizeof(content), file) != NULL) {
                        text = strtok(content, "\r\n");
                        if (text && strcmp(text, majmin) == 0) {
                            snprintf(foundpath, 256, "%s", basePath);
                            found = 1;
                        }
                    }
                    fclose(file);
                }
                if (found)
                    break;
            }
        }
    }
    closedir(dir);
    return found;
}

char *find_devmm_block_path(char *majmin,  char *buf, size_t bufsiz)
{
    char devpath[256] = "";
    char devmm[256];
    /* read /sys/dev/block/<maj:min> link */
    if (access("/sys/dev/block", F_OK) == 0) {
        snprintf(devmm, sizeof(devmm)-1, "/sys/dev/block/%s", majmin);
        if (access(devmm, F_OK) == 0) {
            if (NULL == realpath(devmm, devpath)){
                return NULL;
            }
            if (strstr(devpath, "/block/")) {
                snprintf(buf, bufsiz-1, "%s", devpath);
                return buf;
            }
        }
    }
    if (!searchDevDirs(majmin, devpath, "/sys/block", 1)) {
        //fprintf(stderr, "ERROR find_devmm_block_path: %s\n", majmin);
        return NULL;
    }
    //fprintf(stderr, "OK find_devmm_block_path: %s\n", devpath);

    snprintf(buf, bufsiz-1, "%s", devpath);
	return buf;
}

char *find_dev_block_path(dev_t devno,  char *buf, size_t bufsiz)
{
    char majmin[32];

    snprintf(majmin, sizeof(majmin)-1, "%d:%d", major(devno), minor(devno));
    return find_devmm_block_path(majmin, buf, bufsiz);
}

char *sysfs_blkdev_get_path(dev_t devno,  char *buf, size_t bufsiz)
{
    char devpath[256] = "";
    char majmin[32];
    char devmm[256];
	char *name;

    if (!find_dev_block_path(devno, devpath, 256)) {
        return NULL;
    }
	name = strrchr(devpath, '/');
	if (!name)
		return NULL;
	name++;
    snprintf(buf, bufsiz-1, "/dev/%s", name);
	sysfs_devname_sys_to_dev(buf);
	return buf;    
}

// char *sysfs_blkdev_get_path(dev_t devno, char *buf, size_t bufsiz)
// {
// 	char link[256];
//     char majmin[32];
//     char devmm[256];
// 	char *name;

//     snprintf(majmin, sizeof(majmin)-1, "%d:%d", major(devno), minor(devno));
//     /* read /sys/dev/block/<maj:min> link */
//     snprintf(devmm, sizeof(devmm)-1, "/sys/dev/block/%s", majmin);
//     link[0] = 0;
// 	if (NULL == realpath(devmm, link)){
//         return NULL;
//     }
// 	name = strrchr(link, '/');
// 	if (!name)
// 		return NULL;
// 	name++;
//     snprintf(buf, bufsiz-1, "/dev/%s", name);
// 	sysfs_devname_sys_to_dev(buf);
// 	return buf;
// }

int sysfs_blkdev_from_path(const char *path, dev_t *devno)
{
    int ret;
	struct stat st;
    ret = stat(path, &st);
    if (ret == 0) {
        if (S_ISBLK(st.st_mode)) {
            if (strncmp(path, "/dev/", 5) == 0) {
                *devno = st.st_rdev;
            }else{
                *devno = st.st_dev;
            }
            return 0;
        }else{
            return 1;
        }
    }else{
        return ret;
    }
}

int devno_from_path(const char *path, dev_t *devno)
{
    int ret;
	struct stat st;
    ret = stat(path, &st);
    if (ret == 0) {
        *devno = st.st_dev;
        return 0;
    }else{
        return ret;
    }
}

int majmin_to_dev_t(const char *maj_min, dev_t *result) {
    char *endptr;
    int imaj, imin;
    imaj = strtoul(maj_min, &endptr, 10);
    if (*endptr == ':') {
        endptr++;
        imin = strtoul(endptr, NULL, 10);
        *result = makedev(imaj, imin);
        return 0;
    }
    return 1;
}

static
int get_blkdev_fstype_from_mountinfo(dev_t devno, char *fstype, int fstsize)
{
    int ret;
    FILE *f;
    char sline[512+1] = {0};
    char majmin[128];
    fstype[0] = '\0';
    snprintf(fstype, fstsize, "unknown");
    sprintf(majmin, " %d:%d ", major(devno), minor(devno));
    f = fopen("/proc/self/mountinfo", "r");

    if (f == NULL)
    {
        return -1;
    }

    ret = -1;
    while (fgets(sline, sizeof(sline)-1, f))
    {
        char *token;
        char *where;
        char *token_fstype;

        token = strtok(sline, "-");
        where = strstr(token, majmin);
        if (where)
        {
            token_fstype = strtok(NULL, " ");
            if (token_fstype) {
                strncpy(fstype, token_fstype, fstsize);
                toLowerCase(fstype);
            }
            if (token_fstype)
            {
                ret = 0;
                break;
            }
        }
    }

    fclose(f);
    return ret;
}

static
int get_blkdev_fstype_from_mounts(dev_t devno, char *fstype, int fstsize)
{
    int ret;
    FILE *file;
    char line[512+1] = {0};

    file = fopen("/proc/self/mounts", "r");

    if (file == NULL)
    {
        return -1;
    }

    ret = -1;
    while (fgets(line, sizeof(line)-1, file) != NULL) {
        char *devPath = strtok(line, " ");
        if (!devPath || devPath[0] != '/') {
            continue;
        }
        char *mountPath = strtok(NULL, " ");
        if (!mountPath) {
            continue;
        }        
        char *_fstype = strtok(NULL, " ");
        if (!_fstype) {
            continue;
        }
        dev_t mdevno;
        if (devno_from_path(mountPath, &mdevno) != 0) {
            continue;
        }
        if (devno == mdevno) {
            strncpy(fstype, _fstype, fstsize);
            toLowerCase(fstype);
            ret = 0;
            break;
        }
    }

    fclose(file);
    return ret;
}


int get_blkdev_fstype(dev_t devno, char *fstype, int fstsize)
{
    if (access_file("/proc/self", "mountinfo") == 0) {
        return get_blkdev_fstype_from_mountinfo(devno, fstype, fstsize);
    }

    if (access_file("/proc/self", "mounts") == 0) {
        return get_blkdev_fstype_from_mounts(devno, fstype, fstsize);
    }
    return -1;
}

int get_path_fstype(char *path, char *fstype, int fstsize)
{
    struct stat fs;
    if (stat(path, &fs) < 0)
    {
        return -1;
    }
    
    if (S_ISBLK(fs.st_mode) && strncmp(path, "/dev/", 5) == 0) {
        return get_blkdev_fstype(fs.st_rdev, fstype, fstsize);
    }else{
        return get_blkdev_fstype(fs.st_dev, fstype, fstsize);
    }
}

int find_dir_startswith(const char *dirpath, const char *subdirname, const char *starts, char* buf, int bufsize)
{
    DIR *dir;
    struct dirent *entry;
    char path[512];
    char path2[512];
    int slen = (int)strlen(starts);

    path_join(dirpath, subdirname, path, sizeof(path));

    if ((dir = opendir(path)) == NULL)
        return 2;
    else {
        while ((entry = readdir(dir)) != NULL) {
            if (!strcmp(entry->d_name, ".") ||
                !strcmp(entry->d_name, ".."))
                continue;
            if (strncmp(entry->d_name, starts, slen) == 0) {
                struct stat statbuf;
                path_join(path, entry->d_name, path2, sizeof(path2));
                if (stat(path2, &statbuf) == -1) {
                    continue;
                }
                if (S_ISDIR(statbuf.st_mode)) {
                    strncpy(buf, entry->d_name, bufsize);
                    return 0;
                } 
            }
        }
    }
    return 4;
}

int get_blkdev_type(dev_t devno, char *type, int tsize)
{
	char link[256];
    char majmin[32];
    char blockpath[256];
    char path[256];
    char uuid[256];
    char rdname[16];
	char *name;
    char *token;

    snprintf(type, tsize, "unknown");
    snprintf(majmin, sizeof(majmin)-1, "%d:%d", major(devno), minor(devno));
    
    if (!find_devmm_block_path(majmin, blockpath, sizeof(blockpath)-1)) {
        return 1;
    }
    snprintf(path, sizeof(path)-1, "%s/partition", blockpath);
    if (access(path, F_OK) == 0) {
        snprintf(type, tsize, "part");
        toLowerCase(type);
        return 0;
    }
    snprintf(path, sizeof(path)-1, "%s/dm/uuid", blockpath);
    if (access(path, F_OK) == 0)
    {
        FILE *fp = fopen(path, "r");
        if (fp == NULL) {
            return 1;
        }
        uuid[0] = '\0';
        fgets(uuid, sizeof(uuid)-1, fp);
        fclose(fp);
        token = strchr(uuid, '-');
        if (!token) {
            return 1;
        }
        *token = '\0';
        if (strlen(uuid) > 8) {
            return 1;
        }
        snprintf(type, tsize, "%s", uuid);
        toLowerCase(type);
        return 0;
    }
    if (access_file(blockpath, "start")==0 
    && access_file(blockpath, "size")==0
    && access_file(blockpath, "range")!=0
    && access_file(blockpath, "removable")!=0) {
        snprintf(type, tsize, "part");
        toLowerCase(type);
        return 0;
    }
    memset(rdname, 0, sizeof(rdname));
    if (find_dir_startswith(blockpath, "md", "rd", rdname, sizeof(rdname)-1) == 0) {
        snprintf(path, sizeof(path)-1, "%s/md/%s/block/partition", blockpath, rdname);
        if (access(path, F_OK) == 0)
        {
            snprintf(type, tsize, "raid");
            toLowerCase(type);
            return 0;
        }
    }
    return 1;
}

int get_blkdev_parent(dev_t devno, dev_t *parent, char *mdname, int mdnamelen)
{
    char majmin[32];
    char blockpath[256];
    char path[256];
	char *name;
    char *blockroot;
    char *pnode[32] = {0};
    int sznode = 0;
    int i;
    char *token;

    snprintf(majmin, sizeof(majmin)-1, "%d:%d", major(devno), minor(devno));
    if (!find_devmm_block_path(majmin, blockpath, sizeof(blockpath)-1)) {
        return 1;
    }
    dbg(stderr, "dbg: get_blkdev_parent, find_devmm_block_path majmin=%s, %s\n", majmin, blockpath);

    snprintf(path, sizeof(path)-1, "%s", blockpath);
    blockroot = strstr(path, "/block/");
    if (blockroot) {
        blockroot += 7;
        token = strtok(blockroot, "/");
        if (token) {
            do {
                if (sznode >= sizeof(pnode)/sizeof(pnode[0])) {
                    return 5;
                }
                dbg(stderr, "dbg: pnode[%d]=%s\n", sznode, token);
                pnode[sznode++] = token;
            }while (token = strtok(NULL, "/"));
        }
    }

    if (sznode > 1) {
        snprintf(path, sizeof(path)-1, "/dev/%s", pnode[sznode-2]);
        dbg(stderr, "dbg: got parent pnode[%d]=%s\n", sznode-2, pnode[sznode-2]);
        if (0 == sysfs_blkdev_from_path(path, parent)){
            dbg(stderr, "dbg: parent=%d:%d\n", major(*parent), minor(*parent));
            return 0;
        }
        dbg(stderr, "dbg: get parent failed\n");
    }else{
        DIR *dir;
        struct dirent *entry;
        int filecount = 0;
        char slave[128] = { 0 };
        snprintf(path, sizeof(path)-1, "%s/slaves/", blockpath);
        if ((dir = opendir(path)) == NULL)
            return 2;
        else {
            while ((entry = readdir(dir)) != NULL) {
                if (!strcmp(entry->d_name, ".") ||
                    !strcmp(entry->d_name, ".."))
                    continue;
                filecount++;
                strncpy(slave, entry->d_name, sizeof(slave));
            }
            closedir(dir);
            if (filecount == 0) {
                return 2;
            }
            if (blockroot && !strncmp(blockroot, "md", 2)) {
                memset(majmin, 0, sizeof(majmin));
                snprintf(path, sizeof(path)-1, "%s/slaves/%s/../dev", blockpath, slave);
                if (0 != read_1st(path, majmin, sizeof(majmin))) {
                    return 3;
                }
                if (0 != majmin_to_dev_t(majmin, parent)) {
                    return 3;
                }
                snprintf(mdname, mdnamelen-1, "/dev/%s", slave);
                return 0;
            }
            if (filecount != 1) {
                return 4;
            }
            snprintf(path, sizeof(path)-1, "/dev/%s", slave);
            if (0 == sysfs_blkdev_from_path(path, parent)){
                return 0;
            }
        }
    }
    return 1;
}

int get_one_blk_info(dev_t devno, struct DeviceInfo *device)
{
    int ret;
    char devpath[256];

    if (!sysfs_blkdev_get_path(devno, devpath, sizeof(devpath))){
        dbg(stderr, "dbg: get_one_blk_info:sysfs_blkdev_get_path(devno=%d:%d)\n", major(devno), minor(devno));
        return 1;
    }
    strncpy(device->name, strrchr(devpath, '/')+1, sizeof(device->name));
    strncpy(device->dev, devpath, sizeof(device->dev));
    snprintf(device->maj_min, sizeof(device->maj_min)-1, "%d:%d", major(devno), minor(devno));
    get_blkdev_fstype(devno, device->fstype, sizeof(device->fstype));
    get_blkdev_type(devno, device->type, sizeof(device->type));
    return 0;
}

int myLsblk(dev_t devno, struct DeviceInfo *device, struct DeviceInfo *parent)
{
    int ret;
    dev_t devno_parent;

    ret = get_one_blk_info(devno, device);
    if (ret != 0) {
        dbg(stderr, "dbg: myLsblk:get_one_blk_info1(devno_parent=%d:%d): ret=%d\n", major(devno), minor(devno), ret);
        return ret;
    }
    ret = get_blkdev_parent(devno, &devno_parent, parent->md, sizeof(parent->md));
    if (ret != 0) {
        if (ret == 2) {
            //no parents
            return 0;
        }
        dbg(stderr, "dbg: myLsblk:get_blkdev_parent: ret=%d\n", ret);
        return ret;
    }
    ret = get_one_blk_info(devno_parent, parent);
    if (ret != 0) {
        dbg(stderr, "dbg: myLsblk:get_one_blk_info2(devno_parent=%d:%d): ret=%d\n", major(devno), minor(devno), ret);
        return ret;
    }
    return 0;
}

// //依赖lsblk命令来获取设备信息和父设备，
// int scan_lsblk_val(const char *buf, const char *name, char *out)
// {
//     char keye[64];
//     const char *token;
//     sprintf(keye, "%s=\"", name);
//     token = strstr(buf, keye);
//     if (!token)
//     {
//         return 2;
//     }
//     sscanf(token + strlen(keye) - 1, "\"%[^\"]\"", out);
//     return 0;
// }
//
// int parseLsblk_checkparent(const char *maj_min, const char *maj_min_parent)
// {
//     FILE *fp = popen("lsblk -s -o NAME,MAJ:MIN", "r");
//     if (fp == NULL)
//     {
//         return 2;
//     }
//     char buffer[1024];
//     char *token;
//     int matched = 0;
//     int sparent = 0;
//     while (fgets(buffer, sizeof(buffer), fp) != NULL)
//     {
//         buffer[strcspn(buffer, "\n")] = '\0';
//         while (token = strrchr(buffer, ' '))
//         {
//             *token = '\0';
//             token++;
//             if (*token && *token != ' ')
//             {
//                 break;
//             }
//         }
//         if (token)
//         {
//             if (sparent)
//             {
//                 if (strcmp(token, maj_min_parent) == 0 && strstr(buffer, "\xE2\x94\x94\xE2\x94\x80"))
//                 {
//                     matched++;
//                     break;
//                 }
//                 else
//                 {
//                     matched = 0;
//                     sparent = 0;
//                 }
//             }
//             if (matched == 0 && strcmp(token, maj_min) == 0)
//             {
//                 matched++;
//                 sparent = 1;
//                 continue;
//             }
//         }
//     }
//     pclose(fp);
//     if (matched != 2)
//     {
//         return 1;
//     }
//     return 0;
// }
//
// int parseLsblkOutput(dev_t devno, struct DeviceInfo *device, struct DeviceInfo *parent)
// {
//     FILE *fp = popen("lsblk -s -P -b -o NAME,KNAME,MAJ:MIN,TYPE,FSTYPE", "r");
//     if (fp == NULL)
//     {
//         return 2;
//     }
//
//     int ret = 1;
//     int matched = 0;
//     int sparent = 0;
//     char buffer[1024];
//     char tmp[100];
//     struct DeviceInfo di;
//     char maj_min[128];
//     sprintf(maj_min, "%d:%d", major(devno), minor(devno));
//     memset(device, 0, sizeof(*device));
//     memset(parent, 0, sizeof(*parent));
//     while (fgets(buffer, sizeof(buffer), fp) != NULL)
//     {
//         buffer[strcspn(buffer, "\n")] = '\0';
//
//         memset(&di, 0, sizeof(di));
//         scan_lsblk_val(buffer, "NAME", di.name);
//         memset(tmp, 0, sizeof(tmp));
//         scan_lsblk_val(buffer, "KNAME", tmp);
//         if (tmp[0])
//         {
//             sprintf(di.dev, "/dev/%s", tmp);
//         }
//         if (0 != scan_lsblk_val(buffer, "MAJ:MIN", di.maj_min))
//         {
//             scan_lsblk_val(buffer, "MAJ_MIN", di.maj_min);
//         }
//         scan_lsblk_val(buffer, "TYPE", di.type);
//         toLowerCase(di.type);
//         scan_lsblk_val(buffer, "FSTYPE", di.fstype);
//         toLowerCase(di.fstype);
//
//         if (sparent)
//         {
//             if (parseLsblk_checkparent(device->maj_min, di.maj_min) == 0)
//             {
//                 memcpy(parent, &di, sizeof(di));
//                 matched = 2;
//             }
//             break;
//         }
//         if (strcmp(di.maj_min, maj_min) == 0)
//         {
//             memcpy(device, &di, sizeof(di));
//             sparent = 1;
//             matched = 1;
//             continue;
//         }
//     }
//
//     ret = pclose(fp);
//     if (WIFEXITED(ret))
//     {
//         ret = WEXITSTATUS(ret);
//     }
//     else
//     {
//         ret = 1;
//     }
//     if (matched == 0)
//     {
//         ret = 1;
//     }
//     return ret;
// }

int lvm_parse_pvs_pestart(const char *dev, uint64_t *pe_start)
{
    char cmd_app[255];
    if (!find_command("pvs", cmd_app, sizeof(cmd_app))) {
        return 2;
    }

    char buffer[512];
    sprintf(buffer, "%s -o pe_start --units s %s", cmd_app, dev);

    FILE *fp = popen(buffer, "r");
    if (fp == NULL)
    {
        return 2;
    }

    char *token;
    int matched = 0;
    int nLine = 0;
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        buffer[strcspn(buffer, "\n")] = '\0';
        nLine++;
        token = trimch(buffer, ' ');
        if (nLine == 1)
        {
            if (strcmp(token, "1st PE") != 0)
            {
                break;
            }
            matched++;
        }
        if (nLine == 2)
        {
            *pe_start = strtoll(token, NULL, 10);
            matched++;
            break;
        }
    }
    pclose(fp);
    if (matched != 2)
    {
        return 1;
    }
    return 0;
}

int lvm_parse_volume_range(const char *dev, const char *volume, uint64_t *pe_start, uint64_t *pe_end, uint64_t *pe_size)
{
    char cmd_app[255];
    if (!find_command("pvdisplay", cmd_app, sizeof(cmd_app))) {
        return 2;
    }

    char buffer[512];
    sprintf(buffer, "%s -m --units s %s", cmd_app, dev);

    FILE *fp = popen(buffer, "r");
    if (fp == NULL)
    {
        return 2;
    }

    char *token;
    int matched = 0;
    int nLine = 0;
    int nLine_PE = -1;
    char szFrom[64] = "";
    char szTo[64] = "";
    char szPath[256] = "";

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        buffer[strcspn(buffer, "\n")] = '\0';
        nLine++;
        token = trimch(buffer, ' ');
        if (strncmp(token, "PE Size ", 8) == 0)
        {
            *pe_size = strtoll(trimch(token + 8, ' '), NULL, 10);
            matched++;
        }
        if (strncmp(token, "Physical extent ", 16) == 0)
        {
            token += 16;
            token = trimch(token, ' ');
            if (2 == sscanf(token, "%[0-9] to %[0-9]", szFrom, szTo))
            {
                nLine_PE = nLine;
            }
        }
        if (nLine == nLine_PE + 1 && strncmp(token, "Logical volume\t", 15) == 0)
        {
            token = realpath(trimch(token + 15, ' '), szPath);
            if (token && strcmp(token, volume) == 0)
            {
                *pe_start = strtoll(szFrom, NULL, 10);
                *pe_end = strtoll(szTo, NULL, 10);
                matched++;
                break;
            }
        }
    }
    pclose(fp);
    if (matched != 2)
    {
        return 1;
    }
    return 0;
}

int read_1st(const char *path, char *buf, int bufsize)
{
    buf[0] = 0;
    FILE *fp = fopen(path, "r");
    if (fp)
    {
        fgets(buf, bufsize, fp);
        fclose(fp);
    }
    return fp == NULL ? 2 : 0;
}

int ext_parse_volume_range(const char *dev, const char *volume, uint64_t *sector_start, uint64_t *sector_end)
{
    char buffer[1024];
    char tmpbuf[128];
    uint64_t tmpn;
    const char *dev_basename = dev + 5;
    const char *volume_basename = volume + 5;
    sprintf(buffer, "/sys/block/%s/%s/start", dev_basename, volume_basename);
    read_1st(buffer, tmpbuf, sizeof(tmpbuf));
    if (!tmpbuf[0])
    {
        return 1;
    }
    *sector_start = strtoll(tmpbuf, NULL, 10);
    sprintf(buffer, "/sys/block/%s/%s/size", dev_basename, volume_basename);
    read_1st(buffer, tmpbuf, sizeof(tmpbuf));
    if (!tmpbuf[0])
    {
        return 1;
    }
    tmpn = strtoll(tmpbuf, NULL, 10);
    *sector_end = *sector_start + tmpn - 1;
    return 0;
}

int xfs_get_path_extents(const char *path, struct Vector* pblocks, size_t *pcount)
{
    char cmd_app[255];
    char command[255];
    *pcount = 0;
    if (!find_command("xfs_bmap", cmd_app, sizeof(cmd_app))) {
        return 2;
    }
    
    snprintf(command, 255, "%s \"%s\"", cmd_app, path);
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        return 2;
    }
    int ret;
    char buffer[1024];
    char start[64];
    char end[64];
    uint64_t istart, iend;
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        char *token = (char *)strstr(buffer, "]:");
        if (token != NULL)
        {
            token += 2;
            token = trimch(token, ' ');
            int nssc = sscanf(token, "%[0-9]..%[0-9]", start, end);
            if (nssc == 2)
            {
                istart = strtoull(start, 0, 10);
                iend = strtoull(end, 0, 10);
                vector_pushBack(pblocks, -1);
                while (istart <= iend)
                {
                    vector_pushBack(pblocks, istart);
                    *pcount += 1;
                    istart++;
                }
            }
        }
    }

    ret = pclose(fp);
    if (WIFEXITED(ret))
    {
        ret = WEXITSTATUS(ret);
    }
    else
    {
        ret = 1;
    }
    return ret;
}

#ifdef USE_e2fsprogs

struct lsblk_priv {
	struct Vector* pblocks;
    size_t *pcount;
};

static
int lsb_cb(blk64_t	blknum, void *priv_data)
{
	struct lsblk_priv *lb = (struct lsblk_priv*)priv_data;

    if (*lb->pcount == 0) {
        vector_pushBack(lb->pblocks, -1);
    }
    
    vector_pushBack(lb->pblocks, blknum);
    *lb->pcount += 1;
}

int ext_get_inode_datablocks(const char *dev, ino_t ino, struct Vector* pblocks, size_t *pcount)
{
    struct lsblk_priv lb;
    *pcount = 0;
    lb.pblocks = pblocks;
    lb.pcount = pcount;
    lsblks(dev, ino, lsb_cb, &lb);
    return 0;
}

#else // USE_e2fsprogs

int ext_get_inode_datablocks(const char *dev, ino_t ino, struct Vector* pblocks, size_t *pcount)
{
    int pipefd[2];
    char cmd_app[255];
    char command[255];
    *pcount = 0;
    if (!find_command("debugfs", cmd_app, sizeof(cmd_app))) {
        return 2;
    }
    if (pipe(pipefd) == -1) {
        return 5;
    }
    snprintf(command, sizeof(command), "%s %s -R 'stat <%" PRIu64 ">' 2>&%d", cmd_app, dev, (uint64_t)ino, pipefd[1]);
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return 2;
    }

    char buffer[2048];
    char *token;
    int matched = 0;
    int nssc;
    char start[64];
    char end[64];
    uint64_t istart, iend;
    int nline = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        nline += 1;
        buffer[strcspn(buffer, "\n")] = '\0';
        if (matched == 1)
        {
            /*
            sample 1:
            (ETB0):3154771, (0):3153956, (1-8):3153968-3153975, (9):3154056, (10-11):3154762-3154763, (12):3154770
            sample 2:
            (0):3153956, (1-8):3153968-3153975, (9):3154056, (10-11):3154762-3154763, (12):3154770
            sample 3:
            (ETB0):3154771, (0):3153956, (1-8):3153968-3153975, (9):3154056, (ETB1):33796, (0-32767):34816-67583, (32768-63487):67584-98303, (63488-96255):100352-133119
            sample 4:
            (ETB0):3154771, (ETB1):3154773, (0):3153956, (1-8):3153968-3153975, (9):3154056
            */
            token = strtok(buffer, ",");
            if (!token)
            {
                break;
            }
            vector_pushBack(pblocks, -1);

            do
            {
                while (*token == ' ')
                    token++;
                if (strncmp(token, "(ETB", 4) == 0)
                    continue;
                token = strstr(token, ":");
                if (!token)
                {
                    matched = -1;
                    break;
                }
                token += 1;
                while (*token == ' ')
                    token++;
                nssc = sscanf(token, "%[0-9]-%[0-9]", start, end);
                if (nssc == 1)
                {
                    istart = strtoull(start, 0, 10);
                    vector_pushBack(pblocks, istart);
                    *pcount += 1;
                }
                else if (nssc == 2)
                {
                    istart = strtoull(start, 0, 10);
                    iend = strtoull(end, 0, 10);
                    while (istart <= iend)
                    {
                        vector_pushBack(pblocks, istart);
                        *pcount += 1;
                        istart++;
                    }
                }
                else
                {
                    matched = -1;
                }

            } while (token = strtok(NULL, ","));

            matched++;
            break;
        }
        if (matched == 0)
        {
            if (strncmp(buffer, "BLOCKS:", 7) == 0 || strncmp(buffer, "EXTENTS:", 8) == 0)
            {
                matched++;
            }
        }
    }
    if (nline == 0) {
        memset(buffer, 0, sizeof(buffer));
        read(pipefd[0], buffer, sizeof(buffer)-1);
        fprintf(stderr, "%s", buffer);
    }
    pclose(fp);
    close(pipefd[0]);
    close(pipefd[1]);
    if (matched != 2)
    {
        return 1;
    }
    return 0;
}

#endif //USE_e2fsprogs

int ext_get_path_datablocks(const char *dev, const char *path, struct Vector* pblocks, size_t *pcount)
{
    struct stat fs;

    if (stat(path, &fs) < 0)
    {
        return -1;
    }

    return ext_get_inode_datablocks(dev, fs.st_ino, pblocks, pcount);
}