const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { formatDateTime, parseDateTime } = require('../utils/dateTime');
const { ROLE_VALUES } = require('../auth/roles');

const USER_NAME_MAX_LENGTH = 64;
const USER_ID_PATTERN = /^[0-9a-f]{32}$/;

class UserService {
  constructor(options) {
    const { dataDir } = options;

    this.dataDir = dataDir;
    this.usersFilePath = path.join(this.dataDir, 'users.json');
    this.users = this.loadUsers();
  }

  ensureUsersFile() {
    fs.mkdirSync(this.dataDir, { recursive: true });
    if (!fs.existsSync(this.usersFilePath)) {
      fs.writeFileSync(this.usersFilePath, '[]\n', 'utf8');
    }
  }

  generateUniqueId(existingIds) {
    let id = crypto.randomBytes(16).toString('hex');
    while (existingIds.has(id)) {
      id = crypto.randomBytes(16).toString('hex');
    }
    return id;
  }

  normalizeUsers(users) {
    if (!Array.isArray(users)) {
      return [];
    }

    const existingIds = new Set();
    const existingNames = new Set();
    const normalized = [];

    for (const user of users) {
      if (!user || typeof user !== 'object') {
        continue;
      }

      const userName =
        typeof user.userName === 'string' ? user.userName.trim() : '';
      if (
        !userName ||
        userName.length > USER_NAME_MAX_LENGTH ||
        existingNames.has(userName)
      ) {
        continue;
      }

      let id =
        typeof user.id === 'string' && USER_ID_PATTERN.test(user.id)
          ? user.id
          : '';
      if (!id || existingIds.has(id)) {
        id = this.generateUniqueId(existingIds);
      }

      const roles = Array.isArray(user.roles)
        ? [
            ...new Set(
              user.roles.filter(
                (role) => Number.isInteger(role) && ROLE_VALUES.includes(role)
              )
            ),
          ]
        : [];

      const lastLoginDate = parseDateTime(user.lastLogin);

      existingNames.add(userName);
      existingIds.add(id);
      normalized.push({
        id,
        userName,
        password: typeof user.password === 'string' ? user.password : '',
        roles,
        lastLogin: lastLoginDate ? formatDateTime(lastLoginDate) : '',
      });
    }

    return normalized;
  }

  loadUsers() {
    this.ensureUsersFile();

    let parsed;
    try {
      parsed = JSON.parse(fs.readFileSync(this.usersFilePath, 'utf8'));
    } catch (error) {
      // eslint-disable-next-line no-console
      console.error('load users error:', error.message);
      return [];
    }

    if (!Array.isArray(parsed)) {
      // eslint-disable-next-line no-console
      console.error('load users error: users.json must contain an array');
      return [];
    }

    const users = this.normalizeUsers(parsed);

    // Persist repaired ids/roles/lastLogin, but never overwrite an unreadable file.
    if (JSON.stringify(users) !== JSON.stringify(parsed)) {
      fs.writeFileSync(
        this.usersFilePath,
        `${JSON.stringify(users, null, 2)}\n`,
        'utf8'
      );
    }

    return users;
  }

  saveUsers() {
    fs.mkdirSync(this.dataDir, { recursive: true });
    fs.writeFileSync(
      this.usersFilePath,
      `${JSON.stringify(this.users, null, 2)}\n`,
      'utf8'
    );
  }

  findByUserName(userName) {
    const normalized =
      typeof userName === 'string' ? userName.trim() : '';
    if (!normalized) {
      return null;
    }

    return this.users.find((user) => user.userName === normalized) || null;
  }

  findById(id) {
    if (typeof id !== 'string' || !id) {
      return null;
    }

    return this.users.find((user) => user.id === id) || null;
  }

  updateLastLogin(user) {
    if (!user) {
      return;
    }

    user.lastLogin = formatDateTime(new Date());
    this.saveUsers();
  }
}

module.exports = {
  UserService,
};
