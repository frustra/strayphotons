package main

import (
	"bytes"
	"encoding/json"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strings"
)

func getTextureFormat(path string) int {
	cmd := exec.Command("file", path)
	var out bytes.Buffer
	cmd.Stdout = &out
	err := cmd.Run()
	if err != nil {
		log.Fatal(err)
	}

	colorMap := map[string]int{
		// PNG
		"RGBA,":     6408, // GL_RGBA
		"RGB,":      6407, // GL_RGB
		"grayscale": 6403, // GL_RED
		// BMP + TGA
		"x 32": 6408, // GL_RGBA
		"x 24": 6407, // GL_RGB
		"x 8":  6403, // GL_RED
	}
	for find, format := range colorMap {
		if strings.Contains(out.String(), find) {
			return format
		}
	}

	log.Fatalln("Unknown file format: ", out.String())
	return 0
}

func addTexture(gltf map[string]interface{}, folder, file string) {
	if _, ok := gltf["samplers"]; !ok {
		gltf["samplers"] = make(map[string]interface{})
	}
	samplers := gltf["samplers"].(map[string]interface{})
	if _, ok := samplers["sampler_0"]; !ok {
		samplers["sampler_0"] = struct{}{}
	}

	if _, ok := gltf["textures"]; !ok {
		gltf["textures"] = make(map[string]interface{})
	}
	textures := gltf["textures"].(map[string]interface{})
	if _, ok := textures["texture_"+file]; !ok {
		format := getTextureFormat(path.Join(folder, file))
		log.Printf("\tAdding texture texture_%s %d", file, format)
		internalFormat := format
		if internalFormat == 6403 {
			internalFormat = 33321
		}

		textures["texture_"+file] = struct {
			Format         int    `json:"format"`
			InternalFormat int    `json:"internalFormat"`
			Sampler        string `json:"sampler"`
			Source         string `json:"source"`
			Target         int    `json:"target"`
			Type           int    `json:"type"`
		}{
			format,
			internalFormat,
			"sampler_0",
			"image_" + file,
			3553, // GL_TEXTURE_2D
			5121, // GL_UNSIGNED_BYTE
		}
	}

	if _, ok := gltf["images"]; !ok {
		gltf["images"] = make(map[string]interface{})
	}
	images := gltf["images"].(map[string]interface{})
	if _, ok := images["image_"+file]; !ok {
		log.Printf("\tAdding image image_%s", file)

		images["image_"+file] = struct {
			Uri string `json:"uri"`
		}{file}
	}
}

func processGltf(gltf map[string]interface{}, folder string) {
	if _, ok := gltf["shaders"]; ok {
		log.Println("Stripping shaders")
		delete(gltf, "shaders")
	}

	if _, ok := gltf["programs"]; ok {
		log.Println("Stripping programs")
		delete(gltf, "programs")
	}

	if _, ok := gltf["techniques"]; ok {
		log.Println("Stripping techniques")
		delete(gltf, "techniques")
	}

	if _, ok := gltf["materials"]; ok {
		materials := gltf["materials"].(map[string]interface{})
		for key, value := range materials {
			material := value.(map[string]interface{})
			name := material["name"].(string)
			values := material["values"].(map[string]interface{})
			if _, ok := material["technique"]; ok {
				log.Printf("%s: Removing technique", key)
				delete(material, "technique")
			}

			files, err := ioutil.ReadDir(folder)
			if err != nil {
				log.Fatal(err)
			}

			textureMap := map[string]string{
				"diffuse":   "baseColor",
				"roughness": "roughness",
				"metallic":  "metallic",
				"height":    "height",
			}
			for _, file := range files {
				if !file.IsDir() && strings.HasPrefix(file.Name(), name+"_") {
					for k, v := range textureMap {
						if strings.HasPrefix(file.Name(), name+"_"+k+".") {
							if _, ok := values[v]; !ok {
								log.Printf("%s: Adding texture %s: %s", key, v, file.Name())
								values[v] = "texture_" + file.Name()
							}

							addTexture(gltf, folder, file.Name())
							break
						}
					}
				}
			}
		}
	}
}

func main() {
	if len(os.Args) < 2 {
		log.Fatalf("Missing required arguments.\n\nUsage:\n\t%s <file.gltf>\n", os.Args[0])
	}
	path, err := filepath.Abs(os.Args[1])
	if err != nil {
		log.Fatal(err)
	}

	file, err := ioutil.ReadFile(path)
	if err != nil {
		log.Fatal(err)
	}

	stat, err := os.Stat(path)
	if err != nil {
		log.Fatal(err)
	}

	var gltf map[string]interface{}
	err = json.Unmarshal(file, &gltf)
	if err != nil {
		log.Fatal(err)
	}

	processGltf(gltf, filepath.Dir(path))

	data, err := json.MarshalIndent(gltf, "", "    ")
	if err != nil {
		log.Fatal(err)
	}

	err = ioutil.WriteFile(path, data, stat.Mode())
	if err != nil {
		log.Fatal(err)
	}
}
