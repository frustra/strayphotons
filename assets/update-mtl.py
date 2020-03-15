from pathlib import Path
from os import system

def find_material_textures(mtl, material_name):
    mtl_folder = mtl.parent

    output_map = {}

    texture_map = {
        "_Base_Color" : "map_Kd",
        "_Height"     : "map_Bump",
        "_Roughness"  : "map_Ns",
        "_Metallic"   : "map_Ks",
        "_Occlusion"  : "map_Ka",
        "_Emissive"   : "map_Ke"
    }

    known_image_extensions = [".png", ".bmp", ".tga", ".jpeg", ".jpg"]

    if not mtl_folder.is_dir():
        raise FileNotFoundError(mtl_folder.resolve())

    for tex_file in mtl_folder.iterdir():
        if str(tex_file.name).startswith(material_name):
            tex_file_type = str(tex_file.stem)[len(material_name):]
            if tex_file_type in texture_map:
                if str(tex_file.suffix) in known_image_extensions:
                    output_map[texture_map[tex_file_type]] = str(tex_file.name)
                else:
                   raise Exception("Unexpected extension " + str(tex_file.resolve()))
            else:
                raise Exception("Unexpected potential texture match " + str(tex_file.resolve()))

    return output_map

# Processes mtl contents and returns a list of material names defined in the MTL file
def get_mtl_materials(mtl_contents):
    materials = []
    newmtl = "newmtl "

    material_count = 0

    for line in mtl_contents.splitlines():
        if line.startswith(newmtl):
            material_count = material_count + 1
            materials.append(line[len(newmtl):])

    return materials

def patch_material(mtl_contents, material_name, material_textures):
    mat_name_line = "newmtl " + material_name

    new_mtl_contents = ""

    for line in mtl_contents.splitlines():
        new_mtl_contents = new_mtl_contents + line + "\n"
        if line.startswith(mat_name_line):
            for tex in material_textures:
                new_mtl_contents = new_mtl_contents + str(tex) + " " + str(material_textures[tex]) + "\n"

    return new_mtl_contents

def process_mtl(mtl):
    mtl_file = open(mtl.resolve(), "r")

    print("Processing MTL " + str(mtl.resolve()))

    mtl_contents = mtl_file.read()

    materials = get_mtl_materials(mtl_contents)

    if len(materials) > 0:
        print(materials)
        for material in materials:
            # Find the images that match this material name
            material_textures = find_material_textures(mtl, material)
            # Patch the material with the correct textures for various elements
            print(material_textures)

            mtl_contents = patch_material(mtl_contents, material, material_textures)
    else:
        print("ERROR: MTL contains no materials: " + str(mtl.resolve()))

    print(mtl_contents)

    mtl_file.close()

    mtl_file = open(mtl.resolve(), "w")
    mtl_file.write(mtl_contents)

def recursively_process_mtl(d):
    if not d.is_dir():
        raise FileNotFoundError(d.resolve())

    #print("Processing dir " + str(d.resolve()))
    
    for child in d.iterdir():
        if child.is_dir():
            recursively_process_mtl(child)
        elif child.is_file():
            if child.suffix == ".mtl":
                process_mtl(child)

p = Path.cwd()
p = p / 'models'

recursively_process_mtl(p)