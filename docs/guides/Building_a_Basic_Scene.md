In this guide, we will build a `hello_world` scene that can be loaded, containing a room with some interactive physics objects.

### Creating a New Scene

Start by creating a new empty scene file:
`assets/scenes/hello_world.json`
```json
{
    "entities": []
}
```

This can then be loaded into Stray Photons using the console command:
```
loadscene hello_world
```

Since our scene is currently empty, there will be a black screen, and the player will begin falling from wherever they were since there is no spawn point.

We can fix both of these by adding some entities to the scene. For our example, we'll add a spawn point, a physics platform to stand on, and a light so we can see.

1. Entities are defined by combining a set of components together. For the spawn point, we only need a `name` and a `transform`:
   ```json
   {
       "entities": [
          {
              "name": "global:spawn",
              "transform": {
                  "translate": [0, 0, 0]
              }
          }
       ]
   }
   ```

   When the player is respawned, the position of the special `global:spawn` entity is used. In this case we've set the spawn point to the origin at `[0, 0, 0]`.

2. Next to prevent the player falling, we will add a platform entity below their feet.
   This entity needs `transform`, and `physics` components, as well as a `renderable` component if we want to be able to see it:
   ```json
   {
       "name": "platform",
       "transform": {
           "scale": [10, 1, 10]
       },
       "renderable": {
           "model": "box"
       },
       "physics": {
           "shapes": {
               "model": "box"
           },
           "type": "Static"
       }
   }
   ```

   This definition loads the `box` GLTF model from the assets folder, which is a 1 meter cube centered around the origin (corners from -0.5 to 0.5).
   It is then stretched and moved to provide a fixed 10x10m platform with the top surface a `Y = 0.0`.

   Since this entity's name does not specify a scene, the current scope will be used. In this case the full entity name will be `hello_world:platform`.
   If no `name` is specified, an auto-generated name will be used instead, since all entities must have names.

3. Finally, we can add a light to the scene so that we can see the platform without a flashlight:

   ```json
   {
       "transform": {
           "rotate": [90, -1, 0, 0],
           "translate": [0, 10, 0]
       },
       "light": {
           "intensity": 100,
           "spot_angle": 35,
           "tint": [1, 0.5, 0.5]
       }
   }
   ```

   By default, lights point "forward" in the direction `[0, 0, -1]`. To point the light down at the platform, we need to rotate it around the `[1, 0, 0]` axis by 90 degrees, as shown above.

   The `light` component then defines a spotlight with a 35 degree cone angle (70 degree FOV), a brightness of 100, and a warm white color.

   This entity does not specify a `name`, so and auto-generated one is used. In this case it will be called `hello_world:entity1` if it is the first unnamed entity in the scene.

Once these 3 entities have been added to the scene, we can reload it with the F6 hotkey, or the `reloadscene` and `respawn` commands.

The player should spawn on a platform and be able to walk around on it.

