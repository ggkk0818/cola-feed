(function () {
  'use strict';

  var AUTH_STORAGE_KEY = 'feedServerAuth';
  var form = document.getElementById('loginForm');
  var userNameInput = document.getElementById('userName');
  var passwordInput = document.getElementById('password');
  var loginBtn = document.getElementById('loginBtn');
  var errorBox = document.getElementById('loginError');

  function readStoredAuth() {
    try {
      var raw = window.localStorage.getItem(AUTH_STORAGE_KEY);
      if (!raw) {
        return null;
      }

      var parsed = JSON.parse(raw);
      if (!parsed || typeof parsed.token !== 'string' || !parsed.token) {
        return null;
      }

      return parsed;
    } catch (error) {
      return null;
    }
  }

  function clearStoredAuth() {
    try {
      window.localStorage.removeItem(AUTH_STORAGE_KEY);
    } catch (error) {
      // Ignore storage failures (private mode, disabled storage, ...).
    }
  }

  function getSafeNextPath() {
    var next = new URLSearchParams(window.location.search).get('next');
    if (
      !next ||
      next.charAt(0) !== '/' ||
      next.indexOf('//') === 0 ||
      next.indexOf('\\') !== -1 ||
      next.indexOf('/login') === 0 ||
      /[\u0000-\u001f]/.test(next)
    ) {
      return '/';
    }

    return next;
  }

  function showError(message) {
    errorBox.textContent = message;
    errorBox.hidden = false;
  }

  function hideError() {
    errorBox.hidden = true;
    errorBox.textContent = '';
  }

  function setBusy(isBusy) {
    loginBtn.disabled = isBusy;
    loginBtn.textContent = isBusy ? '登录中...' : '登录';
    form.setAttribute('aria-busy', isBusy ? 'true' : 'false');
  }

  function stripPemMarkers(content) {
    return String(content)
      .replace(/-----BEGIN [^-]+-----/g, '')
      .replace(/-----END [^-]+-----/g, '')
      .replace(/\s+/g, '');
  }

  function wrapPemBody(base64Body, type) {
    var lines = base64Body.match(/.{1,64}/g);

    if (!lines || lines.length === 0) {
      throw new Error('RSA 公钥格式无效。');
    }

    return (
      '-----BEGIN ' +
      type +
      '-----\n' +
      lines.join('\n') +
      '\n-----END ' +
      type +
      '-----'
    );
  }

  function normalizePublicKey(publicKey) {
    var body = stripPemMarkers(publicKey);

    if (!body) {
      throw new Error('RSA 公钥不能为空。');
    }

    return wrapPemBody(body, 'PUBLIC KEY');
  }

  // jsencrypt treats the input as a byte string; encode UTF-8 first so that
  // non-ASCII passwords survive the round trip.
  function stringToUtf8Binary(value) {
    if (typeof window.TextEncoder === 'function') {
      var bytes = new window.TextEncoder().encode(String(value));
      var binary = '';
      for (var index = 0; index < bytes.length; index += 1) {
        binary += String.fromCharCode(bytes[index]);
      }
      return binary;
    }

    return unescape(encodeURIComponent(String(value)));
  }

  function encryptPassword(publicKey, password) {
    if (typeof window.JSEncrypt !== 'function') {
      throw new Error('加密组件加载失败，请刷新页面后重试。');
    }

    var encryptor = new window.JSEncrypt();
    encryptor.setPublicKey(normalizePublicKey(publicKey));

    var encrypted =
      typeof encryptor.encryptOAEP === 'function'
        ? encryptor.encryptOAEP(stringToUtf8Binary(password))
        : false;

    if (!encrypted) {
      throw new Error('密码加密失败，请刷新页面后重试。');
    }

    return encrypted;
  }

  async function fetchPublicKey() {
    var response = await window.fetch('/api/auth/public-key', {
      cache: 'no-store',
      headers: { Accept: 'application/json' },
    });

    if (!response.ok) {
      throw new Error('获取加密公钥失败，请稍后重试。');
    }

    var payload = await response.json();
    var publicKey = payload && payload.data && payload.data.publicKey;
    if (typeof publicKey !== 'string' || !publicKey) {
      throw new Error('获取加密公钥失败，请稍后重试。');
    }

    return publicKey;
  }

  async function handleSubmit(event) {
    event.preventDefault();
    hideError();

    var userName = userNameInput.value.trim();
    var password = passwordInput.value;

    if (!userName || !password) {
      showError('请输入用户名和密码。');
      return;
    }

    setBusy(true);

    try {
      var publicKey = await fetchPublicKey();
      var encryptedPassword = encryptPassword(publicKey, password);

      var response = await window.fetch('/api/auth/login', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          Accept: 'application/json',
        },
        body: JSON.stringify({
          userName: userName,
          password: encryptedPassword,
        }),
      });

      var payload = await response.json().catch(function () {
        return {};
      });

      if (
        !response.ok ||
        !payload ||
        payload.code !== 200 ||
        !payload.data ||
        !payload.data.token
      ) {
        throw new Error(
          (payload && payload.message) || '用户名或密码错误。'
        );
      }

      window.localStorage.setItem(
        AUTH_STORAGE_KEY,
        JSON.stringify(payload.data)
      );
      window.location.replace(getSafeNextPath());
    } catch (error) {
      showError(error && error.message ? error.message : '登录失败，请稍后重试。');
      passwordInput.value = '';
      passwordInput.focus();
    } finally {
      setBusy(false);
    }
  }

  async function redirectIfAuthenticated() {
    var auth = readStoredAuth();
    if (!auth) {
      return false;
    }

    try {
      var response = await window.fetch('/api/auth/me', {
        cache: 'no-store',
        headers: {
          Authorization: 'Bearer ' + auth.token,
          Accept: 'application/json',
        },
      });

      if (response.status === 401) {
        clearStoredAuth();
        return false;
      }

      if (response.ok) {
        window.location.replace(getSafeNextPath());
        return true;
      }
    } catch (error) {
      // Keep the stored token and let the user try logging in again.
    }

    return false;
  }

  async function init() {
    var redirected = await redirectIfAuthenticated();
    if (redirected) {
      return;
    }

    form.addEventListener('submit', handleSubmit);
    userNameInput.addEventListener('input', hideError);
    passwordInput.addEventListener('input', hideError);
    userNameInput.focus();
  }

  init();
})();
