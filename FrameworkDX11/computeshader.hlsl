
struct Box
{
    float4 positionAndRadius;
    float4 velocity;
};



// A read-only structured buffer for the box data
StructuredBuffer<Box> Boxes : register(t0);

// A writeable buffer for the collision results
RWStructuredBuffer<uint2> CollisionPairs : register(u0);

// The atomic counter for thread-safe writing
RWBuffer<uint> AtomicCounter : register(u1);

[numthreads(512, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // Get the global index of this thread
    uint i = dispatchThreadID.x;

    // Get the total number of boxes to avoid out-of-bounds access
    uint numBoxes = 0;
    uint stride;
    Boxes.GetDimensions(numBoxes, stride);

    // Ensure this thread is within the bounds of the box array
    // Explanation: if we have 1024 threads, but only 1000 boxes, if our thread id is >= 1000, exit
    if (i >= numBoxes)
    {
        return;
    }

    // Loop through all subsequent boxes to form unique pairs
    for (uint j = i + 1; j < numBoxes; j++)
    {
        // Get data for box i and j
        float3 pos_i = Boxes[i].positionAndRadius.xyz;
        float radius_i = Boxes[i].positionAndRadius.w;
        float3 pos_j = Boxes[j].positionAndRadius.xyz;
        float radius_j = Boxes[j].positionAndRadius.w;

        // Perform the box-box collision test
        float distSq = dot(pos_i - pos_j, pos_i - pos_j);
        float sumRadii = radius_i + radius_j;

        InterlockedAdd(AtomicCounter[1], 1);
        
        if (distSq < (sumRadii * sumRadii))
        {
            // If they collide, atomically increment the counter to get a write index
            uint write_index;
            InterlockedAdd(AtomicCounter[0], 1, write_index);

            // Write the indices of the colliding pair to the results buffer
            CollisionPairs[write_index] = uint2(i, j);
            break; // only record 1 collision per box
        }
    }
}