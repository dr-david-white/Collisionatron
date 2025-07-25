# Collision-a-tron
A cube collider with simple (not perfect) collisions between non-rotating cubes.  

The purpose of this project is to evaluate the performance of different methods of parallelisation. 

The collisions are *purposely* NOT OPTIMISED - no octrees etc. The idea is to generate a large amount of work.   

These are calculated using one of three methods: 

1. CPU single threaded
2. CPU multi threaded 
3. GPU using Compute Shaders

![ezgif-3e39fc661e92b6](https://github.com/user-attachments/assets/f2174e71-826d-4ffd-9fea-5952f049b22c)
