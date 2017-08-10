#include <iostream>
#include <gasnet.h>

int main() {
  gasnet_init(nullptr, nullptr);
  gasnet_attach(nullptr, 0, 1<<20, 0);
  
  std::cout << "Hello from "<<gasnet_mynode()<<'\n';
  
  return 0;
}
