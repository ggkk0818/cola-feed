const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { formatDateTime, parseDateTime } = require('../utils/dateTime');

const FOOD_TYPE_NAME_MAX_LENGTH = 20;
const DEFAULT_PAGE_SIZE = 7;
const MAX_PAGE_SIZE = 200;
const ID_PATTERN = /^[0-9a-f]{32}$/;
const DATE_PATTERN = /^\d{4}-\d{2}-\d{2}$/;
const ALLOWED_IMAGE_EXTENSIONS = new Set(['.jpg', '.jpeg', '.png']);

class FoodServiceError extends Error {
  constructor(message, statusCode = 400, code = 'FOOD_ERROR') {
    super(message);
    this.name = 'FoodServiceError';
    this.statusCode = statusCode;
    this.code = code;
  }
}

function padTwoDigits(value) {
  return String(value).padStart(2, '0');
}

function formatYearMonth(date) {
  return `${date.getFullYear()}-${padTwoDigits(date.getMonth() + 1)}`;
}

function isValidDateString(value) {
  if (typeof value !== 'string' || !DATE_PATTERN.test(value)) {
    return false;
  }

  const [year, month, day] = value.split('-').map(Number);
  const date = new Date(year, month - 1, day);
  return (
    date.getFullYear() === year &&
    date.getMonth() === month - 1 &&
    date.getDate() === day
  );
}

function parsePageValue(value, defaultValue, maxValue) {
  const parsed = Number.parseInt(value, 10);
  if (!Number.isInteger(parsed) || parsed < 0) {
    return defaultValue;
  }

  return maxValue ? Math.min(parsed, maxValue) : parsed;
}

function parsePaging(query) {
  const pageIndex = parsePageValue(query?.pageIndex, 0);
  const pageSize = parsePageValue(
    query?.pageSize,
    DEFAULT_PAGE_SIZE,
    MAX_PAGE_SIZE
  );
  return {
    pageIndex,
    pageSize: pageSize > 0 ? pageSize : DEFAULT_PAGE_SIZE,
  };
}

function paginate(list, pageIndex, pageSize) {
  const total = list.length;
  const start = pageIndex * pageSize;
  return {
    list: list.slice(start, start + pageSize),
    total,
  };
}

function readString(value) {
  return typeof value === 'string' ? value : '';
}

class FoodService {
  constructor(options) {
    const { dataDir } = options;

    this.dataDir = dataDir;
    this.foodImageDir = path.join(this.dataDir, 'static', 'foodimg');
    this.uploadTempDir = path.join(this.dataDir, '.tmp', 'food-uploads');
    this.foodsFilePath = path.join(this.dataDir, 'foods.json');
    this.foodTypesFilePath = path.join(this.dataDir, 'foodTypes.json');

    fs.mkdirSync(this.foodImageDir, { recursive: true });
    fs.mkdirSync(this.uploadTempDir, { recursive: true });
    this.clearUploadTempDir();

    this.foods = this.loadFoods();
    this.foodTypes = this.loadFoodTypes();
  }

  ensureDataFile(filePath) {
    fs.mkdirSync(this.dataDir, { recursive: true });
    if (!fs.existsSync(filePath)) {
      fs.writeFileSync(filePath, '[]\n', 'utf8');
    }
  }

  clearUploadTempDir() {
    let entries = [];
    try {
      entries = fs.readdirSync(this.uploadTempDir);
    } catch (error) {
      return;
    }

    for (const entry of entries) {
      try {
        fs.unlinkSync(path.join(this.uploadTempDir, entry));
      } catch (error) {
        // A stale file left behind is harmless; ignore cleanup failures.
      }
    }
  }

  generateUniqueId(existingIds) {
    let id = crypto.randomBytes(16).toString('hex');
    while (existingIds.has(id)) {
      id = crypto.randomBytes(16).toString('hex');
    }
    return id;
  }

