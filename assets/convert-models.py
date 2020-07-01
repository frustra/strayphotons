#!/usr/bin/env python3

from pathlib import Path
from os import system

def recursively_process_gltf(d):
    if not d.is_dir():
        raise FileNotFoundError

    print("Processing dir " + str(d.resolve()))
    
    for child in d.iterdir():
        if child.is_dir():
            recursively_process_gltf(child)
        elif child.is_file():
            if child.suffix == ".gltf":
                print("Processing gltf " + str(child.resolve()))
                system("gltf-pipeline -i " + str(child.resolve()) + " -o " + str(child.resolve()))
                glb = child.parent / str(child.stem + ".glb")
                system("gltf-pipeline -i " + str(child.resolve()) + " -b -o " + str(glb.resolve()))

def recursively_process_obj(d):
    if not d.is_dir():
        raise FileNotFoundError

    print("Processing dir " + str(d.resolve()))
    
    for child in d.iterdir():
        if child.is_dir():
            recursively_process_obj(child)
        elif child.is_file():
            # Remove
            #  / child.stem / 
            # to cause it to replace the original models            
            gltf = child.parent / str(child.stem + ".gltf")
            if child.suffix == ".obj":
                print("Processing obj " + str(child.resolve()))

                # Remove 
                # --separate
                # to put all data in the GLTF
                system("obj2gltf --secure --checkTransparency --metallicRoughness -i " + str(child.resolve()) + " -o " + str(gltf.resolve()))

p = Path.cwd()
p = p / 'models'

#recursively_process_obj(p)
recursively_process_gltf(p)
