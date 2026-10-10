from pathlib import Path
from typing import Optional, Dict
import json
import logging
import hashlib

class BuildCache:
    def __init__(self, cache_file: Path, logger: logging.Logger):
        self.cache_file = cache_file

        self.hashes: Dict[str, str] = {}

        self.logger = logger

        self.load()

    def load(self):
        if self.cache_file.exists():
            try:
                data = json.loads(self.cache_file.read_text())
                self.hashes = data.get("hashes", {})
            except Exception:
                self.logger.warning(f"Failed to load build cache from {self.cache_file}")
                self.hashes = {}

    def save(self):
        data = {
            "hashes": self.hashes
        }
        self.cache_file.parent.mkdir(parents=True, exist_ok=True)
        self.cache_file.write_text(json.dumps(data, indent=4, ensure_ascii=False))

    def get(self, target: Path) -> Optional[str]:
        return self.hashes.get(str(target))

    def update(self, target: Path, hash_value: str):
        self.hashes[str(target)] = hash_value

    def is_up_to_date(self, target: Path, hash_value: str) -> bool:
        if not target.exists():
            return False
        return self.get(target) == hash_value

def hash_file(file: Path, extra: list[str]) -> str:
    hasher = hashlib.new("sha256")

    for string in extra:
        hasher.update(string.encode())

    with file.open("rb") as f:
        while chunk := f.read(8192):
            hasher.update(chunk)

    return hasher.hexdigest()

def hash_files(files: list[Path], extra: list[str], logger: logging.Logger) -> str:
    hasher = hashlib.new("sha256")

    for string in extra:
        hasher.update(string.encode())

    for file in sorted(files, key=lambda f: str(f)):
        if not file.exists():
            logger.warning(f"{file} does not exist")
            continue

        with file.open("rb") as f:
            while chunk := f.read(8192):
                hasher.update(chunk)
                
    return hasher.hexdigest()