  normalizeFoodItems(items) {
    if (!Array.isArray(items)) {
      return [];
    }

    const usedIds = new Set();

    return items
      .filter((item) => item && typeof item === 'object')
      .map((item) => {
        let id =
          typeof item.id === 'string' && ID_PATTERN.test(item.id) ? item.id : '';
        if (!id || usedIds.has(id)) {
          id = this.generateUniqueId(usedIds);
        }
        usedIds.add(id);

        return {
          id,
          typeId: readString(item.typeId),
          name: readString(item.name),
          coverImageUrl: readString(item.coverImageUrl),
          coverBinUrl: readString(item.coverBinUrl),
          coverBinSmallUrl: readString(item.coverBinSmallUrl),
        };
      });
  }

  normalizeFoods(foods) {
    if (!Array.isArray(foods)) {
      return [];
    }

    const usedIds = new Set();
    const normalized = [];

    for (const food of foods) {
      if (!food || typeof food !== 'object') {
        continue;
      }

      const date = readString(food.date).trim();
      if (!isValidDateString(date)) {
        continue;
      }

      let id =
        typeof food.id === 'string' && ID_PATTERN.test(food.id) ? food.id : '';
      if (!id || usedIds.has(id)) {
        id = this.generateUniqueId(usedIds);
      }
      usedIds.add(id);

      const createDate = parseDateTime(food.createTime) || new Date();
      const updateDate = parseDateTime(food.updateTime) || createDate;
      const createTime = formatDateTime(createDate);
      const updateTime = formatDateTime(updateDate);

      normalized.push({
        id,
        date,
        morningFoods: this.normalizeFoodItems(food.morningFoods),
        afternoonFoods: this.normalizeFoodItems(food.afternoonFoods),
        createTime,
        updateTime,
        createUser: readString(food.createUser),
        updateUser: readString(food.updateUser),
        isDel: food.isDel === 1 || food.isDel === true ? 1 : 0,
      });
    }

    return normalized;
  }

  normalizeFoodTypes(foodTypes) {
    if (!Array.isArray(foodTypes)) {
      return [];
    }

    const usedIds = new Set();
    const normalized = [];

    for (const foodType of foodTypes) {
      if (!foodType || typeof foodType !== 'object') {
        continue;
      }

      const name = readString(foodType.name).trim();
      if (!name || Array.from(name).length > FOOD_TYPE_NAME_MAX_LENGTH) {
        continue;
      }

      let id =
        typeof foodType.id === 'string' && ID_PATTERN.test(foodType.id)
          ? foodType.id
          : '';
      if (!id || usedIds.has(id)) {
        id = this.generateUniqueId(usedIds);
      }
      usedIds.add(id);

      const createDate = parseDateTime(foodType.createTime) || new Date();
      const updateDate = parseDateTime(foodType.updateTime) || createDate;

      normalized.push({
        id,
        name,
        coverImageUrl: readString(foodType.coverImageUrl),
        coverBinUrl: readString(foodType.coverBinUrl),
        coverBinSmallUrl: readString(foodType.coverBinSmallUrl),
        createTime: formatDateTime(createDate),
        updateTime: formatDateTime(updateDate),
        createUser: readString(foodType.createUser),
        updateUser: readString(foodType.updateUser),
        isDel: foodType.isDel === 1 || foodType.isDel === true ? 1 : 0,
      });
    }

    return normalized;
  }

  loadJsonArray(filePath, label) {
    this.ensureDataFile(filePath);

    let parsed;
    try {
      parsed = JSON.parse(fs.readFileSync(filePath, 'utf8'));
    } catch (error) {
      // eslint-disable-next-line no-console
      console.error(`load ${label} error:`, error.message);
      return [];
    }

    if (!Array.isArray(parsed)) {
      // eslint-disable-next-line no-console
      console.error(`load ${label} error: ${path.basename(filePath)} must contain an array`);
      return [];
    }

    return parsed;
  }

