#include "stdafx.h"
#include "OtherPlayerScript.h"

void OtherPlayerScript::on_sync_position(const XMFLOAT3& newPosition)
{
    // 서버가 알려준 위치로 내 GameObject의 위치를 설정
    if (transform())
    {
        transform()->set_local_position(newPosition);
    }
}

void OtherPlayerScript::update(float deltaTime)
{
    // OtherPlayer는 클라이언트에서 직접 조작하지 않으므로,
    // 이 update 함수는 보통 애니메이션 갱신이나 보간(interpolation) 로직을 처리합니다.
}