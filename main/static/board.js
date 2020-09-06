const NUM_ROWS = 20;
const NUM_COLS = 20;

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
    this.pieces = create2DArray(NUM_ROWS, NUM_COLS);

    // Initially, you can only move in a corner. Place in turn order,
    // going clockwise from top-left (0,0).
    this.frontier = create2DArray(NUM_ROWS, NUM_COLS);
    this.frontier[0][0] = BLUE;
    this.frontier[0][NUM_COLS - 1] = YELLOW;
    this.frontier[NUM_ROWS - 1][NUM_COLS - 1] = RED;
    this.frontier[NUM_ROWS - 1][0] = GREEN;

    // Initially, everyone is allowed to move everywhere.
    this.allowed = create2DArray(NUM_ROWS, NUM_COLS);
    for (var r = 0; r < NUM_ROWS; ++r) {
      for (var c = 0; c < NUM_COLS; ++c) {
	this.allowed[r][c] = BLUE | YELLOW | RED | GREEN;
      }
    }
  }

  isPossible(tile, color, move) {
    var containsFrontier = false;
    const coors = tile.placed(move.rotation, move.flip);
    for (var i = 0; i < coors.length; ++i) {
      const r = coors[i][0] + move.coord[0];
      const c = coors[i][1] + move.coord[1];

      // Verify that the piece is on the board.
      if (r < 0 || r > NUM_ROWS - 1) {
	return false;
      }
      if (c < 0 || c > NUM_COLS - 1) {
	return false;
      }

      // Verify that we are allowed to move here.
      if ((this.allowed[r][c] & color) == 0) {
	return false;
      }

      // Keep track of whether or not we touch the frontier. We must touch in at
      // least one place.
      containsFrontier |= this.frontier[r][c] & color;
    }
    return containsFrontier;
  }

  place(tile, color, move) {
    if (!this.isPossible(tile, color, move)) {
      alert('Error! Trying to place in an impossible location.');
      return;
    }
    const coors = tile.placed(move.rotation, move.flip);
    for (var i = 0; i < coors.length; ++i) {
      const r = coors[i][0] + move.coord[0];
      const c = coors[i][1] + move.coord[1];
      this.pieces[r][c] = color;
    }
    // Update frontier and allowed based on current state of pieces.
    for (var r = 0; r < NUM_ROWS; ++r) {
      for (var c = 0; c < NUM_COLS; ++c) {
        // If there is a piece here, it's not on the frontier or allowed.
        if (this.pieces[r][c]) {
          this.frontier[r][c] = 0;
          this.allowed[r][c] = 0;
          continue;
        }
        // If there is a piece horizontally or vertically separated, then
        // that color can't move here.
        var allowed = BLUE | YELLOW | RED | GREEN;
        if (r > 0 && this.pieces[r - 1][c])
    	  allowed &= ~this.pieces[r - 1][c];
        if (r < NUM_ROWS - 1 && this.pieces[r + 1][c])
	  allowed &= ~this.pieces[r + 1][c];
        if (c > 0 && this.pieces[r][c - 1])
	  allowed &= ~this.pieces[r][c - 1];
        if (c < NUM_COLS - 1 && this.pieces[r][c + 1])
	  allowed &= ~this.pieces[r][c + 1];
        this.allowed[r][c] = allowed;
        // If there is a piece corner separated from us, and this is an allowed
        // spot, then this is on our frontier.
        var frontier = 0;
        if (r > 0 && c > 0 && this.pieces[r - 1][c - 1])
          frontier |= this.pieces[r - 1][c - 1];
        if (r > 0 && c < NUM_COLS - 1 && this.pieces[r - 1][c + 1])
          frontier |= this.pieces[r - 1][c + 1];
        if (r < NUM_ROWS - 1 && c > 0 && this.pieces[r + 1][c - 1])
          frontier |= this.pieces[r + 1][c - 1];
        if (r < NUM_ROWS - 1 && c < NUM_COLS - 1 &&
  	  this.pieces[r + 1][c + 1])
          frontier |= this.pieces[r + 1][c + 1];
        this.frontier[r][c] = frontier & allowed;
      }
    }
    // Always enforce initial frontier.
    this.frontier[0][0] = BLUE;
    this.frontier[0][NUM_COLS - 1] = YELLOW;
    this.frontier[NUM_ROWS - 1][NUM_COLS - 1] = RED;
    this.frontier[NUM_ROWS - 1][0] = GREEN;
  }

  draw(ctx, scale) {
    for (var r = 0; r < NUM_ROWS; ++r) {
      for (var c = 0; c < NUM_COLS; ++c) {
	if (this.pieces[r][c] > 0) {
	  ctx.fillStyle = playerColor(this.pieces[r][c])
	} else {
          ctx.fillStyle = "rgb(200, 200, 200)";
	}
	ctx.fillRect(scale * c + 1, scale * r + 1, scale - 2, scale - 2);
      }
    }
  }
}

export {NUM_ROWS, NUM_COLS, BLUE, YELLOW, RED, GREEN, Board, playerColor,
	playerString};
