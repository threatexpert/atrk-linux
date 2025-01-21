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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='辅助基于toolchains工具编译多个架构，工具链从https://toolchains.bootlin.com/下载')
    parser.add_argument('-s', '--src', nargs='+', help='可多个.c源文件名，不要用完整路径', dest='source', required=True)
    parser.add_argument('-c', '--cd', action='store', help='让脚本cd到这个目录(通常是源码所在目录)', dest='changedir')
    parser.add_argument('-o', '--outputname', action='store', help='编译生成的可执行文件名，会自动添加_架构名在末尾，默认值是第一个源码文件名', dest='outputname')
    parser.add_argument('--tools', action='store', help='编译工具集的目录，默认是当前脚本的目录', dest='tools')
    parser.add_argument('--lib', action='store', help='限定选用的lib，默认只选musl，例如指定musl或glibc', dest='lib', default='musl')
    parser.add_argument('--archs', action='store', help='限制要编译的架构名，多个用逗号分隔，例如x86-64,mips32', dest='archs', default='')
    parser.add_argument('--outdir', action='store', help='统一编译生成到这个目录', dest='outdir')
    parser.add_argument('--definelist', action='store', help='文本文件，每行一个define', dest='definelist')
    parser.add_argument('--exflags', action='store', help='', dest='exflags', default='')
    parser.add_argument('--configure1', action='store', help='', dest='configure1')

    args = parser.parse_args()
    defines = ""
    if args.definelist:
        with open(args.definelist, 'r') as _f:
            for line in _f:
                line = line.strip()
                if line:
                    defines += ' -D' + line

    archs = args.archs.split(",")
    if archs == ['']:
        archs = []

    tools = gettools(args.tools, args.lib)
    if not tools:
        print("no chain tools found for " + args.lib + ".")
        exit(1)
    if args.changedir:
        os.chdir(args.changedir)
    for tool in tools:
        if archs:
            if tool['arch'] not in archs:
                print("%s skipped for building" % (tool['arch'],))
                continue
        if args.configure1 and args.configure1.endswith('.py'):
            if 0 != os.system(f"python3 '{args.configure1}' --gcc \"{tool['gcc']}\""):
                print("configure1 failed.")
                exit(1)
        print("building for %s(%s)..." % (tool['arch'], tool['lib']))
        if not args.outputname:
            out = os.path.join(args.outdir, "%s_%s" % (args.source[0].split('.')[0], tool['arch']))
        else:
            out = os.path.join(args.outdir, "%s_%s" % (args.outputname, tool['arch']))
        srcfiles = ' '.join(args.source)
        if tool['sysroot']:
            sysroot = f"--sysroot={tool['sysroot']}"
        else:
            sysroot = ''
        if os.system(f"{tool['gcc']} {sysroot} -static -s -Os {srcfiles} -o {out} {defines} {args.exflags}") != 0:
            print("err")
            exit(2)
    exit(0)