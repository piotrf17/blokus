// Calls to the server's JSON API (see server/web_server.h).

// An error response from the API.
export class ApiError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

async function request(method, path, body) {
  const options = { method, headers: {} };
  if (body !== undefined) {
    options.headers['Content-Type'] = 'application/json';
    options.body = JSON.stringify(body);
  }
  const response = await fetch(path, options);
  let data = null;
  try {
    data = await response.json();
  } catch (e) {
    // Not JSON, e.g. a proxy's error page.
  }
  if (!response.ok) {
    throw new ApiError(response.status,
                       (data && data.error) || response.statusText);
  }
  return data;
}

export const getPresets = () => request('GET', '/api/presets');
export const getTiles = () => request('GET', '/api/tiles');
export const listGames = () => request('GET', '/api/games');
export const createGame = (config) => request('POST', '/api/games', config);
export const deleteGame = (id) => request('DELETE', `/api/games/${id}`);
export const getGame = (id) => request('GET', `/api/games/${id}`);
export const getLegalMoves =
    (id) => request('GET', `/api/games/${id}/legal_moves`);

// The game after its first `numMoves` moves.
export const getPosition = (id, numMoves) =>
    request('GET', `/api/games/${id}/position?moves=${numMoves}`);

// Waits until the game's version is past `version`, or the server's
// long-poll timeout, and returns the state.
export const waitForGame = (id, version) =>
    request('GET', `/api/games/${id}?wait_version=${version}`);

// `move` is {tile, cells}.
export const playMove = (id, version, move) =>
    request('POST', `/api/games/${id}/moves`, { version, ...move });

export const undo = (id, version) =>
    request('POST', `/api/games/${id}/undo`, { version });