  loadFoods() {
    const parsed = this.loadJsonArray(this.foodsFilePath, 'foods');
    const foods = this.normalizeFoods(parsed);

    if (JSON.stringify(foods) !== JSON.stringify(parsed)) {
      this.saveFoods(foods);
    }

    return foods;
  }

  loadFoodTypes() {
    const parsed = this.loadJsonArray(this.foodTypesFilePath, 'foodTypes');
    const foodTypes = this.normalizeFoodTypes(parsed);

    if (JSON.stringify(foodTypes) !== JSON.stringify(parsed)) {
      this.saveFoodTypes(foodTypes);
    }

    return foodTypes;
  }

  saveFoods(foods = this.foods) {
    fs.writeFileSync(
      this.foodsFilePath,
      `${JSON.stringify(foods, null, 2)}\n`,
      'utf8'
    );
  }

  saveFoodTypes(foodTypes = this.foodTypes) {
    fs.writeFileSync(
      this.foodTypesFilePath,
      `${JSON.stringify(foodTypes, null, 2)}\n`,
      'utf8'
    );
  }

  toPublicFoodItem(item) {
    return {
      id: item.id,
      typeId: item.typeId,
      name: item.name,
      coverImageUrl: item.coverImageUrl,
      coverBinUrl: item.coverBinUrl,
      coverBinSmallUrl: item.coverBinSmallUrl,
    };
  }

  toPublicFood(food) {
    return {
      id: food.id,
      date: food.date,
      morningFoods: food.morningFoods.map((item) => this.toPublicFoodItem(item)),
      afternoonFoods: food.afternoonFoods.map((item) => this.toPublicFoodItem(item)),
      createTime: food.createTime,
      updateTime: food.updateTime,
    };
  }

  toPublicFoodType(foodType) {
    return {
      id: foodType.id,
      name: foodType.name,
      coverImageUrl: foodType.coverImageUrl,
      coverBinUrl: foodType.coverBinUrl,
      coverBinSmallUrl: foodType.coverBinSmallUrl,
      createTime: foodType.createTime,
      updateTime: foodType.updateTime,
    };
  }

  getFoods(query) {
    const { pageIndex, pageSize } = parsePaging(query);
    const activeFoods = this.foods
      .filter((food) => food.isDel === 0)
      .sort((left, right) => {
        const dateCompare = right.date.localeCompare(left.date);
        if (dateCompare !== 0) {
          return dateCompare;
        }
        return right.createTime.localeCompare(left.createTime);
      });

    const page = paginate(activeFoods, pageIndex, pageSize);
    return {
      list: page.list.map((food) => this.toPublicFood(food)),
      total: page.total,
    };
  }

  getFoodTypes(query) {
    const { pageIndex, pageSize } = parsePaging(query);
    const activeFoodTypes = this.foodTypes
      .map((foodType, index) => ({ foodType, index }))
      .filter((entry) => entry.foodType.isDel === 0)
      .sort((left, right) => {
        const timeCompare = right.foodType.createTime.localeCompare(
          left.foodType.createTime
        );
        if (timeCompare !== 0) {
          return timeCompare;
        }
        // Same-second creations keep the newest record first.
        return right.index - left.index;
      })
      .map((entry) => entry.foodType);

    const page = paginate(activeFoodTypes, pageIndex, pageSize);
    return {
      list: page.list.map((foodType) => this.toPublicFoodType(foodType)),
      total: page.total,
    };
  }

  normalizeTypeIdList(value, fieldName) {
    if (value === undefined || value === null) {
      return [];
    }

    if (!Array.isArray(value)) {
      throw new FoodServiceError(`${fieldName} 必须是辅食类型 id 数组`);
    }

    return value.map((typeId) => {
      if (typeof typeId !== 'string' || !typeId.trim()) {
        throw new FoodServiceError(`${fieldName} 包含无效的辅食类型 id`);
      }
      return typeId.trim();
    });
  }

