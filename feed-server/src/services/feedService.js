const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { formatDateTime, parseDateTime } = require('../utils/dateTime');

class FeedService {
  constructor(options) {
    const { dataDir, maxRecords = 12 } = options;
    this.maxRecords = maxRecords;
    this.dataDir = dataDir;
    this.dataFilePath = path.join(this.dataDir, 'feed-records.json');
    this.feedRecords = this.loadFeedRecords();
    this.saveFeedRecords();
  }

  ensureDataFile() {
    fs.mkdirSync(this.dataDir, { recursive: true });
    if (!fs.existsSync(this.dataFilePath)) {
      fs.writeFileSync(this.dataFilePath, '[]', 'utf8');
    }
  }

  generateUniqueId(existingIds) {
    let id = crypto.randomBytes(16).toString('hex');
    while (existingIds.has(id)) {
      id = crypto.randomBytes(16).toString('hex');
    }
    return id;
  }

  normalizeRecords(records) {
    if (!Array.isArray(records)) {
      return [];
    }

    const existingIds = new Set();

    return records
      .filter((record) => typeof record === 'object' && record !== null)
      .map((record) => {
        let id = typeof record.id === 'string' && record.id.length === 32 ? record.id : '';
        if (!id || existingIds.has(id)) {
          id = this.generateUniqueId(existingIds);
        }
        existingIds.add(id);

        const startDate = parseDateTime(record.startTime) || new Date();
        const endDate = parseDateTime(record.endTime) || startDate;
        const startTime = formatDateTime(startDate);
        const endTime = formatDateTime(endDate);
        const duration = Number.isFinite(record.duration)
          ? Math.max(0, Math.floor(record.duration))
          : 0;

        return {
          id,
          startTime,
          endTime,
          duration,
        };
      })
      .slice(-this.maxRecords);
  }

  loadFeedRecords() {
    this.ensureDataFile();

    try {
      const content = fs.readFileSync(this.dataFilePath, 'utf8');
      return this.normalizeRecords(JSON.parse(content));
    } catch (error) {
      return [];
    }
  }

  saveFeedRecords() {
    fs.writeFileSync(this.dataFilePath, JSON.stringify(this.feedRecords, null, 2), 'utf8');
  }

  createFeedRecord(now) {
    const existingIds = new Set(this.feedRecords.map((record) => record.id));
    const formattedNow = formatDateTime(now);
    return {
      id: this.generateUniqueId(existingIds),
      startTime: formattedNow,
      endTime: formattedNow,
      duration: 0,
    };
  }

  feedNow() {
    const now = new Date();
    const THIRTY_MINUTES_MS = 30 * 60 * 1000;
    const lastRecord = this.feedRecords[this.feedRecords.length - 1];
    let currentRecord;

    if (!lastRecord) {
      currentRecord = this.createFeedRecord(now);
      this.feedRecords.push(currentRecord);
    } else {
      const lastStartDate = parseDateTime(lastRecord.startTime);
      const lastStart = lastStartDate ? lastStartDate.getTime() : now.getTime();
      const delta = now.getTime() - lastStart;

      if (delta > THIRTY_MINUTES_MS) {
        currentRecord = this.createFeedRecord(now);
        this.feedRecords.push(currentRecord);
      } else {
        lastRecord.endTime = formatDateTime(now);
        lastRecord.duration = Math.floor((now.getTime() - lastStart) / 1000);
        currentRecord = lastRecord;
      }
    }

    while (this.feedRecords.length > this.maxRecords) {
      this.feedRecords.shift();
    }

    this.saveFeedRecords();
    return currentRecord;
  }

  getStatus() {
    return {
      lastFeed: this.feedRecords[this.feedRecords.length - 1] || null,
      records: this.feedRecords,
    };
  }

  getRecords() {
    return this.feedRecords;
  }
}

module.exports = {
  FeedService,
};
