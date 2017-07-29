#include <networking/IPTables.h>

#include <fcntl.h>
#include <unistd.h>

#include <iostream>

int main(int argc, char* argv[]) {
  networking::IPTables::masquerade(networking::SubnetAddress("10.100.0.0/24"));
  networking::IPTables::clear();

  return 0;
}
