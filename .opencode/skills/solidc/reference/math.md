# Vectors, matrices, SIMD, and numerics

All of `simd.h`, `vec.h`, `matrix.h`, `linear_alg.h`, `cmp.h`, `ckdint.h` are
**header-only, inline, allocation-free** (except `FMat`/PCA in `linear_alg.h`).
Only `sort.h` adds compiled symbols.

Include hierarchy: `simd.h` → `align.h`; `vec.h` → `simd.h`; `matrix.h` →
`vec.h`; `linear_alg.h` → `matrix.h` + `vec.h`. Include the topmost header you
need.

## SIMD (`simd.h`)

`simd_vec_t` = `__m128` (x86) / `float32x4_t` (ARM) / scalar struct. 128-bit
only (SSE2 baseline; SSE4.1/NEON accelerated; scalar fallback). `simd_ivec_t`
for int lanes. Arch macros: `SIMD_ARCH_X86/ARM/ARM64/ARM32/SCALAR`,
`SIMD_HAS_SSE41`, `SIMD_X/Y/Z/W`.

```c
simd_vec_t simd_set(float x,float y,float z,float w);  simd_vec_t simd_set1(float);
simd_vec_t simd_set_zero(void);
simd_vec_t simd_load(const float* p);   void simd_store(float* p, simd_vec_t);
simd_vec_t simd_add/sub/mul/div(a,b);   simd_vec_t simd_madd(a,b,c);  /* a*b+c */
simd_neg/abs/min/max;  simd_sqrt/rsqrt/rcp/floor/ceil/round;
simd_cmpeq/neq/lt/le/gt/ge;             /* return all-1s/all-0s MASKS */
simd_and/or/xor/andnot;  simd_dup_x/y/z/w;  simd_swizzle(v,x,y,z,w);  /* const indices */
simd_blend(false_vec, true_vec, mask);
float simd_hadd/hmin/hmax(v);  float simd_dot3/dot4(a,b);
float simd_length_sq3/length3/length_sq4/length4;
simd_vec_t simd_cross(a,b);  simd_normalize3/4, simd_normalize3_fast;
bool  simd_equals_eps(a,b,eps);  /* ALL four lanes, including padding */
simd_transpose4(r0,r1,r2,r3);    /* in-place macro */
void simd_ascii_lower(char* buf, size_t len);   /* UTF-8 safe (>=0x80 untouched) */
void simd_ascii_upper(char* buf, size_t len);
```

Comparisons are masks, not booleans — combine with `simd_and`/`simd_blend` or
reduce with `simd_check_all`. `simd_swizzle` needs compile-time-constant
indices. On ARM GCC use the portable path (no `__builtin_shufflevector`).

## Vectors (`vec.h`)

Storage types (plain C, for arrays/serialization): `Vec2{x,y}`, `Vec3{x,y,z}`,
`Vec4{x,y,z,w}`. Compute types (16-byte aligned unions of `simd_vec_t` +
`float f32[4]` + named fields): `SimdVec2`, `SimdVec3` (`w` is padding — keep
zero, do not use), `SimdVec4`. **Store in `Vec*`, load to `SimdVec*` for math,
store back.**

```c
SimdVec3 vec3_load(Vec3);  Vec3 vec3_store(SimdVec3);   /* vec2_/vec4_ likewise */
vec2/3/4_add, sub, mul(a,float), div(a,float);
vec3_scale(a,b);              /* Hadamard, NOT dot */
float vec3_dot(a,b);  SimdVec3 vec3_cross(a,b);
float vec3_length_sq(v), vec3_length(v);
vec3_normalize(v), vec3_normalize_fast(v);
float vec3_distance_sq/distance(a,b);
vec3_lerp/project/reject/min/max/abs/neg/clamp;
vec3_relu/sigmoid/tanh, vec3_rotate_x/y/z(v, angle);
vec3_reflect(i,n), vec3_refract(i,n,eta), vec3_faceforward(n,i);
Vec3 vec3_barycentric(a,b,c,p), vec4_perspective_divide(SimdVec4);
float vec3_angle_between(a,b);   float vec3_triple_product(a,b,c);
bool vec3_equals(Vec3 a, Vec3 b, float eps);   /* vec4_equals likewise */
```

`vec*_div` and `vec*_project` zero-guard near-zero divisors. `vec3_normalize`
is **undefined for a zero vector**. `vec4_perspective_divide` does not guard
`w == 0`. `vec2/3_angle_between` use `atan2` for stability.

## Matrices and quaternions (`matrix.h`)

`Mat3 { float m[3][3]; }` and `Mat4 { float m[4][4]; simd_vec_t cols[4]; }` are
**column-major** (`m[col][row]`). `Quat { float x,y,z,w; }`, identity `(0,0,0,1)`.

