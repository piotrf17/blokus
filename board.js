// Lots of algorithms are shared between here and board.cc :/
var board = board || {};

board.WIDTH = 20;
board.HEIGHT = 20;

// Enum for player values.
board.BLUE = 1;
board.YELLOW = 2;
board.RED = 4;
board.GREEN = 8;

board.playerColor = function(player) {
  switch (player) {
    case board.BLUE: return "rgb(0, 0, 200)";
    case board.YELLOW: return "rgb(200, 200, 0)";
    case board.RED: return "rgb(200, 0, 0)";
    case board.GREEN: return "rgb(0, 200, 0)";
  }
}

board.playerString = function(player) {
  switch (player) {
    case board.BLUE: return "blue";
    case board.YELLOW: return "yellow";
    case board.RED: return "red";
    case board.GREEN: return "green";
  }
}

board.nextPlayer = function(player) {
  switch (player) {
    case board.BLUE: return board.YELLOW;
    case board.YELLOW: return board.RED;
    case board.RED: return board.GREEN;
    case board.GREEN: return board.BLUE;
  }
}

function Create2DArray(width, height) {
  var array = Array(width);
  for (var i = 0; i < width; ++i) {
    array[i] = Array(height);
  }
  return array;
}

board.Board = function() {
  // Initially, there are no pieces on the board.
  this.pieces = Create2DArray(board.WIDTH, board.HEIGHT);

  // Initially, you can only move in a corner. Place in turn order,
  // going clockwise from top-left (0,0).
  this.frontier = Create2DArray(board.WIDTH, board.HEIGHT);
  this.frontier[0][0] = board.BLUE;
  this.frontier[board.WIDTH - 1][0] = board.YELLOW;
  this.frontier[board.WIDTH - 1][board.HEIGHT - 1] = board.RED;
  this.frontier[0][board.HEIGHT - 1] = board.GREEN;

  // Initially, everyone is allowed to move everywhere.
  this.allowed = Create2DArray(board.WIDTH, board.HEIGHT);
  for (var i = 0; i < board.WIDTH; ++i) {
    for (var j = 0; j < board.HEIGHT; ++j) {
      this.allowed[i][j] = board.BLUE | board.YELLOW | board.RED | board.GREEN;
    }
  }
}

board.Board.prototype.isPossible = function(tile, color, move) {
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
    if (x < 0 || x > board.WIDTH - 1) return false;
    if (y < 0 || y > board.HEIGHT - 1) return false;

    // Verify that we are allowed to move here.
    if ((this.allowed[x][y] & color) == 0) return false;

    // Keep track of whether or not we touch the frontier. We must touch in at
    // least one place.
    containsFrontier |= this.frontier[x][y] & color;
  }
  return containsFrontier;
}

board.Board.prototype.place = function(tile, color, move) {
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
  for (var i = 0; i < board.WIDTH; ++i) {
    for (var j = 0; j < board.HEIGHT; ++j) {
      // If there is a piece here, it's not on the frontier or allowed.
      if (this.pieces[i][j]) {
        this.frontier[i][j] = 0;
        this.allowed[i][j] = 0;
        continue;
      }
      // If there is a piece horizontally or vertically separated, then
      // that color can't move here.
      var allowed = board.BLUE | board.YELLOW | board.RED | board.GREEN;
      if (i > 0 && this.pieces[i - 1][j])
	allowed &= ~this.pieces[i - 1][j];
      if (i < board.WIDTH - 1 && this.pieces[i + 1][j])
	allowed &= ~this.pieces[i + 1][j];
      if (j > 0 && this.pieces[i][j - 1])
	allowed &= ~this.pieces[i][j - 1];
      if (j < board.HEIGHT - 1 && this.pieces[i][j + 1])
	allowed &= ~this.pieces[i][j + 1];
      this.allowed[i][j] = allowed;
      // If there is a piece corner separated from us, and this is an allowed
      // spot, then this is on our frontier.
      var frontier = 0;
      if (i > 0 && j > 0 && this.pieces[i - 1][j - 1])
        frontier |= this.pieces[i - 1][j - 1];
      if (i > 0 && j < board.HEIGHT - 1 && this.pieces[i - 1][j + 1])
        frontier |= this.pieces[i - 1][j + 1];
      if (i < board.WIDTH - 1 && j > 0 && this.pieces[i + 1][j - 1])
        frontier |= this.pieces[i + 1][j - 1];
      if (i < board.WIDTH - 1 && j < board.HEIGHT - 1 &&
	  this.pieces[i + 1][j + 1])
        frontier |= this.pieces[i + 1][j + 1];
      this.frontier[i][j] = frontier & allowed;

    }
  }
  // Always enforce initial frontier.
  this.frontier[0][0] = board.BLUE;
  this.frontier[board.WIDTH - 1][0] = board.YELLOW;
  this.frontier[board.WIDTH - 1][board.HEIGHT - 1] = board.RED;
  this.frontier[0][board.HEIGHT - 1] = board.GREEN;
}

board.Board.prototype.draw = function(ctx) {
  for (var i = 0; i < board.WIDTH; ++i) {
    for (var j = 0; j < board.HEIGHT; ++j) {
      if (this.pieces[i][j] > 0) {
	ctx.fillStyle = board.playerColor(this.pieces[i][j])
      } else {
        ctx.fillStyle = "rgb(200, 200, 200)";
      }
      ctx.fillRect(BOARD_SCALE * i + 1, BOARD_SCALE * j + 1, BOARD_SCALE - 2,
		   BOARD_SCALE - 2);
    }
  }
}

