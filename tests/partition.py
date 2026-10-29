from build.defs import TESTCLASS, OS
from tests.context import TestContext

from tests.files import get_in_file, get_random_file

from enum import Enum
from pathlib import Path
from dataclasses import dataclass
import logging
import subprocess
import json
import tomllib
import filecmp
import plistlib
import re
import shutil

class Format(Enum):
    MBR = 1
    GPT = 2

format_str_map = {
    Format.MBR: "mbr",
    Format.GPT: "gpt"
}

types = ["unknown", "fat12", "fat16", "fat32"]


@dataclass
class Config_Partition:
    type: str | None
    start: int
    size: int
    bootable: bool

@dataclass
class Config:
    size: int
    bootsector: Path | None

    partitions: list[Config_Partition]


def create_image(logger: logging.Logger, phx: Path, image: Path, format: Format, size: int) -> bool:
    format_str = format_str_map.get(format)
    if not format_str:
        logger.error("Invalid partition format")
        return False

    try:
        subprocess.run([str(phx), "disk", "create", str(image), "raw", str(size)], check=True)
        subprocess.run([str(phx), "partition", "create", str(image), format_str], check=True)

    except Exception as e:
        logger.error(f"Running PHX failed: {e}")
        return False

    return True

def set_bootsector(logger: logging.Logger, phx: Path, image: Path, bootsector: Path) -> bool:
    try:
        subprocess.run([str(phx), "partition", "bootsector", str(image), str(bootsector)], check=True)

    except Exception as e:
        logger.error(f"Running PHX failed: {e}")
        return False

    return True

def add_partition(logger: logging.Logger, phx: Path, image: Path, index: int, start: int, size: int, type: str | None, bootable: bool) -> bool:
    try:
        args = ["partition", "add", str(image), str(index), str(start), str(size)]
        if type is not None: args.extend(["--type", type])
        if bootable: args.append("--bootable")
        subprocess.run([str(phx), *args], check=True)

    except Exception as e:
        logger.error(f"Running PHX failed: {e}")
        return False

    return True

def write_partition(logger: logging.Logger, phx: Path, image: Path, index: int, file: Path) -> bool:
    try:
        subprocess.run([str(phx), "raw", "write", f"{image}:{index}", str(file)], check=True)

    except Exception as e:
        logger.error(f"Running PHX failed: {e}")
        return False

    return True

def read_partition(logger: logging.Logger, phx: Path, image: Path, index: int, file: Path) -> bool:
    try:
        subprocess.run([str(phx), "raw", "read", f"{image}:{index}", str(file)], check=True)

    except Exception as e:
        logger.error(f"Running PHX failed: {e}")
        return False

    return True


def mount(logger: logging.Logger, image_path: Path, host_os: OS) -> Path | None:
    logger.info(f"Mounting {image_path}")

    match host_os:
        case OS.Windows:
            return None # TODO

        case OS.Linux:
            return None # TODO

        case OS.macOS:
            try:
                result = subprocess.run([
                    "hdiutil", "attach",
                    "-plist",
                    "-imagekey",
                    "diskimage-class=CRawDiskImage",
                    "-nomount",
                    str(image_path)
                ], capture_output=True, check=True)

                plist = plistlib.loads(result.stdout)
                system_entities = plist.get("system-entities") if isinstance(plist, dict) else None
                if not isinstance(system_entities, list):
                    logger.error(f"Could not parse plist output of hdiutil: {plist}")
                    return None

                for e in system_entities:
                    if not isinstance(e, dict):
                        logger.warning(f"Entry of plist output is not a dict: {e}")
                        continue

                    dev_entry = e.get("dev-entry")
                    if dev_entry is None:
                        logger.warning(f"Entry of plist output does not have a dev-entry field")
                        continue

                    if re.fullmatch(r"/dev/disk\d+", dev_entry):
                        return Path(dev_entry)

                logger.error(f"Could not parse plist output of hdiutil: {plist}")
                return None

            except Exception as e:
                logger.error(f"Error while mounting {image_path} with hdiutil: {e}")
                return None

        case _:
            logger.error(f"Unknown host os: {host_os}")

def unmount(logger: logging.Logger, device_path: Path, host_os: OS) -> bool:
    logger.info(f"Unmounting {device_path}")

    match host_os:
        case OS.Windows:
            return None # TODO

        case OS.Linux:
            return None # TODO

        case OS.macOS:
            try:
                result = subprocess.run([
                    "hdiutil", "detach",
                    str(device_path)
                ], capture_output=True, check=True)

            except Exception as e:
                logger.error(f"Error while unmounting {device_path} with hdiutil: {e}")
                return None

        case _:
            logger.error(f"Unknown host os: {host_os}")

    return True


