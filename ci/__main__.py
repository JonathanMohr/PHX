from ci.defs import OS, ARCH, OPTIMIZATION, PORTABILITY, LINKING, HOST, BuildContext, BuildMode

from ci.toolchains.toolchain import Toolchain
from ci.toolchains.get import Get_LLVM_Toolchain

from ci.compileCommands import CompileCommands
import ci.cache as cacheModule
import ci.logger as loggerModule
import ci.archive as archiveModule

from pathlib import Path
import sys
import argparse
import logging
import copy
import shutil
import platform
import subprocess

def Build_Sources_To_Objects(logger: logging.Logger, toolchain: Toolchain, mode: BuildMode, src_dir: Path, build_dir: Path, doCompileCommands: bool) -> Path:
    patterns = ["*.c", "*.cpp"]

    files: list[Path] = []
    for pattern in patterns:
        files.extend(src_dir.rglob(pattern))

    objects: list[Path] = []
    for file in files:
        try:
            if file.suffix == ".c":
                object = toolchain.Compile_C_Source(mode, file, file.relative_to(src_dir), build_dir, doCompileCommands)
            elif file.suffix == ".cpp":
                object = toolchain.Compile_CPP_Source(mode, file, file.relative_to(src_dir), build_dir, doCompileCommands)
            else:
                logger.warning(f"Invalid source extension of file {file}")
                continue

        except Exception as e:
            logger.error(f"Compilation of {file} failed: {e}")
            raise e
        
        objects.append(object)

    return objects

def Build_Static_Library(logger: logging.Logger, toolchain: Toolchain, mode: BuildMode, src_dir: Path, build_dir: Path, name: str, doCompileCommands: bool) -> Path:
    objects = Build_Sources_To_Objects(logger, toolchain, mode, src_dir, build_dir, doCompileCommands)
    
    try:
        lib = toolchain.Archive_Objects(mode, objects, name, build_dir)

    except Exception as e:
        logger.error(f"Archiving static library {name} failed: {e}")
        raise e
    
    return lib

def Build_Dynamic_Library(logger: logging.Logger, toolchain: Toolchain, mode: BuildMode, libraries: list[Path], src_dir: Path, build_dir: Path, name: str, doCompileCommands: bool, plugin: bool = False) -> tuple[Path, Path | None, Path | None]:
    objects = Build_Sources_To_Objects(logger, toolchain, mode, src_dir, build_dir, doCompileCommands)
    
    try:
        dylib, implib, debug_info = toolchain.Link_DynamicLibrary(mode, objects, libraries, name, build_dir, plugin)

    except Exception as e:
        logger.error(f"Linking dynamic library {name} failed: {e}")
        raise e
    
    return dylib, implib, debug_info

# uses: target_os, target_arch, werror, portability, linking, assertions, sanitizers, host, sysroot
# kind of: optimization, uses it for release and release_with_debug_info, sets it for debug
# sets: lto, pic, hidden, debuginfo
def Build_Dist_Library(logger: logging.Logger, toolchain: Toolchain, mode: BuildMode, dll_libraries: list[Path], src_dir: Path, build_dir: Path, name: str) -> tuple[list[tuple[Path, Path | None, Path | None]], list[Path]]:

    dynamic_libs: list[tuple[Path, Path | None, Path | None]] = []
    static_libs: list[Path] = []

    # Release

    r_name = f"{name}"
    sr_name = f"{name}s"
    r_build_dir = build_dir / r_name

    r_mode = copy.deepcopy(mode)
    r_mode.lto = True
    r_mode.pic = True
    r_mode.hidden = True
    r_mode.debuginfo = False

    sr_mode = copy.deepcopy(mode)
    sr_mode.lto = False
    sr_mode.pic = False
    sr_mode.hidden = True
    sr_mode.debuginfo = False

    r_dynamic_lib, r_import_lib, r_debug_info = Build_Dynamic_Library(logger, toolchain, r_mode, dll_libraries, src_dir, r_build_dir / "dynamic", r_name, False)
    r_static_lib = Build_Static_Library(logger, toolchain, sr_mode, src_dir, r_build_dir / "static", sr_name, False)

    dynamic_libs.append((r_dynamic_lib, r_import_lib, r_debug_info))
    static_libs.append(r_static_lib)

    # Release with debug info

    rd_name = f"{name}rd"
    srd_name = f"{name}srd"
    rd_build_dir = build_dir / rd_name

    rd_mode = copy.deepcopy(mode)
    rd_mode.lto = True
    rd_mode.pic = True
    rd_mode.hidden = True
    rd_mode.debuginfo = True

    srd_mode = copy.deepcopy(mode)
    srd_mode.lto = False
    srd_mode.pic = False
    srd_mode.hidden = True
    srd_mode.debuginfo = True

    rd_dynamic_lib, rd_import_lib, rd_debug_info = Build_Dynamic_Library(logger, toolchain, rd_mode, dll_libraries, src_dir, rd_build_dir / "dynamic", rd_name, False)
    rd_static_lib = Build_Static_Library(logger, toolchain, srd_mode, src_dir, rd_build_dir / "static", srd_name, False)

    dynamic_libs.append((rd_dynamic_lib, rd_import_lib, rd_debug_info))
    static_libs.append(rd_static_lib)

    # Debug

    d_name = f"{name}d"
    sd_name = f"{name}sd"
    d_build_dir = build_dir / d_name

    d_mode = copy.deepcopy(mode)
    d_mode.optimization = OPTIMIZATION.NONE
    d_mode.lto = False
    d_mode.pic = True
    d_mode.hidden = True
    d_mode.debuginfo = True

    sd_mode = copy.deepcopy(mode)
    sd_mode.optimization = OPTIMIZATION.NONE
    sd_mode.lto = False
    sd_mode.pic = False
    sd_mode.hidden = True
    sd_mode.debuginfo = True

    d_dynamic_lib, d_import_lib, d_debug_info = Build_Dynamic_Library(logger, toolchain, d_mode, dll_libraries, src_dir, d_build_dir / "dynamic", d_name, True)
    d_static_lib = Build_Static_Library(logger, toolchain, sd_mode, src_dir, d_build_dir / "static", sd_name, False)

    dynamic_libs.append((d_dynamic_lib, d_import_lib, d_debug_info))
    static_libs.append(d_static_lib)

    return dynamic_libs, static_libs


