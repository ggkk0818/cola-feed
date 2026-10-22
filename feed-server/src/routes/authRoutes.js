const express = require('express');
const { AuthError } = require('../services/authService');
const { createAuthMiddleware } = require('../middleware/auth');

function createAuthRouter(options) {
  const { authService } = options;
  const router = express.Router();
  const authenticate = createAuthMiddleware(authService);

  router.get('/public-key', (req, res) => {
    res.set('Cache-Control', 'no-store');
    res.json({
      code: 200,
      data: authService.getPublicKeyInfo(),
      messagge: '成功',
    });
  });

  router.post('/login', (req, res) => {
    const { userName, password } = req.body || {};

    try {
      const data = authService.login({
        userName,
        encryptedPassword: password,
      });

      res.set('Cache-Control', 'no-store');
      res.json({
        code: 200,
        data,
        messagge: '成功',
      });
    } catch (error) {
      if (error instanceof AuthError) {
        res.status(error.statusCode).json({
          code: error.statusCode,
          message: error.message,
        });
        return;
      }

      // eslint-disable-next-line no-console
      console.error('login error:', error.message);
      res.status(500).json({
        code: 500,
        message: '登录失败，请稍后重试',
      });
    }
  });

  router.get('/me', authenticate, (req, res) => {
    res.set('Cache-Control', 'no-store');
    res.json({
      code: 200,
      data: authService.describeSession(req.auth),
      messagge: '成功',
    });
  });

  router.post('/logout', authenticate, (req, res) => {
    authService.revokeSession(req.authToken);
    res.json({
      code: 200,
      data: null,
      messagge: '成功',
    });
  });

  return router;
}

module.exports = {
  createAuthRouter,
};
