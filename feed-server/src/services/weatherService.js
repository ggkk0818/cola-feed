const http = require('http');
const https = require('https');
const { URL } = require('url');

class WeatherService {
  constructor(options = {}) {
    const {
      apiBaseUrl,
      weatherApiKey = '',
      locationId = '',
      refreshIntervalMs = 60 * 60 * 1000,
      requestTimeoutMs = 5000,
    } = options;

    this.apiBaseUrl = (apiBaseUrl || '').replace(/\/+$/, '');
    this.weatherApiKey = weatherApiKey;
    this.locationId = locationId;
    this.refreshIntervalMs = refreshIntervalMs;
    this.requestTimeoutMs = requestTimeoutMs;

    this.cachedNow = null;
    this.cachedThreeDay = null;
    this.cacheUpdatedAt = null;
    this.refreshTimer = null;
    this.refreshQueue = Promise.resolve();
  }

  async start() {
    if (this.refreshTimer) {
      return;
    }

    await this.safeRefresh();

    this.refreshTimer = setInterval(() => {
      this.safeRefresh();
    }, this.refreshIntervalMs);
  }

  async safeRefresh() {
    try {
      await this.refreshWeatherData();
    } catch (error) {
      // eslint-disable-next-line no-console
      console.error('weather refresh error:', error.message);
    }
  }

  hasRequiredConfig() {
    return Boolean(this.apiBaseUrl && this.locationId && this.weatherApiKey);
  }

  refreshWeatherData() {
    this.refreshQueue = this.refreshQueue
      .catch(() => {})
      .then(async () => {
        if (!this.hasRequiredConfig()) {
          this.cachedNow = null;
          this.cachedThreeDay = null;
          this.cacheUpdatedAt = null;
          return;
        }

        const [nowWeather, threeDayWeather] = await Promise.all([
          this.fetchNowWeather(),
          this.fetchThreeDayWeather(),
        ]);

        this.cachedNow = nowWeather;
        this.cachedThreeDay = threeDayWeather;
        this.cacheUpdatedAt = new Date().toISOString();
      });

    return this.refreshQueue;
  }

  getWeatherData() {
    if (!this.cachedNow || !this.cachedThreeDay) {
      return null;
    }

    return {
      now: this.cachedNow,
      daily3d: this.cachedThreeDay,
      updateTime: this.cacheUpdatedAt,
    };
  }

  fetchNowWeather() {
    return this.getJson('/v7/weather/now', {
      location: this.locationId,
    });
  }

  fetchThreeDayWeather() {
    return this.getJson('/v7/weather/3d', {
      location: this.locationId,
    });
  }

  getJson(pathname, query = {}) {
    const baseUrl = this.apiBaseUrl.endsWith('/') ? this.apiBaseUrl : `${this.apiBaseUrl}/`;
    const url = new URL(pathname.replace(/^\//, ''), baseUrl);

    Object.entries(query).forEach(([key, value]) => {
      if (value !== undefined && value !== null && value !== '') {
        url.searchParams.set(key, String(value));
      }
    });

    url.searchParams.set('key', this.weatherApiKey);

    return this.requestJson(url);
  }

  requestJson(url) {
    const client = url.protocol === 'http:' ? http : https;

    return new Promise((resolve, reject) => {
      const req = client.request(
        {
          protocol: url.protocol,
          hostname: url.hostname,
          port: url.port || undefined,
          path: `${url.pathname}${url.search}`,
          method: 'GET',
          headers: {
            Accept: 'application/json',
          },
          timeout: this.requestTimeoutMs,
        },
        (res) => {
          let body = '';

          res.setEncoding('utf8');
          res.on('data', (chunk) => {
            body += chunk;
          });

          res.on('end', () => {
            if (!res.statusCode || res.statusCode < 200 || res.statusCode >= 300) {
              reject(new Error(`weather api status ${res.statusCode}`));
              return;
            }

            let parsed;
            try {
              console.log('weather api response:', url, body);
              parsed = JSON.parse(body);
            } catch (error) {
              reject(new Error('weather api invalid json'));
              return;
            }

            if (parsed.code !== '200') {
              reject(new Error(`weather api code ${parsed.code || 'unknown'}`));
              return;
            }

            resolve(parsed);
          });
        }
      );

      req.on('timeout', () => {
        req.destroy(new Error('weather api timeout'));
      });

      req.on('error', (error) => {
        reject(error);
      });

      req.end();
    });
  }
}

module.exports = {
  WeatherService,
};