def Build_Executable(logger: logging.Logger, toolchain: Toolchain, mode: BuildMode, static_libs: list[Path], libraries: list[tuple[list[tuple[Path, Path | None, Path | None]], list[Path]]], src_dir: Path, build_dir: Path, name: str) -> tuple[Path, Path | None]:
    objects = Build_Sources_To_Objects(logger, toolchain, mode, src_dir, build_dir, True)
    
    libs: list[Path] = []

    for static_lib in static_libs:
        libs.append(static_lib)

    for library in libraries:
        dynamic_libraries, static_libraries = library

        if mode.linking == LINKING.DYNAMIC:
            for dynamic_library in dynamic_libraries:
                dylib, implib, debug_info = dynamic_library
                if implib: libs.append(implib)
                else: libs.append(dylib)
        else:
            for static_library in static_libraries:
                libs.append(static_library)

    try:
        executable, executable_debug_info = toolchain.Link_Executable(mode, objects, libs, name, build_dir)

    except Exception as e:
        logger.error(f"Linking executable {name} failed: {e}")
        raise e
    
    return executable, executable_debug_info


def Copy_Path(logger: logging.Logger, src: Path, dst: Path):
    if src.exists():
        if dst.exists():
            if dst.is_file(): dst.unlink()
            elif dst.is_dir(): shutil.rmtree(str(dst))

        if src.is_file():
            shutil.copy2(str(src), str(dst))
        elif src.is_dir():
            shutil.copytree(str(src), str(dst))
    else:
        logger.warning(f"{src} does not exist")


def StageLibraries(logger: logging.Logger, dist_dir: Path, include_path: Path | None, libraries: list[tuple[list[tuple[Path, Path | None, Path | None]], list[Path]]]) -> tuple[Path, Path]:
    license_path = Path("LICENSE")
    license = dist_dir / "LICENSE"

    include_dir = dist_dir / "include"
    lib_dir = dist_dir / "lib"

    # License
    license.parent.mkdir(parents=True, exist_ok=True)
    Copy_Path(logger, license_path, license)

    # Include directory
    if include_path:
        include_dir.mkdir(parents=True, exist_ok=True)
        Copy_Path(logger, include_path, include_dir)

    # Libraries
    if libraries:
        lib_dir.mkdir(parents=True, exist_ok=True)

    for library in libraries:
        dynamic_libraries, static_libraries = library

        for dynamic_library in dynamic_libraries:
            dylib, implib, debug_info = dynamic_library

            dst_dylib = lib_dir / dylib.name
            Copy_Path(logger, dylib, dst_dylib)

            if implib is not None:
                dst_implib = lib_dir / implib.name
                Copy_Path(logger, implib, dst_implib)

            if debug_info is not None:
                dst_debug_info = lib_dir / debug_info.name
                Copy_Path(logger, debug_info, dst_debug_info)

        for static_library in static_libraries:
            dst_static_library = lib_dir / static_library.name
            Copy_Path(logger, static_library, dst_static_library)

    return (include_dir, lib_dir)

