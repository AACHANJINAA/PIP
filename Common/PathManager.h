#pragma once
// 서버·클라 공용 경로 관리자.
//
// 모드
//  - 개발(기본): exe에서 위로 올라가 저장소 루트를 찾고, 저장소의 PathManifest.json에서 별칭 → 폴더를 읽는다.
//  - 배포: exe 옆에 Deploy.json(배포 스크립트 Tools/deploy.py이 생성)이 있을 때만. 별칭 → exe 기준 폴더를 읽는다.
//
// 경로 표기
//  - 기존: "Resource/UI/HP_Bar.dds" (App 루트 기준)
//  - 별칭: "UI:HP_Bar.dds" → Expand 하면 "Resource/UI/HP_Bar.dds" (App 밖 별칭은 절대 경로)
//  리소스 캐시 키와 파생 경로(parent_path)는 Expand 결과를 쓰고, 파일을 실제로 열 때만 ResolveApp 한다.
//
// 설명: 기획 & 계획/PathAlias_Migration_KR.md
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <windows.h>
#include "json.hpp"

namespace common
{
    enum class AppKind { Client, Server };

    enum class PathRoot
    {
        App,            // 배포: exe 폴더 / 개발: Client/Client 또는 Server/Server
        Shader,         // 클라 셰이더(.hlsl)
        Lua,            // 서버 Lua 데이터
        Saved,          // 실행 중 생성 파일(imgui.ini, 물리 덤프, used_files.txt)
        Count
    };

    class PathManager
    {
    public:
        static void Init(AppKind kind)
        {
            namespace fs = std::filesystem;
            _kind = kind;
            const char* app_key = (kind == AppKind::Client) ? "client" : "server";

            const fs::path exe_dir = GetExeDir();
            fs::path app;

            std::string error;
            if (fs::exists(exe_dir / "Deploy.json"))
            {
                _deployed = true;
                _manifest_path = exe_dir / "Deploy.json";
                app = exe_dir;

                nlohmann::json j;
                if (ReadJson(_manifest_path, j, error) && j.value("app", "") != app_key)
                    error = "Deploy.json의 app이 " + std::string(app_key) + "가 아님";
                else if (error.empty())
                {
                    for (auto& [name, rel] : j["aliases"].items())
                        _aliases[FromUtf8(name)] = (app / FromUtf8(rel.get<std::string>())).lexically_normal();
                }
            }
            else
            {
                _deployed = false;
                _repo = FindRepoRoot(exe_dir);
                if (_repo.empty()) _repo = FindRepoRoot(fs::current_path());

                if (_repo.empty())
                {
                    error = "저장소 루트(Common/Packet.h)를 찾을 수 없음";
                    app = fs::current_path();
                }
                else
                {
                    _manifest_path = _repo / "PathManifest.json";
                    nlohmann::json j;
                    if (ReadJson(_manifest_path, j, error))
                    {
                        app = _repo / FromUtf8(j["appRoots"].value(app_key, ""));
                        for (auto& [name, def] : j["aliases"].items())
                            _aliases[FromUtf8(name)] = (_repo / FromUtf8(def.value("dev", ""))).lexically_normal();
                    }
                    else
                    {
                        app = _repo / ((kind == AppKind::Client) ? "Client/Client" : "Server/Server");
                    }
                }
            }

            Root(PathRoot::App) = app;
            Root(PathRoot::Shader) = app / "Shaders";
            Root(PathRoot::Lua) = app / "Lua";
            Root(PathRoot::Saved) = app / "Saved";

            for (auto& r : _roots) r = r.lexically_normal();

            std::error_code ec;
            fs::create_directories(Root(PathRoot::Saved), ec);

            // 안전망: 놓친 상대 경로 호출도 App 기준으로 동작하게 한다.
            SetCurrentDirectoryW(app.c_str());

            if (!_deployed && !_repo.empty())
            {
                // 기록용 정적 객체를 atexit 등록보다 먼저 만들어야 종료 시 핸들러보다 늦게 파괴된다
                UsedMutex();
                UsedFiles();
                std::atexit(&PathManager::FlushUsedFiles);
            }

            LogAndValidate(error);
        }

        static bool IsDeployed() { return _deployed; }

        static const std::filesystem::path& Get(PathRoot root) { return Root(root); }

        // 별칭 폴더의 실제 경로. 없는 별칭이면 빈 경로
        static const std::filesystem::path& GetAlias(const std::string& name)
        {
            static const std::filesystem::path empty;
            auto it = _aliases.find(FromUtf8(name));
            return (it != _aliases.end()) ? it->second : empty;
        }

        // "별칭:나머지" → 논리 경로. App 안이면 App 기준 상대 경로("Resource/UI/a.dds"), 밖이면 절대 경로.
        // 별칭이 아니면 그대로 돌려준다.
        static std::string Expand(const std::string& path)
        {
            size_t colon = 0;
            if (!IsAliasForm(path, colon)) return path;

            const std::string name = path.substr(0, colon);
            auto it = _aliases.find(FromUtf8(name));
            if (it == _aliases.end())
            {
                std::cerr << "[PathManager] 모르는 별칭: " << path << std::endl;
                return path;
            }

            const std::filesystem::path abs = (it->second / std::filesystem::path(path.substr(colon + 1))).lexically_normal();
            const std::filesystem::path rel = abs.lexically_relative(Root(PathRoot::App));
            if (!rel.empty() && *rel.begin() != "..")
                return rel.generic_string();
            return abs.generic_string();
        }

