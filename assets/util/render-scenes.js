ejs = require('./ejs/ejs.js')
fs = require('fs')

var files = fs.readdirSync('../scenes');

global.offset = function(vec, d) {
	return vec.map(function(v, i) { return v + d[i]; });
}

for (var f of files) {
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

      fs.writeFile('../scenes/' + name + '.json', str, function(err) {
        if (err)
          return console.log('Error writing', name, err);

        console.log('Wrote', name);
      });
    });
  }
}
