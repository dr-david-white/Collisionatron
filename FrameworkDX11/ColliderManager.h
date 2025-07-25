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

// A simple collider manager which does simple (not perfect) collisions between non-rotating cubes
// The collisions are *purposly* NOT OPTMISED - no octtress etc. The idea is to generate a large amount of work
// These are calculated using one of three methods:
// CPU single threaded
// CPU multi threaded
// GPU using Compute Shaders


#pragma once

#include <d3d11_1.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <vector>
#include <thread>
#include <mutex>
#include "ThreadPool.h" // Include your new thread pool
#include <atomic>
#include <wrl.h>

class DX11App;

using namespace DirectX;
using namespace std;

constexpr float minX = -10.0f;
constexpr float maxX = 30.0f;
constexpr float minZ = -30.0f;
constexpr float maxZ = 30.0f;
constexpr float box_offset = 10.0f;

constexpr float box_scale = 0.25;

constexpr float gravity = -9.8f;

struct  Box {
    XMFLOAT4 positionAndRadius; // this might seem odd, but this method is explicit in the packing for hlsl
    XMFLOAT4 velocity; // this only needs to be 3, but to avoid HLSL packing errors 4 is again explicit
};

struct CollisionPair {
    unsigned int index1;
    unsigned int index2;
};



class ColliderManager
{
public:

    ColliderManager();
    ~ColliderManager() {
    }

    // Explicitly delete copy constructor and copy assignment - there should only be one version of this
    ColliderManager(const ColliderManager&) = delete;
    ColliderManager& operator=(const ColliderManager&) = delete;


    void init(ID3D11Device* device, ID3D11DeviceContext* context);
    void update(const float deltaTime, ID3D11Device* device, ID3D11DeviceContext* context);
    Box* getBox(const unsigned int boxIndex) 
    { 
        if (boxIndex < m_boxes.size() && boxIndex >= 0)
            return &m_boxes[boxIndex];
        else
            return nullptr;
    } 

    unsigned int getBoxCount() { return m_boxes.size(); }

private: // methods

    void updateMovement(const float deltaTime);
    
    void updateCollisionsCPU();
    void updateCollisionsCPUMultithreaded();
    void updateCollisionsCS(ID3D11DeviceContext* context);

    void initBox();
    void initBoxes();
    void resolveCollision(Box& a, Box& b);
    bool checkCollision(const Box& a, const Box& b);


    HRESULT createStagingReadBuffer(
        ID3D11Device* pDevice,
        Microsoft::WRL::ComPtr < ID3D11Buffer> pSourceBuffer,
        Microsoft::WRL::ComPtr < ID3D11Buffer>* ppBuffer_out);

    HRESULT createStagingWriteBuffer(
        ID3D11Device* pDevice,
        UINT maxBoxes,
        Microsoft::WRL::ComPtr < ID3D11Buffer>* ppStagingBuffer_out);

    // the input buffer to the compute shader
    HRESULT createGPUBoxBuffer(
        ID3D11Device* pDevice,
        UINT maxBoxes,
        Microsoft::WRL::ComPtr < ID3D11Buffer>* ppBuffer_out,
        Microsoft::WRL::ComPtr < ID3D11ShaderResourceView>* ppSRV_out);

    // update the input buffer
    void updateBoxBuffer(
        ID3D11DeviceContext* pContext,
        const std::vector<Box>& boxes);

    // create the output buffer the compute shader will write to
    HRESULT createCollisionOutputBuffer(
        ID3D11Device* pDevice,
        unsigned int maxCollisions,
        Microsoft::WRL::ComPtr < ID3D11Buffer>* ppBuffer_out,
        Microsoft::WRL::ComPtr < ID3D11UnorderedAccessView>* ppUAV_out);

    // create a counter buffer the compute shader will write to
    HRESULT createAtomicCounterBuffer(
        ID3D11Device* pDevice,
        Microsoft::WRL::ComPtr < ID3D11Buffer>* ppBuffer_out,
        Microsoft::WRL::ComPtr < ID3D11UnorderedAccessView>* ppUAV_out);

    // Worker function for each thread
    void findCollisionsWorker(int startIndex, int endIndex, vector<CollisionPair>* results);

    void releaseAndCreateCSResources(ID3D11Device* device);

private: // variables

    ThreadPool          m_threadPool;
    std::atomic<int>    m_jobsRemaining; // For synchronizing
    vector<Box>         m_boxes;

    vector<CollisionPair>           m_collisionResults;
    vector<vector<CollisionPair>>   m_localCollisionResults;
    
    Microsoft::WRL::ComPtr <ID3D11ComputeShader> m_pComputeShader = nullptr; // the compute shader (CS)

    Microsoft::WRL::ComPtr <ID3D11Buffer> m_pBoxBuffer = nullptr; // buffer box info will be passed into the CS
    Microsoft::WRL::ComPtr <ID3D11Buffer> m_pStagingBoxBuffer = nullptr; // a staging buffer to write frame by frame box data to (passed to the box gpu buffer)
    Microsoft::WRL::ComPtr <ID3D11ShaderResourceView> m_pBoxBufferSRV = nullptr; // shader RV for the buffer

    Microsoft::WRL::ComPtr <ID3D11Buffer> m_pCollisionPairBuffer = nullptr; // Collision pairs written to by the CS
    Microsoft::WRL::ComPtr <ID3D11UnorderedAccessView> m_pCollisionPairBufferSRV = nullptr; // shader RV for the Collision pairs

    Microsoft::WRL::ComPtr <ID3D11Buffer> m_pCounterBuffer = nullptr; // Counter written to by the CS
    Microsoft::WRL::ComPtr <ID3D11UnorderedAccessView> m_pCounterBufferSRV = nullptr; // shader RV for the Counter

    Microsoft::WRL::ComPtr <ID3D11Buffer> m_pStagingBufferCollisionPairs = nullptr; // A staging buffer the CPU can read from 
    Microsoft::WRL::ComPtr <ID3D11Buffer> m_pStagingBufferCounter = nullptr; // A staging buffer the CPU can read from 
    
};

