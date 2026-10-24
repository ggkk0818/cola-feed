const express = require('express');
const path = require('path');
const crypto = require('crypto');
const multer = require('multer');
const { ROLES } = require('../auth/roles');
const {
  createAuthMiddleware,
  requireAnyRole,
} = require('../middleware/auth');
const { FoodServiceError } = require('../services/foodService');

const MAX_UPLOAD_BYTES = 50 * 1024 * 1024;
const UPLOAD_FIELDS = [
  'coverImageFile',
  'coverBinFile',
  'coverBinSmallFile',
];
const ALLOWED_IMAGE_EXTENSIONS = new Set(['.jpg', '.jpeg', '.png']);

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

function createUploadMiddleware(foodService) {
  const storage = multer.diskStorage({
    destination(req, file, callback) {
      callback(null, foodService.uploadTempDir);
    },
    filename(req, file, callback) {
      const extension = path.extname(file.originalname || '').toLowerCase();
      callback(null, `${crypto.randomBytes(8).toString('hex')}${extension}`);
    },
  });

  return multer({
    storage,
    limits: {
      fileSize: MAX_UPLOAD_BYTES,
      files: UPLOAD_FIELDS.length,
      fields: 5,
      parts: 10,
      fieldSize: 10 * 1024,
    },
    fileFilter(req, file, callback) {
      if (!UPLOAD_FIELDS.includes(file.fieldname)) {
        callback(new FoodServiceError(`不支持的上传字段：${file.fieldname}`));
        return;
      }

      if (file.fieldname === 'coverImageFile') {
        const extension = path.extname(file.originalname || '').toLowerCase();
        if (!ALLOWED_IMAGE_EXTENSIONS.has(extension)) {
          callback(
            new FoodServiceError('coverImageFile 仅支持 jpg、jpeg、png')
          );
          return;
        }
      }

      callback(null, true);
    },
  }).fields(UPLOAD_FIELDS.map((fieldName) => ({ name: fieldName, maxCount: 1 })));
}

function createFoodRouter(options) {
  const { foodService, authService } = options;
  const router = express.Router();
  const authenticate = createAuthMiddleware(authService);
  const requireFoodAccess = requireAnyRole(
    ROLES.ADMIN,
    ROLES.FOOD_USER,
    ROLES.FOOD_ADMIN
  );
  const upload = createUploadMiddleware(foodService);

  router.use(authenticate, requireFoodAccess);

  router.get('/foods', (req, res) => {
    success(res, foodService.getFoods(req.query));
  });

  router.post(
    '/foods',
    asyncHandler(async (req, res) => {
      const food = foodService.addFood(req.body, req.auth.userId);
      success(res, food);
    })
  );

  router.put(
    '/foods/:id',
    asyncHandler(async (req, res) => {
      const food = foodService.updateFood(
        req.params.id,
        req.body,
        req.auth.userId
      );
      success(res, food);
    })
  );

  router.delete(
    '/foods/:id',
    asyncHandler(async (req, res) => {
      foodService.deleteFood(req.params.id, req.auth.userId);
      success(res, null);
    })
  );

  router.get('/foodTypes', (req, res) => {
    success(res, foodService.getFoodTypes(req.query));
  });

  router.post(
    '/foodTypes',
    upload,
    asyncHandler(async (req, res) => {
      const files = req.files || {};
      try {
        const foodType = foodService.addFoodType({
          name: req.body?.name,
          imageFile: files.coverImageFile?.[0],
          binFile: files.coverBinFile?.[0],
          smallBinFile: files.coverBinSmallFile?.[0],
          userId: req.auth.userId,
        });
        success(res, foodType);
      } catch (error) {
        foodService.removeUploadedFiles(files);
        throw error;
      }
    })
  );

  router.delete(
    '/foodTypes/:id',
    asyncHandler(async (req, res) => {
      foodService.deleteFoodType(req.params.id, req.auth.userId);
      success(res, null);
    })
  );

  // eslint-disable-next-line no-unused-vars
  router.use((error, req, res, next) => {
    foodService.removeUploadedFiles(req.files);

    if (error instanceof multer.MulterError) {
      let message = '上传失败';
      if (error.code === 'LIMIT_FILE_SIZE') {
        message = '单个文件不能超过 50MB';
      } else if (error.code === 'LIMIT_FILE_COUNT') {
        message = '上传文件数量超出限制';
      } else if (error.code === 'LIMIT_UNEXPECTED_FILE') {
        message = `不支持的上传字段：${error.field || ''}`;
      }

      res.status(400).json({
        code: 400,
        data: null,
        message,
      });
      return;
    }

    const statusCode = Number.isInteger(error.statusCode)
      ? error.statusCode
      : 500;
    if (statusCode >= 500) {
      // eslint-disable-next-line no-console
      console.error('food api error:', error);
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
  createFoodRouter,
  MAX_UPLOAD_BYTES,
};
