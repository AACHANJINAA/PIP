# CMake 최소 버전 설정
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# 1. 프로젝트 정의
project(MY_GAME_PROJECT CXX)
set(CMAKE_USE_RELATIVE_PATHS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC 런타임 라이브러리 설정 (정적 라이브러리, /MTd 또는 /MT)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# 2. 외부 라이브러리 (Jolt Physics) 설정
include(FetchContent)
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG "v5.4.0"
)
set(JPH_CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "Enable Jolt cross-platform determinism" FORCE)
FetchContent_GetProperties(JoltPhysics)
FetchContent_MakeAvailable(JoltPhysics)
add_subdirectory(${joltphysics_SOURCE_DIR}/Build ${joltphysics_BINARY_DIR})

# ===================================================================
# 클라이언트 프로젝트 (Client)
# ===================================================================

# -- 클라이언트 소스 파일 수동 정의 --
set(CLIENT_HEADER_FILES
    "Client/Client/GameFramework.h"
    "Client/Client/ObjectManager.h"
    "Client/Client/TimerManager.h"
    "Client/Client/Scene.h"
    "Client/Client/SceneManager.h"
    "Client/Client/InputManager.h"
    "Client/Client/Shader.h"
    "Client/Client/json.hpp"
    "Client/Client/Chess_Scene.h"
    "Client/Client/NetworkManager.h"
    "Client/Client/main.h"
    "Client/Client/Renderer.h"
    "Client/Client/Resource.h"
    "Client/Client/resource1.h"
    "Client/Client/stdafx.h"
    "Client/Client/targetver.h"
    "Client/Client/MainPlayerScript.h"
    "Client/Client/OtherPlayerScript.h"
    "Client/Client/ResourceManager.h"
    "Client/Client/BoardCubeScript.h"
    "Client/Client/DebugShader.h"
    "Client/Client/DefaultObjectShader.h"
    "Client/Client/PlayerShader.h"
    "Client/Client/RootSignature.h"
    "Client/Client/GlbShader.h"
    "Client/Client/ReadGLBMesh.h"
    "Client/Client/Object.h"
    "Client/Client/Behaviour.h"
    "Client/Client/GameObject.h"
    "Client/Client/Component.h"
    "Client/Client/TransformComponent.h"
    "Client/Client/ScriptComponent.h"
    "Client/Client/RenderComponent.h"
    "Client/Client/ColiderComponent.h"
    "Client/Client/LayerManager.h"
    "Client/Client/Mesh.h"
    "Client/Client/CameraComponent.h"
    "Client/Client/ReadFBXMesh.h"
    "Client/Client/ReadGLTFMesh.h"
    "Client/Client/ReadOBJMesh.h"
    "Client/Client/FreeCameraScript.h"
    "Client/Client/GltfShader.h"
    "Client/Client/GltfTestScript.h"
    "Client/Client/Camera.h"
    "Client/Client/FreeCamera.h"
    "Client/Client/DescriptorManager.h"
    "Client/Client/NPCScript.h"
    "Client/Client/DDSTextureLoader12.h"
    "Client/Client/WICTextureLoader12.h"
)
set(CLIENT_SOURCE_FILES
    "Client/Client/GameFramework.cpp"
    "Client/Client/ObjectManager.cpp"
    "Client/Client/TimerManager.cpp"
    "Client/Client/Scene.cpp"
    "Client/Client/SceneManager.cpp"
    "Client/Client/InputManager.cpp"
    "Client/Client/Shader.cpp"
    "Client/Client/Chess_Scene.cpp"
    "Client/Client/NetworkManager.cpp"
    "Client/Client/main.cpp"
    "Client/Client/Renderer.cpp"
    "Client/Client/stdafx.cpp"
    "Client/Client/MainPlayerScript.cpp"
    "Client/Client/OtherPlayerScript.cpp"
    "Client/Client/ResourceManager.cpp"
    "Client/Client/BoardCubeScript.cpp"
    "Client/Client/DebugShader.cpp"
    "Client/Client/DefaultObjectShader.cpp"
    "Client/Client/PlayerShader.cpp"
    "Client/Client/RootSignature.cpp"
    "Client/Client/GlbShader.cpp"
    "Client/Client/ReadGLBMesh.cpp"
    "Client/Client/Object.cpp"
    "Client/Client/Behaviour.cpp"
    "Client/Client/GameObject.cpp"
    "Client/Client/Component.cpp"
    "Client/Client/TransformComponent.cpp"
    "Client/Client/ScriptComponent.cpp"
    "Client/Client/RenderComponent.cpp"
    "Client/Client/ColiderComponent.cpp"
    "Client/Client/LayerManager.cpp"
    "Client/Client/Mesh.cpp"
    "Client/Client/CameraComponent.cpp"
    "Client/Client/ReadFBXMesh.cpp"
    "Client/Client/ReadGLTFMesh.cpp"
    "Client/Client/ReadOBJMesh.cpp"
    "Client/Client/FreeCameraScript.cpp"
    "Client/Client/GltfShader.cpp"
    "Client/Client/GltfTestScript.cpp"
    "Client/Client/Camera.cpp"
    "Client/Client/FreeCamera.cpp"
    "Client/Client/DescriptorManager.cpp"
    "Client/Client/NPCScript.cpp"
    "Client/Client/DDSTextureLoader12.cpp"
    "Client/Client/WICTextureLoader12.cpp"
)
set(CLIENT_RESOURCE_FILES
    "Client/Client/Chess_Client.rc"
    "Client/Client/Resource.rc"
    "Client/Client/Chess_Client.ico"
    "Client/Client/small.ico"
)
set(CLIENT_SHADER_FILES
    "Client/Client/GLB_Shader.hlsl"
    "Client/Client/Debug.hlsl"
    "Client/Client/Shaders.hlsl"
    "Client/Client/Gltf_Shader.hlsl"
)

