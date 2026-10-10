from pathlib import Path
import re
import random

def get_in_file(src_file: Path, out_file: Path, chunk_size: int = 65536) -> int:
    match src_file.suffix:
        case ".txt":
            written = 0
            with open(src_file, "r", encoding="utf-8") as src, open(out_file, "wb") as dst:
                while chunk := src.read(chunk_size):
                    written += dst.write(chunk.encode("utf-8"))
            return written

        case ".hex":
            _NON_HEX = re.compile(r"[^0-9a-fA-F]")

            written = 0
            with open(src_file, "r", encoding="utf-8", errors="ignore") as src, open(out_file, "wb") as dst:
                carry = ""
                while chunk := src.read(chunk_size):
                    data = carry + _NON_HEX.sub("", chunk)
                    if len(data) % 2:
                        carry, data = data[-1], data[:-1]
                    else:
                        carry = ""
                    written += dst.write(bytes.fromhex(data))
            return written

        case _:
            return 0

def get_random_file(out_file: Path, n, chunk_size: int = 65536):
    rng = random.Random()
    with out_file.open("wb") as f:
        remaining = n
        while remaining > 0:
            size = min(chunk_size, remaining)
            f.write(rng.randbytes(size))
            remaining -= size
