from build.defs import TESTCLASS

from pathlib import Path
import logging

import tests.disk as disk
import tests.partition as partition
import tests.filesystem as filesystem

def test(logger: logging.Logger, test_class: TESTCLASS, phx: Path) -> bool:
    project_dir = Path(".")
    test_dir = project_dir / "tests"
    
    if not disk.test(logger, test_class, phx, test_dir):
        return False

    if not partition.test(logger, test_class, phx, test_dir):
        return False

    if not filesystem.test(logger, test_class, phx, test_dir):
        return False

    return True
