
#define F (1 << 14) // 2^14 = 16384
#define convert_to_fixed_point(n) (n * F)
#define convert_to_integer_towards_zero(x) (x / F)
#define convert_to_integer_towards_nearest(x) (x >= 0 ? ((x + F / 2) / F) : ((x - F / 2) / F))
#define add_fixed_point(x, y) (x + y)
#define add_fixed_point_integer(x, n) (x + n * F)
#define subtract_fixed_point(x, y) (x - y)
#define subtract_fixed_point_integer(x, n) (x - n * F)
#define multiply_fixed_point(x, y) (((int64_t) x) * y / F)
#define multiply_fixed_point_integer(x, n) (x * n)
#define divide_fixed_point(x, y) (((int64_t) x) * F / y)
#define divide_fixed_point_integer(x, n) (x / n)