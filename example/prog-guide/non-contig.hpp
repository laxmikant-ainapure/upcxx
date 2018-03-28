constexpr int sdim[] = {32, 64, 32};
constexpr int ddim[] = {16, 32, 64};


future<> rput_strided_example(float* src_base, global_ptr<float> dst_base)
{
  return  rput_strided<3>(src_base, {{sizeof(float),sdim[0]*sizeof(float),sdim[0]*sdim[1]*sizeof(float)}},
                          dst_base, {{sizeof(float),ddim[0]*sizeof(float),ddim[0]*ddim[1]*sizeof(float)}},
                          {{4,3,2}});
}

future<> rget_strided_example(global_ptr<float> src_base, float* dst_base)
{
  return  rget_strided<3>(src_base, {{sizeof(float),sdim[0]*sizeof(float),sdim[0]*sdim[1]*sizeof(float)}},
                          dst_base, {{sizeof(float),ddim[0]*sizeof(float),ddim[0]*ddim[1]*sizeof(float)}},
                          {{4,3,2}});
}