  buildFoodItem(type, id) {
    return {
      id,
      typeId: type.id,
      name: type.name,
      coverImageUrl: type.coverImageUrl,
      coverBinUrl: type.coverBinUrl,
      coverBinSmallUrl: type.coverBinSmallUrl,
    };
  }

  resolveFoodTypes(typeIds, fieldName, usedIds = new Set()) {
    const typeMap = new Map(
      this.foodTypes
        .filter((foodType) => foodType.isDel === 0)
        .map((foodType) => [foodType.id, foodType])
    );

    return typeIds.map((typeId) => {
      const type = typeMap.get(typeId);
      if (!type) {
        throw new FoodServiceError(
          `${fieldName} 引用的辅食类型不存在：${typeId}`
        );
      }

      const id = this.generateUniqueId(usedIds);
      usedIds.add(id);
      return this.buildFoodItem(type, id);
    });
  }

  parseFoodInput(input) {
    const date = readString(input?.date).trim();
    if (!isValidDateString(date)) {
      throw new FoodServiceError('date 格式应为 YYYY-MM-DD');
    }

    return {
      date,
      morningTypeIds: this.normalizeTypeIdList(input?.morningFoods, 'morningFoods'),
      afternoonTypeIds: this.normalizeTypeIdList(
        input?.afternoonFoods,
        'afternoonFoods'
      ),
    };
  }

  addFood(input, userId) {
    const { date, morningTypeIds, afternoonTypeIds } = this.parseFoodInput(input);

    if (this.foods.some((food) => food.isDel === 0 && food.date === date)) {
      throw new FoodServiceError('该日期已有辅食记录，请使用修改接口', 409);
    }

    const usedItemIds = new Set();
    const morningFoods = this.resolveFoodTypes(
      morningTypeIds,
      'morningFoods',
      usedItemIds
    );
    const afternoonFoods = this.resolveFoodTypes(
      afternoonTypeIds,
      'afternoonFoods',
      usedItemIds
    );

    const now = formatDateTime(new Date());
    const usedIds = new Set(this.foods.map((food) => food.id));
    const food = {
      id: this.generateUniqueId(usedIds),
      date,
      morningFoods,
      afternoonFoods,
      createTime: now,
      updateTime: now,
      createUser: readString(userId),
      updateUser: readString(userId),
      isDel: 0,
    };

    this.foods.push(food);
    try {
      this.saveFoods();
    } catch (error) {
      this.foods.pop();
      throw error;
    }

    return this.toPublicFood(food);
  }

  findActiveFood(id) {
    return this.foods.find((food) => food.id === id && food.isDel === 0);
  }

  updateFood(id, input, userId) {
    const food = this.findActiveFood(id);
    if (!food) {
      throw new FoodServiceError('辅食记录不存在', 404);
    }

    const { date, morningTypeIds, afternoonTypeIds } = this.parseFoodInput(input);

    if (
      this.foods.some(
        (item) => item.isDel === 0 && item.date === date && item.id !== food.id
      )
    ) {
      throw new FoodServiceError('该日期已有其他辅食记录', 409);
    }

    const existingByTypeId = new Map();
    for (const item of [...food.morningFoods, ...food.afternoonFoods]) {
      if (item.typeId && !existingByTypeId.has(item.typeId)) {
        existingByTypeId.set(item.typeId, item);
      }
    }

    const typeMap = new Map(
      this.foodTypes
        .filter((foodType) => foodType.isDel === 0)
        .map((foodType) => [foodType.id, foodType])
    );
    const usedItemIds = new Set();

    const buildItems = (typeIds, fieldName) =>
      typeIds.map((typeId) => {
        const type = typeMap.get(typeId);
        const existing = existingByTypeId.get(typeId);
        if (!type && !existing) {
          throw new FoodServiceError(
            `${fieldName} 引用的辅食类型不存在：${typeId}`
          );
        }

        let itemId = existing && existing.id ? existing.id : '';
        if (!itemId || usedItemIds.has(itemId)) {
          itemId = this.generateUniqueId(usedItemIds);
        }
        usedItemIds.add(itemId);

        // A type may be soft-deleted after a day was recorded. Keep the food
        // snapshot so the rest of that day can still be edited.
        if (!type) {
          return {
            ...this.toPublicFoodItem(existing),
            id: itemId,
          };
        }

        return this.buildFoodItem(type, itemId);
      });

    const morningFoods = buildItems(morningTypeIds, 'morningFoods');
    const afternoonFoods = buildItems(afternoonTypeIds, 'afternoonFoods');

    const nextFood = {
      ...food,
      date,
      morningFoods,
      afternoonFoods,
      updateTime: formatDateTime(new Date()),
      updateUser: readString(userId),
    };

    const index = this.foods.indexOf(food);
    this.foods[index] = nextFood;
    try {
      this.saveFoods();
    } catch (error) {
      this.foods[index] = food;
      throw error;
    }

    return this.toPublicFood(nextFood);
  }

