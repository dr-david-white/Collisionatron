#include "ColliderManager.h"
#include "DX11App.h"
#include "DX11Renderer.h"
#include "globals.h"


constexpr int multithreaded_multiplier = 1; // 1 = use the number of native HW threads (probably 16)

ColliderManager::ColliderManager() : m_threadPool(std::thread::hardware_concurrency()* multithreaded_multiplier)
{

}

void ColliderManager::init(ID3D11Device* device, ID3D11DeviceContext* context)
{
	m_localCollisionResults.reserve(m_threadPool.threadCount());
	for (unsigned int i = 0; i < m_threadPool.threadCount(); i++)
	{
		vector<CollisionPair> x;
		m_localCollisionResults.push_back(x);
	}


	initBoxes();

	//if constexpr (use_method == use_gpu) // commented out as the demo now uses all three methods
	{
		// the counter will never need re-creating so create here
		createAtomicCounterBuffer(device, &m_pCounterBuffer, &m_pCounterBufferSRV);
		createStagingReadBuffer(device, m_pCounterBuffer, &m_pStagingBufferCounter);

		releaseAndCreateCSResources(device);


		ID3DBlob* pCSBlob = nullptr;
		HRESULT hr = DX11Renderer::compileShaderFromFile(L"computeshader.hlsl", "main", "cs_5_0", &pCSBlob);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The Compute Shader cannot be compiled, sorry.", L"Error", MB_OK);
			return;

		}

		// Create the vertex shader
		hr = device->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), nullptr, &m_pComputeShader);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The Compute Shader cannot be created, sorry.", L"Error", MB_OK);
		}
	}
}

void ColliderManager::releaseAndCreateCSResources(ID3D11Device* device)
{
	m_pBoxBufferSRV.Reset();
	m_pCollisionPairBuffer.Reset();
	m_pCollisionPairBufferSRV.Reset();
	m_pStagingBufferCollisionPairs.Reset();
	m_pStagingBoxBuffer.Reset();

	// input boxes
	createGPUBoxBuffer(device, m_boxes.size(), &m_pBoxBuffer, &m_pBoxBufferSRV);
	// output collision pairs and counter
	createCollisionOutputBuffer(device, g_cube_count, &m_pCollisionPairBuffer, &m_pCollisionPairBufferSRV);

	// two CPU readable staging buffers
	createStagingReadBuffer(device, m_pCollisionPairBuffer, &m_pStagingBufferCollisionPairs);
	createStagingWriteBuffer(device, g_cube_count, &m_pStagingBoxBuffer);
}

HRESULT ColliderManager::createStagingReadBuffer(
	ID3D11Device* pDevice,
	Microsoft::WRL::ComPtr <ID3D11Buffer> pSourceBuffer, // The GPU buffer to copy from
	Microsoft::WRL::ComPtr <ID3D11Buffer>* ppBuffer_out)
{
	D3D11_BUFFER_DESC sourceDesc;
	pSourceBuffer->GetDesc(&sourceDesc);

	D3D11_BUFFER_DESC stagingDesc = {};
	stagingDesc.ByteWidth = sourceDesc.ByteWidth;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.BindFlags = 0;
	stagingDesc.MiscFlags = 0;

	return pDevice->CreateBuffer(&stagingDesc, nullptr, ppBuffer_out->GetAddressOf());
}


// This buffer is used as a temporary step to get data to the GPU.
HRESULT ColliderManager::createStagingWriteBuffer(
	ID3D11Device* pDevice,
	UINT maxBoxes,
	Microsoft::WRL::ComPtr < ID3D11Buffer>* ppStagingBuffer_out)
{
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.ByteWidth = sizeof(Box) * maxBoxes; // Must match the GPU buffer size
	bufferDesc.Usage = D3D11_USAGE_STAGING;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.BindFlags = 0; // Cannot be bound to shaders
	bufferDesc.MiscFlags = 0;

	return pDevice->CreateBuffer(&bufferDesc, nullptr, ppStagingBuffer_out->GetAddressOf());
}

