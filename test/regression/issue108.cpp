#include <iostream>
#include <upcxx/upcxx.hpp>

void foo0(void) { std::cout << "foo0" << std::endl; }
void foo2I(int a, int b) { std::cout << "foo2I(" << a << "," << b << ")" << std::endl; }

int main(int argc, char *argv[])
{
    upcxx::init();

    foo0();    // works (regular C++)
    (&foo0)(); // works (regular C++)
    upcxx::rpc(upcxx::rank_me(),foo0).wait();  // works
    upcxx::rpc(upcxx::rank_me(),&foo0).wait(); // works

    foo2I(1,2);    // works (regular C++)
    (&foo2I)(1,2); // works (regular C++)
    #if 1
    upcxx::rpc(upcxx::rank_me(),foo2I,1,2).wait();  // FAILS!!!
    #endif
    upcxx::rpc(upcxx::rank_me(),&foo2I,1,2).wait(); // works

    upcxx::finalize();
    return 0;
}   
