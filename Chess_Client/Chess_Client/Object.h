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
    ObjectID object_id() const { return _objectId; } // instance_id() -> object_id()
    bool is_destroyed() const { return _isDestroyed; }
    const std::string& name() const { return _name; }

    // --- [변경] Setter ---
    void set_name(const std::string& name) { _name = name; }
    void set_destroyed(bool isDestroyed) { _isDestroyed = isDestroyed; }

    // [추가] ObjectManager가 ID를 설정하기 위한 함수
    void set_object_id(ObjectID id) { _objectId = id; }

    // --- [변경] destroy 함수 ---
    // 이제 shared_ptr 대신 ObjectID를 받아 처리하는 것을 고려할 수 있으나,
    // 우선은 기존 구조를 유지합니다.
    static void destroy(std::shared_ptr<Object> obj_to_destroy, float delay = 0.0f);
protected:
    std::string _name;
    bool _isDestroyed = false;
    ObjectID _objectId = INVALID_OBJECT_ID;

};