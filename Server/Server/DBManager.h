#pragma once


namespace PIP::SERVER
{
    // DB 작업 결과 후 로직 스레드에서 실행될 콜백
    using DBJobCallback = std::function<void()>;

    enum class DBTaskType  : uint32_t{
        DB_TASK_ERROR = 0,

        LOGIN_AUTH = 1,             // 로그인 및 캐릭터 로드
		LOGIN_LOAD = 2,             // 로그인 후 캐릭터 데이터 로드
        SAVE_CHARACTER = 3,         // 캐릭터 스탯 저장 (HP, 위치 등)
        ADD_ITEM_LOG = 4,           // 수집/획득 로그 기록

        SAVE_INVENTORY = 100,       // 인벤토리 아이템 저장
		SAVE_INVENTORY_ALL = 101    // 인벤토리 전체 저장 (자동 저장용)
    };

    // [추가] 인벤토리 스냅샷 전용 구조체
    // 로직 스레드에서 복사본을 만들어 DB 스레드로 전달할 때 사용합니다.
    struct InventorySnapshot {
        std::unordered_map<GAME::ItemId, uint32_t> materials;
        std::unordered_map<int64_t, GAME::EquipItem> equipments;
    };

    struct DBTask {
        DBTaskType type;
        int logic_thread_idx;   // 콜백을 돌려받을 로직 스레드 번호 (중요!)
        int64_t session_id;     // 대상 세션 ID
        DBJobCallback callback; // 로직 스레드에서 실행될 실제 함수
    
        // [추가] DB 스레드로 넘길 임의의 데이터 (스냅샷 등)
        std::any data;
    };


    class DBManager : public Singleton<DBManager> {
        friend class Singleton<DBManager>;
    private:
        DBManager() = default;
        ~DBManager() override { finalize(); }

        using DBHandler = std::function<void(const DBTask&)>;
    public:
        void initialize(const std::wstring& conn_str);
        void finalize();

        // 로직 스레드에서 DB 요청을 던질 때 사용
        void push_task(DBTask task) { _taskQueue.push(std::move(task)); }
        // [추가] 핸들러 등록 함수
        void RegisterHandler(DBTaskType type, DBHandler handler) {
            _handlers[type] = std::move(handler);
        }
    private:
        void db_worker_thread();
        // [추가] 실제 작업을 수행할 핸들러 함수들
        void Handle_SAVE_INVENTORY_ALL(const DBTask& task) const;
        void Handle_LOGIN_LOAD(const DBTask& task) const;

        std::unordered_map<DBTaskType, DBHandler> _handlers; // 핸들러 맵 주소록

        std::wstring _connectionString;
        std::thread _workerThread;
        concurrency::concurrent_queue<DBTask> _taskQueue;
        std::atomic<bool> _is_running = false;

        // ODBC Handles
        SQLHENV _henv = SQL_NULL_HENV;
        SQLHDBC _hdbc = SQL_NULL_HDBC;
    };
}

