#!/usr/bin/env python3
"""
Builds the segmented demo video without relying on GL screen capture.

A real terminal session runs output/demo.sh inside tmux (so colors, layout
and the actual command output are genuine). The pane is captured on a fixed
time grid and each capture is rasterized with Pillow into a 1920x1080 frame.
Section titles and the inter-section pauses come from demo.sh itself, so the
rendered video shows the same segmented, paused, non-interactive flow.
"""
import os
import shutil
import subprocess
import sys
import time

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "output")
FRAMES = os.path.join(OUT, "frames")
SESSION = "cgltf_cap"

COLS, ROWS = 150, 44
FPS = 12
WIDTH, HEIGHT = 1920, 1080
CELL_W, CELL_H = 12, 22
MARGIN_X, MARGIN_Y = 56, 40

BG = (18, 18, 24)
FG = (220, 220, 220)
TITLE_BG = (10, 42, 54)


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


MONO_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
MONO_BOLD_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
CJK_PATH = "/home/ys/.local/share/fonts/wqy-zenhei.ttc"

FONT = ImageFont.truetype(MONO_PATH, 18)
FONT_BOLD = ImageFont.truetype(MONO_BOLD_PATH, 20)
# WQY Zen Hei Mono (TTC index 1) covers CJK while staying monospace.
FONT_CJK = ImageFont.truetype(CJK_PATH, 18, index=1)
FONT_CJK_BOLD = ImageFont.truetype(CJK_PATH, 20, index=1)


def is_cjk(ch):
    code = ord(ch)
    return (
        0x4E00 <= code <= 0x9FFF or
        0x3000 <= code <= 0x303F or
        0xFF00 <= code <= 0xFFEF
    )


def draw_mono(draw, x, y, text, font, cjk_font, color):
    """
    Draw text on a fixed-width grid. DejaVu handles Latin glyphs (one cell);
    WQY fills in CJK glyphs (two cells). This keeps columns aligned like a
    real terminal.
    """
    cursor = x
    for ch in text:
        use_font = cjk_font if is_cjk(ch) else font
        draw.text((cursor, y), ch, font=use_font, fill=color)
        cursor += CELL_W * (2 if is_cjk(ch) else 1)
    return cursor

# ANSI 16-color palette (dark theme), index 0..15.
PALETTE = [
    (24, 24, 32), (205, 68, 74), (92, 184, 92), (220, 180, 70),
    (78, 140, 220), (190, 110, 200), (80, 180, 190), (210, 210, 210),
    (120, 120, 130), (230, 110, 110), (120, 210, 120), (235, 200, 110),
    (110, 165, 235), (215, 140, 225), (110, 210, 220), (245, 245, 245),
]


def ansi_to_spans(line):
    """Minimal SGR parser yielding (text, fg_rgb|None, bold) spans."""
    spans = []
    fg = None
    bold = False
    buf = ""
    i = 0

    def flush():
        nonlocal buf
        if buf:
            spans.append((buf, fg, bold))
            buf = ""

    while i < len(line):
        if line[i] == "\x1b" and i + 1 < len(line) and line[i + 1] == "[":
            end = line.find("m", i)
            if end == -1:
                break
            flush()
            params = line[i + 2:end].split(";")
            for p in params:
                if p == "0":
                    fg, bold = None, False
                elif p == "1":
                    bold = True
                elif p == "22":
                    bold = False
                elif p == "39":
                    fg = None
                elif p.startswith("3") and p[1:].isdigit() and 0 <= int(p[1:]) <= 7:
                    fg = PALETTE[int(p[1:])]
                elif p.startswith("9") and p[1:].isdigit() and 0 <= int(p[1:]) <= 7:
                    fg = PALETTE[int(p[1:]) + 8]
            i = end + 1
        else:
            buf += line[i]
            i += 1
    flush()
    return spans


def render_frame(text_lines, frame_index):
    img = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(img)

    # Title bar.
    draw.rectangle([0, 0, WIDTH, MARGIN_Y - 8], fill=TITLE_BG)
    draw_mono(draw, MARGIN_X, 8, "cgltf 未知厂商扩展读写保留 — 分段演示",
              FONT_BOLD, FONT_CJK_BOLD, (140, 220, 235))

    y = MARGIN_Y
    for line in text_lines[-ROWS:]:
        x = MARGIN_X
        for text, fg, bold in ansi_to_spans(line):
            color = fg if fg else FG
            x = draw_mono(draw, x, y, text,
                          FONT_BOLD if bold else FONT,
                          FONT_CJK_BOLD if bold else FONT_CJK,
                          color)
        y += CELL_H

    # Progress footer.
    draw.rectangle([0, HEIGHT - 28, WIDTH, HEIGHT], fill=(12, 12, 16))
    draw_mono(draw, MARGIN_X, HEIGHT - 24,
              f"frame {frame_index}  ·  output/ 产物演示  ·  无交互自动播放",
              FONT, FONT_CJK, (150, 150, 160))
    return img


def main():
    if os.path.isdir(FRAMES):
        shutil.rmtree(FRAMES)
    os.makedirs(FRAMES)

    subprocess.run(["tmux", "kill-session", "-t", SESSION],
                   capture_output=True)
    run(["tmux", "new-session", "-d", "-s", SESSION,
         "-x", str(COLS), "-y", str(ROWS)])
    run(["tmux", "send-keys", "-t", SESSION,
         f"cd {ROOT} && bash output/demo.sh", "Enter"])

    # demo.sh runs ~35s; sample until the shell returns to an idle prompt.
    max_seconds = 50
    deadline = time.time() + max_seconds
    last_text = None
    idle_rounds = 0

    # Collect unique terminal states first (the demo is fully driven by tmux).
    snapshots = []
    seen = set()
    while time.time() < deadline:
        raw = run(["tmux", "capture-pane", "-t", SESSION, "-p", "-e"]).stdout
        lines = raw.split("\n")
        # Drop trailing blank lines but keep a stable bottom margin.
        while lines and lines[-1] == "":
            lines.pop()
        signature = "\n".join(lines)

        if signature not in seen:
            seen.add(signature)
            snapshots.append(lines)

        if "演示结束" in signature and signature == last_text:
            idle_rounds += 1
            if idle_rounds >= FPS:
                break
        else:
            idle_rounds = 0
        last_text = signature
        time.sleep(1.0 / FPS)

    subprocess.run(["tmux", "kill-session", "-t", SESSION],
                   capture_output=True)

    #
    # Expand the captured states into a frame timeline. A state that first
    # reveals a new section title is held for SECTION_HOLD seconds (the
    # required inter-section pause); every other state gets one frame.
    #
    SECTION_HOLD = 2.5
    END_HOLD = 2.0
    frame_index = 0

    def emit(lines, count):
        nonlocal frame_index
        img = render_frame(lines, frame_index)
        for _ in range(count):
            img.save(os.path.join(FRAMES, f"f_{frame_index:05d}.png"))
            frame_index += 1

    for idx, lines in enumerate(snapshots):
        joined = "\n".join(lines)
        section_count = joined.count("段落 ")
        prev_count = "\n".join(snapshots[idx - 1]).count("段落 ") if idx > 0 else 0
        is_first_title_frame = section_count > prev_count
        # Pause on each new section title; give other states a readable beat.
        hold = SECTION_HOLD if is_first_title_frame else 1.0
        emit(lines, max(1, int(round(hold * FPS))))

    # Hold the final screen so the ending is readable before the video stops.
    emit(snapshots[-1], int(END_HOLD * FPS))
    print(f"rendered {frame_index} frames from {len(snapshots)} states")


if __name__ == "__main__":
    sys.exit(main())
