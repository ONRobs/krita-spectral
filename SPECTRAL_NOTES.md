# krita-spectral: notes

Spectral (Kubelka-Munk) paint mixing for Krita, based on
[spectral.js](https://github.com/rvanwijnen/spectral.js) v3 by Ronald van Wijnen (MIT).
Blue + yellow gives green, like paint, instead of gray.

It adds one blending mode, **Spectral** (in the *Mix* group), which works

- as the blending mode of normal brushes,
- as the blending mode of the **Color Smudge** engine: then smearing, dulling and
  the final stamp of every dab mix spectrally too, so paint mixes as you drag it,
- as a layer blending mode (layer opacity is the mixing factor).

It's only offered in RGB color spaces (8/16 bit integer, 16/32 bit float). In other color
models it isn't listed; a preset that asks for it paints with Normal instead.

No drying, wetness or fluid simulation; color mixing only. No Mixbox code.


## What was added

All the logic lives in new files:

| File | What it does |
|---|---|
| `libs/pigment/compositeops/KoSpectralMixing.{h,cpp}` | Port of the spectral.js mixing model: linear sRGB -> 38-band reflectance -> K/S mixing -> linear sRGB. MIT licensed. |
| `libs/pigment/compositeops/KoSpectralColorTransfer.{h,cpp}` | Converts color space values to linear sRGB and back, using the profile's real tone curve (lookup tables) and primaries (matrix via `KoColorimetryUtils::Colorimetry`). |
| `libs/pigment/compositeops/KoCompositeOpSpectral.h` | The composite ops: `spectral` (alpha like Over) and `copy_spectral` (alpha like Copy, internal). |
| `libs/pigment/compositeops/KoSpectralCompositeOps.h` | `addSpectralCompositeOps<Traits>()`, registers both ops in a color space. |
| `libs/pigment/compositeops/KoSpectralParallel.{h,cpp}` | Splits large compositions between threads. |
| `plugins/paintops/colorsmudge/KisColorSmudgeSpectralUtils.h` | Picks the spectral variants of the smudge engine's internal OVER/COPY ops. |
| `libs/pigment/tests/TestKoSpectralMixing.*` | Math tests: matches spectral.js, exact endpoints, no drift, neutral grays, profile conversions vs LCMS. |
| `libs/pigment/tests/TestKoCompositeOpSpectral.*` | Op tests for all four bit depths: opacity, masks, transparency, alpha lock, channel flags, HDR fallback, threading. |
| `plugins/paintops/colorsmudge/tests/KisColorSmudgeSpectralTest.*` | Smudge engine tests with 8 presets: mixes spectrally, no drift, neutrals stay neutral, alpha-locked layers, CMYK fallback. |
| `libs/pigment/benchmarks/KoSpectralCompositeOpBenchmark.*` | Benchmark of Spectral vs Normal. |
| `build-tools/spectral/container-build.sh` | Local build in KDE's build container (see below). |
| `build-tools/ci-scripts/github-build-appimage.sh`, `.github/workflows/linux-appimage.yml` | AppImage builds on GitHub Actions. |


## Existing files that were touched, and why

The hook points carry a `krita-spectral` comment (`git grep -n krita-spectral`), but not every
changed line does. The complete list of changes against upstream is always

```bash
git diff --stat $(git merge-base HEAD origin/master) HEAD
```

| File | Change | Why |
|---|---|---|
| `libs/pigment/KoCompositeOpRegistry.h` | `COMPOSITE_SPECTRAL`, `COMPOSITE_COPY_SPECTRAL` | the op IDs (`"spectral"` is what gets saved in .kra files) |
| `libs/pigment/KoCompositeOpRegistry.cpp` | one `m_map.insert(...)` | shows "Spectral" in the Mix category of the blending mode lists |
| `libs/pigment/compositeops/KoCompositeOps.h` | one include, one call in `addStandardCompositeOps()` | registers the ops for RGB color spaces only |
| `libs/pigment/CMakeLists.txt` | 3 sources; per-file `-fno-math-errno -fno-trapping-math` for `KoSpectralMixing.cpp` | the flags let the compiler vectorize `sqrt()` and comparisons |
| `libs/pigment/tests/CMakeLists.txt`, `libs/pigment/benchmarks/CMakeLists.txt` | new test/benchmark | |
| `plugins/paintops/colorsmudge/KisColorSmudgeStrategyBase.{h,cpp}` | `m_useSpectral`; `smearCompositeOp()`/`finalCompositeOp()` return the spectral variants; the `== COMPOSITE_COPY` fast paths also accept `copy_spectral`; the fused dulling path accepts spectral/spectral | otherwise only the color rate step would mix spectrally |
| `plugins/paintops/colorsmudge/KisColorSmudgeStrategyMaskLegacy.cpp` | same for its overrides of `smearCompositeOp()`/`finalCompositeOp()` | legacy engine |
| `plugins/paintops/colorsmudge/KisColorSmudgeStrategyLightness.cpp` | final painter uses `finalCompositeOp()` instead of a hard-coded `COMPOSITE_COPY` | same result for other modes, spectral copy for Spectral |
| `plugins/paintops/colorsmudge/tests/CMakeLists.txt` | new test | |
| `.gitignore` | Gradle state and local build folders | |


## Design decisions worth knowing

- **Exactness over speed where it matters for drift.** `KoSpectral::mix()` subtracts each input's
  own round-trip error, so mixing at factor 0/1, or a color with itself, returns the input
  exactly. Identical colors skip the conversion altogether. White is a flat spectrum and the
  spectrum->RGB conversion is normalized, so mixtures of grays are exactly neutral (spectral.js'
  slightly non-flat white made the smudge engine build up a green cast near dab edges).
  Results stay within 0.5/255 of spectral.js.
- **HDR**: values outside [0, 1] in float color spaces can't be reflectances; those pixels are
  mixed linearly, like Normal.
- **Wide gamut profiles**: colors are converted to linear sRGB for mixing. Colors outside sRGB
  are clamped for building the reflectance curve; the clamped part is carried through the
  round-trip error compensation, so endpoints stay exact.
- **Performance** (300 px dab, 16 bit, 8 cores): Normal ~0.3 ms, Spectral ~2-3.5 ms (flat areas
  ~0.8 ms). Done with: vectorizable band loops (+ an AVX2 clone on GCC/Linux), lookup tables
  instead of `pow()`, per-call caches of prepared colors and of results per mixing factor, and
  threading of areas over 1024 pixels.
- **Known limitation: big brushes.** Brushes up to ~100 px feel fine; at 200 px they are barely
  usable and at 500 px not usable. The threading made the op ~4x faster in the benchmark but
  hardly changed how painting feels, so the bottleneck at large sizes is probably outside the
  composite op (the rest of the smudge engine's per-dab work, or how dabs are scheduled).
  Next step: profile a real smudge stroke (e.g. `perf record` on the running AppImage with
  `kernel.perf_event_paranoid` lowered, or a smudge benchmark built on
  `KisColorSmudgeSpectralTest`) before optimizing further. Explicit xsimd code using Krita's
  per-arch infrastructure would speed up the op itself.
- **Known upstream test failures** (also without this branch, in the build container):
  `TestKoColorSpaceAbstract` (CMYK->RGB with channel flags) and `TestColorConversionSystem`
  (HDR PQ conversion paths). Not touched, to keep the diff minimal.


## Building and running

Everything happens inside KDE's `krita-appimage-2204` build image, with rootless podman
(`CONTAINER_ENGINE=docker` works too), using KDE's prebuilt dependencies. Build output goes
into ignored folders of the source tree (`_build`, `_install`, `_packaging`, ...).
You need ~50 GB of free disk space.

```bash
build-tools/spectral/container-build.sh deps        # download dependencies (first time, and after rebasing)
build-tools/spectral/container-build.sh configure   # configure _build
build-tools/spectral/container-build.sh install     # build everything and install into _install
build-tools/spectral/container-build.sh test Spectral   # run the spectral tests (ctest regex)
build-tools/spectral/container-build.sh appimage    # package _packaging/krita-*.AppImage
```

Run the AppImage with its own settings, so it doesn't touch your normal Krita's:

```bash
XDG_CONFIG_HOME=~/krita-spectral-profile/config XDG_DATA_HOME=~/krita-spectral-profile/data \
  _packaging/krita-*.AppImage
```

GitHub Actions builds an AppImage for every push to `krita-spectral` on the fork
(Actions tab -> run -> Artifacts).


## Rebasing onto a new Krita release

1. Fetch upstream and rebase:

   ```bash
   git fetch origin --tags
   git rebase v5.x.y           # or origin/master, or a release branch like origin/krita/5.3
   ```

2. Conflicts, if any, will be in the files from the table above. The hooks are small; re-apply
   them by hand. Things to check:

   - `KoCompositeOps.h`: the call to `addSpectralCompositeOps<_Traits_>(cs)` must stay inside
     `addStandardCompositeOps()` and guarded by `useRGBOps`.
   - Color smudge strategies: if upstream added a new strategy or a new place that sets a
     composite op on the smear/final painters, route it through `smearCompositeOp()`,
     `finalCompositeOp()` or `KisColorSmudgeSpectralUtils::spectralVariant()`, and any new
     `id() == COMPOSITE_COPY` check through `KisColorSmudgeSpectralUtils::isCopyOp()`.
   - If `KoCompositeOp::ParameterInfo` gained fields, check the copy in
     `KoCompositeOpSpectral::composite()` (threading) still makes sense.

3. Review the result: `git grep -n krita-spectral` and the `git diff --stat` command above.

4. Rebuild and test. The dependencies may have changed, so refresh them first:

   ```bash
   rm -rf _build _install _build-env
   build-tools/spectral/container-build.sh deps
   build-tools/spectral/container-build.sh configure
   build-tools/spectral/container-build.sh install
   build-tools/spectral/container-build.sh test 'Spectral'
   build-tools/spectral/container-build.sh test 'libs-pigment|plugins-paintops'
   build-tools/spectral/container-build.sh appimage
   ```

   For a release with a different build image, update `IMAGE` in
   `build-tools/spectral/container-build.sh` and `BUILD_IMAGE` in the workflow to the one
   in upstream's `.gitlab-ci.yml` (`VM_IMAGE_NAME_LINUX`).

5. Push: `git push --force-with-lease fork krita-spectral`.

If upstream's Qt6 build becomes the default, nothing here is Qt-version specific, but the
container scripts use the Qt5 platform (`Linux/Qt5/Shared`); switch them to Qt6 like upstream's
`linux-qt6` CI jobs do.
