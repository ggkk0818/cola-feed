const { hasAnyRole } = require('../auth/roles');

function extractBearerToken(authorizationHeader) {
  if (typeof authorizationHeader !== 'string') {
    return '';
  }

  const match = authorizationHeader.match(/^Bearer\s+(\S+)$/i);
  return match ? match[1] : '';
}

function sendUnauthorized(res, message = '未授权', reason = 'UNAUTHORIZED') {
  res.set('WWW-Authenticate', 'Bearer');
  res.status(401).json({
    code: 401,
    message,
    reason,
  });
}

function createAuthMiddleware(authService) {
  return function authenticate(req, res, next) {
    const token = extractBearerToken(req.headers.authorization);
    const session = token ? authService.getSession(token) : null;

    if (!session) {
      sendUnauthorized(res);
      return;
    }

    req.auth = session;
    req.authToken = token;
    next();
  };
}

function requireAnyRole(...allowedRoles) {
  return function authorize(req, res, next) {
    if (!req.auth || !hasAnyRole(req.auth.roles, allowedRoles)) {
      sendUnauthorized(res, '无权限访问', 'FORBIDDEN');
      return;
    }

    next();
  };
}

module.exports = {
  extractBearerToken,
  sendUnauthorized,
  createAuthMiddleware,
  requireAnyRole,
};
