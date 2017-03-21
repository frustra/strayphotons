#!/bin/bash

if [[ $# != 1 ]]; then
  echo "usage: $0 model.obj"
  echo "Generate model.gltf from model.obj"
  exit 1
fi

# strip .obj from path
model=${1%.*}

if [ ! -d "../ext/obj2gltf/node_modules" ]; then
  bash -c "cd ../ext/obj2gltf; npm install"
fi
node ../ext/obj2gltf/bin/obj2gltf.js -i "${model}.obj" -o "${model}.gltf"

# run `go build gltf-process` if this doesn't exist
./gltf-process "${model}.gltf"


