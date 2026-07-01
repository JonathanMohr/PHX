from enum import Enum
from dataclasses import dataclass
import logging

from ci.cache import BuildCache
from ci.compileCommands import CompileCommands

class OS(Enum):
    Windows = 1
    macOS = 2
    Linux = 3

class ARCH(Enum):
    x86_64 = 1
    arm64 = 2


class OPTIMIZATION(Enum):
    NONE = 1
    SPEED = 2
    SIZE = 3

class PORTABILITY(Enum):
    MACHINE = 1
    PORTABLE = 2

class LINKING(Enum):
    STATIC = 1
    DYNAMIC = 2


class HOST(Enum):
    HOSTED = 1
    FREESTANDING = 2

@dataclass()
class BuildMode:
    target_os: OS
    target_arch: ARCH

    werror: bool

    lto: bool

    pic: bool
    hidden: bool

    optimization: OPTIMIZATION
    portability: PORTABILITY
    linking: LINKING
    
    assertions: bool
    sanitizers: bool
    debuginfo: bool

    host: HOST

    sysroot: str | None

    project_root: str

@dataclass()
class BuildContext:
    logger: logging.Logger
    buildCache: BuildCache
    compileCommands: CompileCommands