```c
Mat3 mat3_identity(void);  Mat4 mat4_identity(void);
Mat3 mat3_mul/add/add_scalar/scalar_mul/transpose/inverse;  float mat3_determinant(Mat3);
bool mat3_lu(Mat3, Mat3* L, Mat3* U, Mat3* P);
Mat4 mat4_mul/add/sub/div/scalar_mul/transpose/inverse;     float mat4_determinant(Mat4);
Vec4 mat4_mul_vec4(Mat4, Vec4);  Vec3 mat3_mul_vec3(Mat3, Vec3);

Mat4 mat4_translate(Vec3), mat4_scale(Vec3), mat4_rotate_x/y/z(float),
     mat4_rotate(Vec3 axis, float);
Mat4 mat4_ortho(l,r,b,t,n,f), mat4_perspective(fov,aspect,n,f),
     mat4_infinite_perspective(fov,aspect,n),
     mat4_look_at(SimdVec3 eye, SimdVec3 target, SimdVec3 up);
Vec3 mat4_transform_point(Mat4, Vec3), mat4_transform_direction(Mat4, Vec3);
Vec3 mat4_unproject(Mat4 inv_view_proj, Vec3 ndc);
Mat3 mat3_normal_matrix(Mat4 model);   /* inverse-transpose upper-left 3x3 */

Quat quat_identity(void), quat_from_axis_angle(Vec3, float), quat_from_euler(r,p,y);
float quat_dot/length/length_sq;  Quat quat_normalize/conjugate/mul/nlerp/slerp;
SimdVec3 quat_rotate_vec3(Quat q, SimdVec3 v);   /* q must be unit */
Mat4 mat4_from_quat(Quat);  Mat3 mat3_from_quat(Quat);  Quat quat_from_mat4(Mat4);
Mat4 mat4_compose(Vec3 t, Quat r, Vec3 s);   /* T*R*S */
```

`mat3_inverse`/`mat4_inverse` return **identity** for singular input (check the
determinant if singularity matters). `quat_normalize` returns identity for a
degenerate quaternion. `mat4_from_quat` assumes a unit quaternion. `mat4_look_at`
has no guard for `up` parallel to the view direction.

## Linear algebra / ML (`linear_alg.h`)

`FMat { size_t rows, cols; float* data; }` is **row-major**, one allocation.
The invalid sentinel is `{0,0,NULL}`; check `.data`/`fmat_valid`. Every
producing call returns an owned `FMat` released with `fmat_destroy` (safe on
empty/NULL, idempotent-ish). `fmat_get`/`fmat_set` are **unchecked**.

```c
FMat fmat_create(size_t rows, size_t cols);  void fmat_destroy(FMat*);
bool fmat_valid(const FMat*);  float fmat_get(const FMat*, size_t, size_t);
void fmat_set(FMat*, size_t, size_t, float);
FMat fmat_from_array(rows, cols, const float*), fmat_copy, fmat_identity(n), fmat_transpose;
FMat fmat_mul(a,b), fmat_add/sub/hadamard/scale;   bool fmat_add_ip/sub_ip/scale_ip(m, ...);
FMat fmat_from_mat4(const Mat4*);   /* transposes to row-major */
FMat fmat_batch_transform(const Mat4*, const FMat* points);  /* points->cols == 4 */

bool fmat_svd(const FMat* A, FMat* U, FMat* S, FMat* V);   /* thin Jacobi; false if no convergence */
bool fmat_pinv(const FMat* A, FMat* out);   bool fmat_lstsq(const FMat* A, const FMat* b, FMat* out);
bool fmat_pca(const FMat* X, size_t n_components, PCAResult* out);
FMat fmat_pca_transform(const PCAResult*, const FMat* X);   void pca_result_destroy(PCAResult*);

void fmat_rng_seed(FMatRng*, uint64_t);  float fmat_rng_uniform(FMatRng*), fmat_rng_normal(FMatRng*);
bool fmat_he_init(FMat*, size_t fan_in, FMatRng*);
bool fmat_xavier_init(FMat*, size_t fan_in, size_t fan_out, FMatRng*);
FMat fmat_sum_cols/rows/mean_cols/argmax_rows;
FMat fmat_relu/sigmoid/tanh/softmax_rows;   bool fmat_relu_ip/sigmoid_ip/tanh_ip;
float fmat_mse(a,b), fmat_cross_entropy(probs, onehot);
FMat fmat_mul_ta(a,b) /* A^T B */, fmat_mul_tb(a,b) /* A B^T */;
```

Fixed-size decompositions (allocation-free): `orthonormalize`, `basis_from_normal`,
`mat3_eigen_symmetric` (Jacobi), `mat3_svd`, `mat4_qr`, `mat4_power_iteration`,
`mat4_norm_frobenius`, `mat3_is_positive_definite`, `mat4_condition_number`,
`mat4_lu`, `mat4_solve`, `mat3_solve`, `mat4_decompose`.

## Float comparison (`cmp.h`)

```c
typedef enum { CMP_ABSOLUTE, CMP_RELATIVE, CMP_ULPS, CMP_COMBINED } cmp_mode_t;
typedef struct { cmp_mode_t mode; double epsilon; uint64_t ulps; } cmp_config_t;

bool cmp_float(float,float,cmp_config_t);  cmp_double(...);  cmp_long_double(...);
bool cmp_absolute/relative/combined(a,b,eps);  bool cmp_ulps(a,b,ulps);

#define CMP(a, b)            /* default config, C11 _Generic on type of a */
#define CMP_EPS(a,b,eps)  CMP_ABS(a,b,eps)  CMP_ULPS(a,b,ulps)  CMP_COMB(a,b,eps)
```

Use `CMP(a,b)` instead of `==` for floating point. NaN==NaN and matching-signed
infinities compare equal (intentional). `cmp_long_double` casts to `double`
internally. Typical eps: float `1e-6`, double `1e-12`.

## Sorting (`sort.h`)

```c
void sol_qsort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*));
void sol_sort_i32(int32_t* a, size_t n);
void sol_sort_i64(int64_t* a, size_t n);
void sol_sort_f64(double*  a, size_t n);
```

Introsort + radix specializations (radix for `n >= 1024`). In-place. **Not
stable.** `sol_sort_f64` pushes NaNs to the end and canonicalizes them (payloads
lost). Random-access memory only; safe no-ops on NULL/`n < 2`.
