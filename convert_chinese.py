"""Convert Chinese chars in C string literals to octal escapes for MSVC compatibility.
Octal escapes are safer than hex because they consume at most 3 digits,
so a following ASCII digit won't be greedily absorbed."""
import re

SRC = r"C:\Users\LI\WorkBuddy\2026-05-28-19-53-09\film-sim-plugin\ofx\DuXunFilm\DuXunFilmSim.cpp"

with open(SRC, "r", encoding="utf-8") as f:
    content = f.read()

result = []
in_string = False
escape_next = False
current = []

for ch in content:
    if escape_next:
        current.append(ch)
        escape_next = False
        continue

    if ch == '\\' and in_string:
        current.append(ch)
        escape_next = True
        continue

    if ch == '"':
        if in_string:
            result.append(''.join(current))
            result.append('"')
            current = []
            in_string = False
        else:
            result.append('"')
            current = []
            in_string = True
        continue

    if ch == '\n' or ch == '\r':
        if in_string:
            result.append(''.join(current))
            result.append(ch)
            current = []
            in_string = False
        else:
            result.append(ch)
        continue

    if in_string:
        if ord(ch) > 127:
            for b in ch.encode('utf-8'):
                current.append(f'\\{b:03o}')  # 3-digit octal escape
        else:
            current.append(ch)
    else:
        result.append(ch)

final = ''.join(result)

with open(SRC, "w", encoding="utf-8") as f:
    f.write(final)

orig_count = sum(1 for c in content if ord(c) > 127)
new_count = sum(1 for c in final if ord(c) > 127)
print(f"Done. Original non-ASCII chars: {orig_count}, remaining: {new_count}")
