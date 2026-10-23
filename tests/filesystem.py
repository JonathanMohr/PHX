from build.defs import TESTCLASS
from tests.context import TestContext

from enum import Enum
from pathlib import Path
import logging

class Format(Enum):
    FAT12 = 1
    FAT16 = 2
    FAT32 = 3

format_str_map = {
    Format.FAT12: "fat12",
    Format.FAT16: "fat16",
    Format.FAT32: "fat32"
}

def test(logger: logging.Logger, context: TestContext, test_class: TESTCLASS, phx: Path, test_dir: Path, test_build_dir: Path) -> bool:
    build_dir = test_build_dir / "filesystem"
    filesystem_dir = test_dir / "filesystem"

    image_map_path = build_dir / "image_map.json"
    
    build_dir.mkdir(parents=True, exist_ok=True)

    # On Linux also check with fsck.fat
    # Also Check with mtools

    logger.info("Starting filesystem tests")

    # TODO

    logger.info("Finished filesystem tests")

    return True
