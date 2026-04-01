# Jolt Physics Library - Instruction Context

This directory contains the **Jolt Physics Library** (v5.5.1), a high-performance, professional-grade 3D physics engine designed for games and VR applications. It is integrated into the PIP project for server-authoritative physics, kinematic movement, and combat hit detection.

## Project Overview

- **Core Technology**: Modern C++ (C++17/20).
- **Architecture**: Multi-threaded, SIMD-optimized (SSE, AVX, NEON), and cross-platform (Windows, Linux, Android, iOS, macOS, WASM).
- **Namespace**: `JPH`
- **Key Modules**:
    - **`Core/`**: Foundational utilities, memory management, job system, and profiling.
    - **`Math/`**: Highly optimized math primitives with SIMD support.
    - **`Physics/`**: Rigid body management, character controllers (Kinematic/Virtual), vehicles, and soft bodies.
    - **`Collision/`**: Narrow-phase and broad-phase collision detection (GJK, EPA, AABB Trees).
    - **`ObjectStream/`**: Serialization and RTTI system.

## Building and Integrating

Jolt uses **CMake** for build configuration. The main integration point is `Jolt.cmake`.

- **Initialization**:
    - Call `JPH::RegisterTypes()` to initialize the factory and collision handlers.
    - Call `JPH::UnregisterTypes()` for cleanup.
- **Verification**: Use `VerifyJoltVersionID()` to ensure ABI compatibility.
- **Build Options** (via CMake/Macros):
    - `JPH_DOUBLE_PRECISION`: Enable double precision for positions.
    - `JPH_CROSS_PLATFORM_DETERMINISTIC`: Ensure deterministic simulation across different platforms.
    - `JPH_DEBUG_RENDERER`: Enable the built-in debug renderer.
    - `JPH_PROFILE_ENABLED`: Enable the internal profiler.

## Development Conventions

### Naming Conventions
- **Types (Class, Struct, Enum)**: `PascalCase` (e.g., `PhysicsSystem`, `BodyInterface`).
- **Functions**: `snake_case` (e.g., `update_physics()`).
    - Boolean returns: `is_`, `has_`, `can_` prefixes.
- **Variables / Parameters**: `snake_case` (e.g., `delta_time`).
- **Member Variables**: `_` prefix + `camelCase` (e.g., `_bodyInterface`).
- **Constants / Enum values**: `ALL_CAPS_SNAKE_CASE` (e.g., `MAX_BODIES`).

### Modern C++ Principles
- **Memory Management**: Custom allocators are used. Prefer `JPH::Allocate` / `JPH::Free` or `TempAllocator`.
- **SIMD**: Vectors and matrices are SIMD-aligned. Use `JPH_VECTOR_ALIGNMENT` for manual alignment.
- **Floating Point**: Supports single and double precision modes via `Real` type and `JPH_IF_DOUBLE_PRECISION` macros.

## Implementation Notes

- **Authoritative Physics**: In PIP, the server handles physics. Most NPCs are **Kinematic** or **Static** to minimize overhead while allowing for precise `ShapeCast` for movement and combat.
- **Type Conversion**: Use `PIP::Utils` (from `JoltHelper.h` in the main project) to convert between `common::Vec3` and `JPH::Vec3`.
- **Job System**: Jolt provides its own `JobSystem` for parallel simulation steps.
