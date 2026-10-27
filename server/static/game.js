// The game page: shows a game, keeps it up to date by long-polling, lets
// humans place pieces, and steps through finished games. The server owns the
// rules: legal placements come from /legal_moves and past positions from
// /position, and this file only does the geometry of turning pieces.

import * as api from './api.js';
import {
  capitalize, colorsForPlayer, el, pieceSvg, playerColorsLabel, seatLabel,
  svg,
} from './common.js';

const BOARD_SIZE = 20;
// Where each color starts.
const START_CORNERS = {
  blue: [0, 0], yellow: [0, 19], red: [19, 19], green: [19, 0],
};
const SHOW_FITS_KEY = 'blokus.showFits';

const gameId = window.location.pathname.split('/').pop();

// From /api/tiles.
let tiles = [];
// For each tile, a map from a shape's key (see shapeKey) to its orientation
// index.
let orientationByShape = [];

// The latest state from the server.
let state = null;
// The legal placements for state.version, as a map from "tile:orientation"
// to a set of "row,col" offsets of the orientation's top-left corner.
let legal = null;
// The piece in hand: {tile, orientation}.
let selection = null;
// The board cell under the mouse: [row, col].
let hover = null;
// True while a move or undo is being sent.
let busy = false;
// Whether to mark where the piece in hand fits.
let showFits = loadShowFits();

// When reviewing a finished game: the position being shown (from
// /position), or null for the final state.
let reviewPosition = null;
// Positions fetched for review, by number of moves.
const positionCache = new Map();
// The review stop being loaded, so that steps taken while it loads add up.
let reviewTarget = null;

// ---- Geometry ----

// Shifts cells so the smallest row and column are 0, and sorts them.
function normalize(cells) {
  const minRow = Math.min(...cells.map(([r]) => r));
  const minCol = Math.min(...cells.map(([, c]) => c));
  return cells.map(([r, c]) => [r - minRow, c - minCol])
      .sort((a, b) => a[0] - b[0] || a[1] - b[1]);
}

function shapeKey(cells) {
  return normalize(cells).map(([r, c]) => `${r},${c}`).join(';');
}

function rotateClockwise(cells) {
  return cells.map(([r, c]) => [c, -r]);
}

function flipHorizontally(cells) {
  return cells.map(([r, c]) => [r, -c]);
}

function orientationCells(tile, orientation) {
  return tiles[tile].orientations[orientation].cells;
}

// The orientation of `tile` with the same shape as `cells`.
function findOrientation(tile, cells) {
  return orientationByShape[tile].get(shapeKey(cells));
}

// The cell of a piece that sits under the mouse: the one nearest its middle.
function anchorCell(cells) {
  const rows = cells.map(([r]) => r);
  const cols = cells.map(([, c]) => c);
  const midRow = (Math.min(...rows) + Math.max(...rows)) / 2;
  const midCol = (Math.min(...cols) + Math.max(...cols)) / 2;
  let best = cells[0];
  let bestDistance = Infinity;
  for (const cell of cells) {
    const distance = (cell[0] - midRow) ** 2 + (cell[1] - midCol) ** 2;
    if (distance < bestDistance) {
      best = cell;
      bestDistance = distance;
    }
  }
  return best;
}

// ---- State ----

function isHumanTurn() {
  return state && !state.finished &&
      state.seats[state.current_player].type === 'human';
}

// The position on screen: a reviewed position, or the live state.
function shown() {
  return reviewPosition || state;
}

function setLegalMoves(moves) {
  legal = new Map();
  for (const move of moves) {
    if (move.pass) continue;
    const rows = move.cells.map(([r]) => r);
    const cols = move.cells.map(([, c]) => c);
    const orientation = findOrientation(move.tile, move.cells);
    const key = `${move.tile}:${orientation}`;
    if (!legal.has(key)) legal.set(key, new Set());
    legal.get(key).add(`${Math.min(...rows)},${Math.min(...cols)}`);
  }
}

