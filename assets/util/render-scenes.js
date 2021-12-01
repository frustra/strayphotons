ejs = require('./ejs/ejs.js')
fs = require('fs')

var jsonlint = require('./jsonlint/jsonlint.js')
var jsonFormatter = require('./jsonlint/formatter.js').formatter;

JSON._parse = JSON.parse;
JSON.parse = function (json, name) {
  try {
    return JSON._parse(json)
  } catch(e) {
    var formatted = jsonFormatter.formatJson(json).replace(/\r/g, "");
    errFile = 'jsonlint_err.json';
    fs.writeFileSync('./' + errFile, formatted);

    // Re-parse so exception output gets better line numbers
    try {
    	jsonlint.parse(formatted);
    } catch(formattedError) {
    	formattedError.message = "Error parsing: " + name + "\n" + formattedError.message + '\nInvalid json saved as ' + errFile;
    	throw formattedError;
    }
  }
}

var files = fs.readdirSync('../scenes');

global.offset = function(vec, d) {
	return vec.map(function(v, i) { return v + d[i]; });
}

/**
 * Create entities that fill the space between two 3D coordinates
 * model: string name of model
 * corner0: [x, y, z]
 * corner1: [x, y, z]
 * xyzStep: [x, y, z] to indicate the spacing of entities from corner0
 *          to corner1
 *
 * This will create at least one entity in each dimension to allow
 * for creation of straight lines (ex: [0, 0, 0] to [0, 0, 6] or
 * planes of entities (ex: [0, 0, 0] to [9, 0, 9] instead of
 * just rectangular prisms of entities (ex: [0, 0, 0] to [3, 3, 3]).
 *
 * Returns a list of objects that correspond to entities.
 */
global.createTiles = function(model, corner0, corner1, xyzStep) {
	x0 = corner0[0];
	y0 = corner0[1];
	z0 = corner0[2];

	x1 = corner1[0];
	y1 = corner1[1];
	z1 = corner1[2];

	dx = x1 - x0;
	dy = y1 - y0;
	dz = z1 - z0;

	nx = 1;
	if (dx != 0 && xyzStep[0] != 0) {
		nx = dx / xyzStep[0];
	}
	ny = 1;
	if (dy != 0 && xyzStep[1] != 0) {
		ny = dy / xyzStep[1];
	}
	nz = 1;
	if (dz != 0 && xyzStep[2] != 0) {
		nz = dz / xyzStep[2];
	}

	tiles = []
	for (xi = 0, x = x0; xi < nx; xi += 1, x += xyzStep[0]) {
		for (yi = 0, y = y0; yi < ny; yi += 1, y += xyzStep[1]) {
			for (zi = 0, z = z0; zi < nz; zi += 1, z += xyzStep[2]) {
				tiles.push({
					renderable: model,
					transform: {
						translate: [x, y, z]
					},
					physics: {
						model: model,
						dynamic: false
					}
				});
			}
		}
	}

	return tiles;
}

files.forEach(function(f) {
  if (f.endsWith('.scene')) {
    var name = f.slice(0, -6);

    var data = {
      scene_name: name
    };

    var opts = {
      escape: function(val) {
        return JSON.stringify(val);
      }
    };

    ejs.renderFile('../scenes/' + f, data, opts, function(err, str) {
      if (err)
        return console.log('Error rendering', name, err);

      var obj = JSON.parse(str, name);
      str = JSON.stringify(obj, null, '\t');
      str = '{\t"__comment": "autogenerated scene: ' + name + '",' + str.slice(1);

      fs.writeFile('../scenes/' + name + '.json', str, function(err) {
        if (err)
          return console.log('Error writing', name, err);

        console.log('Wrote', name);
      });
    });
  }
});
