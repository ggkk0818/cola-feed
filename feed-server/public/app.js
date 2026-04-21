const recordsBody = document.getElementById('recordsBody');
const feedBtn = document.getElementById('feedBtn');
const elapsedTime = document.getElementById('elapsedTime');
let lastFeedStartMs = null;

function formatDateTime(isoTime) {
  if (!isoTime) {
    return '-';
  }
  const date = new Date(isoTime);
  return date.toLocaleString('zh-CN', { hour12: false });
}

function formatDuration(seconds) {
  const safe = Number.isFinite(seconds) ? Math.max(0, seconds) : 0;
  const minutes = Math.floor(safe / 60);
  const remain = safe % 60;
  return `${minutes}分${remain}秒`;
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
          <td>${record.id || '-'}</td>
          <td>${formatDateTime(record.startTime)}</td>
          <td>${formatDateTime(record.endTime)}</td>
          <td>${formatDuration(record.duration)}</td>
        </tr>
      `
    )
    .join('');

  recordsBody.innerHTML = rows;
}

function renderElapsed() {
  if (!lastFeedStartMs || !Number.isFinite(lastFeedStartMs)) {
    elapsedTime.textContent = '暂无记录';
    return;
  }

  const seconds = Math.max(0, Math.floor((Date.now() - lastFeedStartMs) / 1000));
  elapsedTime.textContent = formatDuration(seconds);
}

async function loadStatus() {
  const response = await fetch('/api/status');
  const result = await response.json();
  const lastFeed = result?.data?.lastFeed || null;

  lastFeedStartMs = lastFeed?.startTime ? new Date(lastFeed.startTime).getTime() : null;
  renderElapsed();
  renderRows(result?.data?.records || []);
}

async function feedNow() {
  feedBtn.disabled = true;
  feedBtn.textContent = '处理中...';

  try {
    await fetch('/api/feed', { method: 'POST' });
    await loadStatus();
  } finally {
    feedBtn.disabled = false;
    feedBtn.textContent = '喂奶';
  }
}

feedBtn.addEventListener('click', feedNow);
setInterval(renderElapsed, 1000);
loadStatus();
