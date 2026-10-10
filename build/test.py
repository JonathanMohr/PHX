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

    context = TestContext(
        use_tsk=True,
        cleanup_artifacts=False
    )

    if not disk.test(logger, context, test_class, phx, test_dir, test_build_dir):
        return False

    if not partition.test(logger, context, test_class, phx, test_dir, test_build_dir):
        return False

    if not filesystem.test(logger, context, test_class, phx, test_dir, test_build_dir):
        return False

    return True
