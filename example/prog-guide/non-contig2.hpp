vector<pair<particle_t*, size_t>> src({{{srcP+12, 22},{srcP+66,12},{srcP,4}}});
vector<pair<global_ptr<particle_t>, size_t>> dest({{{destP+240,38}}});
auto f1 = rput_irregular(src.begin(), src.end(), dest.begin(), dest.end());
f1.wait();