function isPlayable(tile) {
  if (!legal) return false;
  return tiles[tile].orientations.some((_, o) => legal.has(`${tile}:${o}`));
}

// The cells the piece in hand would cover with its anchor at `cell`, and
// whether that's legal.
function placementAt(cell) {
  const cells = orientationCells(selection.tile, selection.orientation);
  const [anchorRow, anchorCol] = anchorCell(cells);
  const rowOffset = cell[0] - anchorRow;
  const colOffset = cell[1] - anchorCol;
  const offsets = legal &&
      legal.get(`${selection.tile}:${selection.orientation}`);
  return {
    cells: cells.map(([r, c]) => [r + rowOffset, c + colOffset]),
    legal: Boolean(offsets && offsets.has(`${rowOffset},${colOffset}`)),
  };
}

// Shows a new state from the server. `fromPoll` is set for long-poll
// results: one with an older version means the server restarted.
function applyState(newState, { fromPoll = false } = {}) {
  if (state) {
    if (newState.version === state.version) return;
    if (newState.version < state.version && !fromPoll) return;
  }
  const previousColor = state && state.current_color;
  state = newState;
  legal = null;
  reviewPosition = null;
  reviewTarget = null;
  positionCache.clear();
  if (!isHumanTurn() || state.current_color !== previousColor) {
    selection = null;
  }
  render();
  if (isHumanTurn()) loadLegalMoves();
}

async function loadLegalMoves() {
  const version = state.version;
  try {
    const result = await api.getLegalMoves(gameId);
    // A newer state will load its own.
    if (result.version !== version || state.version !== version) return;
    setLegalMoves(result.moves);
    render();
  } catch (e) {
    showToast(`Couldn't load moves: ${e.message}`);
  }
}

async function pollForever() {
  let delay = 1000;
  while (true) {
    try {
      applyState(await api.waitForGame(gameId, state.version),
                 { fromPoll: true });
      setConnected(true);
      delay = 1000;
    } catch (e) {
      if (e.status === 404) {
        showToast('This game no longer exists.', { sticky: true });
        return;
      }
      setConnected(false);
      await new Promise((resolve) => setTimeout(resolve, delay));
      delay = Math.min(delay * 2, 15000);
    }
  }
}

// ---- Review ----

// The numbers of moves worth stopping at when reviewing: the start, and
// after each piece. Passes don't change the board, so they're skipped. The
// last stop is the end of the game.
function reviewStops() {
  const stops = [0];
  state.moves.forEach((move, i) => {
    if (!move.pass) stops.push(i + 1);
  });
  stops[stops.length - 1] = state.moves.length;
  return stops;
}

function reviewIndex() {
  const stops = reviewStops();
  return reviewPosition ? stops.indexOf(reviewPosition.num_moves)
                        : stops.length - 1;
}

async function reviewTo(index) {
  if (!state.finished) return;
  const stops = reviewStops();
  index = Math.max(0, Math.min(index, stops.length - 1));
  reviewTarget = index;
  if (index === stops.length - 1) {
    reviewPosition = null;
    render();
    return;
  }
  const numMoves = stops[index];
  const version = state.version;
  let position = positionCache.get(numMoves);
  if (!position) {
    try {
      position = await api.getPosition(gameId, numMoves);
    } catch (e) {
      showToast(`Couldn't load the position: ${e.message}`);
      return;
    }
    if (state.version !== version) return;
    positionCache.set(numMoves, position);
  }
  // A later step may have moved on while this one loaded.
  if (reviewTarget !== index) return;
  reviewPosition = position;
  render();
}

const reviewStep = (delta) =>
    reviewTo((reviewTarget === null ? reviewIndex() : reviewTarget) + delta);

// ---- Actions ----

function selectTile(tile) {
  if (!isHumanTurn() || !isPlayable(tile)) return;
  if (selection && selection.tile === tile) {
    selection = null;
  } else {
    // Start with the first orientation that fits somewhere.
    const orientation = tiles[tile].orientations
        .findIndex((_, o) => legal.has(`${tile}:${o}`));
    selection = { tile, orientation };
  }
  render();
}

