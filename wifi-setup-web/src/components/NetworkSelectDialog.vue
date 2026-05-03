<script setup>
import { computed, ref, watch } from 'vue'
import { fetchWifiList } from '../api/wifi'

const props = defineProps({
  modelValue: {
    type: Boolean,
    default: false,
  },
  initialSsid: {
    type: String,
    default: '',
  },
})

const emit = defineEmits(['update:modelValue', 'select'])

const isLoading = ref(false)
const networkList = ref([])
const errorMessage = ref('')
const showManualInput = ref(false)
const manualSsid = ref('')
const hasAutoScanned = ref(false)

const dialogVisible = computed(() => props.modelValue)

watch(
  () => props.modelValue,
  async (isOpen) => {
    if (!isOpen) {
      showManualInput.value = false
      errorMessage.value = ''
      return
    }

    manualSsid.value = props.initialSsid

    // Auto scan only on first open to avoid repeated background requests.
    if (!hasAutoScanned.value) {
      hasAutoScanned.value = true
      await scanNetworks()
    }
  },
)

async function scanNetworks() {
  if (isLoading.value) {
    return
  }

  isLoading.value = true
  errorMessage.value = ''

  try {
    networkList.value = await fetchWifiList()

    if (networkList.value.length === 0) {
      errorMessage.value = '未扫描到可用网络，请重试。'
    }
  } catch (error) {
    errorMessage.value = error?.message || '扫描网络失败，请稍后重试。'
  } finally {
    isLoading.value = false
  }
}

function closeDialog() {
  emit('update:modelValue', false)
}

function selectNetwork(ssid) {
  emit('select', ssid)
  closeDialog()
}

function confirmManualInput() {
  // Manual input reuses the same select event as list selection.
  const value = manualSsid.value.trim()

  if (!value) {
    errorMessage.value = '请输入网络名称。'
    return
  }

  selectNetwork(value)
}

function handleOverlayClick(event) {
  if (event.target === event.currentTarget) {
    closeDialog()
  }
}
</script>

<template>
  <teleport to="body">
    <div
      v-if="dialogVisible"
      class="network-dialog__overlay"
      @click="handleOverlayClick"
    >
      <section class="network-dialog" role="dialog" aria-modal="true">
        <header class="network-dialog__header">
          <h2>选择网络</h2>
          <button type="button" class="network-dialog__close" @click="closeDialog">
            关闭
          </button>
        </header>

        <div class="network-dialog__actions">
          <button
            type="button"
            class="outline-btn"
            :disabled="isLoading"
            @click="scanNetworks"
          >
            <span class="icon-mask icon-scan" aria-hidden="true"></span>
            {{ isLoading ? '扫描中...' : '扫描网络' }}
          </button>
          <button
            type="button"
            class="outline-btn"
            @click="showManualInput = !showManualInput"
          >
            输入网络名称
          </button>
        </div>

        <div v-if="showManualInput" class="manual-input">
          <input
            v-model.trim="manualSsid"
            type="text"
            placeholder="请输入网络名称"
            maxlength="64"
          />
          <button type="button" class="primary-btn" @click="confirmManualInput">
            确认使用
          </button>
        </div>

        <p v-if="errorMessage" class="network-dialog__message is-error">
          {{ errorMessage }}
        </p>

        <ul class="network-list" :class="{ 'is-loading': isLoading }">
          <li v-if="!isLoading && networkList.length === 0" class="network-list__empty">
            暂无可用网络
          </li>
          <li v-for="ssid in networkList" :key="ssid">
            <button type="button" class="network-item" @click="selectNetwork(ssid)">
              {{ ssid }}
            </button>
          </li>
        </ul>
      </section>
    </div>
  </teleport>
</template>

<style scoped>
.network-dialog__overlay {
  position: fixed;
  inset: 0;
  z-index: 90;
  display: grid;
  place-items: center;
  padding: 20px;
  background: rgb(8 17 31 / 45%);
  backdrop-filter: blur(4px);
}

.network-dialog {
  width: min(520px, 100%);
  border-radius: 18px;
  padding: 22px;
  background: var(--card-bg);
  border: 1px solid var(--card-border);
  box-shadow: 0 18px 36px rgb(17 24 39 / 24%);
}

.network-dialog__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 14px;
}

.network-dialog__header h2 {
  margin: 0;
  font-size: 1.05rem;
  color: var(--text-main);
}

.network-dialog__close {
  border: 0;
  background: transparent;
  color: var(--text-muted);
  cursor: pointer;
}

.network-dialog__actions {
  display: flex;
  gap: 10px;
  margin-bottom: 14px;
}

.outline-btn,
.primary-btn,
.network-item {
  cursor: pointer;
  transition: transform 0.2s ease, background-color 0.2s ease;
}

.outline-btn,
.primary-btn {
  border-radius: 10px;
  border: 1px solid var(--card-border);
  padding: 10px 12px;
  display: inline-flex;
  align-items: center;
  gap: 8px;
  font-size: 0.92rem;
}

.outline-btn {
  background: var(--soft-bg);
  color: var(--text-main);
}

.icon-mask {
  display: inline-block;
  flex: 0 0 auto;
  background-color: var(--icon-color, var(--icon-muted));
  -webkit-mask: var(--icon-url) no-repeat center / contain;
  mask: var(--icon-url) no-repeat center / contain;
}

.icon-scan {
  --icon-url: url('../assets/icons/scan.svg');
  --icon-color: var(--brand);
  width: 16px;
  height: 16px;
}

.primary-btn {
  background: var(--brand);
  color: #fff;
  border-color: transparent;
}

.outline-btn:disabled {
  cursor: not-allowed;
  opacity: 0.65;
}

.manual-input {
  display: flex;
  gap: 10px;
  margin-bottom: 12px;
}

.manual-input input {
  flex: 1;
  min-width: 0;
  border-radius: 10px;
  border: 1px solid var(--card-border);
  background: var(--soft-bg);
  color: var(--text-main);
  padding: 10px 12px;
}

.network-dialog__message {
  margin: 0 0 10px;
  font-size: 0.88rem;
}

.network-dialog__message.is-error {
  color: var(--danger);
}

.network-list {
  list-style: none;
  margin: 0;
  padding: 0;
  display: grid;
  gap: 8px;
  max-height: 280px;
  overflow: auto;
}

.network-list.is-loading {
  opacity: 0.7;
}

.network-list__empty {
  color: var(--text-muted);
  font-size: 0.9rem;
  text-align: center;
  padding: 18px 8px;
}

.network-item {
  width: 100%;
  border: 1px solid var(--card-border);
  border-radius: 10px;
  background: var(--soft-bg);
  color: var(--text-main);
  text-align: left;
  padding: 10px 12px;
}

.network-item:hover,
.outline-btn:hover,
.primary-btn:hover {
  transform: translateY(-1px);
}

@media (max-width: 768px) {
  .network-dialog__overlay {
    align-items: flex-end;
    padding: 0;
  }

  .network-dialog {
    width: 100%;
    border-radius: 18px 18px 0 0;
    max-height: 84vh;
    overflow: auto;
  }

  .network-dialog__actions,
  .manual-input {
    flex-direction: column;
  }

  .outline-btn,
  .primary-btn {
    justify-content: center;
    width: 100%;
  }
}
</style>