#include "pch.h"
#include "DBManager.h"

#include "server.h"

namespace PIP::SERVER
{
	void DBManager::initialize(const std::wstring& conn_str) {
		_connectionString = conn_str;
		_is_running = true;

		if (conn_str == L"DUMMY") {
			_is_dummy_mode = true;
			MYLOG("[DB] DUMMY MODE 활성화: 실제 DB 연결 없이 접속을 허용합니다.");

			// 워커 스레드만 띄우고 ODBC 연결은 건너뜀
			_workerThread = std::thread(&DBManager::db_worker_thread, this);
			return;
		}

		// 1. ODBC 환경 핸들 할당
		if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &_henv) != SQL_SUCCESS) {
			MYERROR("[DB] 환경 핸들 할당 실패");
			return;
		}

		// 2. ODBC 버전 설정 (SQL_OV_ODBC3 권장)
		SQLSetEnvAttr(_henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);

		// 3. 연결 핸들 할당
		if (SQLAllocHandle(SQL_HANDLE_DBC, _henv, &_hdbc) != SQL_SUCCESS) {
			MYERROR("[DB] 연결 핸들 할당 실패");
			return;
		}
		
		// 4. 실제 DB 연결 시도
		SQLWCHAR out_conn_str[1024];
		SQLSMALLINT out_conn_str_len;

		// SQLDriverConnect는 SQLConnect보다 더 유연하게 연결 문자열을 처리합니다.
		SQLRETURN ret = SQLDriverConnect(_hdbc, NULL, (SQLWCHAR*)_connectionString.c_str(), SQL_NTS,
			out_conn_str, 1024, &out_conn_str_len, SQL_DRIVER_NOPROMPT);

		if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
			SQLWCHAR sql_state[6], msg[1024];
			SQLINTEGER native_error;
			SQLSMALLINT msg_len;
			SQLGetDiagRec(SQL_HANDLE_DBC, _hdbc, 1, sql_state, &native_error, msg, 1024, &msg_len);

			
			MYERROR("[DB] MSSQL 서버 연결 실패! SQL State: " << W2S(sql_state) << ", Native Error: " << native_error << ", Message: " << W2S(msg));
		}

		MYLOG("[DB] MSSQL 서버에 성공적으로 연결되었습니다.");

		// 2. 핸들러 등록 (DBTaskType과 실제 처리 함수 매핑)
		RegisterHandler(DBTaskType::SAVE_INVENTORY_ALL, [this](const DBTask& task) { Handle_SAVE_INVENTORY_ALL(task); });
		RegisterHandler(DBTaskType::LOGIN_LOAD, [this](const DBTask& task) { Handle_LOGIN_LOAD(task); });

		// 3. 워커 스레드 시작
		_workerThread = std::thread(&DBManager::db_worker_thread, this);
	}

	void DBManager::finalize()
	{
		if (_is_running.exchange(false)) {
			if (_workerThread.joinable()) _workerThread.join();

			// ODBC 핸들 해제 (할당의 역순)
			if (_hdbc != SQL_NULL_HDBC) {
				SQLDisconnect(_hdbc);
				SQLFreeHandle(SQL_HANDLE_DBC, _hdbc);
			}
			if (_henv != SQL_NULL_HENV) {
				SQLFreeHandle(SQL_HANDLE_ENV, _henv);
			}
			MYLOG("[DB] ODBC 핸들 해제 완료.");
		}
	}

	void DBManager::db_worker_thread() {
		MYLOG("[DB] DB Worker Thread Started.");

		while (_is_running) {
			DBTask task;
			if (_taskQueue.try_pop(task)) {

				if (_is_dummy_mode) {
					if (task.callback) {
						auto* target_queue = Server::Instance()->get_logic_queue(task.logic_thread_idx);
						if (target_queue) {
							target_queue->push({ std::move(task.callback) });
						}
					}
					continue; // 아래의 실제 핸들러(SQL 로직) 실행을 건너뜀
				}

				// 1. 등록된 핸들러 찾아서 실행
				auto it = _handlers.find(task.type);
				if (it != _handlers.end()) {
					it->second(task); // 작업 실행!
				}
				else {
					MYERROR("[DB] 등록되지 않은 DB 작업 타입입니다: " << static_cast<int>(task.type));
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

	void DBManager::Handle_SAVE_INVENTORY_ALL(const DBTask& task) const
	{
		try {
			auto snapshot = std::any_cast<InventorySnapshot>(task.data);

			SQLHSTMT hStmt = SQL_NULL_HSTMT;
			if (SQLAllocHandle(SQL_HANDLE_STMT, _hdbc, &hStmt) != SQL_SUCCESS) {
				MYERROR("[DB] Statement 핸들 할당 실패");
				return;
			}

			SQLBIGINT playerId = task.session_id; // long long

			// =======================================================
			// 1. 기존 인벤토리 데이터 싹 비우기 (DELETE)
			// =======================================================
			std::wstring deleteQuery = L"DELETE FROM PlayerInventory WHERE PlayerId = ?";
			SQLPrepare(hStmt, (SQLWCHAR*)deleteQuery.c_str(), SQL_NTS);

			// 첫 번째 '?' 자리에 playerId 변수 묶기
			SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0, 0, &playerId, 0, NULL);
			SQLExecute(hStmt);

			// Statement를 닫고 파라미터 바인딩을 리셋 (다음 쿼리를 위해 필수!)
			SQLFreeStmt(hStmt, SQL_CLOSE);
			SQLFreeStmt(hStmt, SQL_RESET_PARAMS);


			// =======================================================
			// 2. 인벤토리 꽉꽉 채워넣기 (INSERT)
			// =======================================================
			// ItemUid는 IDENTITY(1,1) 설정으로 DB가 알아서 발급하므로 생략합니다.
			std::wstring insertQuery = L"INSERT INTO PlayerInventory (PlayerId, ItemId, Quantity, EnhanceLevel, IsEquipped) VALUES (?, ?, ?, ?, ?)";

			// DB야, 이 틀(붕어빵 틀) 파싱해서 캐싱해둬!
			SQLPrepare(hStmt, (SQLWCHAR*)insertQuery.c_str(), SQL_NTS);

			// 바인딩할 메모리 변수들 준비
			int itemId = 0;
			int quantity = 0;
			int enhanceLevel = 0;
			SQLSMALLINT isEquipped = 0; // BIT 타입은 보통 C++에서 short(SQLSMALLINT)로 매핑합니다.

			// 각 '?' 자리에 변수의 '주소(&)'를 연결해 둡니다.
			SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0, 0, &playerId, 0, NULL);
			SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &itemId, 0, NULL);
			SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &quantity, 0, NULL);
			SQLBindParameter(hStmt, 4, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &enhanceLevel, 0, NULL);
			SQLBindParameter(hStmt, 5, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_BIT, 0, 0, &isEquipped, 0, NULL);


			// --- [2-1] 재료 아이템 저장 ---
			for (const auto& [id, count] : snapshot.materials) {
				// 변수 값만 살짝 바꿔주고
				itemId = static_cast<int>(id);
				quantity = static_cast<int>(count);
				enhanceLevel = 0;
				isEquipped = 0;

				// 실행! (쿼리 파싱 안 하니까 0.001초 컷)
				SQLExecute(hStmt);
			}

			// --- [2-2] 장비 아이템 저장 ---
			for (const auto& [uid, equip] : snapshot.equipments) {
				itemId = static_cast<int>(equip.item_id);
				quantity = 1;
				enhanceLevel = equip.enhance_level;
				isEquipped = equip.is_equipped ? 1 : 0;

				SQLExecute(hStmt);
			}

			// 다 썼으면 핸들 깔끔하게 반환
			SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
			MYLOG("[DB] 인벤토리 갱신 완료! PlayerID: " << playerId
				<< " | Mats: " << snapshot.materials.size()
				<< " | Equips: " << snapshot.equipments.size());

		}
		catch (const std::bad_any_cast& e) {
			MYERROR("[DB] 인벤토리 저장 실패 - 캐스팅 에러: " << e.what());
		}
	}
	void DBManager::Handle_LOGIN_LOAD(const DBTask& task) const
	{
		try {
			// 로직 스레드가 넘겨준 빈 상자(포인터)를 꺼냅니다.
			auto loaded_data = std::any_cast<std::shared_ptr<std::any>>(task.data);

			SQLHSTMT hStmt = SQL_NULL_HSTMT;
			if (SQLAllocHandle(SQL_HANDLE_STMT, _hdbc, &hStmt) != SQL_SUCCESS) return;

			// 1. SELECT 쿼리 준비 (ItemUid도 가져옵니다!)
			std::wstring query = L"SELECT ItemUid, ItemId, Quantity, EnhanceLevel, IsEquipped FROM PlayerInventory WHERE PlayerId = ?";
			SQLPrepare(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);

			int64_t playerId = task.session_id;
			SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0, 0, &playerId, 0, NULL);

			// 2. 쿼리 실행
			if (SQLExecute(hStmt) == SQL_SUCCESS) {
				// 결과를 받을 변수들
				int64_t itemUid = 0;
				int itemId = 0;
				int quantity = 0;
				int enhanceLevel = 0;
				short isEquipped = 0;

				// 데이터 크기를 받을 버퍼 (ODBC 필수)
				SQLLEN cbUid, cbId, cbQty, cbEnh, cbEq;

				// 컬럼 바인딩 (1번 컬럼부터 순서대로)
				SQLBindCol(hStmt, 1, SQL_C_SBIGINT, &itemUid, 0, &cbUid);
				SQLBindCol(hStmt, 2, SQL_C_SLONG, &itemId, 0, &cbId);
				SQLBindCol(hStmt, 3, SQL_C_SLONG, &quantity, 0, &cbQty);
				SQLBindCol(hStmt, 4, SQL_C_SLONG, &enhanceLevel, 0, &cbEnh);
				SQLBindCol(hStmt, 5, SQL_C_SSHORT, &isEquipped, 0, &cbEq);

				InventorySnapshot snapshot;

				// 3. 데이터 한 줄씩 읽어오기 (Fetch)
				while (SQLFetch(hStmt) == SQL_SUCCESS || SQLFetch(hStmt) == SQL_SUCCESS_WITH_INFO) {

					// [기획 예시] ItemId가 2000 미만이면 재료, 이상이면 장비로 간주
					if (itemId < 2000) {
						snapshot.materials[static_cast<common::packet::ItemId>(itemId)] = quantity;
					}
					else {
						common::packet::EquipItem equip;
						equip.item_uid = itemUid;
						equip.item_id = static_cast<common::packet::ItemId>(itemId);
						equip.enhance_level = enhanceLevel;
						equip.is_equipped = (isEquipped != 0);

						snapshot.equipments[itemUid] = equip;
					}
				}

				// 4. 로직 스레드에서 넘겨준 빈 상자에 스냅샷 쏙 넣기!
				*loaded_data = snapshot;

				MYLOG("[DB] Session " << playerId << " 데이터 로드 완료. (아이템 "
					<< snapshot.materials.size() + snapshot.equipments.size() << "개)");
			}

			SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

		}
		catch (const std::bad_any_cast& e) {
			MYERROR("[DB] 로그인 로드 실패 - 상자(shared_ptr) 캐스팅 에러: " << e.what());
		}
	}
}
