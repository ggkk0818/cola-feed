const express = require('express');
const path = require('path');
const { FeedService } = require('./src/services/feedService');
const { ClientService } = require('./src/services/clientService');
const { WeatherService } = require('./src/services/weatherService');
const { UserService } = require('./src/services/userService');
const { AuthService } = require('./src/services/authService');
const { FoodService } = require('./src/services/foodService');
const { loadConfig } = require('./src/config/appConfig');
const { createAuthRouter } = require('./src/routes/authRoutes');
const { createApiRouter } = require('./src/routes/apiRoutes');
const { createFoodRouter } = require('./src/routes/foodRoutes');

const app = express();
const config = loadConfig();
const { port, udpPort, dataDir, weather } = config;

const feedService = new FeedService({
  dataDir,
  maxRecords: 12,
});

const weatherService = new WeatherService(weather);

const clientService = new ClientService({
  dataDir,
  feedService,
  weatherService,
  udpPort,
});

const userService = new UserService({ dataDir });
const authService = new AuthService({ userService });
const foodService = new FoodService({ dataDir });

if (userService.users.length === 0) {
  // eslint-disable-next-line no-console
  console.warn(
    'users.json has no users; copy data/users.json into DATA_DIR to enable login.'
  );
}

app.use(express.json());
app.use('/api/auth', createAuthRouter({ authService }));
app.use('/api', createApiRouter({ feedService, clientService, authService }));
app.use('/api', createFoodRouter({ foodService, authService }));

// 辅食图片与 bin 文件保存在数据目录中，容器重建后仍可通过数据卷保留。
app.use(
  '/static/foodimg',
  express.static(foodService.foodImageDir, {
    maxAge: '30d',
    immutable: true,
    fallthrough: false,
  })
);

// 登录页使用 jsencrypt 在浏览器端完成 RSA-OAEP 加密。
app.get('/vendor/jsencrypt.min.js', (req, res) => {
  res.sendFile(require.resolve('jsencrypt'), {
    headers: {
      'Cache-Control': 'public, max-age=86400',
    },
  });
});

app.use(express.static(path.join(__dirname, 'public')));

app.get('/login', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'login.html'));
});

app.use((req, res, next) => {
  if (req.path.startsWith('/api')) {
    next();
    return;
  }
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.use((error, req, res, next) => {
  if (res.headersSent) {
    next(error);
    return;
  }

  if (error && error.type === 'entity.parse.failed') {
    res.status(400).json({
      code: 400,
      data: null,
      message: '请求体不是有效的 JSON',
    });
    return;
  }

  if (error && error.type === 'entity.too.large') {
    res.status(413).json({
      code: 413,
      data: null,
      message: '请求体过大',
    });
    return;
  }

  next(error);
});

async function bootstrap() {
  await weatherService.start();
  clientService.start();

  app.listen(port, () => {
    // eslint-disable-next-line no-console
    console.log(`feed-server started on http://localhost:${port}`);
  });
}

bootstrap().catch((error) => {
  // eslint-disable-next-line no-console
  console.error('feed-server bootstrap error:', error.message);
  process.exit(1);
});
