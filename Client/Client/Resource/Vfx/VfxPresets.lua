-- 이펙트 프리셋 (ParticleSystem_Plan_KR.md 3.3, 3.4)
-- VfxManager::play("이름", 위치, 방향)으로 재생한다. 게임 실행 중 이 파일을 고치고 F5를 누르면 다시 읽는다.
--
-- 키 이름·단위는 C++ ParticleSystemSettings(유니티 ParticleSystem 모듈)와 같다.
--   범위 값: 숫자 하나(고정) 또는 { 최소, 최대 }
--   색: { r, g, b } 또는 { r, g, b, a } (0~1)
--   shape: "Point" | "Sphere" | "Hemisphere" | "Cone" | "Edge"
--   blend: "Additive" | "Alpha"
--   render_mode: "Billboard" | "StretchedBillboard"
--   bursts: { { time = 초, count = 개수 }, ... }  (VfxManager 재생은 time = 0 버스트만 사용)
-- 모르는 키나 형식이 틀린 값은 로그로 알리고 기본값을 쓴다.
--
-- 방향: play에 넘긴 방향이 방출 축(Cone의 중심축, Hemisphere의 위쪽)이다.

vfx = {}

-- 기존 프리셋을 복사해 일부 값만 바꾼 변형 만들기
local function extend(base, overrides)
    local t = {}
    for k, v in pairs(base) do t[k] = v end
    for k, v in pairs(overrides) do t[k] = v end
    return t
end

-- 평타 타격 스파크: 칼이 지나가는 방향으로 튀는 흰색 → 주황 불똥
vfx.hit_spark = {
    max_particles = 512,
    start_lifetime = { 0.15, 0.3 },
    start_speed = { 6, 12 },
    start_size = { 0.03, 0.06 },
    start_color = { 1, 1, 1, 1 },
    start_color_2 = { 1, 0.75, 0.3, 1 },
    end_color = { 1, 0.35, 0.05, 0 },
    gravity_modifier = 0.5,
    drag = 3,
    shape = "Cone",
    angle = 35,
    bursts = { { time = 0, count = 40 } },
    blend = "Additive",
    render_mode = "StretchedBillboard",
    length_scale = 0.02,
}

-- 대검 스킬 타격 스파크: 더 많고 크고 넓게
vfx.skill_hit_spark = extend(vfx.hit_spark, {
    max_particles = 1024,
    start_speed = { 10, 18 },
    start_size = { 0.05, 0.1 },
    angle = 50,
    bursts = { { time = 0, count = 120 } },
})
