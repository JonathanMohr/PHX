import sys

CODES = {chr(97 + i): i for i in range(26)}
CODES.update({" ": 26, ".": 27, "!": 28, "?": 29, "\n": 30, "\r": 31})

def pack(s: str):
    s = s.replace("\\r", "\r").replace("\\n", "\n").lower()

    bad = sorted({c for c in s if c not in CODES})
    if bad:
        sys.exit(f"Invalid characters: {bad}")
    if not s:
        sys.exit("String is empty")

    tail = ""
    while s and s[-1] in " \r\n":
        tail = s[-1] + tail
        s = s[:-1]
    s += " " * (-(len(s) + len(tail)) % 3) + tail

    words = []
    for i in range(0, len(s), 3):
        a, b, c = [CODES[ch] for ch in s[i:i + 3]]
        last = 1 if i + 3 >= len(s) else 0
        words.append((a << 11) | (b << 6) | (c << 1) | last)

    return words

if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("Usage: python gpt.py \"text\" [label]")
    words = pack(sys.argv[1])

    code = "dw " + ", ".join(f"0x{w:04x}" for w in words)

    if len(sys.argv) > 2:
        print(f"{sys.argv[2]} {code}")
    else:
        print(code)
