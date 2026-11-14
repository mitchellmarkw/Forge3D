#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the Quick Start guide twice from one text: docs/QUICK_START.md (GitHub) and
src/app/help/quick-start.html (Help → Quick Start Guide in the app).
Run from the repository root: python3 tools/make_quick_start.py"""
import html
import re

TITLE = "Quick Start: your first print"
INTRO = ("In about ten minutes you'll make a key tag with a hole for the ring and your name "
         "standing out on top, then send it to your printer. Every number is in millimetres.")
VIEW = [
    ("Turn the view", "drag with the right mouse button"),
    ("Slide the view", "drag with the middle button (or Shift + right button)"),
    ("Zoom", "the mouse wheel"),
    ("Fit everything in view", "F (Home puts the view back to the start)"),
]
STEPS = [
    ("Add a box and size it",
     "**Build → Add Shape → Box**. In **Properties** on the right, type **Width 50**, **Depth 20** "
     "and **Height 4**, pressing Enter after each.",
     "guide-1-size.png"),
    ("Add the shape for the hole",
     "**Add Shape → Cylinder**. Make it **Radius 3** and **Height 10**, then set its **X** to **-18** so "
     "it stands up through the left end of the tag.",
     "guide-2-cylinder.png"),
    ("Pick both, the tag first",
     "Click **Box 1** in **Objects** on the left, then hold **Ctrl** and click **Cylinder 1**. "
     "Subtract keeps the first shape you pick and cuts the others out of it.",
     "guide-3-select.png"),
    ("Cut the hole",
     "Click **Subtract**. The cylinder cuts its hole and disappears. Nothing is lost: double-click a "
     "step in **Objects** any time to change it, and the tag follows.",
     "guide-4-hole.png"),
    ("Put your name on it",
     "**Design → Emboss**. Type your name, set **Letter height 9** and **Depth 1**, keep **Raised**, "
     "click the top of the tag where the words should go, then click **Emboss**.",
     "guide-5-emboss.png"),
    ("Print it",
     "**Print → Export** saves an STL or 3MF file your slicer opens. **Send to Slicer** opens it in "
     "your slicer straight away (the first time, it asks which slicer you use). If a shape needs a "
     "different face down, **Lay on Face** and click that face.",
     "guide-6-print.png"),
    ("Keep your work",
     "**File → Save** keeps a `.f3d` file with every step, so you can open it later and change the "
     "name, the hole or the size.",
     None),
]
TIPS = [
    "**Ctrl+Z** undoes anything; **Esc** cancels the tool you're in.",
    "Point at any button for a tip about what it does.",
    "**Help → Take the Tour** shows where everything is; this guide is in **Help → Quick Start Guide**.",
    "Made a mistake with the order in step 3? Undo, and pick them again the other way round.",
]


def md_inline(text):
    return text


def html_inline(text):
    text = html.escape(text, quote=False)
    text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"`(.+?)`", r"<code>\1</code>", text)
    return text


def markdown():
    out = [f"# {TITLE}", "", INTRO, "", "## Getting round the view", ""]
    out += [f"- **{what}:** {how}" for what, how in VIEW]
    out.append("")
    for i, (head, body, image) in enumerate(STEPS, 1):
        out += [f"## {i}. {head}", "", body, ""]
        if image:
            out += [f"![{head}](quick-start/{image})", ""]
    out += ["## Good to know", ""] + [f"- {t}" for t in TIPS] + [""]
    return "\n".join(out)


def page():
    parts = [
        "<html><head><meta charset='utf-8'><style>",
        "body { font-family: 'Segoe UI', sans-serif; font-size: 10.5pt; color: #2b2f33; }",
        "h1 { font-size: 16pt; color: #1f5fa8; } h2 { font-size: 12pt; margin-top: 14px; color: #1f5fa8; }",
        "code { font-family: Consolas, monospace; }",
        "</style></head><body>",
        f"<h1>{html.escape(TITLE)}</h1>",
        f"<p>{html_inline(INTRO)}</p>",
        "<h2>Getting round the view</h2><ul>",
    ]
    parts += [f"<li><b>{html.escape(what)}:</b> {html_inline(how)}</li>" for what, how in VIEW]
    parts.append("</ul>")
    for i, (head, body, image) in enumerate(STEPS, 1):
        parts.append(f"<h2>{i}. {html.escape(head)}</h2><p>{html_inline(body)}</p>")
        if image:
            parts.append(f"<p><img src='qrc:/help/{image}' width='640'></p>")
    parts.append("<h2>Good to know</h2><ul>")
    parts += [f"<li>{html_inline(t)}</li>" for t in TIPS]
    parts.append("</ul></body></html>")
    return "\n".join(parts) + "\n"


if __name__ == "__main__":
    with open("docs/QUICK_START.md", "w", encoding="utf-8") as f:
        f.write(markdown())
    with open("src/app/help/quick-start.html", "w", encoding="utf-8") as f:
        f.write(page())
    print("Wrote docs/QUICK_START.md and src/app/help/quick-start.html")
