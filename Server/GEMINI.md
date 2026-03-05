@ARCHITECTURE_GUIDE.md 를 먼저읽고 파악해라

# PIP Server Project Overview

PIP is a high-precision Soul-like action game server developed in C++ using DirectX 12 (client-side) and Jolt Physics. This server is an authoritative, IOCP-based implementation designed for frame-perfect combat and synchronized physics.

## Architecture & Technology Stack

*   **Core Language:** C++ (Modern C++17/20 standards).
*   **Networking:** IOCP-based server core using a worker thread model (IO Workers and Logic Workers).
*   **Physics Engine:** [Jolt Physics](https://github.com/jrouwe/JoltPhysics) - Authoritative simulation for character movement and hit detection.
*   **Entity System:** Component-based architecture. `GameObject` is the base container, with `Actor`, `Player`, and `NPC` as specialized entities.
*   **AI System:** Supports both Behavior Trees (via a custom `BTBuilder`) and Lua 5.4 scripting.
*   **Room System:** Multithreaded management where each `Room` maintains its own `PhysicsSystem` and `GridMap` for spatial partitioning.
*   **Data Formats:** JSON for configuration, custom binary protocol for networking.

## Directory Structure (Server/)

*   `Server/`: Main source code and project files.
    *   `main.cpp`: Entry point, initializes `PhysicsManager` and `Server`.
    *   `server.h/cpp`: IOCP server implementation, session management, and worker threads.
    *   `Room.h/cpp`: Game room logic, physics updates, and player/NPC management.
    *   `GameObject.h/cpp`: Base class for the component-based entity system.
    *   `Component.h`: Base class for all components (Transform, Physics, Hitbox, AI, etc.).
    *   `Actor.h/cpp`, `Player.h/cpp`, `NPC.h/cpp`: Core entity implementations.
    *   `PhysicsManager.h/cpp`: Global Jolt Physics lifecycle management.
    *   `PacketManager.h/cpp` & `PacketHandlers.h/cpp`: Networking protocol and logic.
    *   `AIComponent.h/cpp`, `BehaviorTree.h/cpp`: AI logic and BT implementation.
    *   `lua-5.4.2_Win64_dll17_lib/`: Lua binaries and headers.

## Building and Running

### Prerequisites
*   Visual Studio 2022 or later.
*   Windows SDK.
*   Jolt Physics library (pre-built in the project root's `Jolt/` directory).

### Commands
*   **Build:** Open `Server.sln` in Visual Studio and use **Build Solution (Ctrl+Shift+B)**.
*   **Run:** Execute `x64/Debug/Server.exe` or `x64/Release/Server.exe`.
*   **Test:** (TODO) Stress testing tools are located in the `StressTest` solution (if available in the root).

## Development Conventions

### Naming Conventions
*   **Types (Class, Struct, Enum):** `PascalCase` (e.g., `GameObject`, `PhysicsComponent`).
*   **Functions:** `snake_case` (e.g., `update_physics()`).
    *   **Boolean returning functions:** Use prefixes like `is_`, `has_`, `can_` (e.g., `is_dead()`).
*   **Variables / Parameters:** `snake_case` (e.g., `delta_time`).
*   **Member Variables:** `_` prefix + `camelCase` (e.g., `_physicsSystem`).
*   **Constants / Enum values:** `ALL_CAPS_SNAKE_CASE` (e.g., `MAX_PLAYERS`).

### Coding Principles
*   **Memory Management:** Strictly prefer `std::unique_ptr` and `std::shared_ptr`. Avoid raw `new`/`delete`.
*   **Physics Safety:** Always use `PIP::Utils` (from `JoltHelper.h`) to convert between game types (`common::Vec3`) and Jolt types (`JPH::Vec3`).
*   **Concurrency:** Logic workers process tasks via thread-safe queues. Use `Room::PushJob` to ensure thread-safe interactions with room-specific data.
*   **Packet Handling:** Register new packet types in `PacketManager::Initialize()` and implement handlers in `PacketHandlers.cpp`.

## Key Systems

### Authoritative Physics & Combat
The server runs a full Jolt Physics simulation. Hit detection is performed by checking the attacker's "Attack Shape" against the target's `HitboxComponent`. The system supports lag compensation via a "Rewind" mechanism (future implementation/improvement goal).

### Component-Based AI
NPCs use `AIComponent`, which can be driven by:
1.  **Behavior Trees:** Created using the `BTBuilder` DSL-like syntax.
2.  **Lua Scripts:** Dynamic behavior defined in `.lua` files (e.g., `Monster.lua`).

### Room-based Load Balancing
Players are distributed into `Room` instances. Each room is assigned to a `LogicWorker` thread to parallelize game logic across multiple CPU cores.