// Creates a read-only structured buffer for the GPU.
// pSRV_out will be bound to the shader's 't0' register.
HRESULT ColliderManager::ColliderManager::createGPUBoxBuffer(
	ID3D11Device* pDevice,
	UINT maxBoxes,
	Microsoft::WRL::ComPtr < ID3D11Buffer>* ppBuffer_out,
	Microsoft::WRL::ComPtr < ID3D11ShaderResourceView>* ppSRV_out)
{
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.ByteWidth = sizeof(Box) * maxBoxes;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = sizeof(Box); // Use the corrected 32-byte struct size

	HRESULT hr = pDevice->CreateBuffer(&bufferDesc, nullptr, ppBuffer_out->GetAddressOf());
	if (FAILED(hr)) return hr;

	// --- 2. Create the Shader Resource View (SRV) ---
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN; // Format must be UNKNOWN for structured buffers
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = m_boxes.size();

	hr = pDevice->CreateShaderResourceView(ppBuffer_out->Get(), &srvDesc, ppSRV_out->GetAddressOf());

	return hr;
}

void ColliderManager::updateBoxBuffer(
	ID3D11DeviceContext* pContext,
	const std::vector<Box>& boxes)
{

	// --- Step 1: Write new data into the staging buffer ---
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = pContext->Map(m_pStagingBoxBuffer.Get(), 0, D3D11_MAP_WRITE, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		memcpy(mappedResource.pData, boxes.data(), sizeof(Box) * boxes.size());
		pContext->Unmap(m_pStagingBoxBuffer.Get(), 0);
	}

	// --- Step 2: Command the GPU to copy the data ---
	pContext->CopyResource(m_pBoxBuffer.Get(), m_pStagingBoxBuffer.Get());

}

// Creates a writeable buffer for the compute shader to store collision results.
// pUAV_out will be bound to the shader's 'u0' register.
HRESULT ColliderManager::createCollisionOutputBuffer(
	ID3D11Device* pDevice,
	unsigned int maxCollisions,
	Microsoft::WRL::ComPtr < ID3D11Buffer>* ppBuffer_out,
	Microsoft::WRL::ComPtr < ID3D11UnorderedAccessView>* ppUAV_out)
{
	// --- 1. Create the Buffer Resource ---
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.ByteWidth = sizeof(CollisionPair) * maxCollisions;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = sizeof(CollisionPair);

	HRESULT hr = pDevice->CreateBuffer(&bufferDesc, nullptr, ppBuffer_out->GetAddressOf());
	if (FAILED(hr)) return hr;

	// --- 2. Create the Unordered Access View (UAV) ---
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN; // Format must be UNKNOWN for structured buffers
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = maxCollisions;

	hr = pDevice->CreateUnorderedAccessView(ppBuffer_out->Get(), &uavDesc, ppUAV_out->GetAddressOf());

	return hr;
}

// Creates the single-integer buffer for atomic operations.
// pUAV_out will be bound to the shader's 'u1' register.
HRESULT ColliderManager::createAtomicCounterBuffer(
	ID3D11Device* pDevice,
	Microsoft::WRL::ComPtr < ID3D11Buffer>* ppBuffer_out,
	Microsoft::WRL::ComPtr < ID3D11UnorderedAccessView>* ppUAV_out)
{
	// --- 1. Create the Buffer Resource ---
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.ByteWidth = sizeof(UINT) * 2;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;

	HRESULT hr = pDevice->CreateBuffer(&bufferDesc, nullptr, ppBuffer_out->GetAddressOf());
	if (FAILED(hr)) return hr;

	// --- 2. Create the Unordered Access View (UAV) ---
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R32_UINT; // Typed format for atomic operations
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 2;

	hr = pDevice->CreateUnorderedAccessView(ppBuffer_out->Get(), &uavDesc, ppUAV_out->GetAddressOf());

	return hr;
}

