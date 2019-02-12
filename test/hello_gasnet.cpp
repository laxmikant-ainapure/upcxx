#include <iostream>
#include <sstream>
#include <gasnet.h>

int main() {
  gasnet_init(nullptr, nullptr);
  gasnet_attach(nullptr, 0, 1<<20, 0);
  
  std::ostringstream oss;
  oss << "Hello from "<<gasnet_mynode()<<'\n';
  std::cout << oss.str() << std::flush;
  
  return 0;
}
