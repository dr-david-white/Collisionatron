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

// base class for drawable / renderable objects - such as a cube

#pragma once

#include <d3d11_1.h>
#include <DirectXMath.h>
#include "wrl.h"
#include "structures.h"

using namespace DirectX;

class IRenderable
{
public:
	IRenderable();
	virtual ~IRenderable();

	virtual HRESULT	initMesh(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pContext) = 0;
	virtual void	update(const float deltaTime, ID3D11DeviceContext* pContext);
	virtual void	draw(ID3D11DeviceContext* pContext);
	virtual void	cleanup();

	const ID3D11Buffer* getVertexBuffer() const { return m_vertexBuffer.Get(); }
	const ID3D11Buffer* getIndexBuffer() const { return m_indexBuffer.Get(); }
	const ID3D11ShaderResourceView* getTextureResourceView() const { return m_textureResourceView.Get(); }
	const XMFLOAT4X4* getTransform() const { return &m_world; }
	const ID3D11SamplerState* getTextureSamplerState() const { return m_textureSampler.Get(); }
	ID3D11Buffer* getMaterialConstantBuffer() const { return m_materialConstantBuffer.Get(); }

	void	setPosition(const XMFLOAT3 position) { m_position = position; }
	void	setScale(const float scale) { m_scale = scale; }


protected:

	XMFLOAT4X4													m_world;
	MaterialPropertiesConstantBuffer							m_material;

	Microsoft::WRL::ComPtr < ID3D11Buffer>						m_vertexBuffer = nullptr;
	Microsoft::WRL::ComPtr < ID3D11Buffer>						m_indexBuffer = nullptr;
	Microsoft::WRL::ComPtr < ID3D11ShaderResourceView>			m_textureResourceView = nullptr;
	Microsoft::WRL::ComPtr < ID3D11SamplerState>				m_textureSampler = nullptr;

	Microsoft::WRL::ComPtr < ID3D11Buffer>						m_materialConstantBuffer = nullptr;
	XMFLOAT3													m_position;
	float														m_scale = 1;
	unsigned int												m_vertexCount = 0;
};

