#!/usr/bin/env python3
"""Hooks regen.sh adds to the generated code after every regeneration (run from the repo root).

    python3 port/tools/patch_gen.py port/src/recomp/gen

Overrides normally replace whole functions (--exclude-manual). A hook that must sit inside one, at an instruction,
goes here instead, so a fresh regeneration keeps it. Each patch is found by a unique label, applied once
(re-running changes nothing), and a missing anchor fails loudly instead of silently dropping the hook.
"""
import pathlib
import sys

# (function, label the hook goes after, declaration, hook line)
PATCHES = [
    # none yet for PoA
]


def main():
    gen = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "port/src/recomp/gen")
    files = sorted(gen.glob("recomp_*.c"))
    for func, label, decl, hook in PATCHES:
        head = f"void {func}(void)\n{{"
        for path in files:
            text = path.read_text(encoding="utf-8", newline="")
            nl = "\r\n" if "\r\n" in text else "\n"
            t = text.replace("\r\n", "\n")
            start = t.find(head)
            if start < 0:
                continue
            end = t.find("\n}\n", start)
            body = t[start:end]
            if hook.strip().split(";")[0] in body:  # the call itself, whatever comment follows it
                print(f"patch_gen: {func} already hooked ({path.name})")
                break
            at = body.find("\n" + label + "\n")
            if at < 0:
                sys.exit(f"patch_gen: {label!r} not found in {func} ({path.name}): the hook needs a new anchor")
            at += len(label) + 2
            body = body[:at] + hook + "\n" + body[at:]
            t = t[:start] + decl + "\n" + body + t[end:]
            path.write_text(t.replace("\n", nl), encoding="utf-8", newline="")
            print(f"patch_gen: {func} hooked ({path.name})")
            break
        else:
            sys.exit(f"patch_gen: {func} not found in {gen}")


if __name__ == "__main__":
    main()
