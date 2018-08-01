Voxelization
============

Initial Pass
------------

1. Using the rasterizer, each triangle is rendered.
1. The geometry shader will rotate each triangle so it is viewed from its dominant axis to eliminate gaps.
1. In the fragment shader, each fragment is rotated back to its original position and stored:
	1. If the counter for the voxel is 0, atomically increment it and store in the voxel directly.
	1. If the counter is above 1, increment it and use the value to determine which overflow bin to use.
	1. Store the position and data for the fragment in the corresponding overflow list.

Overflow Pass
-------------

1. Using a compute shader, loop through the overflow fragment list in each bin. Each bin is guaranteed to have no overlapping fragments, so no synchronization or atomic operations are required.
1. Add the overflow data to the existing voxel data contained in the grid
1. The divide for averaging can happen at this step or as a separate pass later, TBD.
1. The size of each bin falls of quite quickly, so all bins higher than a certain (TBD) index can be grouped together to be processed sequencially in a final pass.

Lighting Feedback Pass
----------------------

(This pass can be turned off, and the rendering should operate normally)

1. For each populated voxel, sample the previous frame's voxel grid using a sphere of diffuse cones.
	1. This can potentially be done using only the mipmapped voxel grid, eliminating the need of a duplicate flip buffer.
1. Using the feedback weight, blend the light value with the current frame's light value.
1. Store the light value back to the voxel grid.

Mipmap Pass
-----------

No change for now, it's kind of black magic right now. It could likely be optimized further later.
