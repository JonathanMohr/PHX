from build.defs import TESTCLASS

from pathlib import Path
import logging

def test(logger: logging.Logger, test_class: TESTCLASS, phx: Path, test_dir: Path) -> bool:
    build_dir = test_dir / ".build" / "partition"
    partition_dir = test_dir / "partition"
    
    build_dir.mkdir(parents=True, exist_ok=True)

    # TODO

    return True
