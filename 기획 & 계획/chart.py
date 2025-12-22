import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime

# 한글 폰트 설정 (Windows 기준 'Malgun Gothic')
plt.rcParams['font.family'] = 'Malgun Gothic'
plt.rcParams['axes.unicode_minus'] = False

# 데이터 설정 (2026년 버전)
tasks = [
    {"Task": "최종 기획 및 아키텍처 확정", "Start": "2026-01-01", "End": "2026-01-31", "Owner": "공통", "Color": "#800080"},
    {"Task": "서버 아키텍처 최적화", "Start": "2026-01-01", "End": "2026-01-31", "Owner": "박경준", "Color": "#87CEEB"},
    {"Task": "엔진/툴 프레임워크 기초", "Start": "2026-01-01", "End": "2026-01-31", "Owner": "박대원/임찬진", "Color": "#FFC0CB"},

    {"Task": "물리 엔진 & 서버 충돌 로직", "Start": "2026-02-01", "End": "2026-02-28", "Owner": "박경준", "Color": "#87CEEB"},
    {"Task": "GLTF 파서 & 애니메이션 블렌딩", "Start": "2026-02-01", "End": "2026-02-28", "Owner": "박대원", "Color": "#FFC0CB"},
    {"Task": "PBR 기초 및 UI 시스템 구축", "Start": "2026-02-01", "End": "2026-02-28", "Owner": "임찬진", "Color": "#00FFFF"},

    {"Task": "전투 시스템 / AI / DB 제작 (병렬)", "Start": "2026-03-01", "End": "2026-04-30", "Owner": "박경준",
     "Color": "#87CEEB"},
    {"Task": "보스 전투 / 제작 및 강화 시스템 (병렬)", "Start": "2026-03-01", "End": "2026-04-30", "Owner": "박대원",
     "Color": "#FFC0CB"},
    {"Task": "고급 PBR & 포스트 프로세싱 (병렬)", "Start": "2026-03-01", "End": "2026-04-30", "Owner": "임찬진", "Color": "#00FFFF"},

    {"Task": "공통 콘텐츠 완성 (맵/몹/템)", "Start": "2026-05-01", "End": "2026-05-31", "Owner": "공통", "Color": "#800080"},
    {"Task": "전체 시스템 통합 및 프로토타입", "Start": "2026-05-15", "End": "2026-05-31", "Owner": "공통", "Color": "#800080"},

    {"Task": "성능 최적화 및 안정화", "Start": "2026-06-01", "End": "2026-06-30", "Owner": "공통", "Color": "#800080"},

    {"Task": "최종 마감 및 발표 준비", "Start": "2026-07-01", "End": "2026-07-28", "Owner": "공통", "Color": "#800080"}
]

# 시작일 기준 역순 정렬 (차트 상단부터 일정 시작)
tasks.sort(key=lambda x: x["Start"], reverse=True)

fig, ax = plt.subplots(figsize=(14, 10))

for i, task in enumerate(tasks):
    start = datetime.strptime(task["Start"], "%Y-%m-%d")
    end = datetime.strptime(task["End"], "%Y-%m-%d")
    duration = (end - start).days
    ax.barh(i, duration, left=start, color=task["Color"], alpha=0.8, edgecolor='black', linewidth=0.5)
    ax.text(start, i, f'  {task["Owner"]}', va='center', ha='left', fontsize=9, fontweight='bold')

ax.set_yticks(range(len(tasks)))
ax.set_yticklabels([t["Task"] for t in tasks], fontsize=10)
ax.xaxis.set_major_locator(mdates.MonthLocator())
ax.xaxis.set_major_formatter(mdates.DateFormatter("%b"))
ax.set_title("Slay The Lord (S.T.L) 2026년 상반기 병렬 개발 일정", fontsize=16, pad=20)

# 최종 마감선 (7/28)
deadline = datetime(2026, 7, 28)
ax.axvline(deadline, color='red', linestyle='--', linewidth=2)
ax.text(deadline, -0.5, ' 최종 발표 (07/28)', color='red', fontweight='bold', ha='left')

plt.grid(axis='x', linestyle=':', alpha=0.6)
plt.tight_layout()

# 파일 저장 및 출력
plt.savefig("STL_병렬_개발_계획_최종_2026.png", dpi=300)
plt.show()