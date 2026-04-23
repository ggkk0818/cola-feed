const express = require('express');
const path = require('path');
const { FeedService } = require('./src/services/feedService');
const { ClientService } = require('./src/services/clientService');
const { createApiRouter } = require('./src/routes/apiRoutes');

const app = express();
const port = process.env.PORT || 3000;
const udpPort = Number(process.env.UDP_PORT) || 6113;
const dataDir = process.env.DATA_DIR
  ? path.resolve(process.env.DATA_DIR)
  : path.join(__dirname, 'data');

const feedService = new FeedService({
  dataDir,
  maxRecords: 12,
});

const clientService = new ClientService({
  dataDir,
  feedService,
  udpPort,
});

clientService.start();

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

app.listen(port, () => {
  // eslint-disable-next-line no-console
  console.log(`feed-server started on http://localhost:${port}`);
});