def check_with_diskutil(logger: logging.Logger, device_path: Path, config: Config, format: Format, name: str) -> bool:
    diskutil = shutil.which("diskutil")
    if diskutil is None:
        logger.error("Could not find diskutil")
        return False

    match format:
        case Format.MBR: expected_content = "FDisk_partition_scheme"
        case Format.GPT: expected_content = "GUID_partition_scheme"
        case _:
            logger.error(f"Invalid format for diskutil tests: {format}")
            return False

    try:
        result = subprocess.run([
            diskutil, "list",
            "-plist", device_path.name
        ], capture_output=True, check=True)

    except Exception as e:
        logger.error(f"Error while running diskutil for image ({name}): {e}")
        return False

    data = plistlib.loads(result.stdout)
    if not isinstance(data, dict):
        logger.error(f"Could not parse plist output of diskutil for image ({name}): {data}")
        return False

    disks_data = data.get("AllDisksAndPartitions")
    if not isinstance(disks_data, list):
        logger.error(f"Could not get AllDisksAndPartitions from output of diskutil for image ({name}): {data}")
        return False

    if len(disks_data) < 1:
        logger.error(f"No elements in array ({name}): {disk_data}")
        return False
    if len(disks_data) > 1:
        logger.error(f"Too many elements in array (should have only one) ({name}): {disk_data}")
        return False

    disk_data = disks_data[0]

    disk_content = disk_data.get("Content")
    if not isinstance(disk_content, str):
        logger.error(f"Could not get Content from output of diskutil for image ({name}): {data}")
        return False

    if format == Format.MBR and len(config.partitions) == 0:
        # It throws an error for empty MBR partition tables
        return True
    
    if disk_content != expected_content:
        logger.error(f"Expected format ({expected_content}) does not match actual format: {disk_content} ({name})")
        return False

    disk_size = disk_data.get("Size")
    if not isinstance(disk_size, int):
        logger.error(f"Could not get Size from output of diskutil for image ({name}): {data}")
        return False

    if disk_size != config.size:
        logger.error(f"Expected Size ({config.size}) does not match actual size: {disk_size} ({name})")
        return False

    disk_partitions = disk_data.get("Partitions", [])
    if len(disk_partitions) != len(config.partitions):
        logger.error(f"Expected partition count ({len(config.partitions)}) does not match actual partitio count: {len(disk_partitions)} ({name})")

    index: int = 0
    for disk_partition, partition in zip(disk_partitions, config.partitions):
        # TODO: Also check start by maybe reading or something

        if not isinstance(disk_partition, dict):
            logger.warning(f"Could not get partition {index} from output of diskutil for image ({name}): {data}")
            continue

        partition_content = disk_partition.get("Content")
        if not isinstance(partition_content, str):
            logger.warning(f"Could not get Content of partition {index} ({name}): {data}")
            continue

        partition_size = disk_partition.get("Size")
        if not isinstance(partition_size, int):
            logger.warning(f"Could not get Size of partition {index} ({name}): {data}")
            continue

        if partition.type is not None:
            match partition.type:
                case "unknown": expected_partition_content = "0x7F"
                case "fat12": expected_partition_content = ["DOS_FAT_12"]
                case "fat16": expected_partition_content = ["DOS_FAT_16_S", "DOS_FAT_16", "Windows_FAT_16"]
                case "fat32": expected_partition_content = ["DOS_FAT_32", "Windows_FAT_32"]
                case _:
                    logger.error(f"Unknown type of partition {index} in diskutil tests: {partition.type}")
                    continue

            if partition_content not in expected_partition_content:
                logger.error(f"Expected format of partition {index} ({expected_partition_content}) does not match actual format of partition {index}: {partition_content} ({name})")
                return False

        if partition_size != partition.size:
            logger.error(f"Expected size of partition {index} ({partition.size}) does not match actual format of partition {index}: {partition_size} ({name})")
            return False

        index += 1

    return True


