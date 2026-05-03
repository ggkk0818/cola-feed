const textEncoder = new TextEncoder()

function parsePublicKeyBase64(publicKey) {
  const normalized = String(publicKey)
    .replace('-----BEGIN PUBLIC KEY-----', '')
    .replace('-----BEGIN PUBLIC KEY', '')
    .replace('-----END PUBLIC KEY-----', '')
    .replace('-----END PUBLIC KEY', '')
    .replace(/\s+/g, '')

  if (!normalized) {
    throw new Error('RSA 公钥格式无效。')
  }

  return normalized
}

function base64ToArrayBuffer(base64) {
  const binary = atob(base64)
  const bytes = new Uint8Array(binary.length)

  for (let index = 0; index < binary.length; index += 1) {
    bytes[index] = binary.charCodeAt(index)
  }

  return bytes.buffer
}

function arrayBufferToBase64(buffer) {
  const bytes = new Uint8Array(buffer)
  let binary = ''

  for (let index = 0; index < bytes.length; index += 1) {
    binary += String.fromCharCode(bytes[index])
  }

  return btoa(binary)
}

async function importPublicKey(publicKeyBase64) {
  const keyData = base64ToArrayBuffer(publicKeyBase64)

  try {
    return await crypto.subtle.importKey(
      'spki',
      keyData,
      {
        name: 'RSA-OAEP',
        hash: 'SHA-256',
      },
      false,
      ['encrypt'],
    )
  } catch {
    // Some embedded devices still rely on SHA-1 for RSA-OAEP.
    return crypto.subtle.importKey(
      'spki',
      keyData,
      {
        name: 'RSA-OAEP',
        hash: 'SHA-1',
      },
      false,
      ['encrypt'],
    )
  }
}

export async function encryptPasswordWithRsa(publicKey, password) {
  if (!globalThis.crypto?.subtle) {
    throw new Error('当前浏览器不支持 RSA 加密能力。')
  }

  const keyBase64 = parsePublicKeyBase64(publicKey)
  const cryptoKey = await importPublicKey(keyBase64)
  const encrypted = await crypto.subtle.encrypt(
    { name: 'RSA-OAEP' },
    cryptoKey,
    textEncoder.encode(password),
  )

  return arrayBufferToBase64(encrypted)
}