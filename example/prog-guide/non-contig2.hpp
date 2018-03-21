pair<particle_t*, size_t> src[]={{srcP+12, 22},{srcP+66,12},{srcP,4}};
pair<global_ptr<particle_t>, size_t> dest[]={{destP,38}};
auto f = rput_irregular(src, end(src), dest, end(dest));
f.wait();
