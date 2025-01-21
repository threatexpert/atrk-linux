
/*
依赖的几个命令：

pvs + pvdisplay  LVM分区管理时需要
xfs_bmap         xfs文件系统格式时需要
debugfs          非PLUS版对ext文件系统格式时需要

*/

#include "pred.h"
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include <errno.h>
#include <dirent.h>
#include <getopt.h>
#include "utils.h"
#include "blk.h"
#include "fs.h"

struct QueryBlocks
{
    char path[256];
    struct stat fs;
    struct DeviceInfo dev;
    struct DeviceInfo parent_dev;
    uint64_t parentoffset;
    int blkunitsz;
    struct Vector blknums; //.size includes a "-1"
    size_t blkcount;  // real size
};

void clean_queryblock(struct QueryBlocks *qb)
{
    vector_free(&qb->blknums, 0);
}

int query_blocks(const char *path, struct QueryBlocks *qb)
{
    struct DeviceInfo di, diparent;
    struct stat fs;
    if (stat(path, &fs) < 0)
    {
        fprintf(stderr, "query blocks: stat: %s: %s\n", path, strerror(errno));
        return 2;
    }
    strncpy(qb->path, path, 255);
    memcpy(&qb->fs, &fs, sizeof(fs));
    if (access("/sys/block", F_OK) != 0 && access("/sys/dev/block", F_OK) != 0) {
        perror("query blocks: access /sys/block or /sys/dev/block");
        return 2;
    }
    memset(&di, 0, sizeof(di));
    memset(&diparent, 0, sizeof(diparent));
    if (myLsblk(fs.st_dev, &di, &diparent) != 0)
    {
        fprintf(stderr, "query blocks: lsblk: %s: failed\n", path);
        return 2;
    }
    memcpy(&qb->dev, &di, sizeof(di));
    memcpy(&qb->parent_dev, &diparent, sizeof(diparent));

    int ret;
    unsigned int bbufcount;
    int blocksize = 0;
    int i;
    uint64_t start, end, usize;

    if (strncmp(di.fstype, "xfs", 3) == 0)
    {
        blocksize = 512;
        ret = xfs_get_path_extents(path, &qb->blknums, &qb->blkcount);
        if (ret != 0)
        {
            fprintf(stderr, "query blocks: get extents(xfs): %s: error\n", path);
            return 1;
        }
        if (!qb->blkcount)
        {
            fprintf(stderr, "query blocks: get extents(xfs): %s: no blocks\n", path);
            return 1;
        }
        qb->blkunitsz = blocksize;
    }
    else if (strncmp(di.fstype, "ext", 3) == 0)
    {
        blocksize = fs.st_blksize;
        ret = ext_get_inode_datablocks(di.dev, fs.st_ino, &qb->blknums, &qb->blkcount);
        if (ret != 0)
        {
            sleep(2);
            vector_free(&qb->blknums, 0);
            qb->blkcount = 0;
            ret = ext_get_inode_datablocks(di.dev, fs.st_ino, &qb->blknums, &qb->blkcount);
            if (ret != 0) {
                fprintf(stderr, "query blocks: %s: call debugfs failed\n", path);
                return 1;
            }
        }
        qb->blkunitsz = blocksize;
    }
    else
    {
        fprintf(stderr, "query blocks: %s: unsupported fstype(%s)\n", path, di.fstype);
        return 1;
    }

    if (!diparent.dev[0])
    {
        return 101;
    }
    //当前卷（块设备）有父节点时，查当前卷在父设备中的偏移
    if (strcmp(di.type, "part") == 0)
    {
        ret = ext_parse_volume_range(diparent.dev, di.dev, &start, &end);
        if (ret != 0)
        {
            fprintf(stderr, "query blocks: %s: failed to parse part range\n", path);
            return 1;
        }
        qb->parentoffset = start * 512;
    }
    else if (strcmp(di.type, "raid") == 0)
    {
        ret = ext_parse_volume_range(diparent.dev, diparent.md, &start, &end);
        if (ret != 0)
        {
            fprintf(stderr, "query blocks: %s: failed to parse raid part range\n", path);
            return 1;
        }
        qb->parentoffset = start * 512;
    }
    else if (strcmp(di.type, "lvm") == 0)
    {
        start = 0;
        ret = lvm_parse_pvs_pestart(diparent.dev, &start);
        if (ret != 0)
        {
            fprintf(stderr, "query blocks: %s: failed to get pvs pestart\n", path);
            return 1;
        }
        qb->parentoffset = start * 512;
        // fprintf(stderr, "pvs_pestart: %"PRIu64"\n", (uint64_t)start);
        start = end = usize = 0;
        ret = lvm_parse_volume_range(diparent.dev, di.dev, &start, &end, &usize);
        if (ret != 0)
        {
            fprintf(stderr, "query blocks: %s: failed to parse lvm range\n", path);
            return 1;
        }
        qb->parentoffset += start * usize * 512;
        // fprintf(stderr, "within_lvm_range: start=%"PRIu64", end=%"PRIu64", size=%"PRIu64"\n", (uint64_t)start, (uint64_t)end, (uint64_t)usize);
    }
    else
    {
        fprintf(stderr, "query blocks: %s: unsupported type(%s)\n", path, di.type);
        return 1;
    }

    return 0;
}

