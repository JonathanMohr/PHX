import logging

BUILD_LEVEL = 15
logging.addLevelName(BUILD_LEVEL, "BUILD")
def build_level(self, message, *args, **kwargs):
    if self.isEnabledFor(BUILD_LEVEL):
        self._log(BUILD_LEVEL, message, args, **kwargs)
logging.Logger.build = build_level
