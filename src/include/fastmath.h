inline float
fastlog2(float x) {
  union { float f; uint32_t i; } vx = { x };
  union { uint32_t i; float f; } mx = { (vx.i & 0x007FFFFF) | (0x7e << 23) };
  float y = vx.i;
  y *= 1.0f / (1 << 23);
  return y - 124.22544637f - 1.498030302f * mx.f - 1.72587999f / (0.3520887068f + mx.f);
}

inline float
fastlog(float x) {
  return 0.69314718f * fastlog2(x);
}

inline float
fastpow2(float p) {
  float offset = (p < 0) ? 1.0f : 0.0f;
  float clipp = (p < -126) ? -126.0f : p;
  int w = clipp;
  float z = clipp - w + offset;
  union { uint32_t i; float f; } v = { (uint32_t)((1 << 23) * (clipp + 121.2740838f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z)) };
  return v.f;
}

inline float
fastexp(float p) {
  return fastpow2(1.442695040f * p);
}

inline float
fastpow(float x, float p) {
  return fastpow2(p * fastlog2(x));
}
