from build.defs import TESTCLASS
from tests.context import TestContext

from pathlib import Path
import logging

def test(logger: logging.Logger, context: TestContext, test_class: TESTCLASS, phx: Path, test_dir: Path, test_build_dir: Path) -> bool:
    build_dir = test_build_dir / "filesystem"
    filesystem_dir = test_dir / "filesystem"
    
    build_dir.mkdir(parents=True, exist_ok=True)

    # TODO

    return True
