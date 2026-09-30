#!/usr/bin/env bash
# Fizik regresyon testi: zehra_sim ciktilari kayitli referansla byte byte ayni olmali.
# Kullanim: tests/run_regression.sh <zehra_sim yolu>
#   Bilincli bir fizik degisikliginden sonra referanslari yenilemek icin: UPDATE=1 tests/run_regression.sh build/zehra_sim
set -u
SIM="${1:?zehra_sim yolu gerekli}"
DIR="$(cd "$(dirname "$0")/regression" && pwd)"
fail=0
for args_file in "$DIR"/case*.args; do
  name="$(basename "$args_file" .args)"
  args="$(cat "$args_file")"
  out="$(mktemp)"
  # shellcheck disable=SC2086
  "$SIM" $args > "$out"
  if [ "${UPDATE:-0}" = "1" ]; then
    cp "$out" "$DIR/$name.expected"; echo "GUNCELLENDI $name [$args]"
  elif cmp -s "$out" "$DIR/$name.expected"; then
    echo "GECTI  $name [$args]"
  else
    echo "KALDI  $name [$args]"; diff "$DIR/$name.expected" "$out" | head -20; fail=1
  fi
  rm -f "$out"
done
exit $fail
