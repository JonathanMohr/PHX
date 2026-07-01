from ci.defs import OS, ARCH

from pathlib import Path
import argparse
import logging
import urllib.request, urllib
import shutil
import hashlib
import tarfile
import posixpath
import sys
import platform
import os as os_module

SYSROOT_PATH = Path("sysroots")
TMP_PATH = Path("tmp")

def getSysrootPath(os: OS, arch: ARCH):
    match os:
        case OS.Windows: target_os_str = "windows"
        case OS.macOS: target_os_str = "macos"
        case OS.Linux: target_os_str = "linux"

    match arch:
        case ARCH.x86_64: target_arch_str = "x86_64"
        case ARCH.arm64: target_arch_str = "arm64"

    return SYSROOT_PATH / target_os_str / target_arch_str


def norm_name(name: str) -> str:
    n = posixpath.normpath(name.replace("\\", "/")).lstrip("/")
    if n.startswith("../"):
        raise RuntimeError(f"unsafe path in archive: {name}")
    return n

def resolve_link_target(link_name: str, link_target: str) -> str:
    lt = link_target.replace("\\", "/")
    if posixpath.isabs(lt):
        resolved = lt.lstrip("/")
    else:
        resolved = posixpath.join(posixpath.dirname(link_name), lt)
    
    resolved = posixpath.normpath(resolved)
    if resolved.startswith("../"):
        raise RuntimeError(f"unsafe link target: {link_name} -> {link_target}")

    return resolved

def materialize_links(dest: Path, links: dict[str, str]):
    dest_root = dest.resolve()

    def follow(name: str, _depth: int = 0) -> str:
        if _depth > 40:
            raise RuntimeError(f"symlink chain too deep at {name}")
        target = resolve_link_target(name, links[name])
        if target in links:
            return follow(target, _depth + 1)
        return target

    for link_name in links:
        final_rel = follow(link_name)
        link_path = (dest / link_name)
        real_path = (dest / final_rel)

        for p in (link_path, real_path):
            if dest_root not in p.resolve().parents and p.resolve() != dest_root:
                raise RuntimeError(f"unsafe path: {p}")

        link_path.parent.mkdir(parents=True, exist_ok=True)
        if link_path.exists() or link_path.is_symlink():
            link_path.unlink()

        try:
            rel = os_module.path.relpath(real_path, start=link_path.parent)
            os_module.symlink(rel, link_path,
                              target_is_directory=real_path.is_dir())
        except OSError:
            if not real_path.exists():
                continue
            if real_path.is_dir():
                shutil.copytree(real_path, link_path, symlinks=False, dirs_exist_ok=True)
            else:
                shutil.copy2(real_path, link_path)

def installGlibcLinuxSysroot(logger: logging.Logger, dest: Path, arch: ARCH):
    logger.info("Installing GLibc linux sysroot")

    SHA256 = {
        ARCH.x86_64: "52d61d4446ffebfaa3dda2cd02da4ab4876ff237853f46d273e7f9b666652e1d",
        ARCH.arm64:  "c7176a4c7aacbf46bda58a029f39f79a68008d3dee6518f154dcf5161a5486d8",
    }

    sha256 = SHA256[arch]
    if not sha256:
        raise RuntimeError("Invalid arch for os")
    url = f"https://commondatastorage.googleapis.com/chrome-linux-sysroot/{sha256}"

    # Delete file
    tar_path = TMP_PATH / "sysroot.tar.xz"
    tar_path.parent.mkdir(parents=True, exist_ok=True)
    if tar_path.exists():
        if tar_path.is_dir():
            shutil.rmtree(str(tar_path))
        else:
            tar_path.unlink()

    if dest.exists(): shutil.rmtree(str(dest))

    try:
        # install archive
        with urllib.request.urlopen(url) as response, tar_path.open("wb") as f:
            shutil.copyfileobj(response, f)

        # Check archive
        h = hashlib.sha256()
        with tar_path.open("rb") as f:
            for block in iter(lambda: f.read(1024 * 1024), b""):
                h.update(block)

        if h.hexdigest() != sha256:
            raise RuntimeError("checksum mismatch: file is corrupt or wrong")

        # extract files and folders
        with tarfile.open(str(tar_path), "r:xz") as tar:
            for member in tar.getmembers():
                if member.issym() or member.islnk():
                    continue
                target = (dest / member.name).resolve()
                try:
                    target.relative_to(dest.resolve())
                except ValueError:
                    raise RuntimeError(f"unsafe path in archive: {member.name}")
                tar.extract(member, dest, filter="data")

        # collect links
        links: dict[str, str] = {}
        with tarfile.open(str(tar_path), "r:xz") as tar:
            for member in tar.getmembers():
                if member.issym() or member.islnk():
                    links[norm_name(member.name)] = member.linkname
        materialize_links(dest, links)

    except Exception as e:
        if tar_path.exists():
            if tar_path.is_dir():
                shutil.rmtree(str(tar_path))
            else:
                tar_path.unlink()
    
        if dest.exists(): shutil.rmtree(str(dest))

        raise e


def installSysroot(logger: logging.Logger, os: OS, arch: ARCH) -> Path:
    sysroot_path = getSysrootPath(os, arch)

    # TODO: Caching

    match os:
        case OS.Linux: installGlibcLinuxSysroot(logger, sysroot_path, arch)

        case _:
            raise RuntimeError("Invalid OS")

    return sysroot_path

if __name__ == "__main__":
    argparser = argparse.ArgumentParser()

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

    args = argparser.parse_args()

    logger = logging.getLogger("sysroot")
    logger.setLevel(logging.DEBUG)

    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.DEBUG)
    console_formatter = logging.Formatter("[%(levelname)s] %(message)s")
    console_handler.setFormatter(console_formatter)
    logger.addHandler(console_handler)

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
                sys.exit(1)

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
                sys.exit(1)

    try:
        installSysroot(logger, target_os, target_arch)

    except Exception as e:
        logger.error(e)
        sys.exit(1)
