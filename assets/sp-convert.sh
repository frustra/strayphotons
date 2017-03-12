#!/bin/bash

if [[ $# != 1 ]]; then
  echo "usage: $0 model.obj"
  echo "Generate model.gltf from model.obj"
  exit 1
fi

# strip .obj from path
model=${1%.*}

# obj2gltf must be a command to run https://github.com/AnalyticalGraphicsInc/obj2gltf
obj2gltf -i "${model}.obj" -o "${model}.gltf"

# run `go build gltf-process` if this doesn't exist
./gltf-process "${model}.gltf"


