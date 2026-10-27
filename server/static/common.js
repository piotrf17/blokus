// Shared helpers for the pages.

// The colors, in turn order.
export const COLORS = ['blue', 'yellow', 'red', 'green'];

// The colors each player id plays.
export function colorsForPlayer(numPlayers, player) {
  return COLORS.filter((color, i) => i % numPlayers === player);
}

// A short label for a seat from the API, e.g. "Human", "Medium AI" or
// "Custom AI (5k)".
export function seatLabel(seat) {
  if (seat.type === 'human') return 'Human';
  if (seat.preset === 'custom') {
    return `Custom AI (${shortNumber(seat.mcts.num_iterations)})`;
  }
  return `${capitalize(seat.preset)} AI`;
}

// 300, 3k, 100k, 2.5M.
export function shortNumber(n) {
  if (n >= 1e6) return `${n / 1e6}M`;
  if (n >= 1e3) return `${n / 1e3}k`;
  return String(n);
}

// A label for a player id: its colors, e.g. "Blue & Red".
export function playerColorsLabel(numPlayers, player) {
  return colorsForPlayer(numPlayers, player).map(capitalize).join(' & ');
}

export function capitalize(text) {
  return text.charAt(0).toUpperCase() + text.slice(1);
}

// Creates an element with attributes and children. Attributes starting with
// "on" are event handlers.
export function el(tag, attributes = {}, ...children) {
  const element = document.createElement(tag);
  setAttributes(element, attributes);
  for (const child of children.flat()) {
    if (child === null || child === undefined || child === false) continue;
    element.append(child instanceof Node ? child : String(child));
  }
  return element;
}

const SVG_NS = 'http://www.w3.org/2000/svg';

// Like el, for SVG elements.
export function svg(tag, attributes = {}, ...children) {
  const element = document.createElementNS(SVG_NS, tag);
  setAttributes(element, attributes);
  for (const child of children.flat()) {
    if (child) element.append(child);
  }
  return element;
}

function setAttributes(element, attributes) {
  for (const [name, value] of Object.entries(attributes)) {
    if (value === null || value === undefined || value === false) continue;
    if (name.startsWith('on')) {
      element.addEventListener(name.slice(2), value);
    } else if (name === 'className') {
      element.setAttribute('class', value);
    } else {
      element.setAttribute(name, value === true ? '' : value);
    }
  }
}

// Draws cells ([row, col] pairs) as a small SVG piece.
export function pieceSvg(cells, color, { cellSize = 1, className } = {}) {
  const rows = Math.max(...cells.map(([r]) => r)) + 1;
  const cols = Math.max(...cells.map(([, c]) => c)) + 1;
  return svg('svg', {
    viewBox: `0 0 ${cols} ${rows}`,
    width: cols * cellSize,
    height: rows * cellSize,
    className,
  }, cells.map(([r, c]) => svg('rect', {
    x: c + 0.05, y: r + 0.05, width: 0.9, height: 0.9, rx: 0.15,
    className: `cell ${color}`,
  })));
}

// "3 minutes ago", etc.
export function timeAgo(isoTime) {
  const seconds = (Date.now() - new Date(isoTime).getTime()) / 1000;
  if (seconds < 60) return 'just now';
  const units = [['day', 86400], ['hour', 3600], ['minute', 60]];
  for (const [unit, size] of units) {
    if (seconds >= size) {
      const n = Math.floor(seconds / size);
      return `${n} ${unit}${n === 1 ? '' : 's'} ago`;
    }
  }
}
