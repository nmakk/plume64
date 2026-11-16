#!/bin/bash
# Felvisz egy fajlt a lemezkepre (SEQ-kent), majd visszatolti a kepet a C64U-ra.
# Hasznalat: Savetoc64u.sh fajl [lemezen_nev] [tavoli_kep]
#   alapertelmezett nev: a fajlnev kiterjesztes nelkul
#   alapertelmezett kep: /SD/documents/semmi-egyben.d81
# Kornyezeti valtozo: C64U=192.168.0.158
IP="${C64U:-192.168.0.158}"
FILE="$1"
NAME="${2:-$(basename "${FILE%.*}")}"
REMOTE="${3:-/SD/documents/semmi-egyben.d81}"
set -e
[ -f "$FILE" ] || { echo "Hasznalat: $0 fajl [lemezen_nev] [tavoli_kep]"; exit 1; }
command -v c1541 >/dev/null || { echo "Nincs c1541 (VICE)."; exit 1; }
TMP="$(mktemp -d)"; IMG="$TMP/$(basename "$REMOTE")"
curl -sf -o "$IMG" "ftp://$IP$REMOTE" || { echo "Letoltes sikertelen: $REMOTE"; exit 1; }
cp "$IMG" "${IMG}.bak"                       # biztonsagi masolat a letoltott allapotrol
c1541 "$IMG" -delete "$NAME" >/dev/null 2>&1 || true   # regi azonos nevu fajl torlese
c1541 "$IMG" -write "$FILE" "$NAME,s" >/dev/null 2>&1 || { echo "Irasi hiba (tele a lemez?)"; exit 1; }
curl -sf -T "$IMG" "ftp://$IP$REMOTE" || { echo "Feltoltes sikertelen"; exit 1; }
# A C64U ujra beolvassa a kepet (ha at volt csatolva a driveon)
curl -s -X PUT "http://$IP/v1/drives/a:mount?image=$REMOTE&type=${REMOTE##*.}&mode=readwrite" >/dev/null || true
cp "${IMG}.bak" "$HOME/.c64u-$(basename "$REMOTE").bak" 2>/dev/null || true
echo "Kesz: '$NAME' felkerult ide: $REMOTE (a Drive A ujra csatolva, irhato)"
rm -rf "$TMP"
