#include "pch.h"
#include "DBManager.h"

#include "server.h"

namespace PIP::SERVER
{
    void DBManager::initialize(const std::wstring& conn_str) {
        _connectionString = conn_str;
        _is_running = true;

        // 1. ODBC 환경 초기화 및 연결 (생략: SQLAllocHandle 등 표준 ODBC 코드)
        // 2. 워커 스레드 시작
        _workerThread = std::thread(&DBManager::db_worker_thread, this);
    }

    void DBManager::finalize()
    {
        if (_is_running.exchange(false)) {
            if (_workerThread.joinable()) _workerThread.join();
            // ODBC 해제 로직...
        }
    }

    void DBManager::db_worker_thread() {
        MYLOG("[DB] DB Worker Thread Started.");

        while (_is_running) {
            DBTask task;
            if (_taskQueue.try_pop(task)) {

                // [1] DB 작업 수행 (테스트를 위해 시뮬레이션)
                if (task.type == DBTaskType::LOGIN_LOAD) {
                    // 실제 환경: SQL 쿼리 실행 (SELECT * FROM Characters ...)
                    // 테스트 환경: 0.1초 정도 연산하는 척 대기
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    MYLOG("[DB] Character Data Loaded for Session: " << task.session_id);
                }

                // [2] 작업 완료 후 콜백을 세션 담당 로직 스레드 큐로 반환
                if (task.callback) {
                    // Server 클래스에 구현된 get_logic_queue를 사용
                    auto* target_queue = Server::Instance()->get_logic_queue(task.logic_thread_idx);
                    if (target_queue) {
                        // LogicJob 구조체에 담아서 푸시 (Server.h에 정의된 구조에 맞춤)
                        target_queue->push({ std::move(task.callback) });
                    }
                }
            }
            else {
                // 작업이 없으면 짧게 휴식하여 CPU 점유율 방지
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
}
