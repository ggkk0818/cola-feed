const express = require('express');
const path = require('path');
const { FeedService } = require('./src/services/feedService');
const { ClientService } = require('./src/services/clientService');
const { WeatherService } = require('./src/services/weatherService');
const { loadConfig } = require('./src/config/appConfig');
const { createApiRouter } = require('./src/routes/apiRoutes');

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

app.use(express.json());
app.use('/api', createApiRouter({ feedService, clientService }));
app.use(express.static(path.join(__dirname, 'public')));

app.use((req, res, next) => {
  if (req.path.startsWith('/api')) {
    next();
    return;
  }
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
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
