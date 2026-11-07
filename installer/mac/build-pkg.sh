#!/bin/bash
# Builds ChaosTrigger-<ver>-Mac.pkg with VST3, AU and Standalone as selectable parts.
set -euo pipefail
VER="${CT_VERSION:-0.1.0}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ART="$ROOT/build/ChaosTrigger_artefacts/Release"
WORK="$ROOT/build/pkg"; OUT="$ROOT/installer-out"
rm -rf "$WORK"; mkdir -p "$WORK"/{vst3,au,app,pkgs} "$OUT"

cp -R "$ART/VST3/Chaos Trigger.vst3" "$WORK/vst3/"
cp -R "$ART/AU/Chaos Trigger.component" "$WORK/au/"
cp -R "$ART/Standalone/Chaos Trigger.app" "$WORK/app/"
xattr -cr "$WORK" || true

pkgbuild --root "$WORK/vst3" --install-location "/Library/Audio/Plug-Ins/VST3" \
  --identifier com.ini.chaostrigger.vst3 --version "$VER" "$WORK/pkgs/vst3.pkg"
pkgbuild --root "$WORK/au" --install-location "/Library/Audio/Plug-Ins/Components" \
  --identifier com.ini.chaostrigger.au --version "$VER" \
  --scripts "$ROOT/installer/mac/scripts" "$WORK/pkgs/au.pkg"
pkgbuild --root "$WORK/app" --install-location "/Applications" \
  --identifier com.ini.chaostrigger.app --version "$VER" "$WORK/pkgs/app.pkg"

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Chaos Trigger by iNi</title>
  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="vst3"/><line choice="au"/><line choice="app"/>
  </choices-outline>
  <choice id="vst3" title="VST3 Plugin" description="For Reaper, Ableton Live, Cubase, Bitwig, etc."><pkg-ref id="com.ini.chaostrigger.vst3"/></choice>
  <choice id="au" title="Audio Unit (AU)" description="For Logic Pro and GarageBand."><pkg-ref id="com.ini.chaostrigger.au"/></choice>
  <choice id="app" title="Standalone App" description="Run Chaos Trigger without a DAW."><pkg-ref id="com.ini.chaostrigger.app"/></choice>
  <pkg-ref id="com.ini.chaostrigger.vst3" version="$VER">vst3.pkg</pkg-ref>
  <pkg-ref id="com.ini.chaostrigger.au" version="$VER">au.pkg</pkg-ref>
  <pkg-ref id="com.ini.chaostrigger.app" version="$VER">app.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" \
  "$OUT/ChaosTrigger-Mac.pkg"
echo "Built $OUT/ChaosTrigger-Mac.pkg"
