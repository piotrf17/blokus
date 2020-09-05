const WIDTH = 20;
const HEIGHT = 20;

// Enum for player values.
const BLUE = 1;
const YELLOW = 2;
const RED = 4;
const GREEN = 8;

function playerColor(player) {
  switch (player) {
    case BLUE: return "rgb(0, 0, 200)";
    case YELLOW: return "rgb(200, 200, 0)";
    case RED: return "rgb(200, 0, 0)";
    case GREEN: return "rgb(0, 200, 0)";
  }
}

function playerString(player) {
  switch (player) {
    case BLUE: return "blue";
    case YELLOW: return "yellow";
    case RED: return "red";
    case GREEN: return "green";
  }
}

function nextPlayer(player) {
  switch (player) {
    case BLUE: return YELLOW;
    case YELLOW: return RED;
    case RED: return GREEN;
    case GREEN: return BLUE;
  }
}

function create2DArray(width, height) {
  var array = Array(width);
  for (var i = 0; i < width; ++i) {
    array[i] = Array(height);
  }
  return array;
}

class Board {
  constructor() {
    // Initially, there are no pieces on the board.
    this.pieces = create2DArray(WIDTH, HEIGHT);

    // Initially, you can only move in a corner. Place in turn order,
    // going clockwise from top-left (0,0).
    this.frontier = create2DArray(WIDTH, HEIGHT);
    this.frontier[0][0] = BLUE;
    this.frontier[WIDTH - 1][0] = YELLOW;
    this.frontier[WIDTH - 1][HEIGHT - 1] = RED;
    this.frontier[0][HEIGHT - 1] = GREEN;

    // Initially, everyone is allowed to move everywhere.
    this.allowed = create2DArray(WIDTH, HEIGHT);
    for (var i = 0; i < WIDTH; ++i) {
      for (var j = 0; j < HEIGHT; ++j) {
	this.allowed[i][j] = BLUE | YELLOW | RED | GREEN;
      }
    }
  }

  isPossible(tile, color, move) {
    var containsFrontier = false;
    for (var i = 0; i < tile.coors.length; ++i) {
      // Transform the tile coordinates based on rotation and flip.
      // TODO(piotrf): fix code duplication between here and tiles.js
      var tX, tY;
      switch(move.rotation) {
        case 0:
          tX = tile.coors[i][0]
          tY = tile.coors[i][1];
          break;
        case 1:
          tX = -tile.coors[i][1];
          tY = tile.coors[i][0];
          break;
        case 2:
          tX = -tile.coors[i][0];
          tY = -tile.coors[i][1];
          break;
        case 3:
          tX = tile.coors[i][1];
          tY = -tile.coors[i][0];
          break;
      }
      if (move.flip) {
        tX = -tX;
      }
      var x = tX + move.coord[0];
      var y = tY + move.coord[1];
    
      // Verify that the piece is on the board.
      if (x < 0 || x > WIDTH - 1) return false;
      if (y < 0 || y > HEIGHT - 1) return false;

      // Verify that we are allowed to move here.
      if ((this.allowed[x][y] & color) == 0) return false;

      // Keep track of whether or not we touch the frontier. We must touch in at
      // least one place.
      containsFrontier |= this.frontier[x][y] & color;
    }
    return containsFrontier;
  }

  place(tile, color, move) {
    if (!this.isPossible(tile, color, move)) {
      alert('Error! Trying to place in an impossible location.');
      return;
    }
    // Transform the tile coordinates based on rotation and flip.
    // TODO(piotrf): fix code duplication between here and tiles.js
    for (var i = 0; i < tile.coors.length; ++i) {
      var tX, tY;
      switch(move.rotation) {
        case 0:
          tX = tile.coors[i][0]
          tY = tile.coors[i][1];
          break;
        case 1:
          tX = -tile.coors[i][1];
          tY = tile.coors[i][0];
          break;
        case 2:
          tX = -tile.coors[i][0];
          tY = -tile.coors[i][1];
          break;
        case 3:
          tX = tile.coors[i][1];
          tY = -tile.coors[i][0];
          break;
      }
      if (move.flip) {
        tX = -tX;
      }
      var x = tX + move.coord[0];
      var y = tY + move.coord[1];
      this.pieces[x][y] = color;
    }
    // Update frontier and allowed based on current state of pieces.
    for (var i = 0; i < WIDTH; ++i) {
      for (var j = 0; j < HEIGHT; ++j) {
        // If there is a piece here, it's not on the frontier or allowed.
        if (this.pieces[i][j]) {
          this.frontier[i][j] = 0;
          this.allowed[i][j] = 0;
          continue;
        }
        // If there is a piece horizontally or vertically separated, then
        // that color can't move here.
        var allowed = BLUE | YELLOW | RED | GREEN;
        if (i > 0 && this.pieces[i - 1][j])
    	  allowed &= ~this.pieces[i - 1][j];
        if (i < WIDTH - 1 && this.pieces[i + 1][j])
	  allowed &= ~this.pieces[i + 1][j];
        if (j > 0 && this.pieces[i][j - 1])
	  allowed &= ~this.pieces[i][j - 1];
        if (j < HEIGHT - 1 && this.pieces[i][j + 1])
	  allowed &= ~this.pieces[i][j + 1];
        this.allowed[i][j] = allowed;
        // If there is a piece corner separated from us, and this is an allowed
        // spot, then this is on our frontier.
        var frontier = 0;
        if (i > 0 && j > 0 && this.pieces[i - 1][j - 1])
          frontier |= this.pieces[i - 1][j - 1];
        if (i > 0 && j < HEIGHT - 1 && this.pieces[i - 1][j + 1])
          frontier |= this.pieces[i - 1][j + 1];
        if (i < WIDTH - 1 && j > 0 && this.pieces[i + 1][j - 1])
          frontier |= this.pieces[i + 1][j - 1];
        if (i < WIDTH - 1 && j < HEIGHT - 1 &&
  	  this.pieces[i + 1][j + 1])
        frontier |= this.pieces[i + 1][j + 1];
        this.frontier[i][j] = frontier & allowed;
      }
    }
    // Always enforce initial frontier.
    this.frontier[0][0] = BLUE;
    this.frontier[WIDTH - 1][0] = YELLOW;
    this.frontier[WIDTH - 1][HEIGHT - 1] = RED;
    this.frontier[0][HEIGHT - 1] = GREEN;
  }

  draw(ctx) {
    // TODO(piotrf): this is a hack, the constant is defined in blokus.js.
    // Maybe this whole function should be part of the UI class.
    const BOARD_SCALE = 30;
    for (var i = 0; i < WIDTH; ++i) {
      for (var j = 0; j < HEIGHT; ++j) {
	if (this.pieces[i][j] > 0) {
	  ctx.fillStyle = playerColor(this.pieces[i][j])
	} else {
          ctx.fillStyle = "rgb(200, 200, 200)";
	}
	ctx.fillRect(BOARD_SCALE * i + 1, BOARD_SCALE * j + 1, BOARD_SCALE - 2,
		     BOARD_SCALE - 2);
      }
    }
  }
}

export {WIDTH, HEIGHT, BLUE, YELLOW, RED, GREEN, Board, playerColor,
	playerString};