function turnSelection(transform) {
  if (!selection) return;
  const cells = transform(orientationCells(selection.tile,
                                           selection.orientation));
  selection.orientation = findOrientation(selection.tile, cells);
  render();
}

const rotate = () => turnSelection(rotateClockwise);
const rotateBack = () => turnSelection(
    (cells) => rotateClockwise(rotateClockwise(rotateClockwise(cells))));
const flip = () => turnSelection(flipHorizontally);

async function placeAt(cell) {
  if (busy || !selection || !isHumanTurn()) return;
  const placement = placementAt(cell);
  if (!placement.legal) return;
  busy = true;
  try {
    const newState = await api.playMove(
        gameId, state.version, { tile: selection.tile, cells: placement.cells });
    selection = null;
    applyState(newState);
  } catch (e) {
    showToast(e.message);
    applyState(await api.getGame(gameId), { fromPoll: true });
  } finally {
    busy = false;
  }
}

async function undo() {
  if (busy || !state.can_undo) return;
  busy = true;
  try {
    selection = null;
    applyState(await api.undo(gameId, state.version));
  } catch (e) {
    showToast(e.message);
  } finally {
    busy = false;
  }
}

function loadShowFits() {
  try {
    return window.localStorage.getItem(SHOW_FITS_KEY) === 'true';
  } catch (e) {
    return false;
  }
}

function setShowFits(value) {
  showFits = value;
  try {
    window.localStorage.setItem(SHOW_FITS_KEY, String(value));
  } catch (e) {
    // Not remembered, which is fine.
  }
  document.getElementById('show-fits').checked = value;
  renderBoard();
}

// ---- Rendering ----

function render() {
  renderStatus();
  renderReview();
  renderBoard();
  renderHand();
  renderPlayers();
  document.getElementById('undo').disabled = busy || !state.can_undo;
}

function renderStatus() {
  const status = document.getElementById('status');
  const banner = document.getElementById('banner');
  if (state.finished) {
    const winners = state.winners
        .map((player) => playerColorsLabel(state.num_players, player));
    const best = state.scores[state.winners[0]];
    banner.textContent = state.winners.length > 1
        ? `Game over: ${winners.join(' and ')} tie with ${best}.`
        : `Game over: ${winners[0]} wins with ${best}.`;
    banner.hidden = false;
    status.textContent = 'Game over';
    return;
  }
  banner.hidden = true;
  const color = state.current_color;
  const seat = state.seats[state.current_player];
  status.replaceChildren(
    el('span', { className: `swatch ${color}` }),
    seat.type === 'human'
        ? `${capitalize(color)} to move`
        : el('span', {},
             `${capitalize(color)} (${seatLabel(seat)}) is thinking`,
             el('span', { className: 'dots' })));
}

function renderReview() {
  const review = document.getElementById('review');
  review.hidden = !state.finished;
  if (!state.finished) return;
  const last = reviewStops().length - 1;
  const index = reviewIndex();
  document.getElementById('review-label').textContent =
      index === last ? `End (${last} pieces played)`
                     : `Piece ${index} of ${last}`;
  document.getElementById('review-first').disabled = index === 0;
  document.getElementById('review-prev').disabled = index === 0;
  document.getElementById('review-next').disabled = index === last;
  document.getElementById('review-last').disabled = index === last;
}

function cellRect(r, c, attributes = {}) {
  return svg('rect', {
    x: c + 0.04, y: r + 0.04, width: 0.92, height: 0.92, rx: 0.12,
    ...attributes,
  });
}

