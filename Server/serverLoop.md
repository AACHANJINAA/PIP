graph TD

&nbsp;   subgraph ST \[Phase 1: Bootstrap \& Initialization]

&nbsp;        A\[main 시작] --> B\[P-Core 인덱스 검출 및 선점]

&nbsp;        B --> C\[PhysicsManager 초기화]

&nbsp;        C --> D\[Server::Start 호출]

&nbsp;        D --> E\[WSAStartup \& PacketManager 초기화]

&nbsp;   end



&nbsp;    subgraph LD \[Phase 2: Data \& World Loading]

&nbsp;        E --> F\[Map/HeightMap JSON 데이터 로드]

&nbsp;        F --> G\[100개의 Room 생성 및 초기화]

&nbsp;        G --> H\[Room별 Jolt 물리 지형 \& NPC 스폰]

&nbsp;   end



&nbsp;   subgraph RUN \[Phase 3: Execution - The Dual Loop]

&nbsp;        H --> I\[Worker Threads 생성]

&nbsp;        I --> I1\[IO Workers: IOCP 이벤트 대기]

&nbsp;        I --> I2\[Logic Workers: 60Hz 게임 루프]

&nbsp;        I1 <-->|Job Queue / Shared Memory| I2

&nbsp;        I2 --> J{서버 실행 중?}

&nbsp;        J -- Yes --> I2

&nbsp;   end



&nbsp;   subgraph SD \[Phase 4: Graceful Shutdown]

&nbsp;       J -- No/Enter Key --> K\[Server::Stop 호출]

&nbsp;       K --> L\[모든 스레드에 종료 신호 전송]

&nbsp;       L --> M\[Thread Join: 스레드 안전 종료 대기]

&nbsp;       M --> N\[세션 풀 및 메모리 클린업]

&nbsp;       N --> O\[WSACleanup \& 프로세스 종료]

&nbsp;   end





&nbsp; sequenceDiagram

&nbsp;    participant C as Client

&nbsp;    participant IO as IO Worker (IOCP)

&nbsp;    participant JQ as Logic Job Queue

&nbsp;    participant LW as Logic Worker (60Hz)

&nbsp;    participant RM as Room (Physics/AI)



&nbsp;    Note over IO, LW: \[Runtime State]



&nbsp;    C->>IO: Packet 전송 (Move, Attack 등)

&nbsp;    IO->>IO: 패킷 조립 및 검증

&nbsp;    IO->>JQ: 람다 함수 형태로 작업 Push



&nbsp;    loop Every 16.6ms (60FPS)

&nbsp;        LW->>JQ: Job Queue에서 모든 작업 Pop \& 실행

&nbsp;        LW->>LW: Timer Queue (지연된 작업) 처리



&nbsp;        Note right of LW: Fixed Timestep Physics (60Hz)

&nbsp;        LW->>RM: UpdatePhysics (Jolt 시뮬레이션)



&nbsp;        Note right of LW: Logic Update (AI, AOI, Sync)

&nbsp;        LW->>RM: UpdateLogics (BT 실행 \& 그리드 갱신)

&nbsp;        RM->>IO: BroadcastNpcBatch (NPC 이동 일괄 전송)

&nbsp;    end



&nbsp;    IO->>C: 동기화 패킷 전송


graph TD

&nbsp;    subgraph Logic\_Loop \[Logic Worker Thread (Target 60FPS)]

&nbsp;        START(\[루프 시작]) --> JQ\[1. Job Queue 처리<br/>Login, EnterRoom 등 즉시 실행]

&nbsp;        JQ --> TQ\[2. Timer Queue 처리<br/>예약된 지연 작업 실행]



&nbsp;        subgraph Physics\_Step \[3. Fixed Physics Step (while accumulator >= 16.6ms)]

&nbsp;            PS\[Room::UpdatePhysics 실행]

&nbsp;            PS --> CCV\[Player CharacterController<br/>조작/넉백 물리 계산]

&nbsp;            CCV --> NPCP\[NPC 물리 시뮬레이션<br/>LOD 적용]

&nbsp;        end



&nbsp;        TQ --> PS

&nbsp;        Physics\_Step --> LU\[4. Room::UpdateLogics 실행]



&nbsp;        subgraph Logic\_Detail \[Logic Update 상세]

&nbsp;            AI\[AI/BT 업데이트]

&nbsp;            GM\[GridMap/AOI 갱신]

&nbsp;            BS\[BroadcastNpcBatch<br/>NPC 이동 일괄 전송]

&nbsp;        end



&nbsp;        LU --- AI

&nbsp;        AI --- GM

&nbsp;        GM --- BS



&nbsp;        BS --> SLP\[5. 16ms 유지를 위한 Sleep]

&nbsp;        SLP --> START

&nbsp;    end

