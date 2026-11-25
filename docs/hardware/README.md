# Hardware

Full `vulkaninfo` dumps for each GPU the renderer is measured on.

| GPU | File | Driver | Vulkan API |
|---|---|---|---|
| AMD Radeon RX 5700 XT (RDNA1, discrete), Windows desktop | [vulkaninfo-rx5700xt.txt](vulkaninfo-rx5700xt.txt) | AMD proprietary 25.10.30.02 | 1.4.315 |
| Intel Iris Xe (Tiger Lake GT2, integrated), Ubuntu 24.04 laptop | [vulkaninfo-iris-xe.txt](vulkaninfo-iris-xe.txt) | Mesa 25.2.8 (ANV) | 1.4.318 |

## RX 5700 XT: numbers that matter for compute

- Subgroup (wavefront) size: 64, with `subgroupSizeControl`
- Shared memory per workgroup: 32 KB
- Max invocations per workgroup: 1024
- `shaderFloat16`, `shaderInt8`, `shaderInt64`: yes
- `shaderBufferFloat32AtomicAdd`: no
- No `VK_KHR_acceleration_structure` / `VK_KHR_ray_query`: no hardware ray tracing, so BVH traversal stays in compute shaders on this GPU

## Iris Xe: numbers that matter for compute

- Subgroup size: 8 to 32 (default 32), with `subgroupSizeControl`
- Shared memory per workgroup: 64 KB
- Max invocations per workgroup: 1024
- `shaderFloat16`, `shaderInt8`, `shaderInt64`: yes
- `shaderBufferFloat32AtomicAdd`: no
- No hardware ray tracing on the Intel device

The laptop dump also lists `llvmpipe`, Mesa's CPU software renderer. It is the
only device there exposing `VK_KHR_acceleration_structure` / `VK_KHR_ray_query`.
Make sure the renderer picks the Intel GPU, not llvmpipe, before measuring.
