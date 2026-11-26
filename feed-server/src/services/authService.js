const crypto = require('crypto');
const { formatDateTime } = require('../utils/dateTime');
const { hashPassword, verifyPassword } = require('../auth/password');
const { ROLES } = require('../auth/roles');

const DAY_MS = 24 * 60 * 60 * 1000;
const DEFAULT_TOKEN_CHANNEL = 'web';
const FOOD_CLIENT_TOKEN_CHANNEL = 'foodClient';
const TOKEN_BYTES = 32;
const SESSION_CLEANUP_INTERVAL_MS = 10 * 60 * 1000;

// 不同登录渠道使用不同有效期；扫码登录接入后使用 qr 渠道即可。
const TOKEN_TTL_BY_CHANNEL = Object.freeze({
  web: 7 * DAY_MS,
  qr: 365 * DAY_MS,
  // 辅食客户端由辅食管理员注册后签发，有效期一年。
  [FOOD_CLIENT_TOKEN_CHANNEL]: 365 * DAY_MS,
});

function hashToken(token) {
  return crypto.createHash('sha256').update(token).digest('hex');
}

class AuthError extends Error {
  constructor(message, statusCode = 401, code = 'AUTH_ERROR') {
    super(message);
    this.name = 'AuthError';
    this.statusCode = statusCode;
    this.code = code;
  }
}

class AuthService {
  constructor(options) {
    const {
      userService,
      tokenTtlByChannel = TOKEN_TTL_BY_CHANNEL,
      sessionCleanupIntervalMs = SESSION_CLEANUP_INTERVAL_MS,
    } = options || {};

    if (!userService) {
      throw new Error('AuthService requires a userService');
    }

    this.userService = userService;
    this.tokenTtlByChannel = {
      ...TOKEN_TTL_BY_CHANNEL,
      ...tokenTtlByChannel,
    };
    this.sessions = new Map();
    // 辅食客户端 token 只保存 SHA-256 摘要，便于持久化到 foodClients.json
    // 后仍能在服务重启时恢复会话，同时避免明文 token 落盘。
    this.clientSessionsByHash = new Map();
    this.clientSessionRevokedHandler = null;

    const { publicKey, privateKey } = crypto.generateKeyPairSync('rsa', {
      modulusLength: 2048,
      publicKeyEncoding: {
        type: 'spki',
        format: 'pem',
      },
      privateKeyEncoding: {
        type: 'pkcs8',
        format: 'pem',
      },
    });
    this.publicKey = publicKey;
    this.privateKey = privateKey;

    // A dummy hash keeps failed logins for unknown users roughly as expensive
    // as failed logins for existing users.
    this.dummyPasswordHash = hashPassword(
      crypto.randomBytes(16).toString('hex')
    );

    this.sessionCleanupTimer = setInterval(
      () => this.purgeExpiredSessions(),
      sessionCleanupIntervalMs
    );
    if (typeof this.sessionCleanupTimer.unref === 'function') {
      this.sessionCleanupTimer.unref();
    }
  }

  getPublicKeyInfo() {
    return {
      publicKey: this.publicKey,
      algorithm: 'RSA-OAEP',
      hash: 'SHA-256',
    };
  }

  decryptPassword(encryptedPassword) {
    const cipherText =
      typeof encryptedPassword === 'string' ? encryptedPassword.trim() : '';
    if (!cipherText) {
      throw new AuthError('请输入密码', 400, 'PASSWORD_REQUIRED');
    }

    let cipherBuffer;
    try {
      cipherBuffer = Buffer.from(cipherText, 'base64');
    } catch (error) {
      throw new AuthError(
        '密码密文无效，请刷新页面后重试',
        400,
        'INVALID_PASSWORD_CIPHER'
      );
    }

    if (cipherBuffer.length === 0) {
      throw new AuthError(
        '密码密文无效，请刷新页面后重试',
        400,
        'INVALID_PASSWORD_CIPHER'
      );
    }

    try {
      const plainBuffer = crypto.privateDecrypt(
        {
          key: this.privateKey,
          padding: crypto.constants.RSA_PKCS1_OAEP_PADDING,
          oaepHash: 'sha256',
        },
        cipherBuffer
      );
      return plainBuffer.toString('utf8');
    } catch (error) {
      throw new AuthError(
        '密码密文无效，请刷新页面后重试',
        400,
        'INVALID_PASSWORD_CIPHER'
      );
    }
  }

