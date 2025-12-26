//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "SimpleCamera.h"

SimpleCamera::SimpleCamera():
    m_initialPosition(0, 0, 0),
    m_position(m_initialPosition),
    m_yaw(XM_PI),
    m_pitch(0.0f),
    m_lookDirection(0, 0, -1),
    m_upDirection(0, 1, 0),
    m_moveSpeed(20.0f),
    m_turnSpeed(XM_PIDIV2),
    m_mouseSensitivity(0.003f),
    m_keysPressed{}
{
}

void SimpleCamera::Init(XMFLOAT3 position)
{
    m_initialPosition = position;
    Reset();
}

void SimpleCamera::SetMoveSpeed(float unitsPerSecond)
{
    m_moveSpeed = unitsPerSecond;
}

void SimpleCamera::SetTurnSpeed(float radiansPerSecond)
{
    m_turnSpeed = radiansPerSecond;
}

void SimpleCamera::Reset()
{
    m_position = m_initialPosition;
    m_yaw = XM_PI;
    m_pitch = 0.0f;
    m_lookDirection = { 0, 0, -1 };
}

void SimpleCamera::Update(float elapsedSeconds)
{
    float moveInterval = m_moveSpeed * elapsedSeconds;
    float rotateInterval = m_turnSpeed * elapsedSeconds;

    // Обработка поворота клавишами
    if (m_keysPressed.left)
        m_yaw += rotateInterval;
    if (m_keysPressed.right)
        m_yaw -= rotateInterval;
    if (m_keysPressed.up)
        m_pitch += rotateInterval;
    if (m_keysPressed.down)
        m_pitch -= rotateInterval;

    // Ограничение угла наклона
    m_pitch = min(m_pitch, XM_PIDIV2 - 0.01f);
    m_pitch = max(-XM_PIDIV2 + 0.01f, m_pitch);

    // Вычисляем направление взгляда
    float r = cosf(m_pitch);
    m_lookDirection.x = r * sinf(m_yaw);
    m_lookDirection.y = sinf(m_pitch);
    m_lookDirection.z = r * cosf(m_yaw);

    // Вектор "вправо" (перпендикулярен направлению взгляда и вверх)
    XMVECTOR lookDir = XMLoadFloat3(&m_lookDirection);
    XMVECTOR upDir = XMLoadFloat3(&m_upDirection);
    XMVECTOR rightDir = XMVector3Normalize(XMVector3Cross(upDir, lookDir));

    // Движение вперёд/назад по направлению взгляда (полёт)
    XMVECTOR position = XMLoadFloat3(&m_position);

    if (m_keysPressed.w)
        position = XMVectorAdd(position, XMVectorScale(lookDir, moveInterval));
    if (m_keysPressed.s)
        position = XMVectorSubtract(position, XMVectorScale(lookDir, moveInterval));

    // Движение влево/вправо (стрейф)
    if (m_keysPressed.a)
        position = XMVectorAdd(position, XMVectorScale(rightDir, moveInterval));
    if (m_keysPressed.d)
        position = XMVectorSubtract(position, XMVectorScale(rightDir, moveInterval));

    // Движение вверх/вниз (абсолютное, по мировой оси Y)
    if (m_keysPressed.e)
        position = XMVectorAdd(position, XMVectorScale(upDir, moveInterval));
    if (m_keysPressed.q)
        position = XMVectorSubtract(position, XMVectorScale(upDir, moveInterval));

    XMStoreFloat3(&m_position, position);
}

XMMATRIX SimpleCamera::GetViewMatrix()
{
    return XMMatrixLookToRH(XMLoadFloat3(&m_position), XMLoadFloat3(&m_lookDirection), XMLoadFloat3(&m_upDirection));
}

XMMATRIX SimpleCamera::GetProjectionMatrix(float fov, float aspectRatio, float nearPlane, float farPlane)
{
    return XMMatrixPerspectiveFovRH(fov, aspectRatio, nearPlane, farPlane);
}

void SimpleCamera::OnKeyDown(WPARAM key)
{
    switch (key)
    {
    case 'W':
        m_keysPressed.w = true;
        break;
    case 'A':
        m_keysPressed.a = true;
        break;
    case 'S':
        m_keysPressed.s = true;
        break;
    case 'D':
        m_keysPressed.d = true;
        break;
    case 'Q':
        m_keysPressed.q = true;
        break;
    case 'E':
        m_keysPressed.e = true;
        break;
    case VK_LEFT:
        m_keysPressed.left = true;
        break;
    case VK_RIGHT:
        m_keysPressed.right = true;
        break;
    case VK_UP:
        m_keysPressed.up = true;
        break;
    case VK_DOWN:
        m_keysPressed.down = true;
        break;
    case VK_ESCAPE:
        Reset();
        break;
    }
}

void SimpleCamera::OnKeyUp(WPARAM key)
{
    switch (key)
    {
    case 'W':
        m_keysPressed.w = false;
        break;
    case 'A':
        m_keysPressed.a = false;
        break;
    case 'S':
        m_keysPressed.s = false;
        break;
    case 'D':
        m_keysPressed.d = false;
        break;
    case 'Q':
        m_keysPressed.q = false;
        break;
    case 'E':
        m_keysPressed.e = false;
        break;
    case VK_LEFT:
        m_keysPressed.left = false;
        break;
    case VK_RIGHT:
        m_keysPressed.right = false;
        break;
    case VK_UP:
        m_keysPressed.up = false;
        break;
    case VK_DOWN:
        m_keysPressed.down = false;
        break;
    }
}

void SimpleCamera::OnMouseMove(int dx, int dy)
{
    m_yaw -= dx * m_mouseSensitivity;
    m_pitch -= dy * m_mouseSensitivity;

    // Prevent looking too far up or down.
    m_pitch = min(m_pitch, XM_PIDIV2 - 0.01f);
    m_pitch = max(-XM_PIDIV2 + 0.01f, m_pitch);
}

void SimpleCamera::SetMouseSensitivity(float sensitivity)
{
    m_mouseSensitivity = sensitivity;
}