int dump_blocks(struct QueryBlocks *qb, int readparent, unsigned char **ppdata, struct Vector *pextentPtr)
{
    FILE *fp = NULL;

    if (readparent)
    {
        fp = fopen(qb->parent_dev.dev, "rb");
    }
    else
    {
        fp = fopen(qb->dev.dev, "rb");
    }
    if (!fp)
    {
        return 2;
    }
    unsigned char *mem = malloc(qb->blkunitsz * qb->blkcount);
    int n, j;
    uint64_t offset, bno;
    unsigned char *curr = mem;
    for (n = 0, j = 0; n < qb->blknums.size; n++)
    {
        bno = vector_get(&qb->blknums, n);
        if (bno == -1) {
            if (pextentPtr)
                vector_pushBackp(pextentPtr, curr);
            continue;
        }
        if (readparent)
        {
            offset = qb->parentoffset + bno * qb->blkunitsz;
        }
        else
        {
            offset = bno * qb->blkunitsz;
        }

        if (fseeko(fp, offset, SEEK_SET) != 0)
        {
            fclose(fp);
            free(mem);
            return 1;
        }
        if (1 != fread(mem + j * qb->blkunitsz, qb->blkunitsz, 1, fp))
        {
            fclose(fp);
            free(mem);
            return 1;
        }
        curr += qb->blkunitsz;
        j += 1;
    }
    *ppdata = mem;
    fclose(fp);
    return 0;
}

int vector_dirent_index(struct Vector* vector, const char *name)
{
    int n;
    struct my_dir_entry *item;
    for (n=0; n<vector->size; n++) {
        item = (struct my_dir_entry*)vector_getp(vector, n);
        if (strcmp(name, item->name) == 0) {
            return n;
        }
    }
    return -1;
}

void vector_dirent_cmp(struct Vector* vector1, struct Vector* vector2, struct Vector* vector_in1_notin2, int onlyref) {
    int n;
    struct my_dir_entry *item;
    struct my_dir_entry *entrycp;
    int index;
    for (n=0; n<vector1->size; n++) {
        item = (struct my_dir_entry*)vector_getp(vector1, n);
        index = vector_dirent_index(vector2, item->name);
        if (index == -1) {
            if (onlyref) {
                vector_pushBackp(vector_in1_notin2, item);
            }else{
                entrycp = (struct my_dir_entry *) malloc(sizeof(struct my_dir_entry)+item->name_len);
                memcpy(entrycp, item, sizeof(struct my_dir_entry)+item->name_len);
                vector_pushBackp(vector_in1_notin2, entrycp);
            }
        }
    }
}

int find_hidden_files(const char* fstype, struct Vector *vecdata, uint64_t datalen,
                      const char *dirpath,
                      struct Vector *pvecRawfiles, struct Vector *pvecDirfiles, struct Vector* hidden)
{
    int ret;
    size_t i;
    char *pos;
    size_t poslen;

    do_dir(dirpath, pvecDirfiles);

    if (vecdata->size == 0) {
        return 1;
    }
    for (i=0; i<vecdata->size; i++) {
        pos = (char*)vector_getp(vecdata, i);
        if (i+1 < vecdata->size)
            poslen = (size_t)((char*)vector_getp(vecdata, i+1) - pos);
        else
            poslen = (size_t)((char*)vector_getp(vecdata, 0) + datalen - pos);
        if (strncmp(fstype, "ext", 3) == 0)
            ret = parse_ext2_dir(pos, poslen, pvecRawfiles);
        else if (strncmp(fstype, "xfs", 3) == 0)
            ret = parse_xfs_dir(pos, poslen, pvecRawfiles);
        else
            return 1;
    }
    
    vector_dirent_cmp(pvecRawfiles, pvecDirfiles, hidden, 1);
    return 0;
}

static void printHelp()
{
    fprintf(stderr, "Usage: fhf [OPTIONS]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -f, --file-path\n");
    fprintf(stderr, "  -p, --read-parent\n");
    fprintf(stderr, "  -c, --cmp-parent-only\n");
    fprintf(stderr, "  -o, --output\n");
    fprintf(stderr, "  -v, --verbose\n");
    fprintf(stderr, "  -h, --help\n");
}

