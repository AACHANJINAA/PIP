#pragma once
class Object
{
public:
    Object(const std::string& name = "Object");
    virtual ~Object() = default;

    // KJ설명 :  객체의 복사와 이동을 금지합니다.
    //          모든 Object는 고유해야 하며, 소유권은 스마트 포인터로 관리해야 합니다.
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
    Object(Object&&) = delete;
    Object& operator=(Object&&) = delete;

    // --- Getter  ---
    uint64_t instance_id() const { return _instanceId; }
    bool is_destroyed() const { return _isDestroyed; }

    // ---- Setter ----
    const std::string& name() const { return _name; }
    void set_name(const std::string& name) { _name = name; }
    void set_destroyed(bool isDestroyed) { _isDestroyed = isDestroyed; }

    static void destroy(std::shared_ptr<Object> obj_to_destroy, float delay = 0.0f);
protected:
    std::string _name;
    bool _isDestroyed = false;
    const uint64_t _instanceId;
private:
    // 모든 Object 인스턴스에 대해 고유한 ID를 생성하기 위한 static 변수
	static std::atomic_uint64_t _nextInstanceId; //KJ 설명: atomic으로 변경, 멀티스레드 환경에서 안전하게 ID 생성 가능

};