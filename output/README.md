# GLB 单文件写出修复 — 交付说明

## 背景与根因

`cgltf_write.h` 的 GLB writer 此前只搬运 `data->bin`（仅解析 GLB 时才有值）。
从外部 `.gltf` + `.bin` 解析并 `cgltf_load_buffers()` 后，字节位于
`data->buffers[0].data`，导致：

1. 写出的 GLB 没有 BIN chunk，几何数据丢失；
2. JSON 中 `buffers[0]` 仍保留外部 `"uri"`，违反 GLB 2.0 规范（内嵌 buffer 禁止 uri）；
3. `cgltf_write_file()` 不校验参数、不检查 `fwrite`/`fclose` 返回值，写失败仍返回
   `success` 并留下半截坏文件；chunk 长度直接强转 `uint32_t`，超大输入静默截断。

该缺陷与上游 master（截至本任务时）完全一致，本修复在库内闭环。

## 改动文件

- `cgltf_write.h`
  - `cgltf_write_file()`：校验 `options/path/data` 与 `options->type`
    （非法返回 `cgltf_result_invalid_options`）；GLB 模式下当 `data->bin` 为空时
    回退使用 `buffers[0].data/size`，buffer 未加载则返回 `cgltf_result_invalid_gltf`；
    检查 malloc、两次 JSON 长度、`fwrite/fflush/fclose`，任何失败返回错误码并
    `remove(path)`，绝不留下坏文件。
  - `cgltf_write_glb()`：返回 `cgltf_result`；写入前校验 JSON/BIN/总长不超
    `UINT32_MAX`（加法防溢出），逐段检查写入字节数；JSON 用空格(0x20)、BIN 用
    0x00 填充到 4 字节对齐，header `total length` 与实际文件严格一致。
  - `cgltf_write_buffer()`：GLB 模式下对 `buffers[0]` 省略 `uri`（新增 context
    标志 `is_glb`）。
- `test/test_write_glb.cpp`：新增外部 `.gltf` 加载后写 GLB 的往返、NULL 参数
  拒绝、uri 缺失断言、按逻辑 byteLength 比较 BIN 负载（兼容 BIN chunk 对齐填充）。

## 验收证据

### 1. 读写往返结构一致

`output/glb_verify`（源码 `output/glb_verify.c`，`-Werror` 零警告）逐字段比对
mesh/material/node/accessor/bufferView/buffer/scene 计数、mesh/node 名称、
accessor 类型与数量、bufferView 偏移，并对 BIN 逻辑负载逐字节 `memcmp`；
同时解析容器字节，校验 magic/version/total-length、JSON/BIN chunk magic、
4 字节对齐、chunk 顺序与文件长度自洽；GLB→GLB 二次写出字节完全相同。
结果：`PASSED: 0 failure(s)`（见 `output/demo.log` 第 4 段）。

### 2. 非法输入被拒绝（返回错误码，不产出坏文件）

- NULL `path` / NULL `data` → `cgltf_result_invalid_options`，且无文件创建；
- 伪造 `options->type=99` → `cgltf_result_invalid_options`，无文件；
- 外部 buffer 未 `load_buffers` 即写 GLB → 非 success，目标文件不存在；
- 4 个手工畸形 GLB（坏 magic、坏 JSON chunk magic、截断、total length 撒谎）
  经仓库自带 `cgltf_test` 解析分别返回 3/2/1/1，合法文件返回 0。

### 3. 体积对比（完整数据见 `output/size_report.txt`）

- BIN chunk 与原始二进制**逐字节一致、零膨胀**（已对 5 个样例校验）；
- GLB→GLB 的增量仅来自 JSON 2 空格缩进重排。真实大模型占比可忽略：
  BoomBox +0.01%、Avocado +0.01%、MosquitoInAmber +0.01%、DragonAttenuation +0.03%、
  Buggy +0.46%；小文件因 JSON 占比高出现 11%~28% 的相对增量（绝对量 <1KB）。
- `.gltf + .bin` 打包成单文件 GLB 反而缩小：Box -39.76%、BoxInterleaved -40.88%、
  BoxTextured -37.38%、手工小素材 -15.32%。

### 4. 仓库自带入口回归

`python3 test/test_all.py`（CMake + ASan + `-Werror`）：**Tested files: 726,
Errors: 0**，完整日志 `output/regression.log`。

## output/ 目录

- `run_demo.sh` — 非交互 7 段演示（分段标题 + 段间停顿，无需键盘）
- `demo.cast` / `demo.gif` / `demo.log` — 录屏（asciinema，100x40）、GIF、文本日志
- `make_assets.py` — 生成 <2KB 小素材（42B 非对齐 BIN）与 4 个畸形 GLB
- `assets/` — `triangle.gltf`/`triangle.bin`/`triangle.glb` 及 `bad/` 畸形样本
- `glb_verify.c` / `glb_verify` — 往返 + 容器 + 非法输入校验器
- `size_compare.c` / `size_compare` / `size_report.sh` / `size_report.txt` — 体积对比
- `roundtrip.glb` / `roundtrip2.glb` / `pack_tiny.glb` — 写出产物样例
- `regression.log` — 726 样例全量回归日志

## 复现

```bash
python3 output/make_assets.py
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -I. output/glb_verify.c -o output/glb_verify
./output/glb_verify output/assets/triangle.gltf output/assets/bad
cd test && python3 test_all.py   # 首次会拉取 glTF-Sample-Models
```
