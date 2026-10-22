const express = require('express');
const { ADMIN_ROLES } = require('../auth/roles');
const {
  createAuthMiddleware,
  requireAnyRole,
} = require('../middleware/auth');

function createApiRouter(options) {
  const { feedService, clientService, authService } = options;
  const router = express.Router();

  const authenticate = createAuthMiddleware(authService);
  const requireAdmin = requireAnyRole(...ADMIN_ROLES);

  // 喂奶记录保持匿名可写，供局域网内现有调用方继续使用。
  router.post('/feed', (req, res) => {
    const currentRecord = feedService.feedNow();

    res.json({
      code: 200,
      data: currentRecord,
      messagge: '成功',
    });

    clientService.triggerImmediateSync().catch((error) => {
      // eslint-disable-next-line no-console
      console.error('immediate client sync error:', error.message);
    });
  });

  router.get('/status', authenticate, requireAdmin, (req, res) => {
    res.json({
      code: 200,
      data: feedService.getStatus(),
      messagge: '成功',
    });
  });

  router.get('/clients', authenticate, requireAdmin, (req, res) => {
    res.json({
      code: 200,
      data: {
        clients: clientService.getClients(),
      },
      messagge: '成功',
    });
  });

  return router;
}

module.exports = {
  createApiRouter,
};
