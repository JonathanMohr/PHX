import ci.toolchains.toolchain as toolchain
import ci.cache as cache
import ci.compileCommands as compileCommands
from ci.defs import BuildMode, ARCH, OS, OPTIMIZATION, PORTABILITY, LINKING, HOST

from pathlib import Path
import subprocess
import shutil

def Parse_Dependency_File(dep_file: Path) -> list[str]:
    if not dep_file.exists():
        return []
        
    # TODO: Handle '\ '

    content = dep_file.read_text()
    content = content.replace('\\\n', ' ')

    parts = content.split()

    if not parts:
        return []
        
    return parts[1:]

def Get_Compile_Flags(mode: BuildMode) -> list[str]:
    flags = []

    # Don't leak absolute paths
    flags.append(f"-fdebug-prefix-map={mode.project_root}=project_root")
    flags.append(f"-fmacro-prefix-map={mode.project_root}=project_root")

    # Warnings
    flags.extend([
        "-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wshadow",
        "-Wformat=2", "-Wundef", "-Wunreachable-code",
        "-Wnull-dereference", "-Wunused-parameter", "-Wformat-security",
    ])

    # werror
    if mode.werror: flags.append("-Werror")

    # lto
    if mode.lto: flags.append("-flto=full")

    # pic
    if mode.pic and mode.target_os != OS.Windows: flags.append("-fPIC")

    # hidden
    if mode.hidden: flags.append("-fvisibility=hidden")

    # Optimization
    Useful_Flags = [
        "-ffunction-sections",
        "-fdata-sections",

        "-fno-ident",
        "-fmerge-all-constants"
    ]

    match mode.optimization:
        case OPTIMIZATION.NONE:
            flags.extend([
                "-O0",
                "-fno-inline",
            ])

        case OPTIMIZATION.SPEED:
            flags.extend([
                "-O3",
                "-funroll-loops",

                *Useful_Flags,
            ])

        case OPTIMIZATION.SIZE:
            flags.extend([
                "-Os",

                *Useful_Flags,
            ])

    # Portability
    match mode.portability:
        case PORTABILITY.MACHINE:
            flags.extend([
                "-march=native",
                "-mtune=native",
            ])

        case PORTABILITY.PORTABLE:
            match mode.target_arch:
                case ARCH.x86_64:
                    flags.extend([
                        "-march=x86-64",
                        "-mtune=generic",
                    ])
                case ARCH.arm64:
                    flags.extend([
                        "-march=armv8-a",
                        "-mtune=generic",
                    ])

    # Linking
    match mode.linking:
        case LINKING.STATIC:
            if mode.target_os != OS.macOS:
                flags.extend([
                    "-static"
                ])

        case LINKING.DYNAMIC:
            flags.extend([

            ])

    # Assertions
    if mode.assertions:
        flags.extend([
            
        ])
    else:
        flags.extend([
            "-DNDEBUG"
        ])

    # Sanitizers
    if mode.sanitizers: # TODO: Those require libraries
        flags.extend([
            "-ftrapv",
            "-fstack-protector-all",
            "-D_FORTIFY_SOURCE=2"
        ])
    else:
        flags.extend([

        ])

    # Debug information
    if mode.debuginfo:
        flags.extend([
            "-g3",
            "-fno-omit-frame-pointer"
        ])
    else:
        if mode.optimization == OPTIMIZATION.NONE:
            flags.append("-fno-omit-frame-pointer")
        else:
            flags.append("-fomit-frame-pointer")

    # Host
    match mode.host:
        case HOST.HOSTED:
            flags.extend([

            ])

        case HOST.FREESTANDING:
            flags.extend([
                "-ffreestanding",
                "-nostdlib",
                "-nostdinc",
                "-fno-builtin",
            ])

    # Sysroot
    if mode.sysroot:
        flags.append(f"--sysroot={mode.sysroot}")

    return flags

