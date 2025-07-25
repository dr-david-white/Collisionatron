#include "IRenderable.h"

IRenderable::IRenderable()
{
	m_vertexBuffer = nullptr;
	m_indexBuffer = nullptr;
	m_textureResourceView = nullptr;
	m_textureSampler = nullptr;

	// Initialize the world matrix
	XMStoreFloat4x4(&m_world, XMMatrixIdentity());
}

IRenderable::~IRenderable()
{
	cleanup();
}

void IRenderable::update(const float deltaTime, ID3D11DeviceContext* pContext)
{
	XMMATRIX translate = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	XMMATRIX scale = XMMatrixScaling(m_scale, m_scale, m_scale);
	XMMATRIX world = scale * translate;
	XMStoreFloat4x4(&m_world, world);
}

void IRenderable::draw(ID3D11DeviceContext* pContext)
{
	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	ID3D11Buffer* vbuf = m_vertexBuffer.Get();
	pContext->IASetVertexBuffers(0, 1, &vbuf, &stride, &offset);

	// Set index buffer
	pContext->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

	// Set primitive topology
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set the texture and sampler
	if (m_textureResourceView != nullptr)
	{
		ID3D11ShaderResourceView* srv = m_textureResourceView.Get();
		pContext->PSSetShaderResources(0, 1, &srv);
		ID3D11SamplerState* ss = m_textureSampler.Get();
		pContext->PSSetSamplers(0, 1, &ss);
	}

	// draw
	pContext->DrawIndexed(m_vertexCount, 0, 0);
}

void IRenderable::cleanup()
{
	// we are using com pointers so no release() necessary
}
