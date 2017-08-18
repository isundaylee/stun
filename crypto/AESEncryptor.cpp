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

/* static */ std::string AESKey::randomStringKey() {
  char alphabet[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8',
                     '9', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                     'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q',
                     'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
  size_t n = sizeof(alphabet) / sizeof(alphabet[0]);
  CryptoPP::AutoSeededRandomPool random;
  char key[CryptoPP::AES::MAX_KEYLENGTH];
  for (size_t i = 0; i < CryptoPP::AES::MAX_KEYLENGTH; i++) {
    key[i] = alphabet[random.GenerateByte() % n];
  }
  return std::string(key, CryptoPP::AES::MAX_KEYLENGTH);
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
