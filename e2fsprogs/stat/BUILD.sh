
if [ -z "$OUT" ]; then
    OUT="./out/"
fi
OUT=$(realpath "$OUT")
if [ ! -z "$SRC" ]; then
    cd "$SRC"
fi
if [ ! -f "debugfs.c" ]; then
    echo "当前目录不在源码中??"
    exit 1
fi
if [ -z "$TOOLS" ]; then
    TOOLS=~/build/
fi

mkdir "$OUT"
rm -f "$OUT/debugfs"_*
exflags=' -DBUILD_lsblks -Os -s -Wno-unused-function -ffunction-sections -fdata-sections -Wl,--gc-sections -fno-exceptions -fno-stack-protector  -fomit-frame-pointer  -fmerge-all-constants  -fno-ident -Wl,-z,norelro -fno-math-errno -fno-unroll-loops -fno-unwind-tables -fno-asynchronous-unwind-tables  '
python3 build.py -o debugfs -s debugfs.c ' -I../lib  ../lib/ext2fs/alloc.c ../lib/ext2fs/alloc_sb.c ../lib/ext2fs/alloc_stats.c ../lib/ext2fs/alloc_tables.c ../lib/ext2fs/atexit.c ../lib/ext2fs/badblocks.c ../lib/ext2fs/bb_compat.c ../lib/ext2fs/bb_inode.c ../lib/ext2fs/bitmaps.c ../lib/ext2fs/bitops.c ../lib/ext2fs/blkmap64_ba.c ../lib/ext2fs/blkmap64_rb.c ../lib/ext2fs/blknum.c ../lib/ext2fs/block.c ../lib/ext2fs/bmap.c ../lib/ext2fs/check_desc.c ../lib/ext2fs/closefs.c ../lib/ext2fs/crc16.c ../lib/ext2fs/crc32c.c ../lib/ext2fs/csum.c ../lib/ext2fs/dblist.c ../lib/ext2fs/dblist_dir.c ../lib/ext2fs/digest_encode.c ../lib/ext2fs/dir_iterate.c ../lib/ext2fs/dirblock.c ../lib/ext2fs/dirhash.c  ../lib/ext2fs/dupfs.c ../lib/ext2fs/expanddir.c ../lib/ext2fs/ext_attr.c ../lib/ext2fs/extent.c ../lib/ext2fs/fallocate.c ../lib/ext2fs/fileio.c ../lib/ext2fs/finddev.c ../lib/ext2fs/flushb.c ../lib/ext2fs/freefs.c ../lib/ext2fs/gen_bitmap.c ../lib/ext2fs/gen_bitmap64.c ../lib/ext2fs/get_num_dirs.c ../lib/ext2fs/get_pathname.c ../lib/ext2fs/getsectsize.c ../lib/ext2fs/getsize.c ../lib/ext2fs/hashmap.c ../lib/ext2fs/i_block.c ../lib/ext2fs/icount.c ../lib/ext2fs/imager.c ../lib/ext2fs/ind_block.c ../lib/ext2fs/initialize.c ../lib/ext2fs/inline.c ../lib/ext2fs/inline_data.c ../lib/ext2fs/inode.c ../lib/ext2fs/inode_io.c ../lib/ext2fs/io_manager.c ../lib/ext2fs/ismounted.c ../lib/ext2fs/link.c ../lib/ext2fs/llseek.c ../lib/ext2fs/lookup.c ../lib/ext2fs/mkdir.c ../lib/ext2fs/mkjournal.c ../lib/ext2fs/mmp.c ../lib/ext2fs/namei.c ../lib/ext2fs/native.c ../lib/ext2fs/newdir.c ../lib/ext2fs/nls_utf8.c  ../lib/ext2fs/openfs.c ../lib/ext2fs/orphan.c ../lib/ext2fs/progress.c ../lib/ext2fs/punch.c ../lib/ext2fs/qcow2.c ../lib/ext2fs/rbtree.c ../lib/ext2fs/read_bb.c ../lib/ext2fs/read_bb_file.c ../lib/ext2fs/res_gdt.c ../lib/ext2fs/rw_bitmaps.c ../lib/ext2fs/sha256.c ../lib/ext2fs/sha512.c ../lib/ext2fs/sparse_io.c ../lib/ext2fs/swapfs.c ../lib/ext2fs/symlink.c ../lib/ext2fs/tdb.c  ../lib/ext2fs/undo_io.c ../lib/ext2fs/unix_io.c ../lib/ext2fs/unlink.c ../lib/ext2fs/valid_blk.c ../lib/ext2fs/version.c ../lib/ext2fs/write_bb_file.c' --definelist def.txt --outdir "$OUT" --lib musl --tools "$TOOLS" --archs x86-i686,x86-64 --exflags "$exflags" --configure1 ../configure.py
if [ $? -ne 0 ]; then
    exit 1
fi
