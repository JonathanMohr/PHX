from dataclasses import dataclass
from pathlib import Path
from enum import Enum
import json

class Language(Enum):
    UNKNOWN = 0
    C = 1
    CPP = 2

class CompileCommands:
    @dataclass
    class Command:
        Tool: str
        Lang: Language
        source: Path
        out: Path
        Args: list[str]

    def __init__(self):
        self.commands: list[CompileCommands.Command] = []

    def add(self, tool: str, lang: Language, source: Path, out: Path, args: list[str]):
        self.commands.append(
            CompileCommands.Command(
                Tool=tool,
                Lang=lang,
                source=source,
                out=out,
                Args=args
            )
        )

    def __str__(self):
        return "\n".join(
            f"{cmd.Lang}"
            for cmd in self.commands
        )
    
    def write(self, file: Path):
        data = []

        cwd = Path.cwd()

        for command in self.commands:
            data.append({
                "directory": str(cwd),
                "command": f"{command.Tool} {' '.join(command.Args)}",
                "file": str(command.source)
            })

        file.write_text(json.dumps(data, indent=4, ensure_ascii=False))
