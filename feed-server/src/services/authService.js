const crypto = require('crypto');
const { formatDateTime } = require('../utils/dateTime');
const { hashPassword, verifyPassword } = require('../auth/password');

const DAY_MS = 24 * 60 * 60 * 1000;
const DEFAULT_TOKEN_CHANNEL = 'web';
const TOKEN_BYTES = 32;
const SESSION_CLEANUP_INTERVAL_MS = 10 * 60 * 1000;

// 不同登录渠道使用不同有效期；扫码登录接入后使用 qr 渠道即可。
const TOKEN_TTL_BY_CHANNEL = Object.freeze({
  web: 7 * DAY_MS,
  qr: 365 * DAY_MS,
});

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
    if (!session) {
      return null;
    }

    if (session.expiresAt <= Date.now()) {
      this.sessions.delete(token);
      return null;
    }

    return session;
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
    if (typeof token === 'string' && token) {
      this.sessions.delete(token);
    }
  }

  purgeExpiredSessions() {
    const now = Date.now();
    for (const [token, session] of this.sessions) {
      if (session.expiresAt <= now) {
        this.sessions.delete(token);
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
};
