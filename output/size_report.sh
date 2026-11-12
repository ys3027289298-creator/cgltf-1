#!/usr/bin/env bash
# Size comparison report: GLB->GLB rewrite and .gltf+.bin -> single GLB.
set -euo pipefail
cd "$(dirname "$0")/.."
S=test/glTF-Sample-Models/2.0
mkdir -p output/sizetmp
echo "### 1) GLB -> GLB (original binary vs writer output; BIN chunk is byte-identical)"
printf '%-20s %10s %10s %9s %8s\n' model orig.glb new.glb delta percent
for m in Box BoxInterleaved BoxTextured SimpleInstancing ClearCoatTest Buggy DragonAttenuation BoomBox Avocado BarramundiFish AntiqueCamera MosquitoInAmber; do
  f="$S/$m/glTF-Binary/$m.glb"
  [ -f "$f" ] || continue
  ./output/size_compare "$f" "output/sizetmp/$m.glb" 0 | sed 's#test/glTF-Sample-Models/2.0/##; s#/glTF-Binary##'
done
echo
echo "### 2) .gltf + external .bin -> single self-contained GLB (packaging shrinks files)"
for m in Box BoxInterleaved BoxTextured; do
  d="$S/$m/glTF"
  orig=$(wc -c < "$d/$m.gltf")
  for bb in "$d/"*.bin; do orig=$((orig + $(wc -c < "$bb"))); done
  ./output/size_compare "$d/$m.gltf" "output/sizetmp/pack_$m.glb" 1 >/dev/null
  new=$(wc -c < "output/sizetmp/pack_$m.glb")
  printf '%-20s src(gltf+bin)=%8d  glb=%8d  delta=%+d (%+.2f%%)\n' "$m" "$orig" "$new" "$((new-orig))" "$(python3 -c "print(f'{($new-$orig)*100/$orig:+.2f}')")"
done
orig=$(( $(wc -c < output/assets/triangle.gltf) + $(wc -c < output/assets/triangle.bin) ))
./output/size_compare output/assets/triangle.gltf output/sizetmp/pack_tiny.glb 1 >/dev/null
new=$(wc -c < output/sizetmp/pack_tiny.glb)
printf '%-20s src(gltf+bin)=%8d  glb=%8d  delta=%+d (%+.2f%%)\n' handmade "$orig" "$new" "$((new-orig))" "$(python3 -c "print(f'{($new-$orig)*100/$orig:+.2f}')")"
echo
echo "### 3) JSON vs BIN share inside rewritten GLBs (BIN never grows)"
python3 - <<'PY'
import struct, os
def parse(p):
    b=open(p,'rb').read(); jlen,=struct.unpack_from('<I',b,12)
    off=12+8+jlen; blen=struct.unpack_from('<I',b,off)[0]
    return len(b),jlen,blen
print(f"{'model':<18}{'total':>10}{'json':>8}{'bin':>10}{'json%':>8}")
for m in ['Box','BoxTextured','Buggy','BoomBox','Avocado','MosquitoInAmber']:
    p=f'output/sizetmp/{m}.glb'
    if os.path.exists(p):
        t,j,bl=parse(p); print(f"{m:<18}{t:>10}{j:>8}{bl:>10}{100*j/t:>7.2f}%")
PY
