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

    /// --- [변경] Getter ---
    bool is_destroyed() const { return _isDestroyed; }
    const std::string& name() const { return _name; }
    uint64_t unique_id() const { return _uniqueId; }

    // --- [변경] Setter ---
    void set_name(const std::string& name) { _name = name; }
    void set_destroyed(bool isDestroyed) { _isDestroyed = isDestroyed; }


    // --- [변경] destroy 함수 ---
    static void destroy(std::shared_ptr<Object> obj_to_destroy, float delay = 0.0f);
protected:
    std::string _name;
    uint64_t _uniqueId;
    bool _isDestroyed = false;
private:
    static std::atomic_uint64_t next_id;
};