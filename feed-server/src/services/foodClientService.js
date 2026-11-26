const fs = require('fs');
const path = require('path');
const { formatDate, formatDateTime, parseDateTime } = require('../utils/dateTime');

const DEFAULT_PAGE_SIZE = 20;
const MAX_PAGE_SIZE = 200;
const CHIP_ID_MAX_LENGTH = 64;
const FIRMWARE_VERSION_MAX_LENGTH = 64;
const TOKEN_HASH_PATTERN = /^[0-9a-f]{64}$/;

class FoodClientServiceError extends Error {
  constructor(message, statusCode = 400, code = 'FOOD_CLIENT_ERROR') {
    super(message);
    this.name = 'FoodClientServiceError';
    this.statusCode = statusCode;
    this.code = code;
  }
}

function readString(value) {
  return typeof value === 'string' ? value : '';
}

function readIp(value) {
  const ip = readString(value).trim();
  // Express 在双栈监听时可能返回 IPv4-mapped IPv6 地址。
  return ip.startsWith('::ffff:') ? ip.slice(7) : ip;
}

function parsePageValue(value, defaultValue, maxValue) {
  const parsed = Number.parseInt(value, 10);
  if (!Number.isInteger(parsed) || parsed < 0) {
    return defaultValue;
  }

  return maxValue ? Math.min(parsed, maxValue) : parsed;
}

function parsePaging(query) {
  const pageIndex = parsePageValue(query?.pageIndex, 0);
  const pageSize = parsePageValue(
    query?.pageSize,
    DEFAULT_PAGE_SIZE,
    MAX_PAGE_SIZE
  );
  return {
    pageIndex,
    pageSize: pageSize > 0 ? pageSize : DEFAULT_PAGE_SIZE,
  };
}

function parseBooleanFilter(value) {
  if (value === true || value === 1 || value === 'true' || value === '1') {
    return true;
  }
  if (value === false || value === 0 || value === 'false' || value === '0') {
    return false;
  }
  return null;
}

function paginate(list, pageIndex, pageSize) {
  const total = list.length;
  const start = pageIndex * pageSize;
  return {
    list: list.slice(start, start + pageSize),
    total,
  };
}

class FoodClientService {
  constructor(options) {
    const { dataDir, foodService, authService } = options || {};

    if (!dataDir) {
      throw new Error('FoodClientService requires a dataDir');
    }
    if (!foodService) {
      throw new Error('FoodClientService requires a foodService');
    }
    if (!authService) {
      throw new Error('FoodClientService requires an authService');
    }

    this.dataDir = dataDir;
    this.foodService = foodService;
    this.authService = authService;
    this.foodClientsFilePath = path.join(this.dataDir, 'foodClients.json');

    this.clients = this.loadClients();
    this.restoreClientSessions();
    this.authService.setClientSessionRevokedHandler((tokenHash) =>
      this.handleClientSessionRevoked(tokenHash)
    );
  }

  /**
   * 当客户端 token 被 logout 等入口注销时，同步从 foodClients.json
   * 清除摘要，避免重启后恢复已注销的会话。
   */
  handleClientSessionRevoked(tokenHash) {
    const client = this.clients.find(
      (item) => item.tokenHash && item.tokenHash === tokenHash
    );
    if (!client) {
      return;
    }

    client.tokenHash = '';
    client.tokenExpiresAt = 0;
    try {
      this.saveClients();
    } catch (error) {
      // eslint-disable-next-line no-console
      console.error('persist revoked food client token error:', error.message);
    }
  }

  ensureDataFile() {
    fs.mkdirSync(this.dataDir, { recursive: true });
    if (!fs.existsSync(this.foodClientsFilePath)) {
      fs.writeFileSync(this.foodClientsFilePath, '[]\n', 'utf8');
    }
  }

  loadJsonArray() {
    this.ensureDataFile();

    let parsed;
    try {
      parsed = JSON.parse(fs.readFileSync(this.foodClientsFilePath, 'utf8'));
    } catch (error) {
      // eslint-disable-next-line no-console
      console.error('load foodClients error:', error.message);
      return [];
    }

    if (!Array.isArray(parsed)) {
      // eslint-disable-next-line no-console
      console.error('load foodClients error: foodClients.json must contain an array');
      return [];
    }

    return parsed;
  }

