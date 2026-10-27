// The front page: start a game, or open or delete an existing one.

import * as api from './api.js';
import {
  capitalize, colorsForPlayer, el, playerColorsLabel, seatLabel, shortNumber,
  timeAgo,
} from './common.js';

// The MCTS options a custom AI can set, with labels and input limits.
const MCTS_FIELDS = [
  { key: 'num_iterations', label: 'Iterations', min: 1, step: 1 },
  { key: 'c', label: 'Exploration (c)', min: 0, step: 0.05 },
  { key: 'num_threads', label: 'Threads', min: 1, step: 1 },
  { key: 'num_rollouts_per_iteration', label: 'Rollouts per iteration',
    min: 1, step: 1 },
  { key: 'virtual_loss', label: 'Virtual loss', min: 0, step: 1 },
];

let presets = {};

// One seat's controls: human, an AI preset, or a custom AI.
function seatRow(numPlayers, player) {
  const choice = el('select', { name: `seat-${player}` },
    el('option', { value: 'human' }, 'Human'),
    Object.entries(presets).map(([name, options]) => el('option',
      { value: name },
      `${capitalize(name)} AI (${shortNumber(options.num_iterations)})`)),
    el('option', { value: 'custom' }, 'Custom AI…'));
  choice.value = player === 0 ? 'human' : 'medium';

  const custom = el('div', { className: 'custom-mcts', hidden: true },
    MCTS_FIELDS.map((field) => el('label', {},
      el('span', {}, field.label),
      el('input', {
        type: 'number', name: field.key, min: field.min, step: field.step,
        value: presets.medium[field.key], required: true,
      }))));
  choice.addEventListener('change', () => {
    custom.hidden = choice.value !== 'custom';
  });

  const swatches = colorsForPlayer(numPlayers, player)
      .map((color) => el('span', { className: `swatch ${color}` }));
  return el('div', { className: 'seat' },
    el('div', { className: 'seat-main' },
      el('span', { className: 'seat-name' }, swatches,
        playerColorsLabel(numPlayers, player)),
      choice),
    custom);
}

function renderSeats() {
  const numPlayers = Number(
      document.querySelector('input[name=num_players]:checked').value);
  const seats = document.getElementById('seats');
  seats.replaceChildren(
    ...Array.from({ length: numPlayers }, (_, i) => seatRow(numPlayers, i)));
}

// Builds the API config from the form.
function readConfig() {
  const seats = [...document.querySelectorAll('#seats .seat')].map((row) => {
    const choice = row.querySelector('select').value;
    if (choice === 'human') return { type: 'human' };
    if (choice !== 'custom') return { type: 'ai', preset: choice };
    const mcts = {};
    for (const field of MCTS_FIELDS) {
      mcts[field.key] = Number(row.querySelector(`[name=${field.key}]`).value);
    }
    return { type: 'ai', mcts };
  });
  return { num_players: seats.length, seats };
}

async function startGame(event) {
  event.preventDefault();
  const error = document.getElementById('new-game-error');
  error.hidden = true;
  try {
    const game = await api.createGame(readConfig());
    window.location.href = `/game/${game.id}`;
  } catch (e) {
    error.textContent = e.message;
    error.hidden = false;
  }
}

function gameStatus(game) {
  if (!game.finished) return `In progress, ${game.num_moves} moves`;
  const winners = game.winners
      .map((player) => playerColorsLabel(game.num_players, player));
  return game.winners.length > 1 ? `Tie: ${winners.join(', ')}`
                                 : `${winners[0]} won`;
}

async function renderGames() {
  const container = document.getElementById('games');
  const error = document.getElementById('games-error');
  let games;
  try {
    games = await api.listGames();
    error.hidden = true;
  } catch (e) {
    error.textContent = `Couldn't load games: ${e.message}`;
    error.hidden = false;
    return;
  }
  if (games.length === 0) {
    container.replaceChildren(el('p', { className: 'muted' }, 'No games yet.'));
    return;
  }
  container.replaceChildren(el('ul', { className: 'game-list' },
    games.map((game) => el('li', {},
      el('a', { className: 'game-link', href: `/game/${game.id}` },
        el('span', { className: 'game-seats' },
          game.seats.map((seat, player) => el('span', { className: 'game-seat' },
            colorsForPlayer(game.num_players, player)
                .map((color) => el('span', { className: `swatch ${color}` })),
            seatLabel(seat)))),
        el('span', { className: 'muted' },
          `${gameStatus(game)} · ${timeAgo(game.updated)}`)),
      el('button', {
        className: 'icon-button', title: 'Delete game',
        'aria-label': 'Delete game',
        onclick: async () => {
          if (!confirm('Delete this game?')) return;
          try {
            await api.deleteGame(game.id);
          } catch (e) {
            alert(`Couldn't delete the game: ${e.message}`);
          }
          renderGames();
        },
      }, '✕')))));
}

async function main() {
  try {
    presets = await api.getPresets();
  } catch (e) {
    const error = document.getElementById('new-game-error');
    error.textContent = `Couldn't reach the server: ${e.message}`;
    error.hidden = false;
    return;
  }
  for (const radio of document.querySelectorAll('input[name=num_players]')) {
    radio.addEventListener('change', renderSeats);
  }
  renderSeats();
  document.getElementById('new-game').addEventListener('submit', startGame);
  renderGames();
}

main();