void ColliderManager::initBox()
{
	Box box;

	// Assign random x, y, and z positions within specified ranges
	box.positionAndRadius.x = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / maxX - box_offset));
	box.positionAndRadius.y = box_offset + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX));
	box.positionAndRadius.z = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / maxX - box_offset));

	box.positionAndRadius.w = box_scale;

	// Assign random x-velocity between -1.0f and 1.0f
	float randomXVelocity = -1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 2.0f));
	box.velocity = { randomXVelocity, 0.0f, 0.0f, 0.0f };

	m_boxes.push_back(box);
}

void ColliderManager::initBoxes()
{
	for (int i = 0; i < g_cube_count; ++i) {
		initBox();
	}
}


void ColliderManager::update(const float deltaTime, ID3D11Device* device, ID3D11DeviceContext* context)
{

	if (g_cube_count != m_boxes.size())
	{
		if (g_cube_count > m_boxes.size()) // add some more
		{
			for (unsigned int i = m_boxes.size(); i < g_cube_count; i++)
				initBox();
		}
		else // take some away
		{
			m_boxes.resize(g_cube_count);
			unsigned int box_count = getBoxCount();
		}

		// Re-create compute shader resources.
		// Ideally, we would handle variable box counts without full recreation,
		// but this simplified approach is acceptable for a demo.
		releaseAndCreateCSResources(device);
	}


	updateMovement(deltaTime);

	switch (g_ttype)
	{
		case use_cpu_singlethread:
			updateCollisionsCPU();
		break;
		case use_cpu_multithread:
			updateCollisionsCPUMultithreaded();
			break;
		case use_gpu:
			updateCollisionsCS(context);
			break;
	}
}

void ColliderManager::updateMovement(const float deltaTime)
{
	const float floorY = 0.0f;

	for (unsigned int i = 0; i < m_boxes.size(); i++) {

		Box& box = m_boxes[i];

		// Update velocity due to gravity
		box.velocity.y += gravity * deltaTime;

		// Update position based on velocity
		box.positionAndRadius.x += box.velocity.x * deltaTime;
		box.positionAndRadius.y += box.velocity.y * deltaTime;
		box.positionAndRadius.z += box.velocity.z * deltaTime;

		// Check for collision with the floor
		if (box.positionAndRadius.y - box.positionAndRadius.w < floorY) {
			box.positionAndRadius.y = floorY + box.positionAndRadius.w;
			float dampening = 0.7f;
			box.velocity.y = -box.velocity.y * dampening;
		}

		// Check for collision with the walls
		if (box.positionAndRadius.x - box.positionAndRadius.w < minX || box.positionAndRadius.x + box.positionAndRadius.w > maxX) {
			box.velocity.x = -box.velocity.x;
		}
		if (box.positionAndRadius.z - box.positionAndRadius.w < minZ || box.positionAndRadius.z + box.positionAndRadius.w > maxZ) {
			box.velocity.z = -box.velocity.z;
		}
	}
}



