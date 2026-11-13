#!/usr/bin/env bash
# Segmented, non-interactive demo for the unknown-extension round-trip work.
# Each section prints a title, pauses, runs one verification command, then
# pauses again so screen recordings have clear boundaries. No keyboard input.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PAUSE=2.5

c_reset=$'\033[0m'
c_bold=$'\033[1m'
c_cyan=$'\033[36m'
c_green=$'\033[32m'
c_yellow=$'\033[33m'

title() {
	sleep "$PAUSE"
	printf '\n%s%s================================================%s\n' "$c_bold" "$c_cyan" "$c_reset"
	printf '%s%s %s%s\n' "$c_bold" "$c_cyan" "$1" "$c_reset"
	printf '%s%s================================================%s\n\n' "$c_bold" "$c_cyan" "$c_reset"
}

run() {
	printf '%s$ %s%s\n' "$c_yellow" "$*" "$c_reset"
	eval "$*"
	local rc=$?
	if [ $rc -eq 0 ]; then
		printf '%s[exit 0]%s\n\n' "$c_green" "$c_reset"
	else
		printf '%s[exit %s]%s\n\n' "$c_yellow" "$rc" "$c_reset"
	fi
	return $rc
}

clear
printf '%s仓库%s: %s\n' "$c_bold" "$c_reset" "$ROOT"

title "段落 1/5：素材与改动概览"
run "ls -la output/assets"
run "git diff --stat"

title "段落 2/5：严格构建（-Werror + AddressSanitizer）"
run "gcc -std=c99 -Wall -Wextra -pedantic -Werror -fsanitize=address -g output/src/roundtrip.c -o output/bin/roundtrip"

title "段落 3/5：往返 / 损坏跳过 / 旧文件零差异 全量校验"
run "ASAN_OPTIONS=detect_leaks=1 ./output/bin/roundtrip output/assets/ext.gltf output/assets/known.gltf output/assets/ext.glb"

title "段落 4/5：扩展负载逐字节核对（写回产物中原文保留）"
run "./output/bin/dump_roundtrip output/assets/ext.gltf output/bin/ext.roundtrip.gltf output/bin/ext.roundtrip.glb"
run "grep -n ACME output/bin/ext.roundtrip.gltf | head -12"

title "段落 5/5：损坏样本优雅失败 + 仓库自带入口"
run "ASAN_OPTIONS=detect_leaks=1 ./test/build_final/cgltf_test output/corpus/leak_valueless_key.gltf"
run "ASAN_OPTIONS=detect_leaks=1 ./test/build_final/cgltf_test output/assets/ext.glb"

sleep "$PAUSE"
printf '%s%s演示结束。%s\n' "$c_bold" "$c_green" "$c_reset"
sleep 1