  normalizeClients(clients) {
    if (!Array.isArray(clients)) {
      return [];
    }

    const now = new Date();
    const usedChipIds = new Set();
    const normalized = [];

    for (const client of clients) {
      if (!client || typeof client !== 'object') {
        continue;
      }

      const chipId = readString(client.chipId).trim();
      if (!chipId || usedChipIds.has(chipId)) {
        continue;
      }
      usedChipIds.add(chipId);

      const createDate = parseDateTime(client.createTime) || now;
      const updateDate = parseDateTime(client.updateTime) || createDate;
      const heartbeatDate = parseDateTime(client.heartbeatTime);

      const candidateHash = readString(client.tokenHash).trim();
      const candidateExpiresAt = Number(client.tokenExpiresAt);
      const tokenHash =
        TOKEN_HASH_PATTERN.test(candidateHash) &&
        Number.isFinite(candidateExpiresAt) &&
        candidateExpiresAt > Date.now()
          ? candidateHash
          : '';

      normalized.push({
        chipId,
        ip: readString(client.ip),
        firmwareVersion: readString(client.firmwareVersion),
        heartbeatTime: heartbeatDate ? formatDateTime(heartbeatDate) : '',
        isOnline: client.isOnline === true,
        isDel: client.isDel === 1 || client.isDel === true ? 1 : 0,
        createTime: formatDateTime(createDate),
        createUser: readString(client.createUser),
        updateTime: formatDateTime(updateDate),
        updateUser: readString(client.updateUser),
        tokenHash,
        tokenExpiresAt: tokenHash ? candidateExpiresAt : 0,
      });
    }

    return normalized;
  }

  loadClients() {
    const parsed = this.loadJsonArray();
    const clients = this.normalizeClients(parsed);

    if (JSON.stringify(clients) !== JSON.stringify(parsed)) {
      this.saveClients(clients);
    }

    return clients;
  }

  restoreClientSessions() {
    let changed = false;

    for (const client of this.clients) {
      if (client.isDel !== 0 || !client.tokenHash) {
        continue;
      }

      const restored = this.authService.restoreClientSession({
        chipId: client.chipId,
        tokenHash: client.tokenHash,
        expiresAt: client.tokenExpiresAt,
      });

      if (!restored) {
        client.tokenHash = '';
        client.tokenExpiresAt = 0;
        changed = true;
      }
    }

    if (changed) {
      this.saveClients();
    }
  }

  saveClients(clients = this.clients) {
    fs.mkdirSync(this.dataDir, { recursive: true });
    fs.writeFileSync(
      this.foodClientsFilePath,
      `${JSON.stringify(clients, null, 2)}\n`,
      'utf8'
    );
  }

  normalizeChipId(value) {
    const chipId = readString(value).trim();
    if (!chipId) {
      throw new FoodClientServiceError('chipId 不能为空');
    }
    if (Array.from(chipId).length > CHIP_ID_MAX_LENGTH) {
      throw new FoodClientServiceError(
        `chipId 长度不能超过 ${CHIP_ID_MAX_LENGTH} 个字符`
      );
    }
    if (/[\s/\\]/.test(chipId)) {
      throw new FoodClientServiceError('chipId 不能包含空白字符或斜杠');
    }
    return chipId;
  }

  normalizeFirmwareVersion(value) {
    const firmwareVersion = readString(value).trim();
    if (!firmwareVersion) {
      throw new FoodClientServiceError('firmwareVersion 不能为空');
    }
    if (Array.from(firmwareVersion).length > FIRMWARE_VERSION_MAX_LENGTH) {
      throw new FoodClientServiceError(
        `firmwareVersion 长度不能超过 ${FIRMWARE_VERSION_MAX_LENGTH} 个字符`
      );
    }
    return firmwareVersion;
  }

  toPublicClient(client) {
    return {
      chipId: client.chipId,
      ip: client.ip,
      firmwareVersion: client.firmwareVersion,
      heartbeatTime: client.heartbeatTime,
      isOnline: client.isOnline,
      isDel: client.isDel,
      createTime: client.createTime,
      createUser: client.createUser,
      updateTime: client.updateTime,
      updateUser: client.updateUser,
    };
  }

  findActiveClient(chipId) {
    return this.clients.find(
      (client) => client.chipId === chipId && client.isDel === 0
    );
  }

  /**
   * 注册辅食客户端。app 端 token 用于鉴权，chipId/firmwareVersion
   * 来自客户端参数；注册成功后签发一年期的 FOOD_USER 客户端 token。
   * 同一个 chipId 已被逻辑删除时重新启用，仍有有效记录时返回 409。
   */
  registerClient(input, userId) {
    const chipId = this.normalizeChipId(input?.chipId);
    const firmwareVersion = this.normalizeFirmwareVersion(
      input?.firmwareVersion
    );

    const existing = this.clients.find((client) => client.chipId === chipId);
    if (existing && existing.isDel === 0) {
      throw new FoodClientServiceError('该辅食客户端已注册', 409);
    }

    const issued = this.authService.issueClientToken({ chipId });
    const now = formatDateTime(new Date());
    const operator = readString(userId);
    const previous = existing ? { ...existing } : null;
    const previousTokenHash = existing ? existing.tokenHash : '';
    let client;

    if (existing) {
      client = existing;
      Object.assign(client, {
        ip: '',
        firmwareVersion,
        heartbeatTime: '',
        isOnline: false,
        isDel: 0,
        updateTime: now,
        updateUser: operator,
        tokenHash: issued.tokenHash,
        tokenExpiresAt: issued.expiresAtMs,
      });
    } else {
      client = {
        chipId,
        ip: '',
        firmwareVersion,
        heartbeatTime: '',
        isOnline: false,
        isDel: 0,
        createTime: now,
        createUser: operator,
        updateTime: now,
        updateUser: operator,
        tokenHash: issued.tokenHash,
        tokenExpiresAt: issued.expiresAtMs,
      };
      this.clients.push(client);
    }

    try {
      this.saveClients();
    } catch (error) {
      if (existing && previous) {
        Object.assign(existing, previous);
      } else {
        this.clients.pop();
      }
      this.authService.revokeClientSessionByHash(issued.tokenHash);
      throw error;
    }

    if (previousTokenHash && previousTokenHash !== issued.tokenHash) {
      this.authService.revokeClientSessionByHash(previousTokenHash);
    }

    return {
      token: issued.token,
      tokenType: issued.tokenType,
      expiresIn: issued.expiresIn,
      expiresAt: issued.expiresAt,
      // 客户端初始化时可能还没有发起过 HTTP 请求，ip/心跳留空，
      // 等首次调用最近辅食接口时再更新。
      client: this.toPublicClient(client),
    };
  }