  deleteFood(id, userId) {
    const food = this.findActiveFood(id);
    if (!food) {
      throw new FoodServiceError('辅食记录不存在', 404);
    }

    const previous = {
      isDel: food.isDel,
      updateTime: food.updateTime,
      updateUser: food.updateUser,
    };
    food.isDel = 1;
    food.updateTime = formatDateTime(new Date());
    food.updateUser = readString(userId);

    try {
      this.saveFoods();
    } catch (error) {
      Object.assign(food, previous);
      throw error;
    }

    return null;
  }

  normalizeFoodTypeName(value) {
    const name = typeof value === 'string' ? value.trim() : '';
    const length = Array.from(name).length;
    if (length < 1 || length > FOOD_TYPE_NAME_MAX_LENGTH) {
      throw new FoodServiceError(
        `name 长度必须为 1-${FOOD_TYPE_NAME_MAX_LENGTH} 个字符`
      );
    }
    return name;
  }

  detectImageExtension(file) {
    const originalExtension = path
      .extname(file.originalname || '')
      .toLowerCase();
    if (!ALLOWED_IMAGE_EXTENSIONS.has(originalExtension)) {
      throw new FoodServiceError('coverImageFile 仅支持 jpg、jpeg、png');
    }

    let header;
    try {
      const descriptor = fs.openSync(file.path, 'r');
      const buffer = Buffer.alloc(8);
      fs.readSync(descriptor, buffer, 0, buffer.length, 0);
      fs.closeSync(descriptor);
      header = buffer;
    } catch (error) {
      throw new FoodServiceError('封面图读取失败');
    }

    const isPng =
      header[0] === 0x89 &&
      header[1] === 0x50 &&
      header[2] === 0x4e &&
      header[3] === 0x47 &&
      header[4] === 0x0d &&
      header[5] === 0x0a &&
      header[6] === 0x1a &&
      header[7] === 0x0a;
    if (isPng) {
      return '.png';
    }

    const isJpeg = header[0] === 0xff && header[1] === 0xd8 && header[2] === 0xff;
    if (isJpeg) {
      return '.jpg';
    }

    throw new FoodServiceError('coverImageFile 内容不是有效的 jpg 或 png 图片');
  }

  generateFileBase(targetDir) {
    for (let attempt = 0; attempt < 10; attempt += 1) {
      const base = crypto.randomBytes(16).toString('hex');
      const candidates = [
        path.join(targetDir, `${base}.png`),
        path.join(targetDir, `${base}.jpg`),
        path.join(targetDir, `${base}.bin`),
        path.join(targetDir, `${base}.small.bin`),
      ];
      if (!candidates.some((candidate) => fs.existsSync(candidate))) {
        return base;
      }
    }

    throw new Error('无法生成唯一的图片文件名');
  }

  moveFile(sourcePath, targetPath) {
    fs.renameSync(sourcePath, targetPath);
    return targetPath;
  }