def StageExecutables(logger: logging.Logger, dist_dir: Path, executables: list[tuple[Path, Path | None]]):
    bin_dir = dist_dir / "bin"

    bin_dir.mkdir(parents=True, exist_ok=True)

    # Executables
    for exe in executables:
        executable, executable_debug_info = exe

        dst_exe = bin_dir / executable.name
        Copy_Path(logger, executable, dst_exe)

        if executable_debug_info is not None:
            dst_debug_info = bin_dir / executable_debug_info.name
            Copy_Path(logger, executable_debug_info, dst_debug_info)


def run_git(cmd):
    try:
        return subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode().strip()
    except subprocess.CalledProcessError:
        return None

def get_version() -> str:
    commit_hash = run_git(["git", "rev-parse", "--short", "HEAD"])
    if not commit_hash:
        commit_hash = "unknown"

    last_tag = run_git(["git", "describe", "--tags", "--abbrev=0"])
    
    if last_tag:
        commits_since_tag = run_git(["git", "rev-list", f"{last_tag}..HEAD", "--count"])
        if commits_since_tag and commits_since_tag != "0":
            version = f"{last_tag}-dev.{commits_since_tag}+{commit_hash}"
        else:
            version = f"{last_tag}+{commit_hash}"
    else:
        branch = run_git(["git", "rev-parse", "--abbrev-ref", "HEAD"]) or "unknown-branch"
        version = f"{branch}+{commit_hash}"
    
    return version