        // 절대 경로는 그대로, 상대 경로는 root 기준으로 붙인다.
        static std::filesystem::path Resolve(PathRoot root, const std::filesystem::path& rel)
        {
            if (rel.empty()) return rel;
            std::filesystem::path result = rel.is_absolute() ? rel : Root(root) / rel;
            if (root != PathRoot::Saved) Record(result);
            return result;
        }

        // "Resource/..." 같은 App 기준 논리 경로, 또는 "별칭:..." 용
        static std::filesystem::path ResolveApp(const std::filesystem::path& logical)
        {
            size_t colon = 0;
            if (IsAliasForm(logical.native(), colon))
                return Resolve(PathRoot::App, std::filesystem::path(Expand(logical.string())));
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

        // 개발 모드에서 이번 실행에 연 파일을 Saved/used_files.txt에 합쳐 쓴다(기존 내용 유지). 종료 시 자동 호출.
        // 배포 범위 검사: Tools/deploy.py -Check
        static void FlushUsedFiles()
        {
            if (_deployed || _repo.empty()) return;
            namespace fs = std::filesystem;

            std::set<std::string> lines;
            const fs::path out_path = Root(PathRoot::Saved) / "used_files.txt";
            {
                std::ifstream in(out_path);
                std::string line;
                while (std::getline(in, line)) if (!line.empty()) lines.insert(line);
            }
            {
                std::lock_guard lock(UsedMutex());
                for (const auto& w : UsedFiles())
                {
                    const fs::path rel = fs::path(w).lexically_relative(_repo);
                    if (rel.empty() || *rel.begin() == "..") continue; // 저장소 밖 파일은 제외
                    lines.insert(ToUtf8(rel.generic_wstring()));
                }
            }
            std::ofstream out(out_path, std::ios::trunc);
            for (const auto& l : lines) out << l << '\n';
        }

    private:
        static std::filesystem::path& Root(PathRoot root) { return _roots[static_cast<size_t>(root)]; }

        static std::mutex& UsedMutex() { static std::mutex m; return m; }
        static std::set<std::wstring>& UsedFiles() { static std::set<std::wstring> s; return s; }

        static void Record(const std::filesystem::path& p)
        {
            if (_deployed) return;
            std::lock_guard lock(UsedMutex());
            UsedFiles().insert(p.lexically_normal().native());
        }

        // "이름:나머지" 형태인지. 이름은 2글자 이상의 영문·숫자·_ (드라이브 문자 "C:"와 구분)
        template <typename Str>
        static bool IsAliasForm(const Str& s, size_t& colon)
        {
            colon = s.find(':');
            if (colon == Str::npos || colon < 2) return false;
            for (size_t i = 0; i < colon; ++i)
            {
                const auto c = s[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
            }
            return true;
        }

        static std::wstring FromUtf8(const std::string& s)
        {
            if (s.empty()) return {};
            int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
            std::wstring out(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len);
            return out;
        }

        static bool ReadJson(const std::filesystem::path& p, nlohmann::json& j, std::string& error)
        {
            std::ifstream in(p);
            if (!in.is_open()) { error = ToUtf8(p) + " 을 열 수 없음"; return false; }
            try { in >> j; }
            catch (const std::exception& e) { error = ToUtf8(p) + " 파싱 실패: " + e.what(); return false; }
            if (!j.contains("aliases") || !j["aliases"].is_object()) { error = ToUtf8(p) + " 에 aliases 없음"; return false; }
            return true;
        }

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

        static void LogAndValidate(const std::string& error)
        {
            std::cout << "[PathManager] " << (_deployed ? "배포" : "개발") << " 모드, 매니페스트: "
                      << ToUtf8(_manifest_path) << ", 별칭 " << _aliases.size() << "개\n";
            std::cout << "  App = " << ToUtf8(Root(PathRoot::App)) << "\n";
            std::cout << "  Saved = " << ToUtf8(Root(PathRoot::Saved)) << "\n";

            for (const auto& [name, dir] : _aliases)
                if (!IsDir(dir)) std::cout << "  [별칭 폴더 없음] " << ToUtf8(name) << " = " << ToUtf8(dir) << "\n";

            std::wstring missing;

            auto require = [&](const std::filesystem::path& p)
            {
                if (!IsDir(p)) missing += L"\n" + p.native();
            };
            if (_kind == AppKind::Client) require(Root(PathRoot::Shader));
            else require(Root(PathRoot::Lua));

            std::wstring message;
            if (!error.empty()) message += L"\n" + FromUtf8(error);
            if (!missing.empty()) message += L"\n필수 폴더 없음:" + missing;
            if (!message.empty())
            {
                std::cerr << "[PathManager] 오류:" << ToUtf8(message) << std::endl;
                MessageBoxW(nullptr, (L"경로 설정 오류:" + message).c_str(), L"PathManager", MB_OK | MB_ICONERROR);
            }
        }

        static inline AppKind _kind = AppKind::Client;
        static inline bool _deployed = false;
        static inline std::filesystem::path _repo;
        static inline std::filesystem::path _manifest_path;
        static inline std::unordered_map<std::wstring, std::filesystem::path> _aliases;
        static inline std::array<std::filesystem::path, static_cast<size_t>(PathRoot::Count)> _roots;
    };
}
