#!/usr/bin/env python3
"""
Merge clang -MJ fragments into a compile_commands.json,
remapping container paths.

usage: cdb.py FRAG_DIR OUT_JSON [OLD=NEW ...]
"""
import glob
import json
import os
import sys
from typing import NotRequired, TypedDict, cast


class Entry(TypedDict):
    directory: str
    file: str
    arguments: list[str]
    output: NotRequired[str]


def fix(s: str, maps: list[tuple[str, str]]) -> str:
    for old, new in maps:
        s = s.replace(old, new)
    return s


def main(argv: list[str]) -> None:
    if len(argv) < 2:
        sys.exit(__doc__)

    frag_dir, out, *raw_maps = argv

    maps: list[tuple[str, str]] = []
    for m in raw_maps:
        old, _, new = m.partition("=")
        maps.append((old, new))

    entries: list[Entry] = []
    for path in sorted(glob.glob(os.path.join(frag_dir, "*.json"))):
        with open(path) as fh:
            # -MJ fragments are one object followed by a trailing comma
            e = cast(Entry, json.loads(fh.read().strip().rstrip(",")))

        e["directory"] = fix(e["directory"], maps)
        e["file"] = fix(e["file"], maps)
        if "output" in e:
            e["output"] = fix(e["output"], maps)
        e["arguments"] = [fix(a, maps) for a in e["arguments"]]
        entries.append(e)

    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w") as fh:
        json.dump(entries, fh, indent=1)


if __name__ == "__main__":
    main(sys.argv[1:])
