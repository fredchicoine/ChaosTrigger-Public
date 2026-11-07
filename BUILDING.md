# Building Chaos Trigger from source

You need CMake 3.22+ and a C++17 compiler (Xcode on macOS, Visual Studio 2022 on Windows).

```
git clone --depth 1 -b 8.0.4 https://github.com/juce-framework/JUCE JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The plugins end up in `build/ChaosTrigger_artefacts/Release/`. The AU target only builds on macOS.

## Installers and releases
GitHub Actions (`.github/workflows/build.yml`) builds the Mac `.pkg` (`installer/mac/build-pkg.sh`) and the
Windows Setup `.exe` (`installer/windows.iss`, Inno Setup) on every push.

To publish a release, go to **Actions → Build Chaos Trigger → Run workflow** and type a version such as `v0.2.0`.
You can also push a `v*` tag. The installers are attached to the release with fixed file names, so the
download buttons in the README always point to the newest version.

## Editions
- **Demo / private edition:** built when `Resources/Samples/*.wav` exist. The 4 demo loops are compiled in and preloaded.
- **Public edition:** built with `-DCHAOS_DEMO_SAMPLES=OFF`, or automatically when `Resources/Samples` is absent.
  The plugin starts with empty lanes. The public repository doesn't contain the samples at all.

## Demo samples
`Resources/Samples/*.wav` are compiled into the plugin (`juce_add_binary_data`) and loaded into lanes 1–4 by default.
A project that uses them saves `factory:N` instead of a file path.

## Engine notes
- Pitch (master + lane + key track) never changes timing. Playheads always move at 1×, and a 2-grain
  (60 ms, Hann) granular shifter reads at the pitch ratio. At ratio 1, the sample is read directly.
- All four loops run in parallel from note-on. The sequencer only opens each lane's ADSR gate in turn.
- Lane signal chain: sample → ADSR → Distortion → Bitcrush → Smash → Chorus → Haas → Pan/Vol → lane output bus.
- Master chain (applied on every enabled output bus): Distortion → Bitcrush → Smash → Ring Mod → Filter → Haas.
- On Linux, `ChaosTriggerTest` renders audio, stress-tests automation on every parameter and saves a GUI snapshot.
