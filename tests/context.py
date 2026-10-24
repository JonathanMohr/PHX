from build.defs import OS
from dataclasses import dataclass

@dataclass()
class TestContext:
    host_os: OS

    require_mount: bool

    use_tsk: bool

    use_diskutil: bool

    cleanup_artifacts: bool
