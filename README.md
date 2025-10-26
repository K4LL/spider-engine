# 🕷️ Spider Engine 0.3

A modern **C++20 game engine prototype** built directly on **DirectX 12**.  
It experiments with explicit rendering, reflection-based shader binding, and a modular **Entity–Component System** powered by [Flecs](https://flecs.dev).

> 🧪 *Spider Engine is currently a prototype and architectural sandbox — not production-ready software.*  
> Its systems are unstable, experimental, and under continuous redesign.

---

## 🧠 Overview

Spider Engine’s goal is to provide a **clear, explicit, and modular foundation** for modern graphics programming.  
It isn’t trying to hide DirectX 12 — it’s designed to *teach, explore, and evolve* around it.

The project focuses on:
- **Transparency:** no hidden pipelines or black-box managers.  
- **Control:** developers own their resources, descriptors, and draw calls.  
- **Simplicity:** minimal dependencies, direct mapping to DX12 concepts.  
- **Extensibility:** architecture organized in independent subsystems that can be replaced, re-implemented, or extended.

At this stage, Spider Engine functions as a **skeleton for experimentation** — a base for building editors, tools, or custom render pipelines later on.

---

## ✨ Current Highlights

### 🎮 Core Systems
- **Entity–Component Architecture (Flecs)**  
  Lightweight runtime composition with flexible queries and system management.  
  Game logic and rendering are fully decoupled and can be extended by registering new components.

- **Windowing Layer**  
  Minimal Win32 wrapper with support for resizing, focus, and full-screen toggling.  
  Designed to stay out of the way — only what’s necessary to get a valid DirectX context running.

- **Engine Bootstrap**  
  `CoreEngine` ties together the ECS world, the renderer, and the window system into one controllable loop.  
  Meant for testing and early gameplay/system integration.

---

### 🧩 Shader Compilation & Pipelines
- **DXC Integration**  
  Uses Microsoft’s DirectX Shader Compiler with support for both file-based and in-memory shader sources.  

- **Reflection-Driven Pipelines**  
  Automatically inspects shaders for constant buffers, resources, and samplers to build compatible pipelines without manual root signature setup.  

- **Bind-By-Name Interface**  
  Resources can be bound directly by their shader names, making the workflow concise and expressive for early testing.  

---

### 🧱 Rendering Goals
The rendering layer focuses on being:
- **Explicit** — clear control over what happens on GPU and when.  
- **Predictable** — no hidden batching or background synchronization.  
- **Expandable** — future support for new rendering backends and advanced features like deferred shading, compute pipelines, and material systems.

While the current renderer is functional, it is still considered **experimental** and is being restructured frequently.

---

### 📸 Camera & Scene Foundation
- A straightforward, left-handed camera system with view and projection matrices ready for binding.  
- A minimal `Transform` abstraction used by both objects and cameras.  
- Provides just enough structure to render simple scenes while leaving room for future scene graph and editor integration.

---

### 🧩 Philosophy
Spider Engine’s design aims to serve as a **learning platform and research base**, not as a finished tool.  
It’s written with **clarity over convenience**, to make experimentation with rendering pipelines and ECS design easier.  

Long-term goals include:
- Replacing ad-hoc systems with data-driven ones.  
- Building an editor and runtime reflection system.  
- Implementing a more complete resource manager and material system.

---

## ⚙️ Project Status

| Area | Status | Notes |
|------|---------|-------|
| ECS Core | ✅ Functional | Basic entity/component support. |
| Renderer | ⚠️ Prototype | Stable for testing, not final. |
| Shader System | ⚙️ Experimental | Reflection works; API evolving. |
| Asset Loading | 🧪 Early | Textures and basic models load. |
| Editor/Tools | ❌ Not implemented | Planned for later stages. |

---

## 📚 Documentation
Documentation is available online:  
👉 [**Spider Engine Docs**](https://k4ll.github.io/spider-engine-docs/)  
or in the `docs/` folder within the repository.
