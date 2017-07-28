#include "AESEncryptor.h"

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

namespace crypto {

const Byte kAESKeyPaddingByte = 0xFB;

AESKey::AESKey(Byte* data, size_t size) : key(data, size) {}

AESKey::AESKey(std::string const& key) {
  Byte data[CryptoPP::AES::MAX_KEYLENGTH];

  assertTrue(key.length() <= CryptoPP::AES::MAX_KEYLENGTH,
             "AES encryption key is too long.");

  memset(data, kAESKeyPaddingByte, sizeof(data));
  memcpy(data, key.c_str(), key.length());

  this->key.Assign(data, key.length() <= CryptoPP::AES::MIN_KEYLENGTH
                             ? CryptoPP::AES::MIN_KEYLENGTH
                             : CryptoPP::AES::MAX_KEYLENGTH);
}

AESEncryptor::AESEncryptor(AESKey const& key) : key_(key) {}

/* virtual */ size_t AESEncryptor::encrypt(Byte* data, size_t size,
                                           size_t capacity) /* override */ {
  Byte iv[CryptoPP::AES::BLOCKSIZE];
  random_.GenerateBlock(iv, CryptoPP::AES::BLOCKSIZE);

  CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption encryption(key_.key,
                                                           key_.key.size(), iv);
  encryption.ProcessData(data, data, size);

  assertTrue(size + CryptoPP::AES::BLOCKSIZE <= capacity,
             "Not enough place to store AES encryption IV.");
  memcpy(data + size, iv, CryptoPP::AES::BLOCKSIZE);
  return size + CryptoPP::AES::BLOCKSIZE;
}

/* virtual */ size_t AESEncryptor::decrypt(Byte* data, size_t size,
                                           size_t capacity) /* override */ {
  assertTrue(
      size >= CryptoPP::AES::BLOCKSIZE,
      "AES decryption encountered a size that is less than the IV size.");

  Byte iv[CryptoPP::AES::BLOCKSIZE];
  memcpy(iv, data + (size - CryptoPP::AES::BLOCKSIZE),
         CryptoPP::AES::BLOCKSIZE);

  CryptoPP::CFB_Mode<CryptoPP::AES>::Decryption decryption(key_.key,
                                                           key_.key.size(), iv);
  decryption.ProcessData(data, data, size);
  return size - CryptoPP::AES::BLOCKSIZE;
}
}