void ColliderManager::updateCollisionsCS(ID3D11DeviceContext* context)
{
	updateBoxBuffer(context, m_boxes);

	// 1. Set the Compute Shader
	context->CSSetShader(m_pComputeShader.Get(), nullptr, 0);

	// 2. Bind the Buffers to the Shader
	//    SRV for reading sphere data, UAV for writing collision pairs and the atomic counter.
	ID3D11ShaderResourceView* srv = m_pBoxBufferSRV.Get();
	context->CSSetShaderResources(0, 1, &srv);
	ID3D11UnorderedAccessView* uav = m_pCollisionPairBufferSRV.Get();
	context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
	ID3D11UnorderedAccessView* uav2 = m_pCounterBufferSRV.Get();
	context->CSSetUnorderedAccessViews(1, 1, &uav2, nullptr);

	// 2.5 Reset the counter
	const UINT clearValue[4] = { 0, 0, 0, 0 };
	context->ClearUnorderedAccessViewUint(m_pCounterBufferSRV.Get(), clearValue);

	// 3. Dispatch the Shader
	//    For 1000 boxes, we launch 1000 threads.
	//    The group size (e.g., 64) is defined in the HLSL shader.
	unsigned int num_boxes = g_cube_count;
	unsigned int threadsPerGroup = 512;
	unsigned int thread_groups = (num_boxes + threadsPerGroup - 1) / threadsPerGroup; // Calculate number of groups needed
	context->Dispatch(thread_groups, 1, 1);

	// 4. Unbind resources
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	ID3D11UnorderedAccessView* nullUAV[2] = { nullptr, nullptr };
	context->CSSetShaderResources(0, 1, nullSRV);
	context->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);
	context->CSSetUnorderedAccessViews(1, 1, nullUAV, nullptr);

	// 5. Read results back to the CPU
	//    Copy the collision pair buffer and the atomic counter buffer to staging
	//    buffers so the CPU can read the data.

	context->CopyResource(m_pStagingBufferCollisionPairs.Get(), m_pCollisionPairBuffer.Get());
	context->CopyResource(m_pStagingBufferCounter.Get(), m_pCounterBuffer.Get());

	D3D11_MAPPED_SUBRESOURCE mapped_resource;
	context->Map(m_pStagingBufferCounter.Get(), 0, D3D11_MAP_READ, 0, &mapped_resource);
	UINT* counterValues = static_cast<UINT*>(mapped_resource.pData);
	UINT collision_count = counterValues[0];
	UINT checkCount = counterValues[1];

	context->Map(m_pStagingBufferCollisionPairs.Get(), 0, D3D11_MAP_READ, 0, &mapped_resource);
	CollisionPair* collision_pairs = static_cast<CollisionPair*>(mapped_resource.pData);

	for (unsigned int i = 0; i < collision_count; i++) {
		CollisionPair cp = collision_pairs[i];
		// Process the collision between pairs[i].index1 and pairs[i].index2
		resolveCollision(m_boxes[cp.index1], m_boxes[cp.index2]);
	}

	// release the resources
	context->Unmap(m_pStagingBufferCollisionPairs.Get(), 0);
	context->Unmap(m_pStagingBufferCounter.Get(), 0);
}


void ColliderManager::updateCollisionsCPU()
{
	for (unsigned int i = 0; i < m_boxes.size(); i++) {

		Box& box = m_boxes[i];
		// Check for collisions with other boxes
		for (unsigned int j = i + 1; j < m_boxes.size(); j++) // only check with boxes later in the list, avoids double checks
		{
			Box& other = m_boxes[j];
			if (checkCollision(box, other)) {
				resolveCollision(box, other);
			}
		}
	}
}


// This is the main entry point to run the CPU collision check
void ColliderManager::updateCollisionsCPUMultithreaded()
{
	if (m_boxes.empty()) {
		return;
	}

	for (unsigned int i = 0; i < m_localCollisionResults.size(); i++) {
		m_localCollisionResults[i].clear();
	}

	const int numBoxes = m_boxes.size();
	const int numThreads = m_threadPool.threadCount();
	int workPerThread = numBoxes / numThreads;

	// Set the counter to the number of jobs we're about to create
	m_jobsRemaining = numThreads;

	int startIndex = 0;
	for (int i = 0; i < numThreads; ++i)
	{
		int endIndex = (i == numThreads - 1) ? numBoxes : startIndex + workPerThread;
		vector<CollisionPair>* resultsForThisThread = &m_localCollisionResults[i];

		// Create a lambda function for the job
		auto job = [this, startIndex, endIndex, resultsForThisThread]() {
			findCollisionsWorker(startIndex, endIndex, resultsForThisThread);
			// This thread's job is done, so decrement the counter
			m_jobsRemaining--;

			};

		// Add the job to the thread pool's queue
		m_threadPool.enqueue(job);

		startIndex = endIndex;
	}

	// Wait for all jobs to finish
	// This is a "spin-wait", which is fine here since the work is short
	while (m_jobsRemaining > 0) {
		// hint to the OS scheduler that this thread is cool with being lower priority
		std::this_thread::yield();
	}

	for (vector<CollisionPair> vecCP : m_localCollisionResults) {
		for(const CollisionPair& cp : vecCP){
			// Process the collision between pairs[i].index1 and pairs[i].index2
			resolveCollision(m_boxes[cp.index1], m_boxes[cp.index2]);
		}
	}
}

