from build.defs import TESTCLASS
from tests.context import TestContext

from tests.files import get_in_file

from enum import Enum
from pathlib import Path
import logging
import subprocess
import json
import re

class Format(Enum):
    RAW = 1

format_str_map = {
    Format.RAW: "raw"
}

format_ext_map = {
    Format.RAW: "img"
}

def create_image(logger: logging.Logger, phx: Path, image: Path, format: Format, size: int) -> bool:
    format_str = format_str_map.get(format)
    if not format_str:
        logger.error("Invalid disk format")
        return False

    try:
        subprocess.run([str(phx), "disk", "create", str(image), format_str, str(size)], check=True)

    except Exception as e:
        logger.error(f"Running PHX failed: {e}")
        return False

    return True

def write_image(logger: logging.Logger, phx: Path, image: Path, file: Path) -> bool:
    try:
        subprocess.run([str(phx), "raw", "write", str(image), str(file)], check=True)

    except Exception as e:
        logger.error(f"Running PHX failed: {e}")
        return False

    return True

def read_image(logger: logging.Logger, phx: Path, image: Path, file: Path) -> bool:
    try:
        subprocess.run([str(phx), "raw", "read", str(image), str(file)], check=True)

    except Exception as e:
        logger.error(f"Running PHX failed: {e}")
        return False

    return True


def same_first_n_bytes(path1: Path, path2: Path, n: int, chunk_size: int = 65536) -> bool:
    with path1.open("rb") as f1, path2.open("rb") as f2:
        remaining = n
        while remaining > 0:
            size = min(chunk_size, remaining)
            b1 = f1.read(size)
            b2 = f2.read(size)
            if b1 != b2:
                return False
            if not b1:
                break
            remaining -= len(b1)
    return True


def test(logger: logging.Logger, context: TestContext, test_class: TESTCLASS, phx: Path, test_dir: Path, test_build_dir: Path) -> bool:
    build_dir = test_build_dir / "disk"
    disk_dir = test_dir / "disk"

    image_map_path = build_dir / "image.json"

    build_dir.mkdir(parents=True, exist_ok=True)

    # TODO: Actually do different tests depending on test class
    if test_class == TESTCLASS.NONE:
        return True

    logger.info("Starting disk tests")

    formats = [Format.RAW]

    files = [p for p in disk_dir.rglob("*") if p.is_file()]

    files_map: dict[str, int] = {}

    in_file = build_dir / f"tmp-in.bin"
    out_file = build_dir / f"tmp-out.bin"

    if context.cleanup_artifacts:
        tmp_image = build_dir / "tmp-image.img"

    failed: bool = False
    for i, file in enumerate(files, start=1):
        files_map[str(file.relative_to(disk_dir).as_posix())] = i

        image_name = f"disk{i}"

        for format in formats:
            BLOCK_SIZE = 512

            format_str = format_str_map.get(format)
            if not format_str: format_str = "invalid"

            format_ext = format_ext_map.get(format)
            if not format_ext: format_ext = "idk"

            in_size = get_in_file(file, in_file)
            if in_size == 0:
                logger.error(f"File {file} has an invalid extension")
                failed = True
                continue

            in_blocks = in_size // BLOCK_SIZE
            if in_size % BLOCK_SIZE > 0:
                logger.warning(f"Size of {file} ({in_size}) will be rounded down to {in_blocks * BLOCK_SIZE}")

            if context.cleanup_artifacts:
                image = tmp_image
            else:
                image = build_dir / f"{image_name}-{format_str}.{format_ext}"
            
            if not create_image(logger, phx, image, format, in_blocks * BLOCK_SIZE):
                logger.error(f"Could not create image for {file}")
                failed = True
                continue

            if not write_image(logger, phx, image, in_file):
                logger.error(f"Could not write {file} to image")
                failed = True
                continue

            if not read_image(logger, phx, image, out_file):
                logger.error(f"Could not read image 1 to {out_file}")
                failed = True
                continue

            if same_first_n_bytes(in_file, out_file, in_blocks * BLOCK_SIZE):
                logger.debug(f"Round trip for {file} successful")
            else:
                logger.error(f"Round trip for {file} failed")
                failed = True
                continue

    in_file.unlink(missing_ok=True)
    out_file.unlink(missing_ok=True)

    if context.cleanup_artifacts:
        tmp_image.unlink(missing_ok=True)
    else:
        files_map_list = sorted(files_map.items(), key=lambda item: item[1])
        files_map = dict(files_map_list)
        
        with image_map_path.open("w", encoding="utf-8") as f:
            json.dump(files_map, f, indent=4, ensure_ascii=False)

    logger.info("Finished disk tests")

    return not failed
