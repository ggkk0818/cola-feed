const AUTH_STORAGE_KEY = 'feedServerAuth';
const ADMIN_ROLES = [1, 3, 5];
const ROLE_LABELS = {
  1: '管理员',
  2: '喂奶用户',
  3: '喂奶管理员',
  4: '辅食用户',
  5: '辅食管理员',
};

const recordsBody = document.getElementById('recordsBody');
const clientsBody = document.getElementById('clientsBody');
const feedBtn = document.getElementById('feedBtn');
const elapsedTime = document.getElementById('elapsedTime');
const tabOverviewBtn = document.getElementById('tabOverviewBtn');
const tabClientsBtn = document.getElementById('tabClientsBtn');
const overviewPane = document.getElementById('overviewPane');
const clientsPane = document.getElementById('clientsPane');
const currentUserEl = document.getElementById('currentUser');
const logoutBtn = document.getElementById('logoutBtn');

let lastFeedEndMs = null;
let currentAuth = null;
let currentUser = null;

function readStoredAuth() {
  try {
    const raw = window.localStorage.getItem(AUTH_STORAGE_KEY);
    if (!raw) {
      return null;
    }

    const parsed = JSON.parse(raw);
    if (!parsed || typeof parsed.token !== 'string' || !parsed.token) {
      return null;
    }

    return parsed;
  } catch (error) {
    return null;
  }
}

function clearStoredAuth() {
  try {
    window.localStorage.removeItem(AUTH_STORAGE_KEY);
  } catch (error) {
    // Ignore storage failures (private mode, disabled storage, ...).
  }
}

function getAuthToken() {
  return currentAuth && currentAuth.token ? currentAuth.token : '';
}

function buildLoginUrl() {
  const next = `${window.location.pathname}${window.location.search}`;
  if (!next || next === '/' || next.indexOf('/login') === 0) {
    return '/login.html';
  }

  return `/login.html?next=${encodeURIComponent(next)}`;
}

function redirectToLogin() {
  window.location.replace(buildLoginUrl());
}

function handleUnauthorized() {
  clearStoredAuth();
  currentAuth = null;
  currentUser = null;
  redirectToLogin();
}

async function apiFetch(url, options = {}) {
  const headers = new Headers(options.headers || {});
  const token = getAuthToken();
  if (token) {
    headers.set('Authorization', `Bearer ${token}`);
  }

  const response = await fetch(url, { ...options, headers });

  if (response.status === 401) {
    let payload = null;
    try {
      payload = await response.clone().json();
    } catch (error) {
      payload = null;
    }

    if (payload && payload.reason === 'FORBIDDEN') {
      const forbiddenError = new Error(payload.message || '无权限访问');
      forbiddenError.forbidden = true;
      throw forbiddenError;
    }

    handleUnauthorized();
    throw new Error('unauthorized');
  }

  return response;
}

function canViewAdminData() {
  return (
    !!currentUser &&
    Array.isArray(currentUser.roles) &&
    currentUser.roles.some((role) => ADMIN_ROLES.includes(role))
  );
}

function padTwoDigits(value) {
  return String(value).padStart(2, '0');
}

function formatDateTimeFromDate(date) {
  const year = date.getFullYear();
  const month = padTwoDigits(date.getMonth() + 1);
  const day = padTwoDigits(date.getDate());
  const hours = padTwoDigits(date.getHours());
  const minutes = padTwoDigits(date.getMinutes());
  const seconds = padTwoDigits(date.getSeconds());
  return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
}

function parseDateTime(value) {
  if (typeof value !== 'string') {
    return null;
  }

  const normalized = value.trim().replace(/-/g, '/');
  const parsed = new Date(normalized);
  if (!Number.isNaN(parsed.getTime())) {
    return parsed;
  }

  const fallback = new Date(value);
  return Number.isNaN(fallback.getTime()) ? null : fallback;
}

