from ci.defs import OS, ARCH

from pathlib import Path
import shutil

def archive(os: OS, arch: ARCH, folder: Path, out: Path) -> Path:
    if os == OS.Windows:
        archive_path = shutil.make_archive(str(out), "zip", root_dir=str(folder))
    else:
        archive_path = shutil.make_archive(str(out), "gztar", root_dir=str(folder))

    return Path(archive_path)
