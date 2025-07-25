# Collision-a-tron
A cube collider with simple (not perfect) collisions between non-rotating cubes.  

The purpose of this project is to evaluate the performance of different methods of parallelisation. 

The collisions are *purposely* NOT OPTIMISED - no octrees etc. The idea is to generate a large amount of work.   

These are calculated using one of three methods: 

1. CPU single threaded
2. CPU multi threaded 
3. GPU using Compute Shaders

![ezgif-3e39fc661e92b6](https://github.com/user-attachments/assets/f2174e71-826d-4ffd-9fea-5952f049b22c)

**Results: **
CPU: AMD Ryzen 7 2700X
RAM: 16GB
GPU: NVidia RTX 3700 Ti

In summary, when there are fewer calculations, in this scenario, the single and multi-threaded CPU is more performant. This is possibly due to bottlenecks in firing up the compute shader. When the number of cubes > 2000, the compute shader performs increasingly better. 
<img width="647" height="176" alt="image" src="https://github.com/user-attachments/assets/7d3beb4e-5f61-49e5-83c3-6ba9f99ceb82" />

##**2 – 400 cubes**

Surprisingly, the single threaded CPU was the fastest, outperforming both other options.

##**400 – 2000 cubes**

In this region, the multi-threaded CPU was fastest, outperforming the single-threaded and GPU option. 

##**2000 – 20,000 cubes**

In this region, and presumably onwards, the GPU shines (possibly literally, given the amount of work it has). At 20,000 cubes the FPS for the single and multi-threaded CPU is 4 and 18 respectively. The GPU performs at an admirable 125 FPS. 

At 20,000 cubes the GPU still provides a passable FPS, whereas the CPU falls to less than < 10 fps. 

##**Running the code**
Download, open the sln file (Windows / Visual Studio required). Run in **release** mode. 