![hello_world](https://github.com/frustra/strayphotons/assets/1594638/0b2137d3-68b3-4775-a331-5488bf95f84c)

### Adding Interactive Objects

Now for a slightly more complex example, let's add a cardboard box that can be picked up by the player:
```json
{
    "name": "interactive-box",
    "transform": {
        "rotate": [45, 0, -1, 0],
        "translate": [1, 0, -3]
    },
    "renderable": {
        "model": "cardboard-box"
    },
    "physics": {
        "mass": 1,
        "shapes": {
            "model": "cardboard-box"
        }
    },
    "physics_joints": {},
    "physics_query": {},
    "event_input": {},
    "script": {
        "onTick": "interactive_object"
    }
}
```

The default type for the `physics` component is "Dynamic", which means it will be affected by gravity and can be pushed around.
The weight of physics objects can be defined in 2 ways: `density`, or `mass`. The default `density` is 1000 kg/m^3, which is the density of water.
This is much too heavy for our cardboard box, and won't allow the player to lift it, so instead we can set the `mass` explicitly to 1 kg.

The cardboard box makes use of the `interactive_object` script, that receives and handles grab events from the player.
The `physics_joints`, `physics_query`, and `event_input` components are all required by the `interactive_object` script.

When the object receives a `/interact/grab` event, a physics joint is created between the player and the object, allowing it to be picked up and moved.

![hello_world2](https://github.com/frustra/strayphotons/assets/1594638/0aed357e-dec9-4f8c-92f3-0ee92ab39a58)

### Attaching Entities Together

Entities can be attached in a couple ways:

1. By setting a parent entity in the `transform` component.
   This will set the entity's transform relative to the parent's transform, allowing the creation of a transform tree.

   We can add another shape to the cardboard box using this method as follows:

   ```json
   {
       "transform": {
           "parent": "interactive-box",
           "translate": [-0.35, 0.925, 0],
           "scale": 0.1
       },
       "renderable": {
           "model": "box",
           "color_override": [1, 0, 0, 1]
       },
       "physics": {
           "type": "SubActor",
           "shapes": {
               "model": "box"
           }
       }
   }
   ```

   This scales down the earlier `box` model, and places it on top of the cardboard box using relative positioning.
   The new box is colored red using the `color_override` property of `renderable` so we can tell it apart.

   The `SubActor` physics type adds any physics shapes defined on this entity to the parent physics actor based on the `transform` parent.
   In this case the parent physics actor is the `hello_world:interactive-box` entity.

   With this setup, the red cube will act as if it's part of the cardboard box, behaving as a single actor.

   ![hello_world3](https://github.com/frustra/strayphotons/assets/1594638/56949246-78d4-4fba-bc2b-aa6ea1e71660)

2. By joining multiple `physics` objects together using `physics_joints`.
   This can be useful if objects need to be attached and removed dynamically, or more complex joints are needed, such as hinges or sliders.

   First, lets attach a second cube to our cardboard box. This time we'll make it blue, and we'll leave the physics type as "Dynamic":

   ```json
   {
       "transform": {
           "parent": "interactive-box",
           "translate": [0.35, 0.925, 0],
           "scale": 0.1
       },
       "renderable": {
           "model": "box",
           "color_override": [0, 0, 1, 1]
       },
       "physics": {,
           "type": "Dynamic"
           "shapes": {
               "model": "box"
           }
       }
   }
   ```

   This spawns the blue cube on top of the cardboard box, but it will fall off as soon as the box is moved, since it isn't a `SubActor`.
   The `transform` parent in this case is only used during spawn, as the `Dynamic` physics takes over the position after the first frame.

   To attach this separate entity, we need to add a `physics_joints` component to our entity:

   ```json
   "physics_joints": {
       "type": "Fixed",
       "target": "interactive-box",
       "remote_offset": {
           "translate": [0.35, 0.925, 0]
       }
   }
   ```

   This `Fixed` joint type acts like a "weld", rigidly attaching it to the target. In this case the behavior is very similar to the red `SubActor` cube, but this time with a unique physics actor.

   To demonstrate that this is in-fact a separate physics object, we can change the joint type to `Slider` and add some limits on how far it can move:
   ```json
   "physics_joints": {
       "type": "Slider",
       "limit": [-0.1, 0.6],
       "remote_offset": {
           "translate": [0.35, 0.925, 0]
       },
       "target": "interactive-box"
   }
   ```

   The blue cube should now be able to slide back and forth along the X axis of the cardboard box (the tape line), and stop when it hits the red cube.

   ![hello_world4](https://github.com/frustra/strayphotons/assets/1594638/09828843-110a-4963-b070-e0e734d7cb11)

   Note how the `hello_world:interactive-box` entity can be picked up via the red cube, but not the blue cube, since they are different physics actors.

**See [Component Definitions](../generated/General_Components.md) for more information on component fields.**
