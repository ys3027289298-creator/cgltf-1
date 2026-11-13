# 未知厂商扩展读写保留 — 验证产物

本目录是本次改动的自包含验证材料。库本身的改动在仓库根目录的
`cgltf.h`（解析端）与 `cgltf_write.h`（写入端）。

## 目录结构

- `assets/`：手工构造的小型素材
  - `ext.gltf`：在 10 类对象（根、asset、mesh、primitive、material、node、
    scene、accessor、bufferView、buffer）上各带一个 `ACME_*` 未知扩展。
  - `ext.glb`：自带 BIN 块、带未知扩展的 GLB（用于验证二进制块逐字节保留）。
  - `known.gltf`：只含已知扩展 `KHR_materials_unlit`（用于旧文件零差异）。
  - `data.bin`：42 字节，一个三角形的位置(36B) + 索引(6B)。
- `corpus/`：fuzz 发现的两个损坏回归样本，验证不崩溃、不泄漏、优雅返回错误。
- `src/`：验证程序源码（仅调用仓库公开 API）。
  - `roundtrip.c`：逐字节比对扩展负载、损坏跳过、旧文件零差异、GLB BIN 保留。
  - `dump_roundtrip.c`：产出可肉眼检查的 glTF/GLB 往返文件。
- `bin/`：编译出的可执行文件与往返产物（`ext.roundtrip.gltf/.glb`）。
- `run_verification.sh`：一键复跑全部验证，无需联网、无交互、不等待键盘。
- `demo.sh`：分段终端演示（每段带标题、段间停顿，自动播放）。
- `demo.mp4`：由 `build_video.sh` 渲染的 1080p 分段录屏，含 5 个带标题的
  段落与段间停顿；`src/make_video.py` 用 tmux 捕获真实终端输出、Pillow
  逐帧渲染、ffmpeg 编码（不依赖 OpenGL 屏幕抓取）。
- `build_video.sh`：重新生成 `demo.mp4`（自动准备 Pillow 虚拟环境与等宽
  中文字体，全程无交互）。

## 录屏说明

`demo.mp4` 的五个分段与停顿：

1. 素材与改动概览
2. 严格构建（`-Wall -Wextra -pedantic -Werror` + AddressSanitizer）
3. 往返 / 损坏跳过 / 旧文件零差异 全量校验
4. 扩展负载逐字节核对（写回产物中原文保留）
5. 损坏样本优雅失败 + 仓库自带入口

每个新分段标题停留约 2.5 秒，结尾停留 2 秒；整个流程不读取任何键盘输入。
重新生成：`./output/build_video.sh`。

## 一键复跑

```bash
./output/run_verification.sh            # fuzz 默认 60s
FUZZ_SECONDS=120 ./output/run_verification.sh
```

## 验收点对照

- 往返后扩展数据逐字节一致：`roundtrip.c` 对每个对象的 `cgltf_extension.data`
  做 `strcmp` 比对；`dump_roundtrip` 产物中原始负载文本（含内部空白）原样出现。
- 旧文件零差异：只含已知扩展的 `known.gltf` 连续两次写出结果完全一致，
  且已知扩展仍走类型化解析（`unlit==1`），不会被当成未知扩展重复输出。
- 损坏可跳过：错误类型的扩展值（字符串/数字/数组）被丢弃而主结构正常解析；
  结构性损坏（无值 key、截断）由 `corpus/` 覆盖，返回错误码且无崩溃/泄漏。
- 已知扩展行为不变：写入端对已知扩展仍由原有 `extension_flags` 路径输出，
  新增逻辑全部以"存在未知扩展"为前提，不影响无未知扩展的文档。

## 验证强度

- 库与验证程序均以 `-Wall -Wextra -pedantic -Werror` + AddressSanitizer 构建。
- fuzz（libFuzzer + ASan）在 1600 万次以上变异后无崩溃、无泄漏。
- 仓库自带测试目标（`test/CMakeLists.txt` 的 6 个可执行文件）全部构建并通过。
