#!/bin/bash
# Letolti a lemezkepet a C64U SD-kartyajarol, es kibontja belole az osszes fajlt.
# Hasznalat: Loadfromc64u.sh [tavoli_kep] [celmappa]
#   alapertelmezett kep:  /SD/documents/semmi-egyben.d81
#   alapertelmezett mappa: ./c64u-ki
# Kornyezeti valtozo: C64U=192.168.0.158
IP="${C64U:-192.168.0.158}"
REMOTE="${1:-/SD/documents/semmi-egyben.d81}"
OUT="${2:-c64u-ki}"
set -e
command -v c1541 >/dev/null || { echo "Nincs c1541 (VICE)."; exit 1; }
mkdir -p "$OUT"
IMG="$OUT/$(basename "$REMOTE")"
curl -sf -o "$IMG" "ftp://$IP$REMOTE" || { echo "Letoltes sikertelen: $REMOTE"; exit 1; }
echo "Letoltve: $IMG"
# A -list sorai:  970  "semmi"   seq
c1541 "$IMG" -list 2>/dev/null | sed -nE 's/^[0-9]+ +"(.*)" +(seq|prg|usr|rel) *.*$/\1|\2/p' |
while IFS='|' read -r name type; do
    name="${name%"${name##*[! ]}"}"        # hatso szokozok le
    ext=""; [ "$type" = prg ] && ext=".prg"
    t="${type:0:1}"                        # s / p / u / r
    c1541 "$IMG" -read "$name,$t" "$OUT/$name$ext" </dev/null >/dev/null 2>&1 && echo "Kibontva: $OUT/$name$ext"
done