  login(options) {
    const { userName, encryptedPassword, channel = DEFAULT_TOKEN_CHANNEL } =
      options || {};

    const normalizedUserName =
      typeof userName === 'string' ? userName.trim() : '';
    if (!normalizedUserName) {
      throw new AuthError('请输入用户名', 400, 'USERNAME_REQUIRED');
    }

    const password = this.decryptPassword(encryptedPassword);
    const user = this.userService.findByUserName(normalizedUserName);
    const passwordHash = user ? user.password : this.dummyPasswordHash;
    const passwordMatches = verifyPassword(password, passwordHash);

    if (!user || !passwordMatches) {
      throw new AuthError('用户名或密码错误', 401, 'INVALID_CREDENTIALS');
    }

    this.userService.updateLastLogin(user);
    return this.createSession(user, channel);
  }

  /**
   * 为指定渠道签发 token。当前登录接口固定使用 web（7 天）；
   * 后续扫码登录完成身份校验后，调用 issueToken(user, { channel: 'qr' })
   * 即可获得一年有效期。
   */
  issueToken(user, options = {}) {
    return this.createSession(user, options.channel || DEFAULT_TOKEN_CHANNEL);
  }

  /**
   * 为已注册的辅食客户端签发 token。客户端不是 users.json 中的账号，
   * 因此这里构造一个仅携带 FOOD_USER 角色的客户端会话。
   * 返回值中的 tokenHash/expiresAtMs 由 foodClientService 持久化，
   * token 本身只在注册响应中返回一次。
   */
  issueClientToken(options = {}) {
    const chipId =
      typeof options.chipId === 'string' ? options.chipId.trim() : '';
    if (!chipId) {
      throw new AuthError('chipId 不能为空', 400, 'CHIP_ID_REQUIRED');
    }

    const ttlMs = this.tokenTtlByChannel[FOOD_CLIENT_TOKEN_CHANNEL];
    if (!Number.isFinite(ttlMs) || ttlMs <= 0) {
      throw new Error('unsupported client token ttl');
    }

    const now = Date.now();
    const token = crypto.randomBytes(TOKEN_BYTES).toString('hex');
    const tokenHash = hashToken(token);
    const expiresAt = now + ttlMs;

    this.clientSessionsByHash.set(tokenHash, {
      userId: chipId,
      userName: chipId,
      roles: [ROLES.FOOD_USER],
      channel: FOOD_CLIENT_TOKEN_CHANNEL,
      clientChipId: chipId,
      isClient: true,
      createdAt: now,
      expiresAt,
    });

    return {
      token,
      tokenType: 'Bearer',
      expiresIn: Math.floor(ttlMs / 1000),
      expiresAt: formatDateTime(new Date(expiresAt)),
      expiresAtMs: expiresAt,
      tokenHash,
    };
  }

  /**
   * 服务启动时根据 foodClients.json 中的 token 摘要恢复客户端会话。
   * 返回 false 表示摘要无效或已过期，调用方可清理相关字段。
   */
  restoreClientSession(options = {}) {
    const chipId =
      typeof options.chipId === 'string' ? options.chipId.trim() : '';
    const tokenHash =
      typeof options.tokenHash === 'string' ? options.tokenHash.trim() : '';
    const expiresAt = Number(options.expiresAt);

    if (!chipId || !tokenHash || !Number.isFinite(expiresAt)) {
      return false;
    }

    if (expiresAt <= Date.now()) {
      return false;
    }

    const ttlMs = this.tokenTtlByChannel[FOOD_CLIENT_TOKEN_CHANNEL];
    this.clientSessionsByHash.set(tokenHash, {
      userId: chipId,
      userName: chipId,
      roles: [ROLES.FOOD_USER],
      channel: FOOD_CLIENT_TOKEN_CHANNEL,
      clientChipId: chipId,
      isClient: true,
      createdAt: expiresAt - (Number.isFinite(ttlMs) ? ttlMs : 0),
      expiresAt,
    });

    return true;
  }

