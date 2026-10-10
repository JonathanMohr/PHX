import build.toolchain.toolchain as toolchain
import build.cache as cache
import build.compileCommands as compileCommands
from build.defs import BuildMode, ARCH, OS, OPTIMIZATION, PORTABILITY, LINKING, HOST, BuildContext

from pathlib import Path
import subprocess
import shutil

def Compile_Assembly_Source(self: toolchain.Toolchain, mode: BuildMode, src: Path, src_rel: Path, out_dir: Path, doCompileCommands: bool) -> Path:
    if mode.target_arch != ARCH.x86_64:
        raise RuntimeError(f"Invalid architecture for NASM: {mode.target_arch}")

    NASM = toolchain.Require_Tool("nasm")

    out_file = out_dir / f"{src_rel}.o"

    args: list[str] = []

    flags: list[str] = []
    if mode.debuginfo:
        flags.extend(["-g", "-F", "dwarf"])

    args.extend(flags)

    for define in self.defines:
        k, v = define
        if v: args.extend(["-D", f"{k}={v}"])
        else: args.extend(["-D", k])

    for include in self.includeDirectories:
        args.extend(["-I", str(include)])

    match mode.target_os:
        case OS.Windows:
            fstr = "win64"

        case OS.macOS:
            fstr = "macho64"

        case OS.Linux:
            fstr = "elf64"

    args.extend([
        "-f", fstr,
        str(src),
        "-o", str(out_file)
    ])

    content_hash = cache.hash_files([src], args, self.context.logger)

    #if doCompileCommands: self.context.compileCommands.add("nasm", compileCommands.Language.ASM, src, out_file, args)
    if not self.context.buildCache.is_up_to_date(out_file, content_hash):
        self.context.logger.build(f"Assembling {src} -> {out_file}")

        out_file.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([NASM, *args], check=True)

        self.context.buildCache.update(out_file, content_hash)

    return out_file

def Compile_Assembly_To_Binary(context: BuildContext, src: Path, out: Path):
    NASM = toolchain.Require_Tool("nasm")

    args = ["-fbin", str(src), "-o", str(out)]

    content_hash = cache.hash_files([src], [], context.logger)
    if not context.buildCache.is_up_to_date(out, content_hash):
        context.logger.build(f"Assembling {src} -> {out}")

        out.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([NASM, *args], check=True)

        context.buildCache.update(out, content_hash)
