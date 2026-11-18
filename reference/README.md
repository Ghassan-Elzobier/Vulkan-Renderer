# Reference images

Known-good renders, so a change to the renderer (a BVH, a new scene layout, a shader
rewrite) can prove it still draws the same picture, not only a faster one.

## The settings

Every reference is 800x600 at 1024 samples per pixel, made by the harness:

```
./build/Ray_Tracing_Harness --scene spheres --spp 1024 --skip-timing --out reference/spheres_<device>.png
./build/Ray_Tracing_Harness --scene grid    --spp 1024 --skip-timing --out reference/grid_<device>.png
```

Run from the repository root (the shader path `build/raytrace.comp.spv` is still hard-coded).
`<device>` is `rx5700xt`, `irisxe` or `lavapipe` (the CPU driver the cloud uses).

The image phase always restarts at frame 0 and stops after `--spp` frames, so it does not
depend on the benchmark's warm-up or measured frame counts. `--skip-timing` only saves time.

## Checking a change

```
./build/Ray_Tracing_Harness --scene spheres --skip-timing --out /tmp/spheres.png
./build/Image_Compare reference/spheres_rx5700xt.png /tmp/spheres.png --max-rmse 0 --diff /tmp/diff.png
```

`Image_Compare` prints the RMSE (0 to 255 units), the largest single channel difference and
how many pixels differ, writes an optional diff image (differences scaled x16 by default), and
exits 0 on pass, 1 on fail, 2 on error.

## What to expect

- **Same GPU, same build:** RMSE 0. Every random number comes from integer hashing of the
  pixel and frame number, and the same compiler makes the same floating-point choices.
- **Different GPUs:** small, not 0. Compilers fuse `a*b + c` differently and `sqrt`,
  `normalize` and division may differ by a few ULP, so a ray that grazes a sphere can hit on
  one GPU and miss on the other. Each such path is 1/1024 of its pixel.
- **Fewer samples:** noise. On lavapipe, 1024 against 64 samples gives RMSE about 2.05.

## Which threshold

- **Same GPU, only C++ changed:** `--max-rmse 0`. Nothing that touches the maths changed.
- **Same GPU, shader changed but the picture should not (a BVH replacing brute force):**
  `--max-rmse 0.1`. The compiler may round differently; across whole devices that gives
  about 0.06, while a missing sphere gives about 12.
- **Across devices:** `--max-rmse 0.1`, from the table below.

## Measured

| Comparison | RMSE | Max diff | Pixels differing |
| --- | --- | --- | --- |
| lavapipe spheres, two runs | 0 | 0 | 0 |
| lavapipe spheres, 1024 vs 64 spp | 2.05 | 31 | 46.7% |
| lavapipe spheres, 1024 vs 1100 spp | 0.26 | 2 | 16.0% |
| lavapipe spheres vs a copy with the red sphere deleted (a fake BVH bug) | 12.35 | 138 | 36.4% |
| lavapipe vs RX 5700 XT, spheres | 0.0098 | 1 | 0.03% |
| RX 5700 XT vs Iris Xe, spheres | 0.0615 | 1 | 1.13% |
| RX 5700 XT vs Iris Xe, grid | 0.0620 | 1 | 1.15% |

## Timing at these settings (harness, 800x600, 1 spp per frame, 1000 measured frames)

| Device | spheres (Mpaths/s) | grid (Mpaths/s) |
| --- | --- | --- |
| RX 5700 XT | 3309 | 723 |
| Iris Xe | 340-406 (unsteady, see below) | 61 |
| lavapipe | 20.5 | 3.2 |

The scene is a specialization constant: as a push constant, the never-taken grid loop cost
the spheres scene 3.5% on the RX 5700 XT (3196 against 3313).

On 2026-11-18 the Iris Xe ran well under its 560 Mpaths/s baseline for both versions. Alternating
runs with 30 s pauses gave main 235, 213, 303 against this branch 406, 350, 340, so the branch is
not slower there; the laptop was. The iGPU shares power and cooling with the CPU, so never time
it straight after a build.