# -- 클라이언트 실행 파일 생성 --
add_executable(Client WIN32 
    ${CLIENT_HEADER_FILES}
    ${CLIENT_SOURCE_FILES}
    ${CLIENT_RESOURCE_FILES}
    ${CLIENT_SHADER_FILES}
)
# 셰이더 파일들이 컴파일되지 않고 IDE에만 표시되도록 속성 설정
set_source_files_properties(${CLIENT_SHADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

# 디버깅 시 작업 디렉터리를 원본 소스 폴더로 설정 (원래 VS프로젝트 방식)
set_property(TARGET Client PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Client/Client")

# -- 클라이언트 필터(Source Group) 설정 --
source_group("0. Engine\\0. Core" FILES "Client/Client/GameFramework.h" "Client/Client/ObjectManager.h" "Client/Client/TimerManager.h" "Client/Client/GameFramework.cpp" "Client/Client/ObjectManager.cpp" "Client/Client/TimerManager.cpp")
source_group("0. Engine\\1. GameObject & Components\\0. Object" FILES "Client/Client/Object.h" "Client/Client/Object.cpp")
source_group("0. Engine\\1. GameObject & Components\\1. Behaviour" FILES "Client/Client/Behaviour.h" "Client/Client/Behaviour.cpp")
source_group("0. Engine\\1. GameObject & Components\\2. GameObject" FILES "Client/Client/GameObject.h" "Client/Client/LayerManager.h" "Client/Client/GameObject.cpp" "Client/Client/LayerManager.cpp")
source_group("0. Engine\\1. GameObject & Components\\3. Components" FILES "Client/Client/Component.h" "Client/Client/TransformComponent.h" "Client/Client/ScriptComponent.h" "Client/Client/RenderComponent.h" "Client/Client/ColiderComponent.h" "Client/Client/CameraComponent.h" "Client/Client/Component.cpp" "Client/Client/TransformComponent.cpp" "Client/Client/ScriptComponent.cpp" "Client/Client/RenderComponent.cpp" "Client/Client/ColiderComponent.cpp" "Client/Client/CameraComponent.cpp")
source_group("0. Engine\\2. Scene" FILES "Client/Client/Scene.h" "Client/Client/SceneManager.h" "Client/Client/Scene.cpp" "Client/Client/SceneManager.cpp")
source_group("0. Engine\\3. Renderer" FILES "Client/Client/Shader.h" "Client/Client/Renderer.h" "Client/Client/RootSignature.h" "Client/Client/Shader.cpp" "Client/Client/Renderer.cpp" "Client/Client/RootSignature.cpp")
source_group("0. Engine\\3. Renderer\\1. Shaders" FILES "Client/Client/DebugShader.h" "Client/Client/DefaultObjectShader.h" "Client/Client/PlayerShader.h" "Client/Client/GlbShader.h" "Client/Client/GltfShader.h" "Client/Client/DebugShader.cpp" "Client/Client/DefaultObjectShader.cpp" "Client/Client/PlayerShader.cpp" "Client/Client/GlbShader.cpp" "Client/Client/GltfShader.cpp")
source_group("0. Engine\\3. Renderer\\2. Meshes" FILES "Client/Client/ReadGLBMesh.h" "Client/Client/Mesh.h" "Client/Client/ReadFBXMesh.h" "Client/Client/ReadGLTFMesh.h" "Client/Client/ReadOBJMesh.h" "Client/Client/ReadGLBMesh.cpp" "Client/Client/Mesh.cpp" "Client/Client/ReadFBXMesh.cpp" "Client/Client/ReadGLTFMesh.cpp" "Client/Client/ReadOBJMesh.cpp")
source_group("0. Engine\\3. Renderer\\3. Camera" FILES "Client/Client/Camera.h" "Client/Client/FreeCamera.h" "Client/Client/Camera.cpp" "Client/Client/FreeCamera.cpp")
source_group("0. Engine\\3. Renderer\\4. TextureLoader" FILES "Client/Client/DDSTextureLoader12.h" "Client/Client/WICTextureLoader12.h" "Client/Client/DDSTextureLoader12.cpp" "Client/Client/WICTextureLoader12.cpp")
source_group("0. Engine\\4. Resource" FILES "Client/Client/ResourceManager.h" "Client/Client/DescriptorManager.h" "Client/Client/ResourceManager.cpp" "Client/Client/DescriptorManager.cpp")
source_group("0. Engine\\5. Input" FILES "Client/Client/InputManager.h" "Client/Client/InputManager.cpp")
source_group("0. Engine\\6. Network" FILES "Client/Client/NetworkManager.h" "Client/Client/NetworkManager.cpp")
source_group("1. Game\\0. GameObjects" FILES )
source_group("1. Game\\1. Scenes" FILES "Client/Client/Chess_Scene.h" "Client/Client/Chess_Scene.cpp")
source_group("1. Game\\2. Scripts" FILES "Client/Client/MainPlayerScript.h" "Client/Client/OtherPlayerScript.h" "Client/Client/BoardCubeScript.h" "Client/Client/FreeCameraScript.h" "Client/Client/GltfTestScript.h" "Client/Client/NPCScript.h" "Client/Client/MainPlayerScript.cpp" "Client/Client/OtherPlayerScript.cpp" "Client/Client/BoardCubeScript.cpp" "Client/Client/FreeCameraScript.cpp" "Client/Client/GltfTestScript.cpp" "Client/Client/NPCScript.cpp")
source_group("1. Game\\3. Components" FILES )
source_group("2. Shaders" FILES "Client/Client/GLB_Shader.hlsl" "Client/Client/Debug.hlsl" "Client/Client/Shaders.hlsl" "Client/Client/Gltf_Shader.hlsl")
source_group("3. Resources" FILES "Client/Client/Resource.h" "Client/Client/resource1.h" "Client/Client/Chess_Client.rc" "Client/Client/Resource.rc" "Client/Client/Chess_Client.ico" "Client/Client/small.ico")
source_group("4. External" FILES "Client/Client/json.hpp" "../../Common/Packet.h" "../../Common/PacketStream.h" "../../Common/Vector3.h" "Client/Client/packages.config")
source_group("5. App" FILES "Client/Client/main.h" "Client/Client/stdafx.h" "Client/Client/targetver.h" "Client/Client/main.cpp" "Client/Client/stdafx.cpp")

# -- 클라이언트 PCH, 라이브러리, 빌드 후 명령 설정 --
target_precompile_headers(Client PRIVATE "${CMAKE_SOURCE_DIR}/Client/Client/stdafx.h")
target_compile_definitions(Client PRIVATE NOMINMAX UNICODE _UNICODE)

# --- Include Directories (Forced Relative) ---
file(RELATIVE_PATH REL_COMMON_DIR ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/Common)
file(RELATIVE_PATH REL_DXTEX_INCLUDE_DIR ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/Client/packages/directxtex_desktop_win10.2025.7.10.1/include)
file(RELATIVE_PATH REL_ASSIMP_INCLUDE_DIR ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/Client/packages/AssimpCpp.5.0.1.6/build/native/include)

target_include_directories(Client PRIVATE
    ${REL_COMMON_DIR}
    ${REL_DXTEX_INCLUDE_DIR}
    ${REL_ASSIMP_INCLUDE_DIR}
)

# --- Libraries (Forced Relative) ---
target_link_libraries(Client PRIVATE Jolt::Jolt)

# DirectXTex
set(DIRECTXTEX_PATH_ABS ${CMAKE_SOURCE_DIR}/Client/packages/directxtex_desktop_win10.2025.7.10.1)
file(RELATIVE_PATH REL_DXTEX_LIB_DEBUG ${CMAKE_BINARY_DIR} ${DIRECTXTEX_PATH_ABS}/native/lib/x64/Debug/DirectXTex.lib)
file(RELATIVE_PATH REL_DXTEX_LIB_RELEASE ${CMAKE_BINARY_DIR} ${DIRECTXTEX_PATH_ABS}/native/lib/x64/Release/DirectXTex.lib)
target_link_libraries(Client PRIVATE
    debug ${REL_DXTEX_LIB_DEBUG}
    optimized ${REL_DXTEX_LIB_RELEASE}
)

# Assimp
set(ASSIMP_PATH_ABS ${CMAKE_SOURCE_DIR}/Client/packages/AssimpCpp.5.0.1.6)
file(RELATIVE_PATH REL_ASSIMP_LIB_DEBUG ${CMAKE_BINARY_DIR} ${ASSIMP_PATH_ABS}/build/native/lib/Debug/assimp-vc142-mtd.lib)
file(RELATIVE_PATH REL_ASSIMP_LIB_RELEASE ${CMAKE_BINARY_DIR} ${ASSIMP_PATH_ABS}/build/native/lib/Release/assimp-vc142-mt.lib)
target_link_libraries(Client PRIVATE
    debug ${REL_ASSIMP_LIB_DEBUG}
    optimized ${REL_ASSIMP_LIB_RELEASE}
)

# --- Post-Build Command ---
add_custom_command(TARGET Client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<IF:$<CONFIG:Debug>,${ASSIMP_PATH_ABS}/build/native/bin/Debug/assimp-vc142-mtd.dll,${ASSIMP_PATH_ABS}/build/native/bin/Release/assimp-vc142-mt.dll>"
    $<TARGET_FILE_DIR:Client>
    COMMENT "Copying Assimp DLL to output directory"
)

# ===================================================================
# 서버 프로젝트 (Server)
# ===================================================================

# -- 서버 소스 파일 정의 --
set(SERVER_FILES
    "Server/Server/PacketHandlers.cpp"
    "Server/Server/PacketManager.cpp"
    "Server/Server/MapDataManager.cpp"
    "Server/Server/Room.cpp"
    "Server/Server/server.cpp"
    "Server/Server/Player.cpp"
    "Server/Server/main.cpp"
    "Server/Server/pch.cpp"
    "Server/Server/NPC.cpp"
    "Server/Server/PacketHandlers.h"
    "Server/Server/PacketManager.h"
    "Server/Server/json.hpp"
    "Server/Server/MapDataManager.h"
    "Server/Server/Room.h"
    "Server/Server/server.h"
    "Server/Server/Player.h"
    "Server/Server/pch.h"
    "Server/Server/ServerCore.h"
    "Server/Server/NPC.h"
)

# -- 서버 실행 파일 생성 --
add_executable(Server ${SERVER_FILES})

# -- 서버 필터(Source Group) 설정 --
source_group("소스 파일\\Packet" FILES "Server/Server/PacketHandlers.cpp" "Server/Server/PacketManager.cpp" "Server/Server/PacketHandlers.h" "Server/Server/PacketManager.h")
source_group("소스 파일\\JSON" FILES "Server/Server/json.hpp")
source_group("소스 파일\\Manager\\MapDataManager" FILES "Server/Server/MapDataManager.cpp" "Server/Server/MapDataManager.h")
source_group("소스 파일\\Server" FILES "Server/Server/Room.cpp" "Server/Server/server.cpp" "Server/Server/Room.h" "Server/Server/server.h")
source_group("소스 파일\\Player" FILES "Server/Server/Player.cpp" "Server/Server/Player.h")
source_group("소스 파일\\Core" FILES "Server/Server/main.cpp" "Server/Server/pch.cpp" "Server/Server/pch.h" "Server/Server/ServerCore.h")
source_group("소스 파일\\NPC" FILES "Server/Server/NPC.cpp" "Server/Server/NPC.h")
source_group("소스 파일\\Common" FILES "../../Common/Packet.h" "../../Common/PacketStream.h" "../../Common/Vector3.h")

# -- 서버 PCH, 라이브러리, 빌드 후 명령 설정 --
# 디버깅 시 작업 디렉터리를 프로젝트 루트로 설정
set_property(TARGET Server PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
target_precompile_headers(Server PRIVATE "Server/Server/pch.h")
target_include_directories(Server PRIVATE Common)
target_link_libraries(Server PRIVATE Jolt::Jolt)