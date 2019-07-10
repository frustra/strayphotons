Voxelization
============

Buffer Overview
---------------

Voxel Grid: NxNxN rgba16f buffer, used to store the radiance output of each voxel
Voxel Grid Mipmap: NxNxN rgba16f buffer, log2(N) layers, used to store the mipmaped radiance output of a group of voxels.
Fragment List: 2x rgb10_a2ui buffers, log2(N) layers, stores a position list of populated voxels, these lists alternate with eachother each frame.
Voxel Overflow: rgba16f buffer, log2(N) layers, stores a list of fragment info { position.xyz, fragment_radiance.rgb }
Indirect Buffer: 2x 4*log2(N) uint32 buffer, used to store the number of fragements at each level, and provides the indirect buffer for `glDispatchComputeIndirect`
Voxel Counters: NxNxN uint32 buffer, log2(N) layers, used as a generic per-voxel counter that can be accessed with atomic image operations.


Initial Pass (Fill Pass)
------------------------

The voxel counter buffer should be zeros at this point.
Swap the fragment list and indirect buffers from the previous frame.
Clear all fragment list sizes for the current frame.

1. Using the rasterizer, each triangle is rendered.
1. The geometry shader will rotate each triangle so it is viewed from its dominant axis to eliminate gaps.
1. In the fragment shader, each fragment is rotated back to its original position and stored:
	1. If the counter for the voxel is 0, atomically increment it and store the voxel directly.
	1. If the counter is 1 or more, increment it and use the value to determine which overflow bucket to use.
	1. Store the position and data for the fragment in the corresponding overflow list.
1. Increment the size and indirect compute counters for the list the current fragment was written to (grid or overflow list)

Overflow Pass (Merge Pass)
--------------------------

1. Using a compute shader, loop through the fragment list in each overflow bucket. Each bucket is guaranteed to have no overlapping fragments, so no synchronization or atomic operations are required.
1. Add the overflow fragment to the existing voxel data contained in the grid:
	1. Read the existing voxel data and its current weighting (number of fragments)
	1. Average the voxel with the new fragment data using the updated weight and store it back in the grid.
1. The size of each bucket falls of quite quickly, so once a bucket's size is small enough the remaining fragments can be grouped together and processed sequencially in a final pass.

Mipmap + Clear Pass
-------------------

For each mipmap level, starting with the un-mipmapped grid:

Clear:

1. For each fragment in the previous frame's list:
	1. If the voxel counter is zero, the voxel is no longer populated, so set its radiance to zero.

Mipmap:

1. For each fragment in the current frame's list:
	1. Clear the voxel counter at the current level.
	1. If level > 0: Read the 8 voxels at the level below and store the mipmapped voxel at the current level.
	1. Atomically increment the voxel counter at the next level.
	1. If the counter is 0, add the current fragment to the next mipmap's fragment list.
	1. Increment the size and indirect compute counters for the next mipmap's fragment list.


Lighting Feedback Pass (Outdated, to be updated)
------------------------------------------------

(This pass can be turned off, and the rendering should operate normally)

1. For each populated voxel, sample the previous frame's voxel grid using a sphere of diffuse cones.
	1. This can potentially be done using only the mipmapped voxel grid, eliminating the need of a duplicate flip buffer.
1. Using the feedback weight, blend the light value with the current frame's light value.
1. Store the light value back to the voxel grid.
