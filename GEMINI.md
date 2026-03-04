
# PIP Project Overview

PIP is a high-precision Soul-like action game developed in C++ using DirectX 12 and Jolt Physics. The project features a robust Client-Server architecture designed to handle frame-perfect combat and synchronized physics-based movement.

## Development Philosophies

### Client Development Philosophy

Our client architecture is inspired by the **"Unity-like" DX (Developer Experience)**. The goal is to provide client programmers with a highly productive and intuitive coding environment (Ease of Use). While ensuring ease of development, we maintain strict performance targets for **memory efficiency and execution speed** to deliver **high-fidelity graphics and responsive gameplay**.

### Server Development Philosophy

The server is designed for a **deterministic, physics-driven world**. Our primary focus is on **high-speed processing, thread concurrency control, and minimizing network overhead**. We implement meticulous exception handling to ensure 24/7 stability. The codebase actively utilizes various **OOP Design Patterns** for maintainability, with a strategic roadmap toward adopting **Data-Oriented Design (DOD) and ECS (Entity Component System)** for future scalability.

## Architecture & Technology Stack

- **Core Language:** C++ (Modern C++17/20 standards)
- **Graphics API:** DirectX 12 (Windows-native)
- **Physics Engine:** Jolt Physics - Authoritative physics calculated on the server; high-precision combat hit detection.
- **Networking:** Custom IOCP-based server on Windows. Implements Client-Side Prediction, Server Reconciliation, and Lag Compensation (Rewind system).
- **AI System:** **Behavior Trees (BT)** - BTs are the primary logic framework for complex NPC AI, boss patterns, and reactive combat logic, ensuring modularity and ease of debugging.
- **Entity System:** ECS-like architecture where GameObject acts as a container for various Component and Behavior objects.
- **Data Formats:** glTF/GLB for 3D models, JSON for game data.

## Operational Protocols (Mandatory)

- **Response Language:** Always communicate and provide explanations in **Korean**.
- **File Encoding:** When writing or modifying source files (.cpp,.h,.lua), always use **UTF-8 with BOM (Byte Order Mark)**. This is critical to prevent Korean comments from becoming corrupted in the Windows/Visual Studio environment.
- **Gated Modification Workflow:**
   1. **Diff Review**: Before applying any changes to the files, you must present a detailed **diff comparison** for every file involved.
   2. **Approval**: Wait for explicit user approval before executing the `write_file` or `replace` tools.
   3. **Post-Edit Summary**: Once the modification is completed after approval, output a concise summary of the specific changes made in **English**.

## Coding Conventions

### Client & Server Common

- **Types (Class, Struct, Enum):** `PascalCase` (e.g., `GameObject`, `PhysicsComponent`)
- **Functions:** `snake_case` (e.g., `update_physics()`)
   - Boolean returning functions: Use prefixes like `is_`, `has_`, `can_` (e.g., `is_dead()`)
- **Variables / Parameters:** `snake_case` (e.g., `delta_time`)
- **Member Variables:** `_` prefix + `camelCase` (e.g., `_physicsSystem`)
- **Constants / Enum values:** `ALL_CAPS_SNAKE_CASE` (e.g., `MAX_PLAYERS`)

### Getters/Setters (Client Specific)

- **Getter:** No `get_` prefix, name like the variable, `const` qualifier. (e.g., `speed() const`)
- **Setter:** Use `set_` prefix. (e.g., `set_speed(float new_speed)`)

## Coding Principles

- **Memory Management:** Strictly prefer `std::unique_ptr` and `std::shared_ptr`. Avoid raw `new`/`delete`.
- **Physics Safety:** Always use `PIP::Utils` (from `JoltHelper.h`) to convert between game types (`common::Vec3`) and Jolt types (`JPH::Vec3`).
- **Concurrency:** Logic workers process tasks via thread-safe queues. Use `Room::PushJob` for room-specific data interactions.
- **BT Implementation:** Design modular BT nodes (Action, Condition, Decorator) that can be reused across different NPC types.


우리의 플랜 @.gemini/plan.md
