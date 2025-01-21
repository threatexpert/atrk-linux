#1. 编译
#  需要musl环境，默认在~/toolchains/目录下，每个子目录名例如：aarch64--musl--stable-2022.08-2、x86-i686--musl--stable-2022.08-1、x86-64--musl--stable-2022.08-1
#  当前目录是在atrk源码中

if [ -z "$OUT" ]; then
    OUT="./out/"
fi
OUT=$(realpath "$OUT")
if [ ! -z "$SRC" ]; then
    cd "$SRC"
fi
if [ ! -f "atrk.c" ]; then
    echo "当前目录不在atrk源码中??"
    exit 1
fi
if [ -z "$TOOLS" ]; then
    TOOLS=~/toolchains/
fi

mkdir "$OUT"
rm -f "$OUT/atrk"_*

e2fsprogs_src=" -DUSE_e2fsprogs -I../e2fsprogs/lib ../e2fsprogs/stat/lsblks.c ../e2fsprogs/lib/ext2fs/alloc.c ../e2fsprogs/lib/ext2fs/alloc_sb.c ../e2fsprogs/lib/ext2fs/alloc_stats.c ../e2fsprogs/lib/ext2fs/alloc_tables.c ../e2fsprogs/lib/ext2fs/atexit.c ../e2fsprogs/lib/ext2fs/badblocks.c ../e2fsprogs/lib/ext2fs/bb_compat.c ../e2fsprogs/lib/ext2fs/bb_inode.c ../e2fsprogs/lib/ext2fs/bitmaps.c ../e2fsprogs/lib/ext2fs/bitops.c ../e2fsprogs/lib/ext2fs/blkmap64_ba.c ../e2fsprogs/lib/ext2fs/blkmap64_rb.c ../e2fsprogs/lib/ext2fs/blknum.c ../e2fsprogs/lib/ext2fs/block.c ../e2fsprogs/lib/ext2fs/bmap.c ../e2fsprogs/lib/ext2fs/check_desc.c ../e2fsprogs/lib/ext2fs/closefs.c ../e2fsprogs/lib/ext2fs/crc16.c ../e2fsprogs/lib/ext2fs/crc32c.c ../e2fsprogs/lib/ext2fs/csum.c ../e2fsprogs/lib/ext2fs/dblist.c ../e2fsprogs/lib/ext2fs/dblist_dir.c ../e2fsprogs/lib/ext2fs/digest_encode.c ../e2fsprogs/lib/ext2fs/dir_iterate.c ../e2fsprogs/lib/ext2fs/dirblock.c ../e2fsprogs/lib/ext2fs/dirhash.c  ../e2fsprogs/lib/ext2fs/dupfs.c ../e2fsprogs/lib/ext2fs/expanddir.c ../e2fsprogs/lib/ext2fs/ext_attr.c ../e2fsprogs/lib/ext2fs/extent.c ../e2fsprogs/lib/ext2fs/fallocate.c ../e2fsprogs/lib/ext2fs/fileio.c ../e2fsprogs/lib/ext2fs/finddev.c ../e2fsprogs/lib/ext2fs/flushb.c ../e2fsprogs/lib/ext2fs/freefs.c ../e2fsprogs/lib/ext2fs/gen_bitmap.c ../e2fsprogs/lib/ext2fs/gen_bitmap64.c ../e2fsprogs/lib/ext2fs/get_num_dirs.c ../e2fsprogs/lib/ext2fs/get_pathname.c ../e2fsprogs/lib/ext2fs/getsectsize.c ../e2fsprogs/lib/ext2fs/getsize.c ../e2fsprogs/lib/ext2fs/hashmap.c ../e2fsprogs/lib/ext2fs/i_block.c ../e2fsprogs/lib/ext2fs/icount.c ../e2fsprogs/lib/ext2fs/imager.c ../e2fsprogs/lib/ext2fs/ind_block.c ../e2fsprogs/lib/ext2fs/initialize.c ../e2fsprogs/lib/ext2fs/inline.c ../e2fsprogs/lib/ext2fs/inline_data.c ../e2fsprogs/lib/ext2fs/inode.c ../e2fsprogs/lib/ext2fs/inode_io.c ../e2fsprogs/lib/ext2fs/io_manager.c ../e2fsprogs/lib/ext2fs/ismounted.c ../e2fsprogs/lib/ext2fs/link.c ../e2fsprogs/lib/ext2fs/llseek.c ../e2fsprogs/lib/ext2fs/lookup.c ../e2fsprogs/lib/ext2fs/mkdir.c ../e2fsprogs/lib/ext2fs/mkjournal.c ../e2fsprogs/lib/ext2fs/mmp.c ../e2fsprogs/lib/ext2fs/namei.c ../e2fsprogs/lib/ext2fs/native.c ../e2fsprogs/lib/ext2fs/newdir.c ../e2fsprogs/lib/ext2fs/nls_utf8.c  ../e2fsprogs/lib/ext2fs/openfs.c ../e2fsprogs/lib/ext2fs/orphan.c ../e2fsprogs/lib/ext2fs/progress.c ../e2fsprogs/lib/ext2fs/punch.c ../e2fsprogs/lib/ext2fs/qcow2.c ../e2fsprogs/lib/ext2fs/rbtree.c ../e2fsprogs/lib/ext2fs/read_bb.c ../e2fsprogs/lib/ext2fs/read_bb_file.c ../e2fsprogs/lib/ext2fs/res_gdt.c ../e2fsprogs/lib/ext2fs/rw_bitmaps.c ../e2fsprogs/lib/ext2fs/sha256.c ../e2fsprogs/lib/ext2fs/sha512.c ../e2fsprogs/lib/ext2fs/sparse_io.c ../e2fsprogs/lib/ext2fs/swapfs.c ../e2fsprogs/lib/ext2fs/symlink.c ../e2fsprogs/lib/ext2fs/tdb.c  ../e2fsprogs/lib/ext2fs/undo_io.c ../e2fsprogs/lib/ext2fs/unix_io.c ../e2fsprogs/lib/ext2fs/unlink.c ../e2fsprogs/lib/ext2fs/valid_blk.c ../e2fsprogs/lib/ext2fs/version.c ../e2fsprogs/lib/ext2fs/write_bb_file.c"
configure_py="../e2fsprogs/configure.py"
exflags=" -Os -s -Wno-unused-function -ffunction-sections -fdata-sections -Wl,--gc-sections -fno-exceptions -fno-stack-protector  -fomit-frame-pointer  -fmerge-all-constants  -fno-ident -Wl,-z,norelro -fno-math-errno -fno-unroll-loops -fno-unwind-tables -fno-asynchronous-unwind-tables  "
python3 build.py -s *.c "$e2fsprogs_src" -o atrk --definelist def.txt --outdir "$OUT" --lib musl --tools "$TOOLS" --archs x86-i686,x86-64,aarch64 --exflags "$exflags" --configure1 "$configure_py"
if [ $? -ne 0 ]; then
    exit 1
fi

cat sh-atrk.sh > "$OUT/sh-atrk.sh"
chmod +x "$OUT/sh-atrk.sh"

echo Done.