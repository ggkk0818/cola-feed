const fs = require('fs');
const path = require('path');
const dgram = require('dgram');
const http = require('http');
const { formatDateTime, parseDateTime } = require('../utils/dateTime');

class ClientService {
  constructor(options) {
    const { dataDir, feedService, udpPort = 6113 } = options;

    this.feedService = feedService;
    this.udpPort = udpPort;
    this.dataDir = dataDir;
    this.clientsFilePath = path.join(this.dataDir, 'clients.json');

    this.clients = this.loadClients();
    this.udpServer = null;
    this.syncLoopRunning = false;
    this.syncQueue = Promise.resolve();

    this.saveClients();
  }

  ensureClientsFile() {
    fs.mkdirSync(this.dataDir, { recursive: true });
    if (!fs.existsSync(this.clientsFilePath)) {
      fs.writeFileSync(this.clientsFilePath, '[]', 'utf8');
    }
  }

  normalizeClients(clients) {
    if (!Array.isArray(clients)) {
      return [];
    }

    const now = new Date();
    const nowString = formatDateTime(now);
    const dedupe = new Set();

    return clients
      .filter((item) => typeof item === 'object' && item !== null)
      .map((item) => {
        const chipId = typeof item.chipId === 'string' ? item.chipId.trim() : '';
        if (!chipId || dedupe.has(chipId)) {
          return null;
        }

        dedupe.add(chipId);

        const heartbeatDate = parseDateTime(item.heartbeatTime) || now;
        const updateDate = parseDateTime(item.updateTime) || now;

        return {
          chipId,
          ip: typeof item.ip === 'string' ? item.ip : '',
          heartbeatTime: formatDateTime(heartbeatDate),
          updateTime: formatDateTime(updateDate),
          isOnline: typeof item.isOnline === 'boolean' ? item.isOnline : true,
        };
      })
      .filter(Boolean)
      .map((item) => ({
        ...item,
        heartbeatTime: item.heartbeatTime || nowString,
        updateTime: item.updateTime || nowString,
      }));
  }

  loadClients() {
    this.ensureClientsFile();

    try {
      const content = fs.readFileSync(this.clientsFilePath, 'utf8');
      return this.normalizeClients(JSON.parse(content));
    } catch (error) {
      return [];
    }
  }

  saveClients() {
    fs.writeFileSync(this.clientsFilePath, JSON.stringify(this.clients, null, 2), 'utf8');
  }

  getClients() {
    return this.clients;
  }

  start() {
    this.startUdpListener();
    this.startSyncLoop();
  }

  startUdpListener() {
    if (this.udpServer) {
      return;
    }

    this.udpServer = dgram.createSocket('udp4');

    this.udpServer.on('message', (message, rinfo) => {
      this.handleBroadcastMessage(message, rinfo);
    });

    this.udpServer.on('error', (error) => {
      // eslint-disable-next-line no-console
      console.error('udp listener error:', error.message);
    });

    this.udpServer.bind(this.udpPort, () => {
      if (this.udpServer) {
        this.udpServer.setBroadcast(true);
      }
      // eslint-disable-next-line no-console
      console.log(`udp listener started on port ${this.udpPort}`);
    });
  }

  handleBroadcastMessage(message, rinfo) {
    let payload;

    try {
      payload = JSON.parse(message.toString('utf8'));
    } catch (error) {
      return;
    }

    if (!payload || payload.device_name !== 'Cola-ePaper') {
      return;
    }

    const chipId = typeof payload.chip_id === 'string' ? payload.chip_id.trim() : '';
    if (!chipId) {
      return;
    }

    const now = new Date();
    const nowString = formatDateTime(now);
    const firstSyncReadyTime = formatDateTime(new Date(now.getTime() - 61 * 1000));
    const matched = this.clients.find((client) => client.chipId === chipId);

    if (!matched) {
      this.clients.push({
        chipId,
        ip: rinfo.address,
        heartbeatTime: nowString,
        updateTime: firstSyncReadyTime,
        isOnline: true,
      });
      this.saveClients();
      return;
    }

    matched.ip = rinfo.address;
    matched.heartbeatTime = nowString;
    matched.isOnline = true;
    this.saveClients();
  }

  startSyncLoop() {
    if (this.syncLoopRunning) {
      return;
    }

    this.syncLoopRunning = true;
    this.runSyncLoop();
  }

  async runSyncLoop() {
    while (this.syncLoopRunning) {
      try {
        await this.enqueueSync();
      } catch (error) {
        // eslint-disable-next-line no-console
        console.error('client sync error:', error.message);
      }

      await this.sleep(1000);
    }
  }

  triggerImmediateSync() {
    return this.enqueueSync({ force: true });
  }

  enqueueSync(options = {}) {
    this.syncQueue = this.syncQueue
      .catch(() => {})
      .then(() => this.syncClientsOnce(options));

    return this.syncQueue;
  }

  async syncClientsOnce(options = {}) {
    const { force = false } = options;
    const now = new Date();
    const nowMs = now.getTime();
    const nowString = formatDateTime(now);
    const feedPayload = {
      serverTime: nowString,
      records: this.feedService.getRecords(),
    };

    let hasChanges = false;

    for (let index = 0; index < this.clients.length; index += 1) {
      const client = this.clients[index];
      const updateDate = parseDateTime(client.updateTime);
      const updateTimeMs = updateDate ? updateDate.getTime() : 0;

      if (!force && nowMs - updateTimeMs < 60 * 1000) {
        continue;
      }

      try {
        await this.sendFeedData(client.ip, feedPayload);
        client.updateTime = nowString;
        client.isOnline = true;
        hasChanges = true;
      } catch (error) {
        const heartbeatDate = parseDateTime(client.heartbeatTime);
        const heartbeatMs = heartbeatDate ? heartbeatDate.getTime() : 0;

        if (nowMs - heartbeatMs > 5 * 60 * 1000) {
          client.isOnline = false;
          hasChanges = true;
        }
      }
    }

    if (hasChanges) {
      this.saveClients();
    }
  }

  sendFeedData(ip, payload) {
    const body = JSON.stringify(payload);

    return new Promise((resolve, reject) => {
      const req = http.request(
        {
          hostname: ip,
          port: 80,
          path: '/api/feedData',
          method: 'PUT',
          headers: {
            'Content-Type': 'application/json',
            'Content-Length': Buffer.byteLength(body),
          },
          timeout: 5000,
        },
        (res) => {
          if (res.statusCode && res.statusCode >= 200 && res.statusCode < 300) {
            res.resume();
            resolve();
            return;
          }

          res.resume();
          reject(new Error(`bad status ${res.statusCode}`));
        }
      );

      req.on('timeout', () => {
        req.destroy(new Error('timeout'));
      });

      req.on('error', (error) => {
        reject(error);
      });

      req.write(body);
      req.end();
    });
  }

  sleep(ms) {
    return new Promise((resolve) => {
      setTimeout(resolve, ms);
    });
  }
}

module.exports = {
  ClientService,
};
