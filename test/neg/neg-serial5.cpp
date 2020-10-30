#include <upcxx/upcxx.hpp>

struct U {
  int w,x;
};

struct T { 
  int x,y,z;
  struct upcxx_serialization { // asymmetric deserialization
    template<typename Writer>
    static void serialize (Writer& writer, T const & t) {
        writer.write(t.x);
    }   
    template<typename Reader>
    static U* deserialize(Reader& reader, void* storage) {
        int x = reader.template read<int>();
        U *up = new(storage) U();
        up->x = x;
        return up; 
    }   
  };  
};

struct B {
  T t;
  UPCXX_SERIALIZED_FIELDS(t);
};

int main() {
  upcxx::init();

  B b;
  B b2 = upcxx::serialization_traits<B>::deserialized_value(b);
  
  
  upcxx::finalize();
}
