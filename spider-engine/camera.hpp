#pragma once
#include <DirectXMath.h>

#include "types.hpp"

namespace spider_engine::rendering {
    class Camera {
    private:
        uint32_t width_  = 1;
        uint32_t height_ = 1;

        float fovY_  = DirectX::XM_PIDIV4;
        float nearZ_ = 0.01f;
        float farZ_  = 1000.0f;

        DirectX::XMVECTOR up_               = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        DirectX::XMMATRIX viewMatrix_       = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX projectionMatrix_ = DirectX::XMMatrixIdentity();

    public:
        Transform transform;

        Camera(uint32_t width, uint32_t height) : 
            width_(width), height_(height) 
        {}

        void setViewport(uint32_t width, uint32_t height) {
            width_  = width;
            height_ = height;
            updateProjectionMatrix();
        }

        void setFov(float fovRadians) {
            fovY_ = fovRadians;
            updateProjectionMatrix();
        }

        void setClippingPlanes(float nearZ, float farZ) {
            nearZ_ = nearZ;
            farZ_  = farZ;
            updateProjectionMatrix();
        }

        void updateProjectionMatrix() {
            float aspectRatio = static_cast<float>(width_) / static_cast<float>(height_);
            projectionMatrix_ = DirectX::XMMatrixPerspectiveFovLH(fovY_, aspectRatio, nearZ_, farZ_);
        }

        void updateViewMatrix() {
            DirectX::XMMATRIX rotMatrix = DirectX::XMMatrixRotationQuaternion(transform.rotation);

            DirectX::XMVECTOR forward = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0, 0, 1, 0), rotMatrix);
            DirectX::XMVECTOR target  = DirectX::XMVectorAdd(transform.position, forward);

            viewMatrix_ = DirectX::XMMatrixLookAtLH(transform.position, target, up_);
        }

        DirectX::XMMATRIX getViewMatrix() const {
            return viewMatrix_;
        }

        DirectX::XMMATRIX getProjectionMatrix() const {
            return projectionMatrix_;
        }

        DirectX::XMMATRIX getViewProjectionMatrix() const {
            return projectionMatrix_ * viewMatrix_;
        }
    };
}