const ROLES = Object.freeze({
  ADMIN: 1,
  FEED_USER: 2,
  FEED_ADMIN: 3,
  FOOD_USER: 4,
  FOOD_ADMIN: 5,
});

const ROLE_LABELS = Object.freeze({
  [ROLES.ADMIN]: '管理员',
  [ROLES.FEED_USER]: '喂奶用户',
  [ROLES.FEED_ADMIN]: '喂奶管理员',
  [ROLES.FOOD_USER]: '辅食用户',
  [ROLES.FOOD_ADMIN]: '辅食管理员',
});

const ROLE_VALUES = Object.freeze(Object.values(ROLES));

// 管理员类角色：管理员、喂奶管理员、辅食管理员。
const ADMIN_ROLES = Object.freeze([
  ROLES.ADMIN,
  ROLES.FEED_ADMIN,
  ROLES.FOOD_ADMIN,
]);

function isValidRole(role) {
  return Number.isInteger(role) && ROLE_VALUES.includes(role);
}

function hasAnyRole(roles, allowedRoles) {
  if (!Array.isArray(roles) || !Array.isArray(allowedRoles)) {
    return false;
  }

  return roles.some((role) => allowedRoles.includes(role));
}

module.exports = {
  ROLES,
  ROLE_LABELS,
  ROLE_VALUES,
  ADMIN_ROLES,
  isValidRole,
  hasAnyRole,
};