  deleteFiles(filePaths) {
    for (const filePath of filePaths) {
      try {
        fs.unlinkSync(filePath);
      } catch (error) {
        // Ignore cleanup failures; the original error is more useful.
      }
    }
  }

  addFoodType(options) {
    const { name, imageFile, binFile, smallBinFile, userId } = options;
    const normalizedName = this.normalizeFoodTypeName(name);

    if (!imageFile || !binFile || !smallBinFile) {
      throw new FoodServiceError(
        'coverImageFile、coverBinFile、coverBinSmallFile 均为必填'
      );
    }

    for (const file of [imageFile, binFile, smallBinFile]) {
      if (!file.path || !fs.existsSync(file.path)) {
        throw new FoodServiceError('上传文件不完整，请重试');
      }
    }

    const imageExtension = this.detectImageExtension(imageFile);
    const month = formatYearMonth(new Date());
    const targetDir = path.join(this.foodImageDir, month);
    fs.mkdirSync(targetDir, { recursive: true });

    const base = this.generateFileBase(targetDir);
    const imageName = `${base}${imageExtension}`;
    const binName = `${base}.bin`;
    const smallBinName = `${base}.small.bin`;
    const imagePath = path.join(targetDir, imageName);
    const binPath = path.join(targetDir, binName);
    const smallBinPath = path.join(targetDir, smallBinName);
    const movedFiles = [];

    try {
      movedFiles.push(this.moveFile(imageFile.path, imagePath));
      movedFiles.push(this.moveFile(binFile.path, binPath));
      movedFiles.push(this.moveFile(smallBinFile.path, smallBinPath));

      const now = formatDateTime(new Date());
      const usedIds = new Set(this.foodTypes.map((foodType) => foodType.id));
      const foodType = {
        id: this.generateUniqueId(usedIds),
        name: normalizedName,
        coverImageUrl: `static/foodimg/${month}/${imageName}`,
        coverBinUrl: `static/foodimg/${month}/${binName}`,
        coverBinSmallUrl: `static/foodimg/${month}/${smallBinName}`,
        createTime: now,
        updateTime: now,
        createUser: readString(userId),
        updateUser: readString(userId),
        isDel: 0,
      };

      this.foodTypes.push(foodType);
      try {
        this.saveFoodTypes();
      } catch (error) {
        this.foodTypes.pop();
        throw error;
      }

      return this.toPublicFoodType(foodType);
    } catch (error) {
      this.deleteFiles(movedFiles);
      throw error;
    }
  }

  findActiveFoodType(id) {
    return this.foodTypes.find(
      (foodType) => foodType.id === id && foodType.isDel === 0
    );
  }

  deleteFoodType(id, userId) {
    const foodType = this.findActiveFoodType(id);
    if (!foodType) {
      throw new FoodServiceError('辅食类型不存在', 404);
    }

    const previous = {
      isDel: foodType.isDel,
      updateTime: foodType.updateTime,
      updateUser: foodType.updateUser,
    };
    foodType.isDel = 1;
    foodType.updateTime = formatDateTime(new Date());
    foodType.updateUser = readString(userId);

    try {
      this.saveFoodTypes();
    } catch (error) {
      Object.assign(foodType, previous);
      throw error;
    }

    return null;
  }

  removeUploadedFiles(files) {
    if (!files) {
      return;
    }

    const fileList = Array.isArray(files)
      ? files
      : Object.values(files).flat();

    for (const file of fileList) {
      if (!file || typeof file.path !== 'string' || !file.path) {
        continue;
      }
      try {
        fs.unlinkSync(file.path);
      } catch (error) {
        // The file may already have been moved into place.
      }
    }
  }
}

module.exports = {
  FoodService,
  FoodServiceError,
  DEFAULT_PAGE_SIZE,
  MAX_PAGE_SIZE,
  isValidDateString,
  parsePaging,
};