function renderBoard() {
  const view = shown();
  const board = document.getElementById('board');
  const layers = [];

  // Empty cells, with each color's start corner marked.
  const grid = [];
  for (let r = 0; r < BOARD_SIZE; ++r) {
    for (let c = 0; c < BOARD_SIZE; ++c) {
      const color = view.board[r][c];
      grid.push(cellRect(r, c,
                         { className: color ? `cell ${color}` : 'empty' }));
    }
  }
  layers.push(svg('g', {}, grid));
  for (const [color, [r, c]] of Object.entries(START_CORNERS)) {
    if (view.board[r][c]) continue;
    layers.push(svg('circle', {
      cx: c + 0.5, cy: r + 0.5, r: 0.22, className: `start ${color}`,
    }));
  }

  // Outline the last piece placed.
  const lastMove = [...view.moves].reverse().find((move) => !move.pass);
  if (lastMove) {
    layers.push(svg('g', { className: 'last-move' },
                    lastMove.cells.map(([r, c]) => cellRect(r, c))));
  }

  if (selection && isHumanTurn()) {
    // Dots where the piece in hand fits, at its anchor.
    const offsets =
        legal && legal.get(`${selection.tile}:${selection.orientation}`);
    if (showFits && offsets) {
      const [anchorRow, anchorCol] = anchorCell(
          orientationCells(selection.tile, selection.orientation));
      layers.push(svg('g', { className: 'fits' }, [...offsets].map((offset) => {
        const [r, c] = offset.split(',').map(Number);
        return svg('circle', {
          cx: c + anchorCol + 0.5, cy: r + anchorRow + 0.5, r: 0.12,
          className: state.current_color,
        });
      })));
    }
    if (hover) {
      const placement = placementAt(hover);
      layers.push(svg('g', {
        className: `preview ${placement.legal ? 'legal' : 'illegal'}`,
      }, placement.cells
          .filter(([r, c]) => r >= 0 && r < BOARD_SIZE && c >= 0 &&
                              c < BOARD_SIZE)
          .map(([r, c]) => cellRect(
              r, c, { className: `cell ${state.current_color}` }))));
    }
  }
  board.replaceChildren(...layers);
  board.classList.toggle('placing', Boolean(selection && isHumanTurn()));
}

function renderHand() {
  const hand = document.getElementById('hand');
  if (!isHumanTurn()) {
    hand.hidden = true;
    return;
  }
  hand.hidden = false;
  const color = state.current_color;
  const unplayed = new Set(state.colors[color].unplayed_tiles);
  document.getElementById('hand-title').replaceChildren(
    el('span', { className: `swatch ${color}` }),
    !legal ? 'Loading moves…'
           : selection ? `${capitalize(color)}: place it on the board`
                       : `${capitalize(color)}: pick a piece`);
  document.getElementById('rotate').disabled = !selection;
  document.getElementById('flip').disabled = !selection;

  const tray = document.getElementById('tray');
  tray.replaceChildren(...tiles.filter((tile) => unplayed.has(tile.tile))
      .map(({ tile }) => {
        const selected = selection && selection.tile === tile;
        const playable = isPlayable(tile);
        const orientation = selected ? selection.orientation : 0;
        return el('button', {
          className: `piece${selected ? ' selected' : ''}`,
          disabled: !playable,
          title: playable ? '' : "Doesn't fit anywhere",
          onclick: () => selectTile(tile),
        }, pieceSvg(orientationCells(tile, orientation), color,
                    { cellSize: 12 }));
      }));
}

function renderPlayers() {
  const view = shown();
  const players = document.getElementById('players');
  players.replaceChildren(...state.seats.map((seat, player) => {
    const current = !view.finished && view.current_player === player;
    const winner = view.finished && view.winners.includes(player);
    return el('div', {
      className: `player${current ? ' current' : ''}${winner ? ' winner' : ''}`,
    },
      el('div', { className: 'player-header' },
        el('span', {}, playerColorsLabel(state.num_players, player),
          el('span', { className: 'muted seat-label' },
            seatLabel(seat), winner ? ' · winner' : '')),
        el('span', { className: 'player-score' }, view.scores[player])),
      colorsForPlayer(state.num_players, player).map((color) => {
        const info = view.colors[color];
        return el('div', { className: 'color-row' },
          el('span', { className: `swatch ${color}` }),
          `${info.unplayed_tiles.length} pieces left`,
          info.passed ? el('span', { className: 'muted color-out' }, '· out')
                      : null,
          state.num_players === 2
              ? el('span', { className: 'color-score' }, info.score) : null);
      }));
  }));
}

