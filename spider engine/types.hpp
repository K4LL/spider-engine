#pragma once
#include <DirectXMath.h>

namespace spider_engine::rendering {
	struct alignas(16) Transform {
		DirectX::XMVECTOR scale;
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR rotation;

		Transform() : 
			scale(DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f)),
			position(DirectX::XMVectorZero()),
			rotation(DirectX::XMQuaternionIdentity()) 
		{}
		Transform(DirectX::XMVECTOR scale, 
			      DirectX::XMVECTOR position, 
				  DirectX::XMVECTOR rotation) :
			scale(scale), 
			position(position), 
			rotation(rotation) 
		{}
	};

	struct alignas(16) FrameData {
		DirectX::XMMATRIX projection;
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX model;
	};
}