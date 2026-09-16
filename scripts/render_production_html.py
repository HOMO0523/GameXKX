"""Render a production markdown report to a self-contained HTML page.

Uses the same ink-and-paper palette as the existing design-table page
(`export_game_analysis_html.py`) so generated reports look like one family:

    --ink #251a12   dark ink        --paper #f1ddb5   aged paper
    --red #973e30   seal red        --line  #ba9060   hairline
    headings in KaiTi

The page is a single file with no external assets: it opens straight from disk and
can be attached to a chat message.

Usage:
    python scripts/render_production_html.py --input docs/production/x.md \
        --output docs/production/x.html --title "页面标题"
"""
from __future__ import annotations

import argparse
import html
import re
from pathlib import Path

import markdown

ROOT = Path(__file__).resolve().parents[1]

FRONT_MATTER = re.compile(r"\A---\r?\n.*?\r?\n---\r?\n", re.S)

STYLE = """
:root{
  --ink:#251a12; --muted:#705d49; --paper:#f1ddb5; --paper2:#fff8e9;
  --line:#ba9060; --red:#973e30; --green:#2f6b4b; --blue:#345f79; --shadow:#0006;
}
*{box-sizing:border-box}
body{margin:0;background:#17110d;color:var(--ink);
     font:15px/1.75 "Microsoft YaHei","PingFang SC",sans-serif}
a{color:var(--blue)}
.wrap{max-width:1500px;margin:auto;padding:24px;display:grid;
      grid-template-columns:266px minmax(0,1fr);gap:22px;align-items:start}
nav.toc{position:sticky;top:24px;max-height:calc(100vh - 48px);overflow:auto;
        background:linear-gradient(135deg,#f8edd3,#e6c894);border:1px solid var(--line);
        border-radius:15px;box-shadow:0 12px 30px var(--shadow);padding:16px 18px}
nav.toc h2{font-family:KaiTi,serif;margin:0 0 10px;font-size:19px;color:var(--red)}
nav.toc ul{list-style:none;margin:0;padding-left:0}
nav.toc li{margin:2px 0}
nav.toc ul ul{padding-left:14px;font-size:13.5px}
nav.toc a{display:block;padding:3px 8px;border-radius:7px;text-decoration:none;
          color:var(--ink);border-left:3px solid transparent}
nav.toc a:hover{background:#fff8e7cc;border-left-color:var(--red)}
main{background:linear-gradient(135deg,#f8edd3,#e6c894);border:1px solid var(--line);
     border-radius:15px;box-shadow:0 12px 30px var(--shadow);padding:30px 34px;min-width:0}
h1,h2,h3,h4{font-family:KaiTi,"Microsoft YaHei",serif;margin:1.1em 0 .45em;line-height:1.35}
h1{font-size:30px;margin-top:0;padding-bottom:12px;border-bottom:3px double var(--line)}
h2{font-size:24px;color:var(--red);padding-left:12px;border-left:6px solid var(--red)}
h3{font-size:19px;color:#4a3425;background:#fff6e2cc;border:1px solid #d8b483;
   border-radius:9px;padding:8px 12px}
h4{font-size:16.5px;color:var(--muted)}
p{margin:.7em 0}
code{background:#fff8e7;border:1px solid #d9bb91;border-radius:5px;padding:1px 5px;
     font-family:Consolas,"Courier New",monospace;font-size:13.5px;color:#7a3a26;
     word-break:break-word;overflow-wrap:anywhere}
pre{background:#2b211a;color:#f4e6cd;border-radius:11px;padding:14px 16px;overflow:auto;
    border:1px solid #5b4433}
pre code{background:none;border:none;color:inherit;padding:0;word-break:normal}
blockquote{margin:.9em 0;background:#fff5dc;border-left:5px solid #a96f33;
           border-radius:8px;padding:11px 16px;color:var(--muted)}
blockquote p{margin:.3em 0}
.tablewrap{overflow-x:auto;margin:1em 0;border:1px solid #c9a16f;border-radius:9px}
table{width:100%;border-collapse:collapse;margin:0;font-size:14px}
th{background:#4a3425;color:#fff4dc;text-align:left;padding:9px 11px;font-weight:600;
   position:sticky;top:0;white-space:nowrap}
td{padding:8px 11px;border-top:1px solid #d3b184;vertical-align:top}
tbody tr:nth-child(even){background:#fff8e7aa}
tbody tr:hover{background:#fff3d6}
th:first-child,td:first-child{white-space:nowrap}
ul,ol{padding-left:1.6em}
li{margin:.25em 0}
hr{border:none;border-top:2px dashed var(--line);margin:1.6em 0}
strong{color:#7a2f22}
.meta{color:var(--muted);font-size:13px;text-align:right;margin-top:26px;
      border-top:2px dashed var(--line);padding-top:12px}
@media(max-width:1080px){
  .wrap{grid-template-columns:1fr;padding:14px}
  nav.toc{position:static;max-height:none}
  main{padding:22px 18px}
  th:first-child,td:first-child{white-space:normal}
}
"""

TEMPLATE = """<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{title}</title>
<style>{style}</style></head>
<body><div class="wrap">
<nav class="toc"><h2>目录</h2>{toc}</nav>
<main>
{body}
<div class="meta">由 <code>scripts/render_production_html.py</code> 从
<code>{source}</code> 生成 · {generated}</div>
</main></div></body></html>
"""


def render(md_path: Path, out_path: Path, title: str | None, generated: str) -> None:
    text = md_path.read_text(encoding="utf-8")
    text = FRONT_MATTER.sub("", text)          # drop YAML front matter
    md = markdown.Markdown(
        extensions=["tables", "fenced_code", "toc", "sane_lists", "attr_list"],
        extension_configs={"toc": {"toc_depth": "2-3", "permalink": False}},
    )
    body = md.convert(text)
    # Bare <table> would be clipped by the card's rounded corners; give every table a
    # horizontal scroll container so wide content scrolls instead of disappearing.
    body = body.replace("<table>", '<div class="tablewrap"><table>').replace(
        "</table>", "</table></div>"
    )
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(
        TEMPLATE.format(
            title=html.escape(title or md_path.stem),
            style=STYLE,
            toc=md.toc or "",
            body=body,
            source=html.escape(md_path.relative_to(ROOT).as_posix()),
            generated=generated,
        ),
        encoding="utf-8",
        newline="\n",
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--input", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--title", default=None)
    ap.add_argument("--generated", default="2026-09-16")
    args = ap.parse_args()

    src = args.input if args.input.is_absolute() else ROOT / args.input
    dst = args.output if args.output.is_absolute() else ROOT / args.output
    if not src.is_file():
        raise SystemExit(f"input not found: {src}")
    render(src, dst, args.title, args.generated)
    print(f"{dst}  ({dst.stat().st_size/1024:.1f} KB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
