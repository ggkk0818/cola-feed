const recordsBody = document.getElementById('recordsBody');
const clientsBody = document.getElementById('clientsBody');
const feedBtn = document.getElementById('feedBtn');
const elapsedTime = document.getElementById('elapsedTime');
const tabOverviewBtn = document.getElementById('tabOverviewBtn');
const tabClientsBtn = document.getElementById('tabClientsBtn');
const overviewPane = document.getElementById('overviewPane');
const clientsPane = document.getElementById('clientsPane');

let lastFeedEndMs = null;

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

  const normalizedMatch = value.match(
    /^(\d{4})-(\d{2})-(\d{2})\s(\d{2}):(\d{2}):(\d{2})$/
  );
  if (normalizedMatch) {
    const [, y, m, d, h, min, s] = normalizedMatch;
    const parsed = new Date(
      Number(y),
      Number(m) - 1,
      Number(d),
      Number(h),
      Number(min),
      Number(s)
    );
    return Number.isNaN(parsed.getTime()) ? null : parsed;
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

function renderElapsed() {
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
  const response = await fetch('/api/status');
  const result = await response.json();
  const records = result?.data?.records || [];
  const lastFeed = result?.data?.lastFeed || null;

  lastFeedEndMs = getLatestFeedEndMs(records, lastFeed);
  renderElapsed();
  renderRows(records);
}

async function loadClients() {
  const response = await fetch('/api/clients');
  const result = await response.json();
  const clients = result?.data?.clients || [];
  renderClientRows(clients);
}

async function refreshAll() {
  try {
    await Promise.all([loadStatus(), loadClients()]);
  } catch (error) {
    // Keep UI responsive even if network intermittently fails.
  }
}

async function feedNow() {
  feedBtn.disabled = true;
  feedBtn.textContent = '处理中...';

  try {
    await fetch('/api/feed', { method: 'POST' });
    await refreshAll();
  } finally {
    feedBtn.disabled = false;
    feedBtn.textContent = '喂奶';
  }
}

tabOverviewBtn.addEventListener('click', () => {
  switchTab('overview');
});

tabClientsBtn.addEventListener('click', () => {
  switchTab('clients');
});

feedBtn.addEventListener('click', feedNow);

setInterval(renderElapsed, 1000);
setInterval(refreshAll, 5000);
switchTab('overview');
refreshAll();
