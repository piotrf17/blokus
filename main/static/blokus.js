import * as board from './board.js';
import * as tiles from './tiles.js';

// Constants.
const BOARD_SCALE = 30;
const TILE_SCALE = 15;

// TODO(piotrf): create this from an initial request.
const PLAYERS = [
  { color: board.BLUE, type: 'human'},
  { color: board.YELLOW, type: 'computer'},
  { color: board.RED, type: 'computer'},
  { color: board.GREEN, type: 'computer'},
]

function buildTileMap() {
  // First compute the total size.
  var width = 0;
  var height = 0;
  for (var t = 0; t < tiles.TILES.length; ++t) {
    width += tiles.TILES[t].width + 1;
    if (t == Math.floor(tiles.TILES.length / 2) + 1) {
      height += 5;
    }
  }
  height += 5;
  // Create an array to store the tile map.
  var tileMap = new Array(width);
  for (var i = 0; i < width; ++i) {
    tileMap[i] = new Array(height);
  }
  // Now, fill in the tile map.
  var xOff = 0, yOff = 0;
  for (var t = 0; t < tiles.TILES.length; ++t) {
    var tile = tiles.TILES[t]
    for (var i = 0; i < 5; ++i) {
      for (var j = 0; j < 5; ++j) {
	if (tile.data[i][j]) {
	  tileMap[xOff + i][yOff + j] = t;
	} else {
	  tileMap[xOff + i][yOff + j] = null;
	}
      }
    }
    xOff += tile.width + 1;
    if (t == Math.floor(tiles.TILES.length / 2) + 1) {
      xOff = 0;
      yOff += 5;
    }
  }
  return tileMap;
}

function allTiles() {
  var tileList = Array(21);
  for (var i = 0; i < 21; ++i) {
    tileList[i] = true;
  }
  return tileList;
}

class UI {
  constructor() {
    // Build map from (x,y) -> tile in tile selection areas.
    this.tileMap = buildTileMap();

    this.tileList = {
      'blue': allTiles(),
      'yellow': allTiles(),
      'red': allTiles(),
      'green': allTiles(),
    }
  }

  isBoard(x, y) {
    return x < board.WIDTH * BOARD_SCALE && y < board.HEIGHT * BOARD_SCALE;
  }

  getBoardCoor(x, y) {
    if ((x % BOARD_SCALE < 4 || x % BOARD_SCALE > (BOARD_SCALE - 4)) &&
        (y % BOARD_SCALE < 4 || y % BOARD_SCALE > (BOARD_SCALE - 4))) {
      return [Math.floor((x + 4) / BOARD_SCALE),
    	      Math.floor((y + 4) / BOARD_SCALE)];
    }
    return null;
  }

  isTiles(x, y) {
    return x > 650;
  }

  getTile(x, y) {
    var tileX = Math.floor((x - 650) / TILE_SCALE);
    var player = Math.floor(y / (11 * TILE_SCALE));
    var tileY = Math.floor(y / TILE_SCALE) - 11 * player;
    if (tileY < 10) {
      return this.tileMap[tileX][tileY];
    } else {
      return null;
    }
  }

  drawTiles(ctx, x, y, color, player) {
    ctx.fillStyle = color;
    var x_off = x;
    var y_off = y;
    // NOTE: must match tile map construction code in buildTileMap.
    for (var t = 0; t < tiles.TILES.length; ++t) {
      if (this.tileList[board.playerString(player)][t]) {
	tiles.TILES[t].draw(ctx, x_off, y_off, 0, false, color, TILE_SCALE);
      }
      x_off += TILE_SCALE * (tiles.TILES[t].width + 1);
      if (t == Math.floor(tiles.TILES.length / 2) + 1) {
	x_off = x;
	y_off += TILE_SCALE * 5;
      }
    }
  }

  useTile(tile, color) {
    this.tileList[board.playerString(color)][tile] = false;
  }
}