function formatDateTime(rawValue) {
  if (!rawValue) {
    return '-';
  }

  const parsed = parseDateTime(rawValue);
  return parsed ? formatDateTimeFromDate(parsed) : '-';
}

function formatDuration(seconds) {
  const safe = Number.isFinite(seconds) ? Math.max(0, seconds) : 0;
  const minutes = Math.floor(safe / 60);
  const remain = safe % 60;
  return `${minutes}分${remain}秒`;
}

function formatElapsedSinceLastFeed(seconds) {
  const safe = Number.isFinite(seconds) ? Math.max(0, seconds) : 0;

  if (safe < 5 * 60) {
    return '刚刚';
  }

  if (safe < 60 * 60) {
    const minutes = Math.floor(safe / 60);
    return `${minutes}分钟`;
  }

  const hours = Math.floor(safe / (60 * 60));
  const minutes = Math.floor((safe % (60 * 60)) / 60);
  if (minutes === 0) {
    return `${hours}小时`;
  }

  return `${hours}小时${minutes}分钟`;
}

function getLatestFeedEndMs(records, lastFeed) {
  let latestEndMs = null;

  if (Array.isArray(records)) {
    for (let index = 0; index < records.length; index += 1) {
      const parsedEnd = parseDateTime(records[index]?.endTime);
      if (!parsedEnd) {
        continue;
      }

      const currentEndMs = parsedEnd.getTime();
      if (!Number.isFinite(currentEndMs)) {
        continue;
      }

      if (latestEndMs === null || currentEndMs > latestEndMs) {
        latestEndMs = currentEndMs;
      }
    }
  }

  if (latestEndMs !== null) {
    return latestEndMs;
  }

  const parsedLastEnd = parseDateTime(lastFeed?.endTime);
  return parsedLastEnd ? parsedLastEnd.getTime() : null;
}

function renderRows(records) {
  if (!Array.isArray(records) || records.length === 0) {
    recordsBody.innerHTML = '<tr><td colspan="4" class="empty">暂无喂奶记录</td></tr>';
    return;
  }

  const rows = [...records]
    .reverse()
    .map(
      (record) => `
        <tr>
          <td data-label="ID">${record.id || '-'}</td>
          <td data-label="开始时间">${formatDateTime(record.startTime)}</td>
          <td data-label="结束时间">${formatDateTime(record.endTime)}</td>
          <td data-label="持续时间">${formatDuration(record.duration)}</td>
        </tr>
      `
    )
    .join('');

  recordsBody.innerHTML = rows;
}

function renderClientRows(clients) {
  if (!Array.isArray(clients) || clients.length === 0) {
    clientsBody.innerHTML = '<tr><td colspan="5" class="empty">暂无客户端数据</td></tr>';
    return;
  }

  const rows = clients
    .map(
      (client) => `
        <tr>
          <td data-label="chipId">${client.chipId || '-'}</td>
          <td data-label="IP">${client.ip || '-'}</td>
          <td data-label="心跳时间">${formatDateTime(client.heartbeatTime)}</td>
          <td data-label="更新时间">${formatDateTime(client.updateTime)}</td>
          <td data-label="状态">${client.isOnline ? '在线' : '离线'}</td>
        </tr>
      `
    )
    .join('');

  clientsBody.innerHTML = rows;
}

function renderPermissionDenied() {
  elapsedTime.textContent = '无权限查看';
  recordsBody.innerHTML =
    '<tr><td colspan="4" class="empty">当前账号无权限查看喂奶记录</td></tr>';
  clientsBody.innerHTML =
    '<tr><td colspan="5" class="empty">当前账号无权限查看客户端列表</td></tr>';
}

function renderElapsed() {
  if (!canViewAdminData()) {
    elapsedTime.textContent = '无权限查看';
    return;
  }

  if (!lastFeedEndMs || !Number.isFinite(lastFeedEndMs)) {
    elapsedTime.textContent = '暂无记录';
    return;
  }

  const seconds = Math.max(0, Math.floor((Date.now() - lastFeedEndMs) / 1000));
  elapsedTime.textContent = formatElapsedSinceLastFeed(seconds);
}