def check_with_tsk(logger: logging.Logger, image_path: Path, config: Config, format: Format, name: str) -> bool:
    import pytsk3

    match format:
        case Format.MBR: expected_vstype = pytsk3.TSK_VS_TYPE_DOS
        case Format.GPT: expected_vstype = pytsk3.TSK_VS_TYPE_GPT
        case _:
            logger.error(f"Invalid format for tsk tests: {format}")
            return False

    try:
        image = pytsk3.Img_Info(str(image_path))
    except IOError as error:
        logger.error(f"Could not open image {name}: {error}")
        return False

    if image.get_size() != config.size:
        logger.error(f"Size of image {name} does not match")
        return False

    try:
        volume_info = pytsk3.Volume_Info(image)
    except IOError as error:
        # It throws an error for empty MBR partition tables
        if format == Format.MBR and len(config.partitions) == 0:
            return True
        
        logger.error(f"Could not find partition table {name}: {error}")
        return False

    vstype = volume_info.info.vstype
    if vstype != expected_vstype:
        logger.error(f"Expected type and found type of partition table do not match for {name}. Expected: {expected_vstype}; Found: {vstype}")

    sector_size = volume_info.info.block_size

    index: int = 0
    for partition in volume_info:
        if not (partition.flags & pytsk3.TSK_VS_PART_FLAG_ALLOC):
            continue

        if index >= len(config.partitions):
            continue

        config_partition = config.partitions[index]

        pstart = partition.start * sector_size
        if pstart != config_partition.start:
            logger.error(f"Start of partition {index} does not match. Expected: {config_partition.start}; Found: {pstart}")
            return False

        psize = partition.len * sector_size
        if psize != config_partition.size:
            logger.error(f"Size of partition {index} does not match. Expected: {config_partition.size}; Found: {psize}")
            return False

        # TODO: Type and bootable

        index += 1

    if index > len(config.partitions):
        logger.error(f"More partitions found than expected. Expected: {len(config.partitions)}, Found: {index}")
        return False

    if index < len(config.partitions):
        logger.error(f"Less partitions found than expected. Expected: {len(config.partitions)}, Found: {index}")
        return False

    return True


