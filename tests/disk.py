from build.defs import TESTCLASS

from pathlib import Path
import logging

def test(logger: logging.Logger, test_class: TESTCLASS, phx: Path, test_dir: Path) -> bool:
    build_dir = test_dir / ".build" / "disk"
    disk_dir = test_dir / "disk"

    # TODO

    return True
