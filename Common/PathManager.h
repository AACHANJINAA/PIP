#pragma once
// 서버·클라 공용 경로 관리자.
// exe 위치를 기준으로 배포 폴더인지 저장소(개발) 폴더인지 판단하고, 루트별 실제 경로를 계산한다.
// 리소스 캐시 키는 논리 경로("Resource/...")를 그대로 쓰고, 파일을 실제로 열 때만 Resolve 한다.
#include <array>
#include <filesystem>
#include <string>
#include <iostream>
#include <windows.h>

namespace common
{
    enum class AppKind { Client, Server };

    enum class PathRoot
    {
        App,            // 배포: exe 폴더 / 개발: Client/Client 또는 Server/Server
        Shader,         // 클라 셰이더(.hlsl)
        Lua,            // 서버 Lua 데이터
        ClientResource, // 서버가 읽는 클라 리소스(배포 시 서버 전용 사본)
        CommonData,     // Common/MapData, Common/World_Batch_glTF
        Saved,          // 실행 중 생성 파일(imgui.ini, 물리 덤프)
        Count
    };

    class PathManager
    {
    public:
        static void Init(AppKind kind)
        {
            namespace fs = std::filesystem;
            _kind = kind;

            const fs::path exe_dir = GetExeDir();
            fs::path app;
            fs::path repo;

            _deployed = (kind == AppKind::Client)
                ? (IsDir(exe_dir / "Resource") && IsDir(exe_dir / "Shaders"))
                : IsDir(exe_dir / "Lua");

            if (_deployed)
            {
                app = exe_dir;
            }
            else
            {
                repo = FindRepoRoot(exe_dir);
                if (repo.empty()) repo = FindRepoRoot(fs::current_path());

                if (!repo.empty())
                    app = repo / ((kind == AppKind::Client) ? "Client/Client" : "Server/Server");
                else
                    app = fs::current_path(); // 최후 수단: 예전처럼 작업 폴더 기준
            }

            Root(PathRoot::App) = app;
            Root(PathRoot::Shader) = app / "Shaders";
            Root(PathRoot::Lua) = app / "Lua";
            Root(PathRoot::Saved) = app / "Saved";

            if (_deployed)
            {
                Root(PathRoot::ClientResource) = app / "Data/ClientResource";
                Root(PathRoot::CommonData) = (kind == AppKind::Client) ? app / "Common" : app / "Data/Common";
            }
            else
            {
                const fs::path base = repo.empty() ? app / "../.." : repo;
                Root(PathRoot::ClientResource) = base / "Client/Client/Resource";
                Root(PathRoot::CommonData) = base / "Common";
            }

            for (auto& r : _roots) r = r.lexically_normal();

            std::error_code ec;
            fs::create_directories(Root(PathRoot::Saved), ec);

            // 안전망: 놓친 상대 경로 호출도 App 기준으로 동작하게 한다.
            SetCurrentDirectoryW(app.c_str());

            LogAndValidate();
        }

        static bool IsDeployed() { return _deployed; }

        static const std::filesystem::path& Get(PathRoot root) { return Root(root); }

        // 절대 경로는 그대로, 상대 경로는 root 기준으로 붙인다.
        static std::filesystem::path Resolve(PathRoot root, const std::filesystem::path& rel)
        {
            if (rel.empty() || rel.is_absolute()) return rel;
            return Root(root) / rel;
        }

        // "Resource/..." 같은 App 기준 논리 경로용
        static std::filesystem::path ResolveApp(const std::filesystem::path& logical)
        {
            return Resolve(PathRoot::App, logical);
        }

        // FMOD, Assimp처럼 UTF-8 char*를 받는 API용
        static std::string ToUtf8(const std::filesystem::path& p)
        {
            const std::wstring& w = p.native();
            if (w.empty()) return {};
            int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
            std::string out(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), len, nullptr, nullptr);
            return out;
        }

    private:
        static std::filesystem::path& Root(PathRoot root) { return _roots[static_cast<size_t>(root)]; }

        static bool IsDir(const std::filesystem::path& p)
        {
            std::error_code ec;
            return std::filesystem::is_directory(p, ec);
        }

        static std::filesystem::path GetExeDir()
        {
            std::wstring buf(MAX_PATH, L'\0');
            for (;;)
            {
                DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
                if (n == 0) return std::filesystem::current_path();
                if (n < buf.size()) { buf.resize(n); break; }
                buf.resize(buf.size() * 2);
            }
            return std::filesystem::path(buf).parent_path();
        }

        // 위로 올라가며 Common/Packet.h 가 있는 폴더(저장소 루트)를 찾는다.
        static std::filesystem::path FindRepoRoot(std::filesystem::path dir)
        {
            std::error_code ec;
            while (!dir.empty())
            {
                if (std::filesystem::exists(dir / "Common/Packet.h", ec)) return dir;
                std::filesystem::path parent = dir.parent_path();
                if (parent == dir) break;
                dir = parent;
            }
            return {};
        }

        static void LogAndValidate()
        {
            static const char* names[] = { "App", "Shader", "Lua", "ClientResource", "CommonData", "Saved" };
            std::cout << "[PathManager] " << (_deployed ? "배포" : "개발") << " 모드\n";
            for (size_t i = 0; i < _roots.size(); ++i)
            {
                const PathRoot r = static_cast<PathRoot>(i);
                if (_kind == AppKind::Client && (r == PathRoot::Lua || r == PathRoot::ClientResource)) continue;
                if (_kind == AppKind::Server && r == PathRoot::Shader) continue;
                std::cout << "  " << names[i] << " = " << ToUtf8(_roots[i]) << "\n";
            }

            std::wstring missing;
            auto require = [&](const std::filesystem::path& p)
            {
                if (!IsDir(p)) missing += L"\n" + p.native();
            };
            if (_kind == AppKind::Client)
            {
                require(Root(PathRoot::App) / "Resource");
                require(Root(PathRoot::Shader));
            }
            else
            {
                require(Root(PathRoot::Lua));
                require(Root(PathRoot::ClientResource));
                require(Root(PathRoot::CommonData));
            }

            if (!missing.empty())
            {
                std::cerr << "[PathManager] 필수 폴더 없음:" << ToUtf8(missing) << std::endl;
                MessageBoxW(nullptr, (L"필수 폴더를 찾을 수 없습니다:" + missing).c_str(), L"PathManager", MB_OK | MB_ICONERROR);
            }
        }

        static inline AppKind _kind = AppKind::Client;
        static inline bool _deployed = false;
        static inline std::array<std::filesystem::path, static_cast<size_t>(PathRoot::Count)> _roots;
    };
}
