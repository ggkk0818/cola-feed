const crypto = require('crypto');

const SCRYPT_PARAMS = Object.freeze({
  N: 16384,
  r: 8,
  p: 1,
});
const SALT_BYTES = 16;
const KEY_BYTES = 64;
const PREFIX = 'scrypt';
const MAX_PARAM_N = 1 << 20;
const MAX_PARAM_R = 32;
const MAX_PARAM_P = 16;
const MAX_KEY_BYTES = 128;

function hashPassword(password) {
  const salt = crypto.randomBytes(SALT_BYTES);
  const derivedKey = crypto.scryptSync(
    String(password ?? ''),
    salt,
    KEY_BYTES,
    SCRYPT_PARAMS
  );

  return [
    PREFIX,
    SCRYPT_PARAMS.N,
    SCRYPT_PARAMS.r,
    SCRYPT_PARAMS.p,
    salt.toString('base64'),
    derivedKey.toString('base64'),
  ].join('$');
}

function verifyPassword(password, storedHash) {
  if (typeof storedHash !== 'string') {
    return false;
  }

  const parts = storedHash.split('$');
  if (parts.length !== 6 || parts[0] !== PREFIX) {
    return false;
  }

  const N = Number(parts[1]);
  const r = Number(parts[2]);
  const p = Number(parts[3]);

  if (
    !Number.isInteger(N) ||
    !Number.isInteger(r) ||
    !Number.isInteger(p) ||
    N < 2 ||
    N > MAX_PARAM_N ||
    r < 1 ||
    r > MAX_PARAM_R ||
    p < 1 ||
    p > MAX_PARAM_P
  ) {
    return false;
  }

  let salt;
  let expected;
  try {
    salt = Buffer.from(parts[4], 'base64');
    expected = Buffer.from(parts[5], 'base64');
  } catch (error) {
    return false;
  }

  if (
    salt.length === 0 ||
    expected.length < 16 ||
    expected.length > MAX_KEY_BYTES
  ) {
    return false;
  }

  let actual;
  try {
    actual = crypto.scryptSync(String(password ?? ''), salt, expected.length, {
      N,
      r,
      p,
      maxmem: 256 * N * r,
    });
  } catch (error) {
    return false;
  }

  return (
    actual.length === expected.length &&
    crypto.timingSafeEqual(actual, expected)
  );
}

module.exports = {
  hashPassword,
  verifyPassword,
  SCRYPT_PARAMS,
};