def Get_Link_Flags(mode: BuildMode) -> list[str]:
    flags = ["-fuse-ld=lld"]

    # Don't leak absolute paths
    flags.append(f"-fdebug-prefix-map={mode.project_root}=project_root")

    # werror

    # lto
    if mode.lto: flags.append("-flto=full")

    # pic

    # Optimization
    Useful_Flags: list[str] = []

    if mode.lto:
        if mode.target_os != OS.Windows:
            Useful_Flags.extend([
                "-Wl,-O2"
            ])

    if mode.target_os == OS.Windows:
        Useful_Flags.extend([
            "-Wl,/OPT:REF",
            "-Wl,/OPT:ICF"
        ])
    if mode.target_os == OS.macOS:
        Useful_Flags.append("-Wl,-dead_strip")
    elif mode.target_os == OS.Linux:
        Useful_Flags.extend([
            "-Wl,--gc-sections",
            "-Wl,--as-needed"
        ])

    match mode.optimization:
        case OPTIMIZATION.NONE:
            if mode.lto:
                flags.extend([
                    "-O0"
                ])
                if mode.target_os != OS.Windows:
                    flags.extend([
                        "-Wl,-O0"
                    ])

        case OPTIMIZATION.SPEED:
            if mode.lto:
                flags.extend([
                    "-O3",
                    "-funroll-loops",
                ])

            flags.extend([
                *Useful_Flags,
            ])

        case OPTIMIZATION.SIZE:
            if mode.lto:
                flags.extend([
                    "-Os",
                ])

            flags.extend([
                *Useful_Flags,
            ])

    # Portability
    if mode.lto:
        match mode.portability:
            case PORTABILITY.MACHINE:
                flags.extend([
                    "-march=native",
                    "-mtune=native",
                ])

            case PORTABILITY.PORTABLE:
                match mode.target_arch:
                    case ARCH.x86_64:
                        flags.extend([
                            "-march=x86-64",
                            "-mtune=generic",
                        ])
                    case ARCH.arm64:
                        flags.extend([
                            "-march=armv8-a",
                            "-mtune=generic",
                        ])

    # Linking
    match mode.linking:
        case LINKING.STATIC:
            if mode.target_os != OS.macOS:
                flags.extend([
                    "-static"
                ])

        case LINKING.DYNAMIC:
            flags.extend([

            ])

    # Assertions

    # Sanitizers

    # Debug information
    if mode.debuginfo:
        if mode.target_os == OS.Windows:
            flags.extend([
                "-Wl,/DEBUG"
            ])

        if mode.lto:
            flags.append("-fno-omit-frame-pointer")
    else:
        if mode.lto:
            if mode.optimization == OPTIMIZATION.NONE:
                flags.append("-fno-omit-frame-pointer")
            else:
                flags.append("-fomit-frame-pointer")

        if mode.target_os == OS.Windows:
            flags.extend([
                "-Wl,/DEBUG:NONE"
            ])

        elif mode.target_os == OS.macOS:
            flags.extend([
                "-Wl,-x",
                "-Wl,-S"
            ])

        elif mode.target_os == OS.Linux:
            flags.extend([
                "-Wl,--strip-all"
            ])

    # Host
    match mode.host:
        case HOST.HOSTED:
            flags.extend([

            ])

        case HOST.FREESTANDING:
            flags.extend([
                "-nostdlib",
                "-nostartfiles",
            ])

    # Sysroot
    if mode.sysroot:
        flags.append(f"--sysroot={mode.sysroot}")

    return flags


def Get_Target(mode: BuildMode) -> str:
    match mode.target_os:
        case OS.Windows:
            match mode.target_arch:
                case ARCH.x86_64:
                    return "x86_64-pc-windows-msvc"
                case ARCH.arm64:
                    return "aarch64-pc-windows-msvc"
                
        case OS.macOS:
            match mode.target_arch:
                case ARCH.x86_64:
                    return "x86_64-apple-macosx"
                case ARCH.arm64:
                    return "aarch64-apple-macosx"
                
        case OS.Linux:
            match mode.target_arch:
                case ARCH.x86_64:
                    return "x86_64-unknown-linux-gnu"
                case ARCH.arm64:
                    return "aarch64-unknown-linux-gnu"
                
    raise RuntimeError("Unknown combination of target_os and target_arch")


def Compile_C_Source(self: toolchain.Toolchain, mode: BuildMode, src: Path, src_rel: Path, out_dir: Path, doCompileCommands: bool) -> Path:
    CLANG = toolchain.Require_Tool("clang")

    out_file = out_dir / f"{src_rel}.o"
    dep_file = out_dir / f"{src_rel}.d"

    args: list[str] = []

    flags: list[str] = Get_Compile_Flags(mode)
    flags.append(f"-std={self.stdc}")
    flags.append(f"--target={Get_Target(mode)}")
    args.extend(flags)

    for define in self.defines:
        k, v = define
        args.append(f"-D{k}={v}" if v is not None else f"-D{k}")

    for includeDirectory in self.includeDirs:
        args.append(f"-I{includeDirectory}")

    args.extend([
        "-MMD", "-MF", str(dep_file),
        "-c", str(src),
        "-o", str(out_file)
    ])

    deps = Parse_Dependency_File(dep_file)
    content_hash = cache.hash_files([src, *map(Path, deps)], args, self.context.logger)

    if doCompileCommands: self.context.compileCommands.add("clang", compileCommands.Language.C, src, out_file, args)
    if not self.context.buildCache.is_up_to_date(out_file, content_hash):
        self.context.logger.build(f"Compiling {src} -> {out_file}")

        out_file.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([CLANG, *args], check=True)

        new_deps = Parse_Dependency_File(dep_file)
        new_content_hash = cache.hash_files([src, *map(Path, new_deps)], args, self.context.logger)

        self.context.buildCache.update(out_file, new_content_hash)

    return out_file

