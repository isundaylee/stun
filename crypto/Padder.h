#pragma once

#include <crypto/Encryptor.h>

#include <common/Util.h>

#include <unistd.h>

namespace crypto {

class Padder : public Encryptor {
public:
  Padder(size_t minSize);

  virtual size_t encrypt(Byte* data, size_t size, size_t capacity) override;
  virtual size_t decrypt(Byte* data, size_t size, size_t capacity) override;

private:
  size_t minSize_;
};
}
