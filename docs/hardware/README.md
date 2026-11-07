# Hardware

Full `vulkaninfo` dumps for each GPU the renderer is measured on.

| GPU | File | Driver | Vulkan API |
|---|---|---|---|
| AMD Radeon RX 5700 XT (RDNA1, discrete) | [vulkaninfo-rx5700xt.txt](vulkaninfo-rx5700xt.txt) | AMD proprietary 25.10.30.02 | 1.4.315 |

## RX 5700 XT: numbers that matter for compute

- Subgroup (wavefront) size: 64, with `subgroupSizeControl`
- Shared memory per workgroup: 32 KB
- Max invocations per workgroup: 1024
- `shaderFloat16`, `shaderInt8`, `shaderInt64`: yes
- `shaderBufferFloat32AtomicAdd`: no
- No `VK_KHR_acceleration_structure` / `VK_KHR_ray_query`: no hardware ray tracing, so BVH traversal stays in compute shaders on this GPU
