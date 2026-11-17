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

A change that should not alter the picture (a BVH replacing brute force) is checked against
the reference from the same GPU with `--max-rmse 0`. Across devices, use the tolerance
measured in the table below.

## Measured

| Comparison | RMSE | Max diff | Pixels differing |
| --- | --- | --- | --- |
| lavapipe spheres, two runs | 0 | 0 | 0 |
| lavapipe spheres, 1024 vs 64 spp | 2.05 | 31 | 46.7% |
| lavapipe spheres, 1024 vs 1100 spp | 0.26 | 2 | 16.0% |