def Compile_CPP_Source(self: toolchain.Toolchain, mode: BuildMode, src: Path, src_rel: Path, out_dir: Path, doCompileCommands: bool) -> Path:
    CLANGXX = toolchain.Require_Tool("clang++")

    out_file = out_dir / f"{src_rel}.o"
    dep_file = out_dir / f"{src_rel}.d"

    args: list[str] = []

    flags: list[str] = Get_Compile_Flags(mode)
    flags.append(f"-std={self.stdcpp}")
    flags.append(f"--target={Get_Target(mode)}")
    args.extend(flags)

    for define in self.defines:
        k, v = define
        args.append(f"-D{k}={v}" if v is not None else f"-D{k}")

    for includeDirectory in self.includeDirs:
        args.append(f"-I{includeDirectory}")

    args.extend([
        "-MMD", "-MF", str(dep_file),
        "-c", str(src),
        "-o", str(out_file)
    ])

    deps = Parse_Dependency_File(dep_file)
    content_hash = cache.hash_files([src, *map(Path, deps)], args, self.context.logger)

    if doCompileCommands: self.context.compileCommands.add("clang++", compileCommands.Language.CPP, src, out_file, args)
    if not self.context.buildCache.is_up_to_date(out_file, content_hash):
        self.context.logger.build(f"Compiling {src} -> {out_file}")

        out_file.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([CLANGXX, *args], check=True)

        new_deps = Parse_Dependency_File(dep_file)
        new_content_hash = cache.hash_files([src, *map(Path, new_deps)], args, self.context.logger)

        self.context.buildCache.update(out_file, new_content_hash)

    return out_file

def Archive_Objects(self: toolchain.Toolchain, mode: BuildMode, objects: list[Path], name: str, out_dir: Path) -> Path:
    LLVM_AR = toolchain.Require_Tool("llvm-ar")

    if mode.target_os == OS.Windows:
        lib = out_dir / f"{name}.lib"
    else:
        lib = out_dir / f"lib{name}.a"

    args: list[str] = [
        "rcs", str(lib),
        *map(str, objects)
    ]

    content_hash = cache.hash_files(objects, [], self.context.logger)
    if not self.context.buildCache.is_up_to_date(lib, content_hash):
        self.context.logger.build(f"Archiving {lib}")

        lib.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([LLVM_AR, *args], check=True)

        self.context.buildCache.update(lib, content_hash)

    return lib

def Link_Executable(self: toolchain.Toolchain, mode: BuildMode, objects: list[Path], libraries: list[Path], name: str, out_dir: Path) -> tuple[Path, Path | None]:
    CLANGXX = toolchain.Require_Tool("clang++")
    
    if mode.target_os == OS.macOS:
        llvm_dsymutil = shutil.which("llvm-dsymutil")
        if llvm_dsymutil:
            LLVM_DSYMUTIL = llvm_dsymutil
        else:
            LLVM_DSYMUTIL = shutil.which("dsymutil")

        if not LLVM_DSYMUTIL: toolchain.Require_Tool("llvm-dsymutil")

    if mode.target_os == OS.Windows:
        executable = out_dir / f"{name}.exe"
    else:
        executable = out_dir / f"{name}"

    match mode.target_os:
        case OS.Windows:
            debug_info = executable.with_suffix(".pdb")

        case OS.macOS:
            debug_info = executable.parent / (executable.name + ".dSYM")

        case OS.Linux:
            debug_info = None

    if not mode.debuginfo: debug_info = None

    args: list[str] = [*map(str, objects)]

    for libraryDirectory in self.libraryDirs:
        args.append(f"-L{libraryDirectory}")

    args.extend([*map(str, libraries)])

    for library in self.libraries:
        args.append(f"-l{library}")

    flags: list[str] = Get_Link_Flags(mode)
    flags.append(f"--target={Get_Target(mode)}")

    args.extend(flags)

    if debug_info and mode.target_os == OS.Windows:
        args.append(f"-Wl,/PDB:{debug_info}")

    if mode.lto and mode.target_os == OS.macOS:
        args.extend([f"-Wl,-object_path_lto,{out_dir}/lto-objects"])

    args.extend([
        "-o", str(executable)
    ])

    content_hash = cache.hash_files([*objects, *libraries], args, self.context.logger)

    rebuild = not self.context.buildCache.is_up_to_date(executable, content_hash)
    if mode.debuginfo and debug_info:
        if debug_info.exists():
            if debug_info.is_dir():
                if not any(debug_info.rglob("*")):
                    rebuild = True
        else:
            rebuild = True
    
    if rebuild:
        self.context.logger.build(f"Linking {executable}")

        executable.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([CLANGXX, *args], check=True)

        if debug_info and mode.debuginfo:
            match mode.target_os:
                case OS.Windows:
                    pass

                case OS.macOS:
                    if debug_info.exists(): shutil.rmtree(str(debug_info))
                    subprocess.run([LLVM_DSYMUTIL, str(executable), "-o", str(debug_info)], check=True)

                case _:
                    self.context.logger.warning("Invalid target for debug info in another file")

        self.context.buildCache.update(executable, content_hash)

    return executable, debug_info

