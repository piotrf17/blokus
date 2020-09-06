// Raw tile data.
const RAW_TILES = [
  // 1 block tiles.
  [  // 0.
    [1, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  // 2 block tiles.
  [ // 1.
    [1, 1, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  // 3 block tiles.
  [ // 2.
    [1, 1, 0, 0, 0],
    [0, 1, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 3.
    [1, 1, 1, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  // 4 block tiles.
  [ // 4.
    [1, 1, 0, 0, 0],
    [1, 1, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 5.
    [0, 1, 0, 0, 0],
    [1, 1, 1, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 6.
    [1, 1, 1, 1, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 7.
    [1, 1, 1, 0, 0],
    [0, 0, 1, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 8.
    [1, 1, 0, 0, 0],
    [0, 1, 1, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  // 5 block tiles.
  [ // 9.
    [1, 0, 0, 0, 0],
    [1, 1, 1, 1, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 10.
    [1, 0, 0, 0, 0],
    [1, 1, 1, 0, 0],
    [1, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 11.
    [1, 0, 0, 0, 0],
    [1, 0, 0, 0, 0],
    [1, 1, 1, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 12.
    [1, 1, 0, 0, 0],
    [0, 1, 1, 1, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 13.
    [1, 0, 0, 0, 0],
    [1, 1, 1, 0, 0],
    [0, 0, 1, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 14.
    [1, 1, 1, 1, 1],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 15.
    [1, 1, 1, 0, 0],
    [0, 1, 1, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 16.
    [1, 0, 0, 0, 0],
    [1, 1, 0, 0, 0],
    [0, 1, 1, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 17.
    [1, 1, 0, 0, 0],
    [1, 0, 0, 0, 0],
    [1, 1, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 18.
    [1, 1, 0, 0, 0],
    [0, 1, 1, 0, 0],
    [0, 1, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 19.
    [0, 1, 0, 0, 0],
    [1, 1, 1, 0, 0],
    [0, 1, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
  [ // 20.
    [1, 1, 1, 1, 0],
    [0, 1, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0],
  ],
];

// Container for tiles, filled in via buildTiles(), made up of Tile objects.
const TILES = []

class Tile {
  constructor(data) {
    this.data = data
    // Compute the height and width of the final tile, and also assemble
    // a list of the coordinates for quicker lookup later.
    this.numRows = 0;
    this.numCols = 0;
    this.coors = []
    for (var r = 0; r < 5; ++r) {
      for (var c = 0; c < 5; ++c) {
	if (data[r][c]) {
	  this.numRows = Math.max(this.numRows, r);
	  this.numCols = Math.max(this.numCols, c);
	  this.coors.push([r, c]);
	}
      }
    }
    this.numRows += 1;
    this.numCols += 1;
  }

  placed(rotation, flip) {
    var coors = [];
    for (var i = 0; i < this.coors.length; ++i) {
      var tX, tY;
      switch(rotation) {
        case 0:
          tX = this.coors[i][0];
          tY = this.coors[i][1];
          break;
        case 1:
          tX = -this.coors[i][1];
          tY = this.coors[i][0];
          break;
        case 2:
          tX = -this.coors[i][0];
          tY = -this.coors[i][1];
          break;
        case 3:
          tX = this.coors[i][1];
          tY = -this.coors[i][0];
          break;
      }
      if (flip) {
        tX = -tX;
      }
      coors.push([tX, tY]);
    }
    return coors;
  }

  draw(ctx, x, y, rotation, flip, color, scale, border=-1) {
    ctx.fillStyle = color;
    const coors = this.placed(rotation, flip);
    for (var i = 0; i < coors.length; ++i) {
      ctx.fillRect(scale * coors[i][1] + x - border,
		   scale * coors[i][0] + y - border,
    		   scale + 2 * border, scale + 2 * border);
    }
  }
}

function buildTiles() {
  for (var t = 0; t < RAW_TILES.length; ++t) {
    TILES.push(new Tile(RAW_TILES[t]));
  }
}

export {buildTiles, TILES};