// The worker function is now a private member method
void ColliderManager::findCollisionsWorker(int startIndex, int endIndex, vector<CollisionPair>* results)
{
	int localCollisionCounter = 0;
	using namespace DirectX;

	for (int i = startIndex; i < endIndex; ++i)
	{
		XMVECTOR box1Data = XMLoadFloat4(&m_boxes[i].positionAndRadius);

		for (int j = i + 1; j < m_boxes.size(); ++j)
		{
			localCollisionCounter++;
			XMVECTOR box2Data = XMLoadFloat4(&m_boxes[j].positionAndRadius);

			XMVECTOR distVec = XMVectorSubtract(box1Data, box2Data);
			XMVECTOR distSqVec = XMVector3LengthSq(distVec);

			float radius1 = XMVectorGetW(box1Data);
			float radius2 = XMVectorGetW(box2Data);
			float sumRadii = radius1 + radius2;

			float distSq;
			XMStoreFloat(&distSq, distSqVec);

			if (distSq < sumRadii * sumRadii)
			{
				results->push_back({ (unsigned int)i, (unsigned int)j });
			}
		}
	}
}

void ColliderManager::resolveCollision(Box& a, Box& b) {
	XMFLOAT3 normal = { a.positionAndRadius.x - b.positionAndRadius.x, a.positionAndRadius.y - b.positionAndRadius.y, a.positionAndRadius.z - b.positionAndRadius.z };

	float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
	// Normalize the normal vector
	if (length > 0)
	{
		normal.x /= length;
		normal.y /= length;
		normal.z /= length;
	}

	float relativeVelocityX = a.velocity.x - b.velocity.x;
	float relativeVelocityY = a.velocity.y - b.velocity.y;
	float relativeVelocityZ = a.velocity.z - b.velocity.z;

	// Compute the relative velocity along the normal
	float impulse = relativeVelocityX * normal.x + relativeVelocityY * normal.y + relativeVelocityZ * normal.z;

	// Ignore collision if objects are moving away from each other
	if (impulse > 0) {
		return;
	}

	// Compute the collision impulse scalar
	float e = 0.01f; // Coefficient of restitution (0 = inelastic, 1 = elastic)
	float dampening = 0.9f; // Dampening factor (0.9 = 10% energy reduction)
	float j = -(1.0f + e) * impulse * dampening;

	// Apply the impulse to the boxes' velocities
	a.velocity.x += j * normal.x;
	a.velocity.y += j * normal.y;
	a.velocity.z += j * normal.z;
	b.velocity.x -= j * normal.x;
	b.velocity.y -= j * normal.y;
	b.velocity.z -= j * normal.z;
}


bool ColliderManager::checkCollision(const Box& a, const Box& b) {

	return (std::abs(a.positionAndRadius.x - b.positionAndRadius.x) < (a.positionAndRadius.w + b.positionAndRadius.w)) &&
		(std::abs(a.positionAndRadius.y - b.positionAndRadius.y) < (a.positionAndRadius.w + b.positionAndRadius.w)) &&
		(std::abs(a.positionAndRadius.z - b.positionAndRadius.z) < (a.positionAndRadius.w + b.positionAndRadius.w));
}