  revokeClientSessionByHash(tokenHash) {
    if (typeof tokenHash === 'string' && tokenHash) {
      this.clientSessionsByHash.delete(tokenHash);
    }
  }

  /**
   * 允许 foodClientService 在客户端 token 被注销时同步清理持久化摘要，
   * 避免服务重启后旧 token 又被恢复。
   */
  setClientSessionRevokedHandler(handler) {
    this.clientSessionRevokedHandler =
      typeof handler === 'function' ? handler : null;
  }

  createSession(user, channel) {
    const ttlMs = this.tokenTtlByChannel[channel];
    if (!Number.isFinite(ttlMs) || ttlMs <= 0) {
      throw new Error(`unsupported login channel: ${channel}`);
    }

    const now = Date.now();
    const token = crypto.randomBytes(TOKEN_BYTES).toString('hex');
    const session = {
      token,
      userId: user.id,
      userName: user.userName,
      roles: [...user.roles],
      channel,
      createdAt: now,
      expiresAt: now + ttlMs,
    };

    this.sessions.set(token, session);

    return {
      token,
      tokenType: 'Bearer',
      expiresIn: Math.floor(ttlMs / 1000),
      expiresAt: formatDateTime(new Date(session.expiresAt)),
      user: this.toPublicUser(user),
    };
  }

  getSession(token) {
    if (typeof token !== 'string' || !token) {
      return null;
    }

    const session = this.sessions.get(token);
    if (session) {
      if (session.expiresAt <= Date.now()) {
        this.sessions.delete(token);
        return null;
      }

      return session;
    }

    const tokenHash = hashToken(token);
    const clientSession = this.clientSessionsByHash.get(tokenHash);
    if (!clientSession) {
      return null;
    }

    if (clientSession.expiresAt <= Date.now()) {
      this.clientSessionsByHash.delete(tokenHash);
      return null;
    }

    return clientSession;
  }

  describeSession(session) {
    return {
      id: session.userId,
      userName: session.userName,
      roles: [...session.roles],
      channel: session.channel,
      expiresAt: formatDateTime(new Date(session.expiresAt)),
    };
  }

  revokeSession(token) {
    if (typeof token !== 'string' || !token) {
      return;
    }

    this.sessions.delete(token);

    const tokenHash = hashToken(token);
    if (this.clientSessionsByHash.delete(tokenHash)) {
      if (this.clientSessionRevokedHandler) {
        this.clientSessionRevokedHandler(tokenHash);
      }
    }
  }

  purgeExpiredSessions() {
    const now = Date.now();
    for (const [token, session] of this.sessions) {
      if (session.expiresAt <= now) {
        this.sessions.delete(token);
      }
    }

    for (const [tokenHash, session] of this.clientSessionsByHash) {
      if (session.expiresAt <= now) {
        this.clientSessionsByHash.delete(tokenHash);
      }
    }
  }

  toPublicUser(user) {
    return {
      id: user.id,
      userName: user.userName,
      roles: [...user.roles],
    };
  }

  stop() {
    clearInterval(this.sessionCleanupTimer);
  }
}

module.exports = {
  AuthService,
  AuthError,
  TOKEN_TTL_BY_CHANNEL,
  DEFAULT_TOKEN_CHANNEL,
  FOOD_CLIENT_TOKEN_CHANNEL,
};