function switchTab(tabName) {
  const isOverview = tabName === 'overview';
  tabOverviewBtn.classList.toggle('active', isOverview);
  tabClientsBtn.classList.toggle('active', !isOverview);
  overviewPane.classList.toggle('active', isOverview);
  clientsPane.classList.toggle('active', !isOverview);
}

async function loadStatus() {
  const response = await apiFetch('/api/status', { cache: 'no-store' });
  if (!response.ok) {
    throw new Error(`status request failed: ${response.status}`);
  }

  const result = await response.json();
  const records = result?.data?.records || [];
  const lastFeed = result?.data?.lastFeed || null;

  lastFeedEndMs = getLatestFeedEndMs(records, lastFeed);
  renderElapsed();
  renderRows(records);
}

async function loadClients() {
  const response = await apiFetch('/api/clients', { cache: 'no-store' });
  if (!response.ok) {
    throw new Error(`clients request failed: ${response.status}`);
  }

  const result = await response.json();
  const clients = result?.data?.clients || [];
  renderClientRows(clients);
}

async function refreshAll() {
  if (!canViewAdminData()) {
    renderPermissionDenied();
    return;
  }

  const results = await Promise.allSettled([loadStatus(), loadClients()]);
  if (
    results.some(
      (result) =>
        result.status === 'rejected' &&
        result.reason &&
        result.reason.forbidden
    )
  ) {
    renderPermissionDenied();
  }
}

async function feedNow() {
  feedBtn.disabled = true;
  feedBtn.textContent = '处理中...';

  try {
    await apiFetch('/api/feed', { method: 'POST' });
    await refreshAll();
  } catch (error) {
    // apiFetch redirects on an invalid token; keep the page quiet otherwise.
  } finally {
    feedBtn.disabled = false;
    feedBtn.textContent = '喂奶';
  }
}

async function loadCurrentUser() {
  const response = await apiFetch('/api/auth/me', { cache: 'no-store' });
  if (!response.ok) {
    throw new Error(`me request failed: ${response.status}`);
  }

  const result = await response.json();
  const user = result?.data;
  if (!user || !Array.isArray(user.roles)) {
    throw new Error('invalid current user payload');
  }

  currentUser = user;
  currentUserEl.textContent = user.userName || '';
  const roleLabels = user.roles
    .map((role) => ROLE_LABELS[role])
    .filter(Boolean);
  currentUserEl.title = roleLabels.length
    ? `角色：${roleLabels.join('、')}`
    : '';
}

async function logout() {
  logoutBtn.disabled = true;
  const token = getAuthToken();

  try {
    if (token) {
      await fetch('/api/auth/logout', {
        method: 'POST',
        headers: {
          Authorization: `Bearer ${token}`,
        },
      });
    }
  } catch (error) {
    // Local state is cleared below even when the request cannot reach the server.
  }

  clearStoredAuth();
  currentAuth = null;
  currentUser = null;
  redirectToLogin();
}

async function startApp() {
  try {
    await loadCurrentUser();
  } catch (error) {
    // A 401 already redirected to the login page; other failures leave the
    // page empty rather than sending the user in circles.
    return;
  }

  tabOverviewBtn.addEventListener('click', () => {
    switchTab('overview');
  });

  tabClientsBtn.addEventListener('click', () => {
    switchTab('clients');
  });

  feedBtn.addEventListener('click', feedNow);
  logoutBtn.addEventListener('click', logout);

  setInterval(renderElapsed, 1000);
  setInterval(refreshAll, 5000);
  switchTab('overview');
  refreshAll();
}

function init() {
  currentAuth = readStoredAuth();
  if (!currentAuth) {
    redirectToLogin();
    return;
  }

  startApp();
}

init();
