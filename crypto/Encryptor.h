#pragma once

#include <common/Util.h>

#include <cryptopp/osrng.h>

#include <unistd.h>

namespace crypto {

class Encryptor {
public:
  virtual size_t encrypt(Byte* data, size_t size, size_t capacity) = 0;
  virtual size_t decrypt(Byte* data, size_t size, size_t capacity) = 0;

protected:
  CryptoPP::AutoSeededRandomPool random_;
};
};
