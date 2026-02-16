# VulkanDrawingApp

## Overview
VulkanDrawingApp is a high-performance interactive drawing application developed using **C++23** and the **Vulkan 1.3** API. The project implements a 2D rendering engine that utilizes persistent textures and advanced synchronization to allow real-time image manipulation.

This software is a technical modification and extension of the **How to Vulkan 2026** codebase (Sascha Willems), specifically adapted for dynamic texture state management and programmable shading via **Slang**.

## Technical Specifications
* **Graphics API:** Vulkan 1.3 (Requires `synchronization2` and `dynamic_rendering` support).
* **Language:** C++23.
* **Shading Language:** Slang (Compiled to SPIR-V 1.4).
* **Memory Management:** Vulkan Memory Allocator (VMA).
* **Windowing/Input:** SDL3.
* **Loader:** Volk.

## Implementation Details

### Synchronization and Barriers
The application utilizes a **Double Buffering** architecture managed through `VkFence` and `VkSemaphore`. Drawing persistence is achieved by using a canvas texture that transitions between read and write states within a single frame.

The implementation leverages **Vulkan Synchronization 2** (`VkImageMemoryBarrier2`) to resolve data dependencies:
1.  **Write Stage:** The texture transitions to `VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL` for draw operations.
2.  **Read Stage:** The texture transitions to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` to be sampled by the display shader during the swapchain presentation pass.

### Memory Management
The canvas texture, and transfer buffers are managed through **VMA**, optimizing device memory allocation and ensuring efficient resource lifecycle management.

## Build Requirements
* CMake 3.31 or higher.
* Vulkan SDK 1.3.
* C++23 compliant compiler.
* Integrated dependencies: `SDL3`, `glm`, `volk`, `vulkan-memory-allocator`, `slang`.

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## User Controls

### Interaction
| Input | Action |
| :--- | :--- |
| **Left Mouse Button** | Paint on canvas |
| **Right Mouse Button** | Eraser mode |
| **Space Bar** | Clear canvas (Reset to white) |

### Color Palette (Numerical Keys)
| Key | Color |
| :--- | :--- |
| **1** | Red |
| **2** | Yellow |
| **3** | Green |
| **4** | Cyan |
| **5** | Blue |
| **6** | Magenta |
| **7** | Orange |
| **8** | Gold / Yellow-Grey |
| **9** | Black |
| **0** | Grey |

## License
This project incorporates code under the MIT license from Sascha Willems (How to Vulkan 2026). Modifications and drawing application logic are property of **Tomás Federico Civiero**.
