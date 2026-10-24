(function () {
  'use strict';

  if (!window.Vue || typeof window.Vue.createApp !== 'function') {
    document.body.innerHTML =
      '<div class="boot-screen">Vue 运行时加载失败，请刷新页面后重试。</div>';
    return;
  }

  const { createApp } = window.Vue;

  const AUTH_STORAGE_KEY = 'feedServerAuth';
  const ADMIN_ROLES = [1, 3, 5];
  const FOOD_ROLES = [1, 4, 5];
  const ROLE_LABELS = {
    1: '管理员',
    2: '喂奶用户',
    3: '喂奶管理员',
    4: '辅食用户',
    5: '辅食管理员',
  };

  const FOOD_ICON_PLACEHOLDER =
    "data:image/svg+xml;charset=utf-8,%3Csvg xmlns='http://www.w3.org/2000/svg' width='48' height='48' viewBox='0 0 48 48'%3E%3Crect width='48' height='48' rx='10' fill='%23e5e7eb'/%3E%3Cpath d='M13 24h22v3H13z' fill='%239ca3af'/%3E%3Cpath d='M15 29h18a9 9 0 0 1-18 0z' fill='%239ca3af'/%3E%3C/svg%3E";

  function readStoredAuth() {
    try {
      const raw = window.localStorage.getItem(AUTH_STORAGE_KEY);
      if (!raw) {
        return null;
      }

      const parsed = JSON.parse(raw);
      if (!parsed || typeof parsed.token !== 'string' || !parsed.token) {
        return null;
      }

      return parsed;
    } catch (error) {
      return null;
    }
  }

  function clearStoredAuth() {
    try {
      window.localStorage.removeItem(AUTH_STORAGE_KEY);
    } catch (error) {
      // Ignore storage failures (private mode, disabled storage, ...).
    }
  }

  function buildLoginUrl() {
    const next = `${window.location.pathname}${window.location.search}`;
    if (!next || next === '/' || next.indexOf('/login') === 0) {
      return '/login.html';
    }

    return `/login.html?next=${encodeURIComponent(next)}`;
  }

  function redirectToLogin() {
    window.location.replace(buildLoginUrl());
  }

  function padTwoDigits(value) {
    return String(value).padStart(2, '0');
  }

  function parseDateTime(value) {
    if (typeof value !== 'string') {
      return null;
    }

    const normalized = value.trim().replace(/-/g, '/');
    const parsed = new Date(normalized);
    if (!Number.isNaN(parsed.getTime())) {
      return parsed;
    }

    const fallback = new Date(value);
    return Number.isNaN(fallback.getTime()) ? null : fallback;
  }

  function formatDateTimeFromDate(date) {
    return (
      `${date.getFullYear()}-${padTwoDigits(date.getMonth() + 1)}-` +
      `${padTwoDigits(date.getDate())} ${padTwoDigits(date.getHours())}:` +
      `${padTwoDigits(date.getMinutes())}:${padTwoDigits(date.getSeconds())}`
    );
  }

  function formatDateTime(rawValue) {
    if (!rawValue) {
      return '-';
    }

    const parsed = parseDateTime(rawValue);
    return parsed ? formatDateTimeFromDate(parsed) : '-';
  }

  function formatDuration(seconds) {
    const safe = Number.isFinite(seconds) ? Math.max(0, seconds) : 0;
    const minutes = Math.floor(safe / 60);
    const remain = safe % 60;
    return `${minutes}分${remain}秒`;
  }

  function formatElapsedSinceLastFeed(seconds) {
    const safe = Number.isFinite(seconds) ? Math.max(0, seconds) : 0;

    if (safe < 5 * 60) {
      return '刚刚';
    }

    if (safe < 60 * 60) {
      return `${Math.floor(safe / 60)}分钟`;
    }

    const hours = Math.floor(safe / (60 * 60));
    const minutes = Math.floor((safe % (60 * 60)) / 60);
    return minutes === 0 ? `${hours}小时` : `${hours}小时${minutes}分钟`;
  }

  function getLatestFeedEndMs(records, lastFeed) {
    let latestEndMs = null;

    if (Array.isArray(records)) {
      for (const record of records) {
        const parsedEnd = parseDateTime(record?.endTime);
        if (!parsedEnd) {
          continue;
        }

        const endMs = parsedEnd.getTime();
        if (
          Number.isFinite(endMs) &&
          (latestEndMs === null || endMs > latestEndMs)
        ) {
          latestEndMs = endMs;
        }
      }
    }

    if (latestEndMs !== null) {
      return latestEndMs;
    }

    const parsedLastEnd = parseDateTime(lastFeed?.endTime);
    return parsedLastEnd ? parsedLastEnd.getTime() : null;
  }

  function shortFoodName(name) {
    const characters = Array.from(String(name || ''));
    return characters.slice(0, 2).join('') || '?';
  }

  function resolveAssetUrl(value) {
    if (!value || typeof value !== 'string') {
      return FOOD_ICON_PLACEHOLDER;
    }

    const trimmed = value.trim();
    if (!trimmed) {
      return FOOD_ICON_PLACEHOLDER;
    }

    if (/^(?:https?:)?\/\//i.test(trimmed) || /^data:/i.test(trimmed)) {
      return trimmed;
    }

    return trimmed.startsWith('/') ? trimmed : `/${trimmed}`;
  }

  createApp({
    data() {
      return {
        auth: null,
        user: null,
        bootError: '',
        logoutBusy: false,
        activeTab: 'overview',
        nowMs: Date.now(),
        records: [],
        lastFeedEndMs: null,
        overviewLoading: false,
        overviewPermissionDenied: false,
        clients: [],
        clientsLoading: false,
        clientsPermissionDenied: false,
        foods: [],
        foodTotal: 0,
        foodPageIndex: 0,
        foodPageSize: 7,
        foodsLoading: false,
        foodsError: '',
        tooltip: {
          visible: false,
          name: '',
          url: '',
          x: 0,
          y: 0,
          placement: 'above',
        },
        refreshing: false,
        elapsedTimer: null,
        refreshTimer: null,
      };
    },

    computed: {
      canViewAdminData() {
        return (
          !!this.user &&
          Array.isArray(this.user.roles) &&
          this.user.roles.some((role) => ADMIN_ROLES.includes(role))
        );
      },

      canViewFoods() {
        return (
          !!this.user &&
          Array.isArray(this.user.roles) &&
          this.user.roles.some((role) => FOOD_ROLES.includes(role))
        );
      },

      roleTitle() {
        if (!this.user || !Array.isArray(this.user.roles)) {
          return '';
        }

        const labels = this.user.roles
          .map((role) => ROLE_LABELS[role])
          .filter(Boolean);
        return labels.length ? `角色：${labels.join('、')}` : '';
      },

      elapsedText() {
        if (this.overviewPermissionDenied) {
          return '无权限查看';
        }

        if (!this.lastFeedEndMs || !Number.isFinite(this.lastFeedEndMs)) {
          return '暂无记录';
        }

        const seconds = Math.max(
          0,
          Math.floor((this.nowMs - this.lastFeedEndMs) / 1000)
        );
        return formatElapsedSinceLastFeed(seconds);
      },

      overviewRecords() {
        return [...this.records].reverse();
      },

      foodPageCount() {
        return Math.max(1, Math.ceil(this.foodTotal / this.foodPageSize));
      },
    },

    mounted() {
      window.addEventListener('scroll', this.hideFoodTooltip, true);
      window.addEventListener('resize', this.hideFoodTooltip);
      this.init();
    },

    beforeUnmount() {
      window.removeEventListener('scroll', this.hideFoodTooltip, true);
      window.removeEventListener('resize', this.hideFoodTooltip);
      this.stopTimers();
    },

    methods: {
      formatDateTime,
      formatDuration,
      shortFoodName,
      resolveAsset: resolveAssetUrl,

      async init() {
        this.bootError = '';
        this.stopTimers();
        this.auth = readStoredAuth();

        if (!this.auth) {
          redirectToLogin();
          return;
        }

        try {
          await this.loadCurrentUser();
        } catch (error) {
          if (error && error.message === 'unauthorized') {
            return;
          }
          this.bootError = '登录状态校验失败，请稍后重试。';
          return;
        }

        this.startTimers();
        this.switchTab('overview');
        this.refreshAll();
      },

      startTimers() {
        this.elapsedTimer = setInterval(() => {
          this.nowMs = Date.now();
        }, 1000);
        this.refreshTimer = setInterval(() => {
          this.refreshAll();
        }, 5000);
      },

      stopTimers() {
        if (this.elapsedTimer) {
          clearInterval(this.elapsedTimer);
          this.elapsedTimer = null;
        }
        if (this.refreshTimer) {
          clearInterval(this.refreshTimer);
          this.refreshTimer = null;
        }
      },

      async apiFetch(url, options = {}) {
        const headers = new Headers(options.headers || {});
        const token = this.auth && this.auth.token ? this.auth.token : '';
        if (token) {
          headers.set('Authorization', `Bearer ${token}`);
        }

        const response = await fetch(url, { ...options, headers });

        if (response.status === 401) {
          let payload = null;
          try {
            payload = await response.clone().json();
          } catch (error) {
            payload = null;
          }

          if (payload && payload.reason === 'FORBIDDEN') {
            const forbiddenError = new Error(
              payload.message || '无权限访问'
            );
            forbiddenError.forbidden = true;
            throw forbiddenError;
          }

          this.handleUnauthorized();
          throw new Error('unauthorized');
        }

        return response;
      },

      handleUnauthorized() {
        clearStoredAuth();
        this.auth = null;
        this.user = null;
        this.stopTimers();
        redirectToLogin();
      },

      async loadCurrentUser() {
        const response = await this.apiFetch('/api/auth/me', {
          cache: 'no-store',
        });
        if (!response.ok) {
          throw new Error(`me request failed: ${response.status}`);
        }

        const result = await response.json();
        const user = result?.data;
        if (!user || !Array.isArray(user.roles)) {
          throw new Error('invalid current user payload');
        }

        this.user = user;
      },

      async loadStatus() {
        this.overviewLoading = true;
        this.overviewPermissionDenied = false;

        try {
          const response = await this.apiFetch('/api/status', {
            cache: 'no-store',
          });
          if (!response.ok) {
            throw new Error(`status request failed: ${response.status}`);
          }

          const result = await response.json();
          this.records = Array.isArray(result?.data?.records)
            ? result.data.records
            : [];
          this.lastFeedEndMs = getLatestFeedEndMs(
            this.records,
            result?.data?.lastFeed || null
          );
        } catch (error) {
          if (error && error.forbidden) {
            this.records = [];
            this.lastFeedEndMs = null;
            this.overviewPermissionDenied = true;
          }
        } finally {
          this.overviewLoading = false;
        }
      },

      async loadClients() {
        this.clientsLoading = true;
        this.clientsPermissionDenied = false;

        try {
          const response = await this.apiFetch('/api/clients', {
            cache: 'no-store',
          });
          if (!response.ok) {
            throw new Error(`clients request failed: ${response.status}`);
          }

          const result = await response.json();
          this.clients = Array.isArray(result?.data?.clients)
            ? result.data.clients
            : [];
        } catch (error) {
          if (error && error.forbidden) {
            this.clients = [];
            this.clientsPermissionDenied = true;
          }
        } finally {
          this.clientsLoading = false;
        }
      },

      async refreshAll() {
        if (!this.user || this.refreshing) {
          return;
        }

        if (!this.canViewAdminData) {
          this.overviewPermissionDenied = true;
          this.clientsPermissionDenied = true;
          return;
        }

        this.refreshing = true;
        try {
          await Promise.allSettled([this.loadStatus(), this.loadClients()]);
        } finally {
          this.refreshing = false;
        }
      },

      async loadFoods() {
        if (!this.canViewFoods) {
          this.foodsError = '当前账号无权限查看辅食记录';
          return;
        }

        if (this.foodsLoading) {
          return;
        }

        this.foodsLoading = true;
        this.foodsError = '';
        this.hideFoodTooltip();

        try {
          const params = new URLSearchParams({
            pageIndex: String(this.foodPageIndex),
            pageSize: String(this.foodPageSize),
          });
          const response = await this.apiFetch(`/api/foods?${params.toString()}`, {
            cache: 'no-store',
          });
          if (!response.ok) {
            throw new Error(`foods request failed: ${response.status}`);
          }

          const result = await response.json();
          const data = result?.data || {};
          this.foods = Array.isArray(data.list) ? data.list : [];
          this.foodTotal = Number.isFinite(data.total)
            ? data.total
            : this.foods.length;

          if (this.foodPageIndex >= this.foodPageCount) {
            this.foodPageIndex = Math.max(0, this.foodPageCount - 1);
          }
        } catch (error) {
          if (error && error.forbidden) {
            this.foodsError = '当前账号无权限查看辅食记录';
          } else if (error && error.message !== 'unauthorized') {
            this.foodsError = '辅食记录加载失败，请稍后重试';
          }
          this.foods = [];
          this.foodTotal = 0;
        } finally {
          this.foodsLoading = false;
        }
      },

      changeFoodPage(delta) {
        const nextIndex = this.foodPageIndex + delta;
        if (nextIndex < 0 || nextIndex >= this.foodPageCount) {
          return;
        }

        this.foodPageIndex = nextIndex;
        this.loadFoods();
      },

      switchTab(tabName) {
        if (tabName === 'foods' && !this.canViewFoods) {
          return;
        }

        this.activeTab = tabName;
        this.hideFoodTooltip();

        if (tabName === 'foods') {
          this.loadFoods();
        }
      },

      showFoodTooltip(event, food) {
        const target = event && event.currentTarget;
        if (!target || !food) {
          return;
        }

        const rect = target.getBoundingClientRect();
        const padding = 16;
        const halfWidth = Math.min(
          150,
          Math.max(60, (window.innerWidth - padding * 2) / 2)
        );
        const minX = padding + halfWidth;
        const maxX = window.innerWidth - padding - halfWidth;
        const center = rect.left + rect.width / 2;
        const x = Math.min(Math.max(center, minX), Math.max(minX, maxX));
        const placeBelow = rect.top < 90;

        this.tooltip = {
          visible: true,
          name: food.name || '',
          url: resolveAssetUrl(food.coverImageUrl),
          x,
          y: placeBelow ? rect.bottom + 10 : rect.top - 10,
          placement: placeBelow ? 'below' : 'above',
        };
      },

      hideFoodTooltip() {
        if (this.tooltip.visible) {
          this.tooltip.visible = false;
        }
      },

      handleFoodImageError(event) {
        const target = event && event.target;
        if (!target || target.dataset.fallback === '1') {
          return;
        }

        target.dataset.fallback = '1';
        target.src = FOOD_ICON_PLACEHOLDER;
      },

      async logout() {
        if (this.logoutBusy) {
          return;
        }

        this.logoutBusy = true;
        const token = this.auth && this.auth.token ? this.auth.token : '';

        try {
          if (token) {
            await fetch('/api/auth/logout', {
              method: 'POST',
              headers: {
                Authorization: `Bearer ${token}`,
              },
            });
          }
        } catch (error) {
          // Local state is cleared below even when the request cannot reach the server.
        }

        clearStoredAuth();
        this.auth = null;
        this.user = null;
        this.stopTimers();
        redirectToLogin();
      },
    },
  }).mount('#app');
})();