class Blokus {
  constructor() {
    // Initialize the tiles module.
    tiles.buildTiles();

    this.board = new board.Board();
    this.holdingPiece = null;
    this.pendingMove = null;
    this.possibleMove = false;
    this.currentPlayer = 0;
    this.mouseX = 0;
    this.mouseY = 0;

    this.canvas = document.getElementById('board');
    this.ctx = this.canvas.getContext('2d');

    this.ui = new UI();

    this.waiting = false;
    if (PLAYERS[this.currentPlayer] == 'computer') {
      this.waiting = true;
      this.getNextServerMove();
    }

    // Draw the initial board.
    this.draw();
  }

  draw() {
    // Clear the canvas.
    this.ctx.fillStyle = "rgb(255,255,255)";
    this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

    // Draw the board.
    this.board.draw(this.ctx);

    // Draw tiles to be selected.
    // TODO(piotrf): only draw available tiles.
    this.ui.drawTiles(this.ctx, 650, 0, board.playerColor(board.BLUE), board.BLUE);
    this.ui.drawTiles(this.ctx, 650, TILE_SCALE * 11, board.playerColor(board.YELLOW), board.YELLOW);
    this.ui.drawTiles(this.ctx, 650, TILE_SCALE * 22, board.playerColor(board.RED), board.RED);
    this.ui.drawTiles(this.ctx, 650, TILE_SCALE * 33, board.playerColor(board.GREEN), board.GREEN);

    // Draw the piece the mouse is holding.
    if (this.holdingPiece != null) {
      // TODO(piotrf): move game logic out of draw function.
      if (this.ui.isBoard(this.mouseX, this.mouseY)) {
	var newBoardCoor = this.ui.getBoardCoor(this.mouseX, this.mouseY);
	if (newBoardCoor != null) {
	  if (this.pendingMove.coord == null ||
	      newBoardCoor[0] != this.pendingMove.coord[0] ||
	      newBoardCoor[1] != this.pendingMove.coord[1]) {
	    this.pendingMove.coord = newBoardCoor;
	    this.possibleMove = this.board.isPossible(
	      tiles.TILES[this.holdingPiece], PLAYERS[this.currentPlayer].color,
	      this.pendingMove);
	  }
	  tiles.TILES[this.holdingPiece].draw(
	    this.ctx,
	    this.pendingMove.coord[0] * BOARD_SCALE,
	    this.pendingMove.coord[1] * BOARD_SCALE,
	    this.pendingMove.rotation,
	    this.pendingMove.flip,
	    this.possibleMove ? "rgb(191,255,0)" : "rgb(255,105,180)",
	    BOARD_SCALE, 3);
	} else {
	  this.pendingMove.coord = null;
	  this.possibleMove = false;
	}
      }
      tiles.TILES[this.holdingPiece].draw(
	this.ctx, this.mouseX, this.mouseY, this.pendingMove.rotation,
	this.pendingMove.flip, board.playerColor(PLAYERS[this.currentPlayer].color),
	BOARD_SCALE);
    }
    
    var curColor = PLAYERS[this.currentPlayer].color
    this.ctx.font = '30px Arial';
    this.ctx.fillStyle = board.playerColor(curColor);
    if (this.waiting) {
      this.ctx.fillText(
	'Waiting for ' + board.playerString(curColor) + '.', 100, 650);
    } else {
      this.ctx.fillText(
	'Current player is ' + board.playerString(curColor) + '.', 100, 650);
    }
  }

