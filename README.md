# Collision-a-tron
A cube collider with simple (not perfect) collisions between non-rotating cubes.  

The purpose of this project is to evaluate the performance of different methods of parallelisation. 

The collisions are *purposely* NOT OPTIMISED - no octrees etc. The idea is to generate a large amount of work.   

These are calculated using one of three methods: 

1. CPU single threaded
2. CPU multi threaded 
3. GPU using Compute Shaders