  getClients(query) {
    const { pageIndex, pageSize } = parsePaging(query);
    const chipIdFilter = readString(query?.chipId).trim();
    const onlineFilter = parseBooleanFilter(query?.isOnline);

    const activeClients = this.clients
      .map((client, index) => ({ client, index }))
      .filter((entry) => entry.client.isDel === 0)
      .filter(
        (entry) =>
          !chipIdFilter || entry.client.chipId.includes(chipIdFilter)
      )
      .filter(
        (entry) =>
          onlineFilter === null || entry.client.isOnline === onlineFilter
      )
      .sort((left, right) => {
        const timeCompare = right.client.updateTime.localeCompare(
          left.client.updateTime
        );
        if (timeCompare !== 0) {
          return timeCompare;
        }
        return right.index - left.index;
      })
      .map((entry) => entry.client);

    const page = paginate(activeClients, pageIndex, pageSize);
    return {
      list: page.list.map((client) => this.toPublicClient(client)),
      total: page.total,
    };
  }

  deleteClient(chipId, userId) {
    const normalizedChipId = this.normalizeChipId(chipId);
    const client = this.findActiveClient(normalizedChipId);
    if (!client) {
      throw new FoodClientServiceError('辅食客户端不存在', 404);
    }

    const previous = { ...client };
    const tokenHash = client.tokenHash;

    client.isDel = 1;
    client.updateTime = formatDateTime(new Date());
    client.updateUser = readString(userId);
    client.tokenHash = '';
    client.tokenExpiresAt = 0;

    try {
      this.saveClients();
    } catch (error) {
      Object.assign(client, previous);
      throw error;
    }

    if (tokenHash) {
      this.authService.revokeClientSessionByHash(tokenHash);
    }

    return null;
  }

  buildRecentFoods(now) {
    const foods = [];

    // 当天 + 未来 7 天 + 过去 7 天，共 15 天；没有记录的日期也返回空数组。
    for (let offset = -7; offset <= 7; offset += 1) {
      const day = new Date(
        now.getFullYear(),
        now.getMonth(),
        now.getDate() + offset
      );
      const date = formatDate(day);
      const food = this.foodService.getFoodByDate(date);

      foods.push({
        date,
        morningFoods: food ? food.morningFoods : [],
        afternoonFoods: food ? food.afternoonFoods : [],
      });
    }

    return foods;
  }

  /**
   * 供辅食客户端读取最近辅食。客户端 token 必须与 chipId 绑定；
   * app 端的管理员/辅食用户 token 仍可用于联调。
   * 调用同时刷新 ip、固件版本、心跳时间和在线状态。
   */
  getRecentFoods(input, options = {}) {
    const chipId = this.normalizeChipId(input?.chipId);
    const firmwareVersion = this.normalizeFirmwareVersion(
      input?.firmwareVersion
    );

    const auth = options.auth;
    if (
      auth &&
      auth.channel === 'foodClient' &&
      auth.clientChipId !== chipId
    ) {
      throw new FoodClientServiceError('客户端 token 与 chipId 不匹配', 403);
    }

    const client = this.findActiveClient(chipId);
    if (!client) {
      throw new FoodClientServiceError('辅食客户端不存在', 404);
    }

    const now = new Date();
    const nowString = formatDateTime(now);
    const previous = { ...client };

    client.ip = readIp(options.ip) || client.ip;
    client.firmwareVersion = firmwareVersion;
    client.heartbeatTime = nowString;
    client.isOnline = true;
    client.updateTime = nowString;
    client.updateUser = chipId;

    try {
      this.saveClients();
    } catch (error) {
      Object.assign(client, previous);
      throw error;
    }

    return {
      serverTime: nowString,
      foods: this.buildRecentFoods(now),
    };
  }
}

module.exports = {
  FoodClientService,
  FoodClientServiceError,
  DEFAULT_PAGE_SIZE,
  MAX_PAGE_SIZE,
};