def test(logger: logging.Logger, context: TestContext, test_class: TESTCLASS, phx: Path, test_dir: Path, test_build_dir: Path) -> bool:
    build_dir = test_build_dir / "partition"
    partition_dir = test_dir / "partition"

    image_map_path = build_dir / "image_map.json"

    build_dir.mkdir(parents=True, exist_ok=True)

    # TODO: On Linux also check fdisk, sfdisk, etc. or similar on other OS

    # TODO: Add tests for reading images
    # TODO: Add tests for removing partitions

    # TODO: Actually do different tests depending on test class
    if test_class == TESTCLASS.NONE:
        return True

    logger.info("Starting partition tests")

    formats = [Format.MBR] # TODO

    image_configs = [p for p in partition_dir.rglob("*.toml") if p.is_file()]
    image_map: dict[str, int] = {}

    data_in_file = build_dir / f"tmp-in.bin"
    data_out_file = build_dir / f"tmp-out.bin"
    bootsector_file = build_dir / f"tmp-bootsector.bin"

    if context.cleanup_artifacts:
        tmp_image = build_dir / "tmp-image.img"

    failed: bool = False
    for i, config_file in enumerate(image_configs, start=1):
        try:
            with config_file.open("rb") as f:
                config_data = tomllib.load(f)
        except Exception as e:
            logger.warning(f"Could not read {config_file}: {e}")
            continue

        config_data_image = config_data.get("image")
        if not isinstance(config_data_image, dict):
            logger.warning(f"{'No' if config_data_image is None else 'Invalid'} 'image' field in {config_file}")
            continue

        size = config_data_image.get("size")
        if not isinstance(size, int) or isinstance(size, bool):
            logger.warning(f"{'No' if size is None else 'Invalid'} size specified in {config_file}")
            continue

        bootsector = config_data_image.get("bootsector")
        if isinstance(bootsector, str):
            bootsector = Path(bootsector)
        elif bootsector is not None:
            logger.warning(f"Invalid bootsector specified in {config_file}")
            continue


        config = Config(size, bootsector, [])


        config_partitions = config_data.get("partitions")
        if config_partitions is None: config_partitions = []
        if not isinstance(config_partitions, list):
            logger.warning(f"Invalid partitions specified in {config_file}")
            continue

        skip = False
        for config_partition in config_partitions:
            if not isinstance(config_partition, dict):
                logger.warning(f"Invalid partitions specified in {config_file}")
                skip = True
                break

            config_partition_type = config_partition.get("type", None)
            if not isinstance(config_partition_type, str) and config_partition_type is not None:
                logger.warning(f"Invalid type of partition specified in {config_file}")
                skip = True
                break

            config_partition_start = config_partition.get("start")
            if not isinstance(config_partition_start, int) or isinstance(config_partition_start, bool):
                logger.warning(f"{'No' if config_partition_start is None else 'Invalid'} start of partition specified in {config_file}")
                skip = True
                break

            config_partition_size = config_partition.get("size")
            if not isinstance(config_partition_size, int) or isinstance(config_partition_size, bool):
                logger.warning(f"{'No' if config_partition_size is None else 'Invalid'} size of partition specified in {config_file}")
                skip = True
                break

            config_partition_bootable = config_partition.get("bootable", False)
            if not isinstance(config_partition_bootable, bool):
                logger.warning(f"Invalid bootable field of partition specified in {config_file}")
                skip = True
                break

            config.partitions.append(Config_Partition(
                config_partition_type,
                config_partition_start,
                config_partition_size,
                config_partition_bootable
            ))

        if skip: continue

        image_map[str(config_file.relative_to(partition_dir).as_posix())] = i

        image_name = f"image{i}"

        for format in formats:
            format_str = format_str_map.get(format)
            if not format_str: format_str = "invalid"

            if context.cleanup_artifacts:
                image = tmp_image
            else:
                image = build_dir / f"{image_name}-{format_str}.img"
            
            if not create_image(logger, phx, image, format, config.size):
                logger.error(f"Could not create image for {config_file}")
                failed = True
                continue

            if config.bootsector is not None:
                bootsector_size = get_in_file(config_file.parent / config.bootsector, bootsector_file)

                if bootsector_size < 512:
                    logger.error(f"Bootsector file ({bootsector_size}) too small")
                    failed = True
                    continue

                if bootsector_size > 512:
                    logger.warning(f"Bootsector file ({bootsector_size}) too big")

                if not set_bootsector(logger, phx, image, bootsector_file):
                    logger.error(f"Could not set bootsector of image for {config_file}")
                    failed = True
                    continue

            local_failed = False
            for i, partition in enumerate(config.partitions, start=1):
                if not add_partition(
                    logger,
                    phx,
                    image,
                    i,
                    partition.start,
                    partition.size,
                    partition.type,
                    partition.bootable
                ):
                    logger.error(f"Could not add partition to image for {config_file}")
                    local_failed = True
                    break

                get_random_file(data_in_file, partition.size)

                if not write_partition(logger, phx, image, i, data_in_file):
                    logger.error(f"Could not write data to partition for {config_file}")
                    local_failed = True
                    continue

                if not read_partition(logger, phx, image, i, data_out_file):
                    logger.error(f"Could not read data from partition for {config_file}")
                    local_failed = True
                    continue

                if filecmp.cmp(str(data_in_file), str(data_out_file), shallow=False):
                    logger.debug(f"Round trip for partition {i} of {config_file} successful")
                else:
                    logger.error(f"Round trip for partition {i} of {config_file} failed")
                    local_failed = True
                    continue

            if local_failed:
                failed = True
                continue

            if context.require_mount:
                device_path = mount(logger, image, context.host_os)
                if not device_path:
                    logger.error(f"Mounting failed")
                    failed = True
                    continue


            if context.use_diskutil:
                logger.debug("Checking with diskutil")
                if not check_with_diskutil(logger, device_path, config, format, str(config_file)):
                    logger.error(f"diskutil check failed for {config_file}")
                    failed = True


            if context.use_tsk:
                logger.debug("Checking with TSK")
                if not check_with_tsk(logger, image, config, format, str(config_file)):
                    logger.error(f"TSK check failed for {config_file}")
                    failed = True


            if context.require_mount:
                if not unmount(logger, device_path, context.host_os):
                    logger.error(f"Unmounting failed")
                    failed = True
                    continue

    bootsector_file.unlink(missing_ok=True)
    data_in_file.unlink(missing_ok=True)
    data_out_file.unlink(missing_ok=True)

    if context.cleanup_artifacts:
        tmp_image.unlink(missing_ok=True)
    else:
        image_map_list = sorted(image_map.items(), key=lambda item: item[1])
        image_map = dict(image_map_list)

        with image_map_path.open("w", encoding="utf-8") as f:
            json.dump(image_map, f, indent=4, ensure_ascii=False)

    logger.info("Finished partition tests")

    return not failed
