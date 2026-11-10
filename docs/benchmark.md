# Running the benchmark

The renderer doubles as a GPU benchmark. It renders 100 warm-up frames, times the next 1000 frames with GPU timestamp queries, prints the results and exits on its own.

If this is the first time on a machine, install the [prerequisites](#prerequisites) first.

## Commands

Run every command from the project root. The program loads `build/raytrace.comp.spv` from the folder you launch it in, so it fails to start anywhere else.

Always benchmark a Release build. Debug builds turn on the Vulkan validation layer, which slows every frame.

### Windows (PowerShell or Command Prompt)

```powershell
cmake -B build
cmake --build build --config Release
.\build\Release\Ray_Tracing_Renderer.exe
```

### Linux

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/Ray_Tracing_Renderer
```

The two platforms choose the build type at different steps. Visual Studio keeps Debug and Release in one build folder and picks one when you build (`--config`). Ninja on Linux picks one when you configure (`CMAKE_BUILD_TYPE`), which is also why the Linux executable sits directly in `build/` with no `Release/` subfolder.

The first command is needed once per build folder. After that, the build and run commands are enough, and CMake reconfigures itself if `CMakeLists.txt` changes.

## Reading the output

Sample output from a Release build on a laptop with Intel Iris Xe graphics (Linux, Wayland session):

```text
GPU: Intel(R) Iris(R) Xe Graphics (TGL GT2), timestampPeriod: 52.0833ns
Resolution: 800x600
GPU: Intel(R) Iris(R) Xe Graphics (TGL GT2)
Frames: 1000 (1 spp each, max 8 bounces)
Dispatch avg 2.21395 ms, min1.67766 ms, max4.6437 ms
Mpaths/s: 216.807
```

| Line | Meaning |
|---|---|
| `timestampPeriod` | Nanoseconds per GPU timestamp tick, used to convert timestamps to milliseconds. |
| `Resolution` | Size of the rendered image. The window asks for 800x600, but display scaling can make the image larger, so compare runs at the same resolution. |
| `Frames` | Number of timed frames, not counting the 100 warm-up frames. Each frame traces one sample (path) per pixel. |
| `Dispatch avg / min / max` | GPU time of the ray-tracing compute dispatch alone. Copying the image to the window and presenting it are not included. |
| `Mpaths/s` | Millions of paths per second: pixels per frame divided by the average dispatch time. |

## Getting consistent results

- **Let the run finish.** Results print only after all 1000 timed frames. Closing the window early exits without printing anything.
- **Reduce noise.** Close other GPU-heavy programs and, on a laptop, plug in the charger. Integrated GPUs change clock speed with power and temperature, so compare the average over a few runs rather than a single run.
- **Compare like with like on Linux.** The windowing system changes the numbers. On the Iris Xe laptop above, the same build averaged about 2.2 ms per dispatch on Wayland and about 0.8 ms through X11, so only compare runs made the same way.
- **Linux with VS Code installed as a snap:** launching from VS Code's built-in terminal crashes at startup with a GLib error mentioning `antialiasing`. Any of these avoids it:
  - Run it from a normal terminal instead (same Wayland results as everywhere else).
  - Install VS Code from the `.deb` package instead of the snap.
  - Run `XDG_SESSION_TYPE=x11 ./build/Ray_Tracing_Renderer`, which goes through X11 and skips the GTK code that crashes. Its results are X11 numbers (see above).

## Prerequisites

### Windows

- Visual Studio with the "Desktop development with C++" workload
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) (provides the Vulkan headers, `glslc` and the validation layer, and sets `VULKAN_SDK`)
- CMake 3.20 or newer
- Git (CMake downloads GLFW with it during the configure step)

### Linux (Ubuntu 24.04)

```bash
sudo apt install -y \
  cmake ninja-build git g++ \
  libvulkan-dev vulkan-validationlayers glslc \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libwayland-dev libxkbcommon-dev wayland-protocols
```

You also need a Vulkan driver for your GPU: `mesa-vulkan-drivers` for Intel and AMD, or the proprietary NVIDIA driver.
