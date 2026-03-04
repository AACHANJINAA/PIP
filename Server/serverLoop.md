graph TD

    subgraph ST \[Phase 1: Bootstrap \& Initialization]

         A\[main 시작] --> B\[P-Core 인덱스 검출 및 선점]

         B --> C\[PhysicsManager 초기화]

         C --> D\[Server::Start 호출]

         D --> E\[WSAStartup \& PacketManager 초기화]

    end



     subgraph LD \[Phase 2: Data \& World Loading]

         E --> F\[Map/HeightMap JSON 데이터 로드]

         F --> G\[100개의 Room 생성 및 초기화]

         G --> H\[Room별 Jolt 물리 지형 \& NPC 스폰]

    end



    subgraph RUN \[Phase 3: Execution - The Dual Loop]

         H --> I\[Worker Threads 생성]

         I --> I1\[IO Workers: IOCP 이벤트 대기]

         I --> I2\[Logic Workers: 60Hz 게임 루프]

         I1 <-->|Job Queue / Shared Memory| I2

         I2 --> J{서버 실행 중?}

         J -- Yes --> I2

    end



    subgraph SD \[Phase 4: Graceful Shutdown]

        J -- No/Enter Key --> K\[Server::Stop 호출]

        K --> L\[모든 스레드에 종료 신호 전송]

        L --> M\[Thread Join: 스레드 안전 종료 대기]

        M --> N\[세션 풀 및 메모리 클린업]

        N --> O\[WSACleanup \& 프로세스 종료]

    end





  sequenceDiagram

     participant C as Client

     participant IO as IO Worker (IOCP)

     participant JQ as Logic Job Queue

     participant LW as Logic Worker (60Hz)

     participant RM as Room (Physics/AI)



     Note over IO, LW: \[Runtime State]



     C->>IO: Packet 전송 (Move, Attack 등)

     IO->>IO: 패킷 조립 및 검증

     IO->>JQ: 람다 함수 형태로 작업 Push



     loop Every 16.6ms (60FPS)

         LW->>JQ: Job Queue에서 모든 작업 Pop \& 실행

         LW->>LW: Timer Queue (지연된 작업) 처리



         Note right of LW: Fixed Timestep Physics (60Hz)

         LW->>RM: UpdatePhysics (Jolt 시뮬레이션)



         Note right of LW: Logic Update (AI, AOI, Sync)

         LW->>RM: UpdateLogics (BT 실행 \& 그리드 갱신)

         RM->>IO: BroadcastNpcBatch (NPC 이동 일괄 전송)

     end



     IO->>C: 동기화 패킷 전송



graph TD

     subgraph Logic\_Loop \[Logic Worker Thread (Target 60FPS)]

         START(\[루프 시작]) --> JQ\[1. Job Queue 처리<br/>Login, EnterRoom 등 즉시 실행]

         JQ --> TQ\[2. Timer Queue 처리<br/>예약된 지연 작업 실행]



         subgraph Physics\_Step \[3. Fixed Physics Step (while accumulator >= 16.6ms)]

             PS\[Room::UpdatePhysics 실행]

             PS --> CCV\[Player CharacterController<br/>조작/넉백 물리 계산]

             CCV --> NPCP\[NPC 물리 시뮬레이션<br/>LOD 적용]

         end



         TQ --> PS

         Physics\_Step --> LU\[4. Room::UpdateLogics 실행]



         subgraph Logic\_Detail \[Logic Update 상세]

             AI\[AI/BT 업데이트]

             GM\[GridMap/AOI 갱신]

             BS\[BroadcastNpcBatch<br/>NPC 이동 일괄 전송]

         end



         LU --- AI

         AI --- GM

         GM --- BS



         BS --> SLP\[5. 16ms 유지를 위한 Sleep]

         SLP --> START

     end

