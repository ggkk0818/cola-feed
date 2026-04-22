const express = require('express');

function createApiRouter(options) {
  const { feedService, clientService } = options;
  const router = express.Router();

  router.post('/feed', (req, res) => {
    const currentRecord = feedService.feedNow();

    res.json({
      code: 200,
      data: currentRecord,
      messagge: '成功',
    });
  });

  router.get('/status', (req, res) => {
    res.json({
      code: 200,
      data: feedService.getStatus(),
      messagge: '成功',
    });
  });

  router.get('/clients', (req, res) => {
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