def Link_DynamicLibrary(self: toolchain.Toolchain, mode: BuildMode, objects: list[Path], libraries: list[Path], name: str, out_dir: Path, plugin: bool) -> tuple[Path, Path | None, Path | None]:
    CLANGXX = toolchain.Require_Tool("clang++")
    
    if mode.target_os == OS.macOS:
        llvm_dsymutil = shutil.which("llvm-dsymutil")
        if llvm_dsymutil:
            LLVM_DSYMUTIL = llvm_dsymutil
        else:
            LLVM_DSYMUTIL = shutil.which("dsymutil")

        if not LLVM_DSYMUTIL: toolchain.Require_Tool("llvm-dsymutil")

    match mode.target_os:
        case OS.Windows:
            dylib = out_dir / f"{name}.dll"
            implib = out_dir / f"{name}.lib"
            debug_info = dylib.with_suffix(".pdb")

        case OS.macOS:
            if plugin: dylib = out_dir / f"lib{name}.so"
            else:      dylib = out_dir / f"lib{name}.dylib"
            implib = None
            debug_info = dylib.parent / (dylib.name + ".dSYM")

        case OS.Linux:
            dylib = out_dir / f"lib{name}.so"
            implib = None
            debug_info = None

    if not mode.debuginfo: debug_info = None

    args: list[str] = [*map(str, objects)]

    for libraryDirectory in self.libraryDirs:
        args.append(f"-L{libraryDirectory}")

    args.extend([*map(str, libraries)])

    for library in self.libraries:
        args.append(f"-l{library}")

    flags: list[str] = Get_Link_Flags(mode)
    flags.append(f"--target={Get_Target(mode)}")
    if mode.target_os == OS.Windows:
        flags.append("-Wl,/DLL")
    else:
        flags.append("-shared")

    if mode.target_os == OS.Windows:
        flags.append(f"-Wl,/IMPLIB:{implib}")
    elif mode.target_os == OS.macOS:
        flags.append(f"-Wl,-install_name,@rpath/lib{name}.dylib")

    args.extend(flags)

    if debug_info and mode.target_os == OS.Windows:
        args.append(f"-Wl,/PDB:{debug_info}")

    if mode.lto and mode.target_os == OS.macOS:
        args.extend([f"-Wl,-object_path_lto,{out_dir}/lto-objects"])

    args.extend([
        "-o", str(dylib)
    ])

    

    content_hash = cache.hash_files([*objects, *libraries], args, self.context.logger)

    rebuild = not self.context.buildCache.is_up_to_date(dylib, content_hash)
    if mode.debuginfo and debug_info:
        if debug_info.exists():
            if debug_info.is_dir():
                if not any(debug_info.rglob("*")):
                    rebuild = True
        else:
            rebuild = True

    if rebuild:
        self.context.logger.build(f"Linking {dylib}")

        dylib.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([CLANGXX, *args], check=True)

        if debug_info and mode.debuginfo:
            match mode.target_os:
                case OS.Windows:
                    pass

                case OS.macOS:
                    if debug_info.exists(): shutil.rmtree(str(debug_info))
                    subprocess.run([LLVM_DSYMUTIL, str(dylib), "-o", str(debug_info)], check=True)

                case _:
                    self.context.logger.warning("Invalid target for debug info in another file")

        self.context.buildCache.update(dylib, content_hash)

    return dylib, implib, debug_info
