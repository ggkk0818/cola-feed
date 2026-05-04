import JSEncrypt from 'jsencrypt'

function stripPemMarkers(content) {
  return String(content)
    .replace(/-----BEGIN [^-]+-----/g, '')
    .replace(/-----END [^-]+-----/g, '')
    .replace(/\s+/g, '')
}

function wrapPemBody(base64Body, type) {
  const lines = base64Body.match(/.{1,64}/g)

  if (!lines || lines.length === 0) {
    throw new Error('RSA 密钥格式无效。')
  }

  return `-----BEGIN ${type}-----\n${lines.join('\n')}\n-----END ${type}-----`
}

function normalizePublicKey(publicKey) {
  const body = stripPemMarkers(publicKey)

  if (!body) {
    throw new Error('RSA 公钥不能为空。')
  }

  return wrapPemBody(body, 'PUBLIC KEY')
}

function normalizePrivateKey(privateKey) {
  const source = String(privateKey)
  const body = stripPemMarkers(source)

  if (!body) {
    throw new Error('RSA 私钥不能为空。')
  }

  const keyType = source.includes('BEGIN RSA PRIVATE KEY')
    ? 'RSA PRIVATE KEY'
    : 'PRIVATE KEY'

  return wrapPemBody(body, keyType)
}

export function encryptTextWithRsa(publicKey, plainText) {
  const encryptor = new JSEncrypt()
  encryptor.setPublicKey(normalizePublicKey(publicKey))

  const content = String(plainText ?? '')
  const encrypted =
    typeof encryptor.encryptOAEP === 'function'
      ? encryptor.encryptOAEP(content)
      : false

  if (!encrypted) {
    throw new Error('RSA-OAEP 加密失败，请检查公钥格式。')
  }

  return encrypted
}

export function decryptTextWithRsa(privateKey, cipherText) {
  const decryptor = new JSEncrypt()
  decryptor.setPrivateKey(normalizePrivateKey(privateKey))

  const decrypted = decryptor.decrypt(String(cipherText ?? ''))

  if (!decrypted) {
    throw new Error('RSA 解密失败，请检查私钥或密文。')
  }

  return decrypted
}

// Kept for compatibility with existing call sites.
export async function encryptPasswordWithRsa(publicKey, password) {
  return encryptTextWithRsa(publicKey, password)
}