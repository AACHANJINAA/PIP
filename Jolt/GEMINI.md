# Jolt Physics - Project Context

This directory contains the **Jolt Physics Library**, a high-performance, professional-grade physics engine for games. It is integrated into the **PIP (Slay The Lord)** project as the core engine for server-authoritative physics and combat hit detection.

## Project Overview

- **Core Technology**: Modern C++ (C++17/20/23).
- **Architecture**: Multi-threaded, SIMD-optimized (SSE, AVX, NEON) physics simulation.
- **Integration**: Used by the PIP server for kinematic movement, collision checks, and hit detection.

## Directory Structure

- **`Jolt/Core/`**: Foundational utilities, memory management (`Memory.h`), Job System (`JobSystem.h`), and profiling (`Profiler.h`).
- **`Jolt/Math/`**: Highly optimized math primitives (`Vec3.h`, `Mat44.h`, `Quat.h`) with SIMD support.
- **`Jolt/Physics/`**: The core physics logic:
    - **`Body/`**: Rigid body management and interfaces.
    - **`Collision/`**: Narrow-phase and broad-phase collision detection.
    - **`Constraints/`**: Joints and constraints (distance, hinge, etc.).
    - **`Character/`**: Character controllers for players and NPCs.
    - **`SoftBody/`** & **`Vehicle/`**: Specialized physics systems.
- **`Jolt/Geometry/`**: Geometric shapes and algorithms (AABB, Convex Hull, GJK, EPA).
- **`Jolt/ObjectStream/`**: Serialization system for physics assets.

## Building and Running

The project uses **CMake** for build configuration.

- **Primary Build File**: `Jolt/Jolt.cmake` contains the logic for including Jolt source files into a project.
- **Build Directory**: `Build/` contains the generated build files (e.g., `CMakeCache.txt`).
- **Initialization**: 
    - Call `JPH::RegisterTypes()` to initialize the factory and collision handlers.
    - Call `JPH::UnregisterTypes()` for cleanup.

## Development Conventions

### Naming Conventions
- **Types (Class, Struct, Enum)**: `PascalCase` (e.g., `PhysicsSystem`).
- **Functions**: `snake_case` (e.g., `update_physics()`).
    - Boolean returns: `is_`, `has_`, `can_` prefixes.
- **Member Variables**: `_` prefix + `camelCase` (e.g., `_bodyInterface`).
- **Variables / Parameters**: `snake_case` (e.g., `delta_time`).
- **Constants / Enum values**: `ALL_CAPS_SNAKE_CASE` (e.g., `MAX_BODIES`).

### Modern C++ Principles
- **Smart Pointers**: Prefer `std::unique_ptr` and `std::shared_ptr`.
- **ComPtr**: Use `Microsoft::WRL::ComPtr` for DirectX-related resource management if applicable.
- **Type Safety**: Use `PIP::Utils` (from `JoltHelper.h`) to convert between `common::Vec3` and `JPH::Vec3`.

## Implementation Notes

- **Server-Side Physics**: Most NPCs are handled as **Kinematic** or **Static** objects to minimize simulation overhead while allowing for precise shape-casting (`ShapeCast`) for movement and combat.
- **Deterministic Simulation**: Jolt is used to ensure the server world state is authoritative and consistent across all connected clients.
- **Debug Visualization**: The server can broadcast shape and hitbox data for the client to render debug wireframes.
