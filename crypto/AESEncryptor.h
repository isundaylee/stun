#pragma once

#include <cryptopp/secblock.h>

#include <crypto/Encryptor.h>

namespace crypto {

struct AESKey {
public:
  AESKey(Byte* data, size_t size);
  AESKey(std::string const& key);

  static std::string randomStringKey();

  CryptoPP::SecByteBlock key;
};

class AESEncryptor : public Encryptor {
public:
  explicit AESEncryptor(AESKey const& key);

  virtual size_t encrypt(Byte* data, size_t size, size_t capacity) override;
  virtual size_t decrypt(Byte* data, size_t size, size_t capacity) override;

private:
  AESKey key_;
};
} // namespace crypto