int writedata(FILE *fp, unsigned char *data, size_t datalen)
{
    size_t writeBytes;
    unsigned char *pos;
    pos = data;
    while (datalen > 0)
    {
        writeBytes = fwrite(pos, 1, datalen, fp);
        if (writeBytes == 0)
        {
            return 1;
        }
        datalen -= writeBytes;
        pos += writeBytes;
    }
    return 0;
}

#ifdef FHF_MAIN
int fhf_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    int option;
    int optionIndex = 0;
    char *fileName = NULL;
    int read_parent = 0;
    int cmp_parent_only = 0;
    char *output = NULL;
    int verbose = 0;

    dbg(stderr, "DEBUGON = 1\n");

    static struct option longOptions[] = {
        {"file-path", required_argument, 0, 'f'},
        {"read-parent", no_argument, 0, 'p'},
        {"cmp-parent-only", no_argument, 0, 'c'},
        {"verbose", no_argument, 0, 'v'},
        {"output", required_argument, 0, 'o'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    while ((option = getopt_long(argc, argv, "f:o:pcvh", longOptions, &optionIndex)) != -1)
    {
        switch (option)
        {
        case 'f':
            fileName = optarg;
            break;
        case 'o':
            output = optarg;
            break;
        case 'p':
            read_parent += 1;
            break;
        case 'c':
            cmp_parent_only = 1;
            break;
        case 'v':
            verbose += 1;
            break;            
        case 'h':
            printHelp();
            return 0;
        case '?':
            return 1;
        default:
            break;
        }
    }
    if (!fileName)
    {
        printHelp();
        return 1;
    }

    int ret;
    int i;
    struct QueryBlocks qb;
    struct stat st;
    memset(&qb, 0, sizeof(qb));
    vector_init(&qb.blknums);

    ret = stat(fileName, &st);
    if (ret != 0) {
        fprintf(stderr, "stat: %s: %s\n", fileName, strerror(errno));
        return 1;
    }
    ret = query_blocks(fileName, &qb);
    if (qb.dev.dev[0]){
        if (verbose >= 1) {
            fprintf(stdout,
                    "----details of partition----\n"
                    "FILE=%s\n"
                    "DEV_NAME=%s\n"
                    "DEV=%s\n"
                    "DEV_MAJ_MIN=%s\n"
                    "INODE=%" PRIu64 "\n"
                    "BLOCKS=%" PRIu64 "\n"
                    "TYPE=%s\n"
                    "FSTYPE=%s\n"
                    "PARENT_DEV=%s\n",
                    fileName, qb.dev.name, qb.dev.dev, qb.dev.maj_min, (uint64_t)qb.fs.st_ino,
                    (uint64_t)qb.fs.st_blocks, qb.dev.type, qb.dev.fstype, qb.parent_dev.dev);

            fprintf(stdout, "----details of blocks----\n");
            if (qb.parent_dev.dev[0])
                fprintf(stdout, "offset resides in parentdev=%" PRIu64 "\n", qb.parentoffset);
            fprintf(stdout, "blocksize=%d\n", qb.blkunitsz);
            fprintf(stdout, "blockcount=%zu\n", qb.blkcount);
            if (verbose >= 2) {
                if (qb.blkcount) {
                    for (i = 0; i < qb.blknums.size; i++) {
                        uint64_t bno = (uint64_t)vector_get(&qb.blknums, i);
                        if (bno == -1) {
                            if (i > 0)
                                fprintf(stdout, ", ");
                            continue;
                        }
                        fprintf(stdout, "%" PRIu64 " ", bno);
                    }
                    fprintf(stdout, "\n");
                }
            }
        }
    }else{
        fprintf(stdout, "query blocks: %s: no partition details.\n", fileName);
    }

    if (ret != 0 && ret != 101) //101表示设备没有找到父节点，缺少父节点信息将继续执行
    {
        clean_queryblock(&qb);
        return 1;
    }
    ret = 0;
    FILE *outputfp = NULL;
    if (output && strcmp(output, "-") == 0) {
        outputfp = stdout;
    }

    fprintf(stdout, "----FHF----\n");

    unsigned char *blkdata0 = NULL;
    unsigned char *blkdatap = NULL;
    struct Vector extent_ptr0;
    struct Vector extent_ptrp;
    uint64_t datalen = qb.blkunitsz * qb.blkcount;
    vector_init(&extent_ptr0);
    vector_init(&extent_ptrp);
    do
    {
        ret = dump_blocks(&qb, 0, &blkdata0, &extent_ptr0);
        if (ret != 0) {
            fprintf(stderr, "dump_blocks: %s: error-0\n", fileName);
        }
        if (qb.parent_dev.dev[0]) {
            ret = dump_blocks(&qb, 1, &blkdatap, &extent_ptrp);
            if (ret != 0) {
                fprintf(stderr, "dump_blocks: %s: error-p\n", fileName);
            }
        }
        //缺少一个设备的信息就跳过对比
        if (!blkdata0 || !blkdatap) {
            ret = 1;
            break;
        }

        if (memcmp(blkdata0, blkdatap, datalen) != 0)
        {
            fprintf(stdout, "compare_blocks: DIFF! (Suspicious!)\n");
            if (output && outputfp == NULL)
            {
                outputfp = fopen(output, "wb");
                if (outputfp == NULL)
                {
                    perror("fopen");
                    ret = 1;
                    break;
                }
            }
            if (outputfp) {
                writedata(outputfp, blkdata0, datalen);
                writedata(outputfp, "\n--------0274d4295e64cd697adfad0028b9ee--------\n", 48);
                writedata(outputfp, blkdatap, datalen);
            }
        }
        else
        {
            fprintf(stdout, "compare_blocks: NODIFF.\n");
        }
    } while (0);

    if (!cmp_parent_only)
    {
        unsigned char *blkdata = NULL;
        struct Vector *extent_ptr = NULL;
        do
        {
            if (read_parent >= 2) { //参数-pp的情况 表示明确要求得从父节点设备读取数据进行分析
                blkdata = blkdatap;
                extent_ptr = &extent_ptrp;
            }
            else if (read_parent >= 1 && blkdatap) { //参数-p的情况 表示如果没有父节点，就从当前节点读取数据进行分析
                blkdata = blkdatap;
                extent_ptr = &extent_ptrp;
            }
            else {
                blkdata = blkdata0;
                extent_ptr = &extent_ptr0;
            }
            if (blkdata == NULL) {
                fprintf(stderr, "dump_blocks: %s: error-2\n", fileName);
                ret = 1;
                break;
            }
            if (output && outputfp == NULL)
            {
                outputfp = fopen(output, "wb");
                if (outputfp == NULL)
                {
                    fprintf(stderr, "fopen: %s: %s\n", output, strerror(errno));
                    ret = 1;
                    break;
                }
            }
            if (outputfp) {
                writedata(outputfp, blkdata, datalen);
            }

            if (S_ISDIR(st.st_mode)) {
                if (!strcmp(qb.dev.fstype, "ext4") || !strcmp(qb.dev.fstype, "ext3") || !strcmp(qb.dev.fstype, "ext2")
                    || !strcmp(qb.dev.fstype, "xfs") )
                {
                    struct Vector vecRawfiles;
                    struct Vector vecDirfiles;
                    struct Vector vecHiddenfiles;
                    struct my_dir_entry* dentry;
                    vector_init(&vecRawfiles);
                    vector_init(&vecDirfiles);
                    vector_init(&vecHiddenfiles);

                    find_hidden_files(qb.dev.fstype, extent_ptr, datalen, fileName, &vecRawfiles, &vecDirfiles, &vecHiddenfiles);
                    fprintf(stdout, "Found %zu files in path of %s\n", vecDirfiles.size, fileName);
                    fprintf(stdout, "Parsed out %zu files in blocks of %s\n", vecRawfiles.size, fileName);
                    if (verbose >= 2) {
                        if (vecRawfiles.size)
                            fprintf(stdout, "----file list start----\n");
                        for (i=0; i<vecRawfiles.size; i++) {
                            dentry = (struct my_dir_entry*)vector_getp(&vecRawfiles, i);
                            fprintf(stdout, "%s\n", dentry->name);
                        }
                        if (vecRawfiles.size)
                            fprintf(stdout, "----file list end----\n");                        
                    }
                    if (vecHiddenfiles.size > 0) {
                        fprintf(stdout, "(Suspicious!) Found hidden files in %s\n", fileName);
                        for (i=0; i<vecHiddenfiles.size; i++) {
                            dentry = (struct my_dir_entry*)vector_getp(&vecHiddenfiles, i);
                            fprintf(stdout, "%s\n", dentry->name);
                        }
                    }else{
                        fprintf(stdout, "No hidden files found in %s\n", fileName);
                    }
                    vector_free(&vecHiddenfiles, 0);
                    vector_free(&vecRawfiles, 1);
                    vector_free(&vecDirfiles, 1);
                }else{
                    fprintf(stdout, "Unsupported fstype(%s)\n", qb.dev.fstype);
                    ret = 1;
                }
            }
        } while (0);
    }

    free(blkdata0);
    free(blkdatap);
    vector_free(&extent_ptr0, 0);
    vector_free(&extent_ptrp, 0);
    clean_queryblock(&qb);
    if (outputfp && outputfp != stdout)
    {
        fclose(outputfp);
    }
    return ret;
}
