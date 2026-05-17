const path = require('path');

function parseNumber(value, defaultValue) {
  const parsed = Number(value);
  return Number.isFinite(parsed) && parsed > 0 ? parsed : defaultValue;
}

function loadConfig() {
  return {
    port: parseNumber(process.env.PORT, 3000),
    udpPort: parseNumber(process.env.UDP_PORT, 6113),
    dataDir: process.env.DATA_DIR
      ? path.resolve(process.env.DATA_DIR)
      : path.join(__dirname, '../../data'),
    weather: {
      apiBaseUrl:
        process.env.WEATHER_API_BASE_URL || '',
      weatherApiKey: process.env.WEATHER_API_KEY || '',
      locationId: process.env.WEATHER_LOCATION_ID || process.env.WEATHER_LOCATION || '',
      refreshIntervalMs: parseNumber(process.env.WEATHER_REFRESH_INTERVAL_MS, 60 * 60 * 1000),
      requestTimeoutMs: parseNumber(process.env.WEATHER_REQUEST_TIMEOUT_MS, 5000),
    },
  };
}

module.exports = {
  loadConfig,
};