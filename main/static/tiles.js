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
    [1, 0, 0, 0, 0],
    [1, 1, 0, 0, 0],
    [1, 0, 0, 0, 0],
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
    this.width = 0;
    this.height = 0;
    this.coors = []
    for (var i = 0; i < 5; ++i) {
      for (var j = 0; j < 5; ++j) {
	if (data[i][j]) {
	  this.width = Math.max(this.width, i);
	  this.height = Math.max(this.height, j);
	  this.coors.push([i, j]);
	}
      }
    }
    this.height += 1;
    this.width += 1;
  }

  draw(ctx, x, y, rotation, flip, color, scale, border=-1) {
    ctx.fillStyle = color;
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
      ctx.fillRect(scale * tX + x - border, scale * tY + y - border,
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
