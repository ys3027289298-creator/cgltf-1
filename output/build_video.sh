#!/usr/bin/env bash
# Reproduces output/demo.mp4 from the real terminal flow in output/demo.sh.
# Uses tmux to capture genuine PTY output and Pillow to rasterize 1080p
# frames; ffmpeg encodes the result. Fully non-interactive.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VENV="${VENV:-/tmp/venv}"
if [ ! -x "$VENV/bin/python" ] || ! "$VENV/bin/python" -c "import PIL" 2>/dev/null; then
	echo "### Preparing isolated Python environment with Pillow"
	python3 -m venv "$VENV"
	"$VENV/bin/pip" --quiet install pillow
fi

# WQY Zen Hei supplies the CJK glyphs DejaVu lacks; fetch only if missing.
FONT_DIR="$HOME/.local/share/fonts"
FONT="$FONT_DIR/wqy-zenhei.ttc"
if [ ! -f "$FONT" ]; then
	echo "### Fetching a monospace CJK font (local user install, no root)"
	mkdir -p "$FONT_DIR"
	work="$(mktemp -d)"
	( cd "$work" && apt-get download fonts-wqy-zenhei )
	dpkg-deb -x "$work"/fonts-wqy-zenhei_*.deb "$work/extracted"
	cp "$work/extracted/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc" "$FONT"
	rm -rf "$work"
fi

echo "### Rendering frames from the live tmux session"
"$VENV/bin/python" output/src/make_video.py

echo "### Encoding output/demo.mp4"
ffmpeg -hide_banner -loglevel error -y \
	-framerate 12 -i output/frames/f_%05d.png \
	-c:v libx264 -pix_fmt yuv420p -preset medium -crf 20 \
	output/demo.mp4

echo "### Wrote output/demo.mp4"
ffprobe -v error -show_entries format=duration -of csv=p=0 output/demo.mp4