def main() -> bool:
    dist_build = True

    argparser = argparse.ArgumentParser()

    argparser.add_argument(
        "-v", "--version",
        dest="version",
        metavar="NAME",
        type=str,
        help="Version string"
    )

    argparser.add_argument(
        "--os",
        dest="os",
        type=str,
        choices=[
            "windows", "macos", "linux"
        ],
        default=None,
        help="Set target os"
    )
    argparser.add_argument(
        "--arch",
        dest="arch",
        type=str,
        choices=[
            "x86-64", "x86_64", "x86",
            "arm64", "arm"
        ],
        default=None,
        help="Set target arch"
    )
    argparser.add_argument(
        "--sysroot",
        dest="sysroot",
        type=str,
        default=None,
        help="Set sysroot path"
    )

    argparser.add_argument(
        "-a", "--archive",
        dest="archive",
        action="store_true",
        help="Archive the projekt"
    )
    argparser.add_argument(
        "-A", "--archive-name",
        dest="archive_name",
        metavar="NAME",
        type=str,
        help="Name of the archive to create"
    )

    args = argparser.parse_args()

    logger = logging.getLogger("ci")
    logger.setLevel(logging.DEBUG)

    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.DEBUG)
    console_formatter = logging.Formatter("[%(levelname)s] %(message)s")
    console_handler.setFormatter(console_formatter)
    logger.addHandler(console_handler)

    if args.version:
        version = args.version
    else:
        # TODO: Should be that: version = get_version()
        version = "placeholder"

    archive_name = args.archive_name or "phx"

    host_os = False
    host_arch = False

    match args.os:
        case "windows": target_os = OS.Windows
        case "macos": target_os = OS.macOS
        case "linux": target_os = OS.Linux

        case None | _:
            if args.os is not None:
                logger.warning("Invalid OS")
            
            os_uname = platform.system().lower()
            if (os_uname == "windows"): target_os = OS.Windows
            elif (os_uname == "darwin"): target_os = OS.macOS
            elif (os_uname == "linux"): target_os = OS.Linux
            else:
                logger.error("Invalid OS")
                return False

    match args.arch:
        case "x86-64" | "x86_64" | "x86":
            target_arch = ARCH.x86_64
        case "arm64" | "arm":
            target_arch = ARCH.arm64

        case None | _:
            if args.arch is not None:
                logger.warning("Invalid arch")
            
            cpu_arch = platform.machine().lower()
            if (cpu_arch in ["x86_64", "amd64"]): target_arch = ARCH.x86_64
            elif (cpu_arch in ["arm64", "aarch64"]): target_arch = ARCH.arm64
            else:
                logger.error("Invalid arch")
                return False

    isHost = host_os and host_arch

    project_dir = Path(".")
    # include_dir = project_dir / "include"
    # lib_dir = project_dir / "libs"
    tools_dir = project_dir / "tools"

    archives_path = project_dir / "archives"

    os_dir = project_dir / "os"
    windows_dir = os_dir / "windows"

    dist_dir = project_dir / "dist"
    general_build_dir = project_dir / "build"
    general_log_dir = project_dir / "logs"

    specific_build_dir = general_build_dir / ("dist_build" if dist_build else "local_build")
    specific_log_dir = general_log_dir / ("dist_build" if dist_build else "local_build")

    compileCommandsPath = project_dir / "compile_commands.json"


    buildCache = cacheModule.BuildCache(general_build_dir / "cache.json", logger)
    compileCommands = CompileCommands()


    buildContext = BuildContext(logger, buildCache, compileCommands)
    toolchain = Get_LLVM_Toolchain(buildContext)
    toolchain.Set_STDC("c99")
    toolchain.Set_STDCPP("c++98")
    toolchain.Add_Define("VERSION", f"\"{version}\"")

    dll_libraries: list[Path] = []
    
    sysroot_path: str | None = args.sysroot
    if sysroot_path:
        sysroot_path = str(Path(sysroot_path).resolve())

    match target_os:
        case OS.Windows: linking = LINKING.DYNAMIC
        case OS.Linux: linking = LINKING.DYNAMIC
        case OS.macOS: linking = LINKING.DYNAMIC
        case _: linking = LINKING.STATIC

    try:
        if dist_dir.exists(): shutil.rmtree(str(dist_dir))

        if dist_build:
            # werror = True
            # lto = True except static library
            # pic = True on dynamic library
            # hidden = True on library
            # optimization = SPEED or SIZE
            # portability = MACHINE or PORTABLE
            # linking = static or dynamic
            # assertions = off
            # sanitizers = off
            # debuginfo = False except specific library
            # host = specific per lib

            buildMode = BuildMode(
                target_os=target_os,
                target_arch=target_arch,
                werror=True, # set
                lto=True, # set
                pic=False, # set
                hidden=False, # set
                optimization=OPTIMIZATION.SPEED,
                portability=PORTABILITY.PORTABLE,
                linking=linking,
                assertions=False, # set
                sanitizers=False, # set
                debuginfo=False, # set
                host=HOST.FREESTANDING, # set
                sysroot=sysroot_path,
                project_root=str(project_dir.resolve())
            )

            match buildMode.target_os:
                case OS.Windows: target_os_str = "windows"
                case OS.macOS: target_os_str = "macos"
                case OS.Linux: target_os_str = "linux"

            match buildMode.target_arch:
                case ARCH.x86_64: target_arch_str = "x86_64"
                case ARCH.arm64: target_arch_str = "arm64"

            match buildMode.optimization:
                case OPTIMIZATION.SPEED: optimization_str = "speed"
                case OPTIMIZATION.SIZE: optimization_str = "size"

            match buildMode.portability:
                case PORTABILITY.PORTABLE: portability_str = "portable"
                case PORTABILITY.MACHINE: portability_str = "machine"

            match buildMode.linking:
                case LINKING.STATIC: linking_str = "static"
                case LINKING.DYNAMIC: linking_str = "dynamic"

            build_dir = specific_build_dir / target_os_str / target_arch_str / optimization_str / portability_str / linking_str
            log_dir = specific_log_dir / target_os_str / target_arch_str / optimization_str / portability_str / linking_str

            if buildMode.target_os == OS.Windows:
                windows_dll = Build_Static_Library(logger, toolchain, buildMode, windows_dir / "dll", build_dir / "os" / "windows", "windows_dll", True)
                dll_libraries.append(windows_dll)

            toolchain.Add_Define("PHX_BUILD")

            dist_include_dir, dist_lib_dir = StageLibraries(logger, dist_dir, None, [])


            phx_lfsToolchain = copy.copy(toolchain)
            phx_lfsToolchain.Add_Include_Directory(tools_dir / "phx-lfs")

            phx_lfsBuildMode = copy.copy(buildMode)
            phx_lfsBuildMode.werror = False
            phx_lfsBuildMode.host = HOST.HOSTED

            phx_lfs = Build_Executable(logger, phx_lfsToolchain, phx_lfsBuildMode, [], [], tools_dir / "phx-lfs", build_dir / "tools" / "phx-lfs", "phx-lfs")

            StageExecutables(logger, dist_dir, [phx_lfs])

        else:
            pass

    except Exception as e:
        logger.error(f"Build failed: {e}")

        buildCache.save()
        return False

    compileCommands.write(compileCommandsPath)
    buildCache.save()

    if args.archive:
        archives_path.mkdir(parents=True, exist_ok=True)
        archive = archiveModule.archive(target_os, target_arch, dist_dir, archives_path / archive_name)
        
    return True

if not main():
    sys.exit(1)
