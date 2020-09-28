Entity Transform Component lifecycle:

1. Entity creation from JSON
2. Read player position and init PhysX controller
3. Create flashlight entity with player parent
4. PhysXManager frame:
	For each dirty entity:
		ClearDirty()
		Get global position, rotation, scale
		Replace PxShape if scale changed
		Set PhysX actor pose
	For each dynamic entity:
		Set position/rotation
		ClearDirty()
5. GameLogic frame:
	For each trigger area / triggerable pair:
		If triggerable in area, queue console command
6. HumanControlSystem frame:
	Calculates / stores player velocity on controller
	Moves player PhysX actor
	Updates player entity with PhysX position
6. GraphicsManager frame:
	For each player view:
		Update view with current transform
		For each light w/ view:
			Update view with current transform
			ForwardPass()
		Read light positions to shader // mirrors
		ForwardPass() // voxel grid
		Read light positions to shader // light sensors
		ForwardPass() // main pass
		Read light positions to shader // post process

ForwardPass():
	For each renderable w/ transform:
		Get global position