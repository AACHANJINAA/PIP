#include "pch.h"
#include "PacketManager.h"
#include "server.h"

//namespace PIP::server
//{
//    HANDLE g_iocp = nullptr;
//    std::atomic<int> g_new_id = 0;
//}


using namespace PIP;
int main()
{
	//// ================= Jolt 테스트 코드 시작 =================
	//std::cout << "--- Jolt Physics Test Start ---" << std::endl;

	//// Jolt 물리 엔진 초기화
	//JPH::RegisterDefaultAllocator();
	//JPH::Factory::sInstance = new JPH::Factory();
	//JPH::RegisterTypes();
	//JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);
	//JPH::JobSystemThreadPool job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
	//    std::thread::hardware_concurrency() - 1);

	//// 간단한 박스 형태(Shape) 생성 시도
	//JPH::BoxShapeSettings box_shape_settings(JPH::Vec3(1.0f, 2.0f, 3.0f));
	//JPH::ShapeSettings::ShapeResult box_shape_settings_result = box_shape_settings.Create();
	//JPH::ShapeRefC box_shape = box_shape_settings_result.Get();

	//if (box_shape_settings_result.HasError())    {
	//    std::cout << "Jolt Test Failed: Could not create box shape. Error: " <<
	//    box_shape_settings_result.GetError().c_str() << std::endl;
	//}
	//else if (box_shape != nullptr)
	//{
	//	JPH::Vec3 extent = static_cast<const JPH::BoxShape*>(box_shape.GetPtr())->GetHalfExtent();
	//	std::cout << "Jolt Test Success: BoxShape created with half-extent: "
	//			 << extent.GetX() << ", "
	//			 << extent.GetY() << ", "
	//			 << extent.GetZ() << std::endl;
	//}
	//else
	//{
	// std::cout << "Jolt Test Failed: Box shape is null." << std::endl;
	//}

	//std::cout << "--- Jolt Physics Test End ---" << std::endl;


    


    // I/O 스레드는 2개, 로직 스레드는 나머지 CPU 코어 수만큼 할당합니다.
	// (최소 1개의 로직 스레드는 보장)
	int total_cores = static_cast<int>(std::thread::hardware_concurrency());
	int logic_worker_thread_count = std::max(1, total_cores - 2);
	int io_worker_thread_count = 2;

    // 서버 스탈트!
    server::Server::Instance()->Start(io_worker_thread_count, logic_worker_thread_count);

    // 서버가 종료될 때까지 대기 (콘솔에서 Enter 키를 누르면 종료)
	MYLOG("Press Enter to stop the server..." << std::endl);
    std::cin.get();

    server::Server::Instance()->Stop();
    WSACleanup();
    
    return 0;
}