  play() {
    var blokus = this;

    var top = this.canvas.getBoundingClientRect().top;
    var left = this.canvas.getBoundingClientRect().left;

    this.canvas.onmousemove = function(event) {
      if (blokus.waiting) return;
      blokus.mouseX = event.clientX - left;
      blokus.mouseY = event.clientY - top;
      blokus.draw();
    }

    this.canvas.onmouseup = function(event) {
      if (blokus.waiting) return;
      var x = event.clientX - left;
      var y = event.clientY - top;
      if (blokus.ui.isTiles(x, y)) {
	// We may have clicked on a tile in the tile area.
	var tile = blokus.ui.getTile(x, y);
	if (tile != null) {
	  blokus.holdingPiece = tile;
	  blokus.pendingMove = {
	    'coord': null,
	    'rotation': 0,
	    'flip': false,
	  }
	}
      } else if (blokus.ui.isBoard(x, y)) {
	// We're trying to put down a piece. 
	if (blokus.holdingPiece != null && blokus.possibleMove) {
	  blokus.board.place(tiles.TILES[blokus.holdingPiece],
			     PLAYERS[blokus.currentPlayer].color, blokus.pendingMove);
	  blokus.ui.useTile(blokus.holdingPiece, PLAYERS[blokus.currentPlayer].color);
	  // Inform the server of our move.
	  var moveData = {
	    tile: blokus.holdingPiece,
	    move: blokus.pendingMove
	  }
	  var curColor = PLAYERS[blokus.currentPlayer].color;
	  $.ajax({
	    /*	  url: '/game/' + board.playerString(curColor) + '/place',*/
	    url: '/game/0/place',  // xxx
	    type: 'POST',
	    dataType: 'json',
	    contentType: 'application/json',
	    data: JSON.stringify(moveData)})
	    .fail(function() {
	      alert('ajax request /game/place failed!');
	    });
	  // Switch to the next player.
	  // TODO(piotrf): make this a function in the 'model'.
	  blokus.currentPlayer = (blokus.currentPlayer + 1) % 4;
	  blokus.lastHumanMove = blokus.pendingMove;
	  blokus.pendingMove = null;
	  blokus.holdingPiece = null;
	  if (PLAYERS[blokus.currentPlayer].type == 'computer') {
	    blokus.waiting = true;
	    blokus.getNextServerMove();
	  }
	}

      }
      blokus.draw();
    }

    document.onkeypress = function(event) {
      if (blokus.waiting) return;
      switch(event.keyCode) {
      case 114:  // 'r'
	if (blokus.holdingPiece != null) {
	  blokus.pendingMove.rotation = (blokus.pendingMove.rotation + 3) % 4;
	  // Null out the pending move coordinate so that we're forced to
	  // recompute if the move is possible.
	  blokus.pendingMove.coord = null;
	}
	break;
      case 102:  // 'f'
	if (blokus.holdingPiece != null) {
	  blokus.pendingMove.flip = !blokus.pendingMove.flip;
	  // Null out the pending move coordinate so that we're forced to
	  // recompute if the move is possible.
	  blokus.pendingMove.coord = null;
	}
	break;
      }
      blokus.draw();
    }
  }

  getNextServerMove() {
    var blokus = this;
    $.getJSON('/game/next_move', function(data) {
      // TODO(piotrf): hacky.
      if (data.move.coord[0] == blokus.lastHumanMove.coord[0] &&
	  data.move.coord[1] == blokus.lastHumanMove.coord[1]) {
	blokus.getNextServerMove();
      } else {
	console.log('next machine move!');
	console.log(data);
	blokus.board.place(tiles.TILES[data.tile],
			   PLAYERS[blokus.currentPlayer].color, data.move);
	blokus.ui.useTile(data.tile, PLAYERS[blokus.currentPlayer].color);
	blokus.currentPlayer = (blokus.currentPlayer + 1) % 4;
	blokus.draw();
	if (PLAYERS[blokus.currentPlayer].type == 'computer') {
	  blokus.getNextServerMove();
	} else {
	  blokus.waiting = false;
	}
      }
    })
      .fail(function(data) {
	alert('ajax request to /game/next_move failed!');
      });
  }
}
    
function main() {
  var blokus = new Blokus();

  blokus.play();
}

window.onload = main;