let toastTimer = null;

function showToast(message, { sticky = false } = {}) {
  const toast = document.getElementById('toast');
  toast.textContent = message;
  toast.hidden = false;
  clearTimeout(toastTimer);
  if (!sticky) toastTimer = setTimeout(() => { toast.hidden = true; }, 4000);
}

function setConnected(connected) {
  document.getElementById('connection').hidden = connected;
}

// ---- Input ----

// The board cell at a mouse event, or null.
function cellAt(event) {
  const board = document.getElementById('board');
  const point = board.createSVGPoint();
  point.x = event.clientX;
  point.y = event.clientY;
  const local = point.matrixTransform(board.getScreenCTM().inverse());
  const row = Math.floor(local.y);
  const col = Math.floor(local.x);
  if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
    return null;
  }
  return [row, col];
}

function setUpInput() {
  const board = document.getElementById('board');
  board.addEventListener('mousemove', (event) => {
    const cell = cellAt(event);
    if (String(cell) === String(hover)) return;
    hover = cell;
    if (selection) renderBoard();
  });
  board.addEventListener('mouseleave', () => {
    hover = null;
    if (selection) renderBoard();
  });
  board.addEventListener('click', (event) => {
    const cell = cellAt(event);
    if (!cell || !selection) return;
    // On touch screens there's no hover: the first tap shows where the
    // piece would go, and a second tap on the same cell places it.
    if (String(cell) !== String(hover)) {
      hover = cell;
      renderBoard();
      return;
    }
    placeAt(cell);
  });
  board.addEventListener('wheel', (event) => {
    if (!selection) return;
    event.preventDefault();
    if (event.deltaY > 0) rotate(); else rotateBack();
  }, { passive: false });

  document.addEventListener('keydown', (event) => {
    if (event.ctrlKey || event.metaKey || event.altKey) return;
    if (event.target instanceof HTMLInputElement) return;
    const key = event.key.toLowerCase();
    if (key === 'r') {
      if (event.shiftKey) rotateBack(); else rotate();
    } else if (key === 'f') {
      flip();
    } else if (key === 'h') {
      setShowFits(!showFits);
    } else if (key === 'escape') {
      selection = null;
      render();
    } else if (state.finished && key === 'arrowleft') {
      reviewStep(-1);
    } else if (state.finished && key === 'arrowright') {
      reviewStep(1);
    } else if (state.finished && key === 'home') {
      reviewTo(0);
    } else if (state.finished && key === 'end') {
      reviewTo(Infinity);
    } else {
      return;
    }
    event.preventDefault();
  });
  document.getElementById('rotate').addEventListener('click', rotate);
  document.getElementById('flip').addEventListener('click', flip);
  document.getElementById('undo').addEventListener('click', undo);
  const showFitsBox = document.getElementById('show-fits');
  showFitsBox.checked = showFits;
  showFitsBox.addEventListener('change',
                               () => setShowFits(showFitsBox.checked));
  document.getElementById('review-first')
      .addEventListener('click', () => reviewTo(0));
  document.getElementById('review-prev')
      .addEventListener('click', () => reviewStep(-1));
  document.getElementById('review-next')
      .addEventListener('click', () => reviewStep(1));
  document.getElementById('review-last')
      .addEventListener('click', () => reviewTo(Infinity));
}

async function main() {
  try {
    tiles = await api.getTiles();
    orientationByShape = tiles.map((tile) => new Map(
        tile.orientations.map((o, i) => [shapeKey(o.cells), i])));
    applyState(await api.getGame(gameId));
  } catch (e) {
    showToast(e.status === 404 ? 'There is no such game.'
                               : `Couldn't load the game: ${e.message}`,
              { sticky: true });
    return;
  }
  document.title = `Blokus · ${state.seats.map(seatLabel).join(' vs ')}`;
  setUpInput();
  pollForever();
}

main();
