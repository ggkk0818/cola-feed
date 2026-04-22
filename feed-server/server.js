const express = require('express');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const app = express();
const port = process.env.PORT || 3000;
const MAX_RECORDS = 12;
const THIRTY_MINUTES_MS = 30 * 60 * 1000;
const dataDir = path.join(__dirname, 'data');
const dataFilePath = path.join(dataDir, 'feed-records.json');

function ensureDataFile() {
  fs.mkdirSync(dataDir, { recursive: true });
  if (!fs.existsSync(dataFilePath)) {
    fs.writeFileSync(dataFilePath, '[]', 'utf8');
  }
}

function generateUniqueId(existingIds) {
  let id = crypto.randomBytes(16).toString('hex');
  while (existingIds.has(id)) {
    id = crypto.randomBytes(16).toString('hex');
  }
  return id;
}

function padTwoDigits(value) {
  return String(value).padStart(2, '0');
}

function formatDateTime(date) {
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

function normalizeRecords(records) {
  if (!Array.isArray(records)) {
    return [];
  }

  const existingIds = new Set();

  return records
    .filter((record) => typeof record === 'object' && record !== null)
    .map((record) => {
      let id = typeof record.id === 'string' && record.id.length === 32 ? record.id : '';
      if (!id || existingIds.has(id)) {
        id = generateUniqueId(existingIds);
      }
      existingIds.add(id);

      const startDate = parseDateTime(record.startTime) || new Date();
      const endDate = parseDateTime(record.endTime) || startDate;
      const startTime = formatDateTime(startDate);
      const endTime = formatDateTime(endDate);
      const duration = Number.isFinite(record.duration) ? Math.max(0, Math.floor(record.duration)) : 0;

      return {
        id,
        startTime,
        endTime,
        duration,
      };
    })
    .slice(-MAX_RECORDS);
}

function loadFeedRecords() {
  ensureDataFile();

  try {
    const content = fs.readFileSync(dataFilePath, 'utf8');
    return normalizeRecords(JSON.parse(content));
  } catch (error) {
    return [];
  }
}

let feedRecords = loadFeedRecords();

function saveFeedRecords() {
  fs.writeFileSync(dataFilePath, JSON.stringify(feedRecords, null, 2), 'utf8');
}

function createFeedRecord(now) {
  const existingIds = new Set(feedRecords.map((record) => record.id));
  const formattedNow = formatDateTime(now);
  return {
    id: generateUniqueId(existingIds),
    startTime: formattedNow,
    endTime: formattedNow,
    duration: 0,
  };
}

saveFeedRecords();

/**
 * FeedRecord shape:
 * {
 *   id: string, // 32-char unique string
 *   startTime: string,
 *   endTime: string,
 *   duration: number // seconds
 * }
 */

app.use(express.json());

app.post('/api/feed', (req, res) => {
  const now = new Date();
  const lastRecord = feedRecords[feedRecords.length - 1];
  let currentRecord;

  if (!lastRecord) {
    currentRecord = createFeedRecord(now);
    feedRecords.push(currentRecord);
  } else {
    const lastStartDate = parseDateTime(lastRecord.startTime);
    const lastStart = lastStartDate ? lastStartDate.getTime() : now.getTime();
    const delta = now.getTime() - lastStart;

    if (delta > THIRTY_MINUTES_MS) {
      currentRecord = createFeedRecord(now);
      feedRecords.push(currentRecord);
    } else {
      lastRecord.endTime = formatDateTime(now);
      lastRecord.duration = Math.floor((now.getTime() - lastStart) / 1000);
      currentRecord = lastRecord;
    }
  }

  while (feedRecords.length > MAX_RECORDS) {
    feedRecords.shift();
  }

  saveFeedRecords();

  res.json({
    code: 200,
    data: currentRecord,
    messagge: '成功',
  });
});

app.get('/api/status', (req, res) => {
  res.json({
    code: 200,
    data: {
      lastFeed: feedRecords[feedRecords.length - 1] || null,
      records: feedRecords,
    },
    messagge: '成功',
  });
});

app.use(express.static(path.join(__dirname, 'public')));

app.use((req, res, next) => {
  if (req.path.startsWith('/api')) {
    next();
    return;
  }
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.listen(port, () => {
  // eslint-disable-next-line no-console
  console.log(`feed-server started on http://localhost:${port}`);
});
