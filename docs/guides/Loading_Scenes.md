## Scene Overview

In Stray Photons scenes are large collections of entities that make up the world. A single scene might represent a level, a room, or just an object that can be loaded individually such as the `player` scene.
Scenes can be loaded in the background and pop in and out atomically, allowing for no loading screens and an immersive experience.

### Loading Scenes

A small list of scenes is available in the in-game Pause Menu. More scenes can be accessed by loading through the console.

Scenes are defined in `assets/scenes/*.json`, where there are many test scenes and examples available.
You can load any scene from the console using the following command:

```
loadscene <scene_name>
```

Examples:
```
loadscene sponza
loadscene life
loadscene test1
loadscene menu
```

### Scene Connections

Scenes can define connection points to request another scene be loaded. When scenes are loaded, they will be moved in place so that any matching scene connection points line up.
While scenes loaded via the console stay loaded forever, scenes requested by a connection point will be automatically unloaded once there are not more active requests.
Scene connection requests can be conditional based on trigger zones, game logic (such as doors being open), or any other signalling via an expression.

The default `menu` scene is connected to a space station demo world via scene connections. Breaking down this station layout as an example:
- `menu` defines a scene connection to `station-center` at the top of the elevator shaft.  
  `station-center` is requested if either of the elevator doors are open, or the player is near the doors.
- `menu` also defines a scene connection to `game_state`, which is always requested.
- The `station-center` scene then defines another connection point at the bottom of the elevator shaft to the `blackhole1` scene.  
- This continues in a chain of scene connections that allows for a full game world to be created without any loading screens.

The space station layout can be represented visually like this:
```
         game_state (referenced by all)             menu - station-center
                                                                 |
   ... - blackhole5 - blackhole6 - blackhole7 - blackhole8 - blackhole1 - blackhole2 - blackhole3 - blackhole4 - blackhole5 - ...
```
Notice how loops can be created resulting in potentially non-euclidean worlds.

You can "fast-travel" to a point in the station by running the `loadscene blackhole2` command for example.

### Scene Overlapping and Overrides

Multiple scenes can also be loaded at once with overlapping entity definitions. This allows entities to be overwritten and have state shared between scenes.
And example of this can be seen in the `test1` + `test1-override` demo scenes:

```
loadscene test1
# Add/Remove test1-override scene without unloading other active scenes
addscene test1-override
removescene test1-override
```

When scenes are loaded, they are placed at a matching scene connection in one of the already loaded scenes, or at the world origin if no connection points are found.
