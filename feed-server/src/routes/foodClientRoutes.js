const express = require('express');
const { ROLES } = require('../auth/roles');
const {
  createAuthMiddleware,
  requireAnyRole,
} = require('../middleware/auth');

function success(res, data) {
  res.json({
    code: 200,
    data,
    message: '成功',
  });
}

function asyncHandler(handler) {
  return function handleAsyncRoute(req, res, next) {
    Promise.resolve(handler(req, res, next)).catch(next);
  };
}

function createFoodClientRouter(options) {
  const { foodClientService, authService } = options;
  const router = express.Router();
  const authenticate = createAuthMiddleware(authService);
  const requireFoodAdmin = requireAnyRole(ROLES.ADMIN, ROLES.FOOD_ADMIN);
  const requireFoodAccess = requireAnyRole(
    ROLES.ADMIN,
    ROLES.FOOD_USER,
    ROLES.FOOD_ADMIN
  );

  router.post(
    '/foodClients/register',
    authenticate,
    requireFoodAdmin,
    asyncHandler((req, res) => {
      const result = foodClientService.registerClient(
        req.body,
        req.auth.userId
      );
      success(res, result);
    })
  );

  router.get(
    '/foodClients',
    authenticate,
    requireFoodAdmin,
    asyncHandler((req, res) => {
      success(res, foodClientService.getClients(req.query));
    })
  );

  // 客户端读取最近辅食：请求头 Authorization 使用注册接口返回的客户端 token。
  // 放在 /:chipId 之前，避免未来新增同前缀路由时被参数路由吞掉。
  router.get(
    '/foodClients/recentFoods',
    authenticate,
    requireFoodAccess,
    asyncHandler((req, res) => {
      const result = foodClientService.getRecentFoods(req.query, {
        auth: req.auth,
        ip: req.ip,
      });
      success(res, result);
    })
  );

  router.delete(
    '/foodClients/:chipId',
    authenticate,
    requireFoodAdmin,
    asyncHandler((req, res) => {
      foodClientService.deleteClient(req.params.chipId, req.auth.userId);
      success(res, null);
    })
  );

  // eslint-disable-next-line no-unused-vars
  router.use((error, req, res, next) => {
    const statusCode = Number.isInteger(error.statusCode)
      ? error.statusCode
      : 500;
    if (statusCode >= 500) {
      // eslint-disable-next-line no-console
      console.error('food client api error:', error);
    }

    res.status(statusCode).json({
      code: statusCode,
      data: null,
      message: statusCode >= 500 ? '服务器内部错误' : error.message,
    });
  });

  return router;
}

module.exports = {
  createFoodClientRouter,
};
