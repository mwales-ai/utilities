#!/usr/bin/env python3
"""
reflow_md_tables.py - Re-align the pipes in Markdown (GFM) tables.

Finds every table in a Markdown file and pads the cells so the column
separators line up.

Rules:
  * A table starts with a header row followed by a delimiter row
    (e.g. |---|:---:|---:|).
  * The table ends at a blank line or at any line without a pipe character.
  * The header row sets the number of columns. Short rows are filled with
    empty cells. Extra cells on a row are dropped if they are empty;
    otherwise they are kept as-is and a warning is printed (no new columns
    are ever invented).
  * Column widths come from the widest cell in each column.
  * If a row cannot fit within the page width (default 120), that row is
    written compactly (no padding) and is allowed to run long. The other
    rows are still aligned with each other.
  * Column alignment markers (:--, :-:, --:) are kept and applied to the
    padding.
  * Tables inside fenced code blocks (``` or ~~~) are left untouched.

Usage:
  reflow_md_tables.py README.md                 # print result to stdout
  reflow_md_tables.py -i docs/*.md              # rewrite files in place
  reflow_md_tables.py -w 100 -i notes.md        # custom page width
  reflow_md_tables.py --check docs/*.md         # exit 1 if anything would change
  cat file.md | reflow_md_tables.py -           # read stdin
"""

import argparse
import re
import sys
import unicodedata

FENCE_RE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")
DELIM_CELL_RE = re.compile(r"^:?-+:?$")
MIN_COL_WIDTH = 3  # GFM delimiter cells need at least 3 chars to look sane


# ---------------------------------------------------------------- helpers

def display_width(text):
    """Width of text in a monospace editor (wide CJK/emoji count as 2)."""
    width = 0
    for ch in text:
        if unicodedata.combining(ch):
            continue
        width += 2 if unicodedata.east_asian_width(ch) in ("W", "F") else 1
    return width


def split_row(line):
    """Split a table row into raw cells, honoring escaped pipes (\\|)."""
    s = line.strip()
    cells, buf = [], []
    i = 0
    ended_with_pipe = False
    while i < len(s):
        ch = s[i]
        if ch == "\\" and i + 1 < len(s):
            buf.append(s[i:i + 2])
            i += 2
            ended_with_pipe = False
            continue
        if ch == "|":
            cells.append("".join(buf))
            buf = []
            ended_with_pipe = True
        else:
            buf.append(ch)
            ended_with_pipe = False
        i += 1
    cells.append("".join(buf))

    if s.startswith("|"):
        cells.pop(0)
    if ended_with_pipe and cells:
        cells.pop()
    return [c.strip() for c in cells]


def has_pipe(line):
    """True if the line contains at least one unescaped pipe."""
    escaped = False
    for ch in line:
        if escaped:
            escaped = False
        elif ch == "\\":
            escaped = True
        elif ch == "|":
            return True
    return False


def is_delimiter_row(line):
    if not has_pipe(line):
        return False
    cells = split_row(line)
    return bool(cells) and all(DELIM_CELL_RE.match(c.replace(" ", "")) for c in cells)


def is_table_start(header_line, next_line):
    return (
        bool(header_line.strip())
        and has_pipe(header_line)
        and is_delimiter_row(next_line)
    )


def parse_align(cell):
    cell = cell.replace(" ", "")
    left, right = cell.startswith(":"), cell.endswith(":")
    if left and right:
        return "center"
    if right:
        return "right"
    if left:
        return "left"
    return None


def pad_cell(text, width, align):
    gap = width - display_width(text)
    if gap <= 0:
        return text
    if align == "right":
        return " " * gap + text
    if align == "center":
        left = gap // 2
        return " " * left + text + " " * (gap - left)
    return text + " " * gap


def delimiter_cell(width, align):
    if align == "center":
        return ":" + "-" * (width - 2) + ":"
    if align == "left":
        return ":" + "-" * (width - 1)
    if align == "right":
        return "-" * (width - 1) + ":"
    return "-" * width


def join_cells(indent, cells):
    return indent + "| " + " | ".join(cells) + " |"


# ---------------------------------------------------------- table logic

