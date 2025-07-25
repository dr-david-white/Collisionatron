# Collision-a-tron
A simple collider manager which does simple (not perfect) collisions between non-rotating cubes.  

The collisions are *purposely* NOT OPTIMISED - no octrees etc. The idea is to generate a large amount of work.   

These are calculated using one of three methods: 

1. CPU single threaded
2. CPU multi threaded 
3. GPU using Compute Shaders
