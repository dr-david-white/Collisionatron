// MIT License
// Copyright (c) 2025 David White
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// Scene class for encapsulating the responsibily for storing / initialising the world and whatever is in it

#pragma once

#include "constants.h"
#include "Camera.h"
#include <d3d11_1.h>
#include "Cube.h"
#include <vector>
#include "ColliderManager.h"

typedef vector<Cube*> vecTypeDrawables;

class Scene
{
public:
	Scene() = default;
	~Scene() = default;

	HRESULT		init(HWND hwnd, const Microsoft::WRL::ComPtr<ID3D11Device>& device, const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& context);
	void		cleanUp();
	Camera*		getCamera() { return m_pCamera; }

	void		update(const float deltaTime);
	
	const LightPropertiesConstantBuffer& getLightProperties() { return m_lightProperties; }

private:
	void setupLightProperties();

private:
	Camera* m_pCamera;
	
	Microsoft::WRL::ComPtr <ID3D11Device>			m_pd3dDevice;
	Microsoft::WRL::ComPtr <ID3D11DeviceContext>	m_pImmediateContext;
	Microsoft::WRL::ComPtr <ID3D11Buffer>			m_pConstantBuffer;
	Microsoft::WRL::ComPtr <ID3D11Buffer>			m_pLightConstantBuffer;

	
	vecTypeDrawables		m_vecDrawables;
	ColliderManager			m_colliderManager;

	LightPropertiesConstantBuffer m_lightProperties;
};

