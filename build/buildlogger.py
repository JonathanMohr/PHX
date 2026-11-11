from typing import Any, cast
import logging

BUILD_LEVEL = 15
logging.addLevelName(BUILD_LEVEL, "BUILD")

class BuildLogger(logging.Logger):
    def build(self, message: object, *args: object, **kwargs: Any) -> None:
        kwargs.setdefault("stacklevel", 2)
        self.log(BUILD_LEVEL, message, *args, **kwargs)

logging.setLoggerClass(BuildLogger)

def get(name: str) -> BuildLogger:
    return cast(BuildLogger, logging.getLogger(name))
