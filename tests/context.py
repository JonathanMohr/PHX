from dataclasses import dataclass

@dataclass(frozen=True)
class TestContext:
    use_tsk: bool
