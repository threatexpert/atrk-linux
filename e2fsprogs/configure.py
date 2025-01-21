import sys, os, argparse


def gettools(toolsdir='', lib=''):
    if not toolsdir:
        toolsdir = os.path.dirname(os.path.realpath(__file__))
    tools = []
    for item in os.listdir(toolsdir):
        item_path = os.path.join(toolsdir, item)
        if os.path.isdir(item_path):
            nparts = item.split('--')
            if len(nparts) != 3:
                continue
            arch = nparts[0]
            libname = nparts[1]
            if lib:
                if libname != lib:
                    continue
            rootdir = item_path
            gcc = os.popen(f'echo {item_path}/bin/*-linux-gcc').read().rstrip("\n")
            sysroot = os.popen(f'echo {item_path}/*-buildroot-*/sysroot').read().rstrip("\n")
            if not os.path.exists(sysroot):
                sysroot = ''
            tools.append(
                {
                    "arch": arch,
                    "rootdir": rootdir,
                    "gcc": gcc,
                    "sysroot": sysroot,
                    "lib": libname
                }
            )
    return tools


def config_with_cc(cc, restore=False):
    print(f"configuring for {cc}...")

    gccname = os.path.basename(cc)
    chost = f'{gccname}'.split('-')[0].split('_')[0]
    if os.path.exists(f'./configured/{gccname}/config.h') and os.path.exists(f'./configured/{gccname}/ext2_types.h'):
        if restore:
            print(f"{gccname}: files all exist, restoring...")
            if 0 != os.system(f'cp -f ./configured/{gccname}/config.h ./lib/config.h') \
                or 0 != os.system(f'cp -f ./configured/{gccname}/ext2_types.h ./lib/ext2fs/ext2_types.h'):
                print('err: restore')
                exit(2)
        else:
            print(f"{gccname}: files all exist")
        return
    os.system(f"make clean > /dev/null")
    if 0 != os.system(f"./configure --host={chost} CC={cc} > /dev/null"):
        print("err")
        exit(2)
    print("copying configured files....")
    os.makedirs(f'configured/{gccname}/', exist_ok=True)
    if 0 != os.system(f'cp -f ./lib/config.h ./lib/ext2fs/ext2_types.h ./configured/{gccname}/'):
        print('err')
        exit(2)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='辅助基于toolchains工具编译多个架构，工具链从https://toolchains.bootlin.com/下载')
    parser.add_argument('--tools', action='store', help='编译工具集的目录，默认是当前脚本的目录', dest='tools')
    parser.add_argument('--lib', action='store', help='限定选用的lib，默认只选musl，例如指定musl或glibc', dest='lib', default='musl')
    parser.add_argument('--archs', action='store', help='限制要编译的架构名，多个用逗号分隔，例如x86-64,mips32', dest='archs', default='')
    parser.add_argument('--gcc', action='store', help='gcc path', dest='gcc', default='')
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)
    
    if args.gcc:
        config_with_cc(args.gcc, restore=True)
        if 0 != os.system(f'cp -f {script_dir}/configured/crc32c_table.h {script_dir}/lib/ext2fs/') \
            or 0 != os.system(f'cp -f {script_dir}/configured/ext2_err.c {script_dir}/lib/ext2fs/') \
            or 0 != os.system(f'cp -f {script_dir}/configured/ext2_err.h {script_dir}/lib/ext2fs/') \
            or 0 != os.system(f'cp -f {script_dir}/configured/dirpaths.h {script_dir}/lib/'):
            print('err: restore2')
            exit(2)
        print("done.")
        exit(0)

    archs = args.archs.split(",")
    if archs == ['']:
        archs = []

    tools = gettools(args.tools, args.lib)
    if not tools:
        print("no chain tools found for " + args.lib + ".")
        exit(1)
    for tool in tools:
        if archs:
            if tool['arch'] not in archs:
                continue
        config_with_cc(tool['gcc'])

    exit(0)