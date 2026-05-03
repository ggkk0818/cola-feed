<script setup>
import { computed, ref } from 'vue'
import NetworkSelectDialog from '../components/NetworkSelectDialog.vue'
import {
  fetchRsaPublicKey,
  requestWifiConnect,
} from '../api/wifi'
import { encryptPasswordWithRsa } from '../utils/rsa'

const ssid = ref('')
const password = ref('')
const isDialogVisible = ref(false)
const isConnecting = ref(false)
const statusMessage = ref('')
const errorMessage = ref('')

const canConnect = computed(
  () => !isConnecting.value && ssid.value.trim() && password.value,
)

function openNetworkDialog() {
  isDialogVisible.value = true
}

function handleSelectNetwork(selectedSsid) {
  ssid.value = selectedSsid
  errorMessage.value = ''
}

async function handleConnect() {
  if (!canConnect.value) {
    errorMessage.value = '请先输入网络名称和密码。'
    return
  }

  isConnecting.value = true
  errorMessage.value = ''
  statusMessage.value = ''

  try {
    // Connection flow: obtain RSA key first, then send encrypted password.
    const publicKey = await fetchRsaPublicKey()
    const encryptedPassword = await encryptPasswordWithRsa(publicKey, password.value)

    await requestWifiConnect({
      ssid: ssid.value.trim(),
      password: encryptedPassword,
    })

    statusMessage.value = '网络连接中，请查看设备屏幕获取连接状态。'
  } catch (error) {
    errorMessage.value = error?.message || '发送连接请求失败，请稍后重试。'
  } finally {
    isConnecting.value = false
  }
}
</script>

<template>
  <main class="wifi-setup-page">
    <section class="wifi-card">
      <p class="wifi-card__tag">设备网络配置</p>
      <h1>快速连接 Wi-Fi</h1>

      <div class="form-row">
        <label for="ssid">网络名称</label>
        <div class="input-wrap">
          <span class="icon-mask icon-wifi" aria-hidden="true"></span>
          <input
            id="ssid"
            v-model="ssid"
            type="text"
            placeholder="点击选择网络"
            readonly
            @click="openNetworkDialog"
          />
          <button type="button" class="scan-btn" @click="openNetworkDialog">
            <span class="icon-mask icon-scan" aria-hidden="true"></span>
            扫描网络
          </button>
        </div>
      </div>

      <div class="form-row">
        <label for="password">网络密码</label>
        <div class="input-wrap">
          <span class="icon-mask icon-password" aria-hidden="true"></span>
          <input
            id="password"
            v-model="password"
            type="password"
            placeholder="请输入网络密码"
            maxlength="64"
          />
        </div>
      </div>

      <button
        type="button"
        class="connect-btn"
        :disabled="!canConnect"
        @click="handleConnect"
      >
        {{ isConnecting ? '连接中...' : '连接' }}
      </button>

      <p v-if="statusMessage" class="hint is-success">{{ statusMessage }}</p>
      <p v-if="errorMessage" class="hint is-error">{{ errorMessage }}</p>
    </section>

    <NetworkSelectDialog
      v-model="isDialogVisible"
      :initial-ssid="ssid"
      @select="handleSelectNetwork"
    />
  </main>
</template>

<style scoped>
.wifi-setup-page {
  min-height: 100vh;
  display: grid;
  place-items: center;
  padding: 24px 16px;
}

.wifi-card {
  width: min(680px, 100%);
  padding: 28px;
  border-radius: 24px;
  background: var(--card-bg);
  border: 1px solid var(--card-border);
  box-shadow: 0 20px 40px rgb(15 23 42 / 12%);
  animation: rise-in 0.45s ease;
}

.wifi-card__tag {
  margin: 0;
  color: var(--brand);
  font-size: 0.9rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.wifi-card h1 {
  margin: 8px 0 24px;
  font-size: clamp(1.45rem, 2.8vw, 2rem);
  color: var(--text-main);
}

.form-row {
  margin-bottom: 16px;
}

.form-row label {
  display: inline-block;
  margin-bottom: 8px;
  color: var(--text-muted);
  font-size: 0.92rem;
}

.input-wrap {
  display: flex;
  align-items: center;
  border: 1px solid var(--card-border);
  border-radius: 14px;
  background: var(--soft-bg);
  padding: 0 10px;
  gap: 8px;
}

.icon-mask {
  display: inline-block;
  flex: 0 0 auto;
  background-color: var(--icon-color, var(--icon-muted));
  -webkit-mask: var(--icon-url) no-repeat center / contain;
  mask: var(--icon-url) no-repeat center / contain;
}

.icon-wifi {
  --icon-url: url('../assets/icons/wifi.svg');
  --icon-color: var(--icon-muted);
  width: 18px;
  height: 18px;
}

.icon-password {
  --icon-url: url('../assets/icons/password.svg');
  --icon-color: var(--icon-muted);
  width: 18px;
  height: 18px;
}

.icon-scan {
  --icon-url: url('../assets/icons/scan.svg');
  --icon-color: var(--brand);
  width: 15px;
  height: 15px;
}

.input-wrap input {
  flex: 1;
  min-width: 0;
  border: 0;
  background: transparent;
  color: var(--text-main);
  padding: 12px 2px;
  outline: none;
}

.scan-btn {
  border: 0;
  border-left: 1px solid var(--card-border);
  background: transparent;
  color: var(--brand);
  cursor: pointer;
  padding: 10px 10px 10px 12px;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  white-space: nowrap;
}

.connect-btn {
  margin-top: 8px;
  width: 100%;
  border: 0;
  border-radius: 14px;
  padding: 12px;
  color: #fff;
  font-weight: 600;
  letter-spacing: 0.02em;
  background: linear-gradient(135deg, #0ea5a4, #14b8a6);
  cursor: pointer;
}

.connect-btn:disabled {
  cursor: not-allowed;
  opacity: 0.65;
}

.hint {
  margin: 12px 0 0;
  font-size: 0.9rem;
}

.hint.is-success {
  color: var(--success);
}

.hint.is-error {
  color: var(--danger);
}

@keyframes rise-in {
  from {
    opacity: 0;
    transform: translateY(10px);
  }

  to {
    opacity: 1;
    transform: translateY(0);
  }
}

@media (max-width: 720px) {
  .wifi-card {
    border-radius: 18px;
    padding: 22px 16px;
  }

  .input-wrap {
    flex-wrap: wrap;
    padding-top: 8px;
    padding-bottom: 8px;
  }

  .input-wrap input {
    width: calc(100% - 26px);
  }

  .scan-btn {
    width: 100%;
    justify-content: center;
    border-left: 0;
    border-top: 1px solid var(--card-border);
    padding-top: 10px;
  }
}
</style>