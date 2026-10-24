from build.defs import TESTCLASS, OS
from tests.context import TestContext

from pathlib import Path
import logging
import shutil

import tests.disk as disk
import tests.partition as partition
import tests.filesystem as filesystem

def test(logger: logging.Logger, project_dir: Path, build_dir: Path, test_class: TESTCLASS, phx: Path, host_os: OS) -> bool:
    test_build_dir = build_dir / "tests"
    test_dir = project_dir / "tests"

    context = TestContext(
        host_os=host_os,

        require_mount=False,

        use_tsk=False,
        use_diskutil=False,

        cleanup_artifacts=False
    )

    try:
        import pytsk3

        logger.debug("Using TSK")
        context.use_tsk = True
    except ImportError:
        logger.debug("Not using TSK")
        logger.debug("Install python module pytsk3 to test with TSK")
        context.use_tsk = False


    if host_os == OS.macOS:
        if shutil.which("diskutil") is None:
            logger.warning("Could not find diskutil")
        else:
            context.use_diskutil = True


    if context.use_diskutil:
        context.require_mount = True


    if not disk.test(logger, context, test_class, phx, test_dir, test_build_dir):
        return False

    if not partition.test(logger, context, test_class, phx, test_dir, test_build_dir):
        return False

    if not filesystem.test(logger, context, test_class, phx, test_dir, test_build_dir):
        return False

    return True
