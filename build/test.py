from build.defs import TESTCLASS
from tests.context import TestContext

from pathlib import Path
import logging

import tests.disk as disk
import tests.partition as partition
import tests.filesystem as filesystem

def test(logger: logging.Logger, project_dir: Path, build_dir: Path, test_class: TESTCLASS, phx: Path) -> bool:
    test_build_dir = build_dir / "tests"
    test_dir = project_dir / "tests"

    try:
        import pytsk3

        logger.debug("Using TSK")
        use_tsk = True
    except ImportError:
        logger.debug("Not using TSK")
        logger.debug("Install python module pytsk3 to test with TSK")
        use_tsk = False

    context = TestContext(
        use_tsk=use_tsk,
        cleanup_artifacts=False
    )

    if not disk.test(logger, context, test_class, phx, test_dir, test_build_dir):
        return False

    if not partition.test(logger, context, test_class, phx, test_dir, test_build_dir):
        return False

    if not filesystem.test(logger, context, test_class, phx, test_dir, test_build_dir):
        return False

    return True
