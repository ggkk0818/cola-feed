import http from './http'

function ensureSuccess(response) {
  // Backend convention: successful business response must include code === 200.
  const payload = response?.data ?? {}

  if (payload.code !== 200) {
    throw new Error(payload.message || '请求失败，请稍后重试。')
  }

  return payload
}

export async function fetchWifiList() {
  const payload = ensureSuccess(await http.get('/api/wifi/list'))
  const list = Array.isArray(payload.data) ? payload.data : []

  // Keep only valid SSID strings to simplify rendering and selection.
  return list
    .map((item) => item?.ssid)
    .filter((ssid) => typeof ssid === 'string' && ssid.trim().length > 0)
}

export async function fetchRsaPublicKey() {
  const payload = ensureSuccess(await http.get('/api/rsa/public_key'))
  const key = payload?.data?.public_key

  if (!key || typeof key !== 'string') {
    throw new Error('未获取到可用的 RSA 公钥。')
  }

  return key
}

export async function requestWifiConnect({ ssid, password }) {
  const payload = ensureSuccess(
    await http.post('/api/wifi/connect', {
      ssid,
      password,
    }),
  )

  return payload.message || '连接请求已发送'
}