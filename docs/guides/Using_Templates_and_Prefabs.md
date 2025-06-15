When building a scene, it's common for multiple entities to be combined together to form a larger object.
Reusing these objects can then become tedious if they're copied to multiple places.

To make defining groups of entities easier, Stray Photons provides a number of prefab scripts.
Prefab scripts run during scene load and can generate new entities automatically based on their parameters.
Prefabs scripts can load in templates, expand GLTF models into individual entities, or perform custom actions such as generating walls along a path.

Here are some of the included prefab scripts and their usage:

## `gltf` Prefab



## `template` Prefab

## `tile` Prefab



-----------



## Using Prefabs in a Scene

When building a scene, it's common for multiple entities to be combined together to form a larger object.
Reusing these objects can then become tedious if they're copied to multiple places.

To make defining groups of entities easier, Stray Photons provides a number of prefab scripts.
Prefab scripts run during scene load and can generate new entities automatically based on their parameters.
Prefabs scripts can load in templates, expand GLTF models into individual entities, or perform custom actions such as generating walls along a path.


### Template Prefabs

As an example, we can look at the `template` prefab script:
```json
"script": {
    "name": "prefab_template",
    "parameters": {
        "source": "interactive"
    }
}
```
This prefab takes a single argument "source", which is the name of the json template to load from the `assets/scenes/templates/` directory.

In this case, the `interactive` template defines all the components required to make a Dynamic physics actor interactable with the player.

Our cardboard box from earlier can be updated to replace the interactive_object script with the equivelent template like so:
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
    "script": {
        "name": "prefab_template",
        "parameters": {
            "source": "interactive"
        }
    }
}
```