def format_table(lines, page_width, warn, first_lineno):
    indent = re.match(r"\s*", lines[0]).group()

    header = split_row(lines[0])
    ncols = len(header)

    aligns = [parse_align(c) for c in split_row(lines[1])]
    if len(aligns) != ncols:
        warn(first_lineno + 1,
             f"delimiter row has {len(aligns)} columns, header has {ncols}; "
             f"using header")
    aligns = (aligns + [None] * ncols)[:ncols]

    # rows[k] = (cells, extra_cells, source_line_number); index 0 is header
    raw_rows = [(header, first_lineno)] + [
        (split_row(l), first_lineno + 2 + k) for k, l in enumerate(lines[2:])
    ]
    rows = []
    for cells, lineno in raw_rows:
        extra = []
        if len(cells) < ncols:
            cells = cells + [""] * (ncols - len(cells))
        elif len(cells) > ncols:
            extra = cells[ncols:]
            cells = cells[:ncols]
            if any(extra):
                warn(lineno,
                     f"row has {ncols + len(extra)} cells but header has "
                     f"{ncols}; extra cells kept but not aligned")
            else:
                extra = []  # stray trailing empty cells: just drop them
        rows.append((cells, extra))

    def compact_width(cells):
        return display_width(join_cells(indent, cells))

    def widths_for(row_ids):
        return [
            max([MIN_COL_WIDTH] + [display_width(rows[r][0][c]) for r in row_ids])
            for c in range(ncols)
        ]

    def table_width(widths):
        return len(indent) + sum(widths) + 3 * ncols + 1

    # Rows that could be aligned: fit on their own and have no extra cells.
    fitting = [
        r for r, (cells, extra) in enumerate(rows)
        if not extra and compact_width(cells) <= page_width
    ]

    # If the aligned table is still too wide, evict the rows that blow up the
    # width the most until the rest fit. Evicted rows are written compactly.
    widths = widths_for(fitting)
    while fitting and table_width(widths) > page_width:
        def score(r):
            rest = [x for x in fitting if x != r]
            return (table_width(widths_for(rest)), -compact_width(rows[r][0]))
        fitting.remove(min(fitting, key=score))
        widths = widths_for(fitting)

    fitting = set(fitting)
    out = []
    for r, (cells, extra) in enumerate(rows):
        if r in fitting:
            line = join_cells(
                indent, [pad_cell(c, w, a) for c, w, a in zip(cells, widths, aligns)]
            )
        else:
            line = join_cells(indent, cells + extra)
        out.append(line)
        if r == 0:  # delimiter row goes right after the header
            out.append(join_cells(
                indent, [delimiter_cell(w, a) for w, a in zip(widths, aligns)]
            ))
    return out


def reflow(text, page_width, warn):
    eol = "\r\n" if "\r\n" in text else "\n"
    trailing_newline = text.endswith(("\n", "\r"))
    lines = text.splitlines()

    out = []
    fence = None
    i, n = 0, len(lines)
    while i < n:
        line = lines[i]
        m = FENCE_RE.match(line)

        if fence:
            if m and m.group(1)[0] == fence[0] and len(m.group(1)) >= len(fence):
                fence = None
            out.append(line)
            i += 1
            continue
        if m:
            fence = m.group(1)
            out.append(line)
            i += 1
            continue

        if i + 1 < n and is_table_start(line, lines[i + 1]):
            j = i + 2
            while j < n and lines[j].strip() and has_pipe(lines[j]):
                j += 1
            out.extend(format_table(lines[i:j], page_width, warn, i + 1))
            i = j
            continue

        out.append(line)
        i += 1

    result = eol.join(out)
    if trailing_newline:
        result += eol
    return result


# ------------------------------------------------------------------- CLI

def main(argv=None):
    p = argparse.ArgumentParser(
        description="Re-align the pipe characters in Markdown tables.")
    p.add_argument("files", nargs="+", help="Markdown file(s), or - for stdin")
    p.add_argument("-w", "--width", type=int, default=120,
                   help="page width; rows wider than this are not padded "
                        "(default: 120)")
    p.add_argument("-i", "--in-place", action="store_true",
                   help="rewrite the files instead of printing to stdout")
    p.add_argument("--check", action="store_true",
                   help="don't write anything; exit 1 if any file would change")
    args = p.parse_args(argv)

    if len(args.files) > 1 and not (args.in_place or args.check):
        p.error("multiple files need --in-place or --check")
    if "-" in args.files and args.in_place:
        p.error("cannot use --in-place with stdin")

    changed_any = False
    for path in args.files:
        def warn(lineno, msg, _path=path):
            print(f"{_path}:{lineno}: warning: {msg}", file=sys.stderr)

        if path == "-":
            text = sys.stdin.read()
        else:
            with open(path, encoding="utf-8", newline="") as f:
                text = f.read()

        new = reflow(text, args.width, warn)
        changed = new != text
        changed_any |= changed

        if args.check:
            if changed:
                print(f"{path}: tables need reflowing", file=sys.stderr)
        elif args.in_place:
            if changed:
                with open(path, "w", encoding="utf-8", newline="") as f:
                    f.write(new)
                print(f"reflowed {path}", file=sys.stderr)
        else:
            sys.stdout.write(new)

    return 1 if (args.check and changed_any) else 0


if __name__ == "__main__":
    sys.exit(main())
