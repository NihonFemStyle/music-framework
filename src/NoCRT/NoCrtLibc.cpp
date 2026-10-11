// C library subset for the no-CRT build.
//
// This translation unit deliberately includes no headers: every declaration it
// needs is written out here, so nothing collides with the inline helpers or
// intrinsic declarations in the CRT headers that the rest of the project uses.
// It is compiled with intrinsics disabled (see CMakeLists.txt) so the compiler
// does not turn these loops back into calls to themselves.

typedef decltype(sizeof 0) size_t;
typedef unsigned short wint_t;
typedef unsigned short wctype_t;
typedef unsigned long long u64;
typedef long long i64;

extern "C" {
__declspec(dllimport) void* __stdcall GetProcessHeap(void);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int code);
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* text);
__declspec(dllimport) int __stdcall GetStringTypeW(unsigned long infoType, const wchar_t* source, int count, unsigned short* types);
__declspec(dllimport) wchar_t* __stdcall CharLowerW(wchar_t* text);
__declspec(dllimport) wchar_t* __stdcall CharUpperW(wchar_t* text);

#if defined(_M_IX86) || defined(_M_X64)
void __movsb(unsigned char* destination, const unsigned char* source, size_t count);
void __stosb(unsigned char* destination, unsigned char value, size_t count);
#pragma intrinsic(__movsb, __stosb)
#endif

#pragma function(memset, memcpy, memmove, memcmp, memchr, strlen, wcslen, abs, labs, fabs, sqrt, sin, cos, exp, log, pow, fmod, floor, ceil)

// The compiler emits a reference to _fltused whenever floating point is used.
int _fltused = 0x9875;

__declspec(noreturn) void __cdecl mo_nocrt_fatal(const char* reason);

// ---------------------------------------------------------------------------
// Memory and string primitives
// ---------------------------------------------------------------------------
void* __cdecl memset(void* destination, int value, size_t count)
{
#if defined(_M_IX86) || defined(_M_X64)
  __stosb(static_cast<unsigned char*>(destination), static_cast<unsigned char>(value), count);
#else
  auto* output = static_cast<volatile unsigned char*>(destination);
  while (count--) *output++ = static_cast<unsigned char>(value);
#endif
  return destination;
}

void* __cdecl memcpy(void* destination, const void* source, size_t count)
{
#if defined(_M_IX86) || defined(_M_X64)
  __movsb(static_cast<unsigned char*>(destination), static_cast<const unsigned char*>(source), count);
#else
  auto* output = static_cast<volatile unsigned char*>(destination);
  auto* input = static_cast<const volatile unsigned char*>(source);
  while (count >= 8 && ((reinterpret_cast<size_t>(output) | reinterpret_cast<size_t>(input)) & 7) == 0)
  {
    *reinterpret_cast<volatile u64*>(output) = *reinterpret_cast<const volatile u64*>(input);
    output += 8; input += 8; count -= 8;
  }
  while (count--) *output++ = *input++;
#endif
  return destination;
}

void* __cdecl memmove(void* destination, const void* source, size_t count)
{
  auto* output = static_cast<unsigned char*>(destination);
  auto* input = static_cast<const unsigned char*>(source);
  if (output == input || !count) return destination;
  if (output < input || output >= input + count) return memcpy(destination, source, count);
  auto* backOut = static_cast<volatile unsigned char*>(output + count);
  auto* backIn = static_cast<const volatile unsigned char*>(input + count);
  while (count--) *--backOut = *--backIn;
  return destination;
}

int __cdecl memcmp(const void* left, const void* right, size_t count)
{
  auto* a = static_cast<const volatile unsigned char*>(left);
  auto* b = static_cast<const volatile unsigned char*>(right);
  for (; count; --count, ++a, ++b) if (*a != *b) return *a < *b ? -1 : 1;
  return 0;
}

void* __cdecl memchr(const void* buffer, int value, size_t count)
{
  auto* p = static_cast<const volatile unsigned char*>(buffer);
  for (; count; --count, ++p) if (*p == static_cast<unsigned char>(value)) return const_cast<unsigned char*>(const_cast<const unsigned char*>(p));
  return nullptr;
}

size_t __cdecl strlen(const char* text)
{
  const volatile char* p = text;
  while (*p) ++p;
  return static_cast<size_t>(p - text);
}

size_t __cdecl wcslen(const wchar_t* text)
{
  const volatile wchar_t* p = text;
  while (*p) ++p;
  return static_cast<size_t>(p - text);
}

long __cdecl wcstol(const wchar_t* text, wchar_t** end, int base)
{
  const wchar_t* p = text;
  while (*p == L' ' || *p == L'\t' || *p == L'\n' || *p == L'\r' || *p == L'\f' || *p == L'\v') ++p;
  bool negative = false;
  if (*p == L'+' || *p == L'-') { negative = *p == L'-'; ++p; }
  if ((base == 0 || base == 16) && p[0] == L'0' && (p[1] == L'x' || p[1] == L'X')) { p += 2; base = 16; }
  else if (base == 0) base = p[0] == L'0' ? 8 : 10;
  const wchar_t* digitsStart = p;
  unsigned long value = 0;
  for (;; ++p)
  {
    int digit;
    if (*p >= L'0' && *p <= L'9') digit = *p - L'0';
    else if (*p >= L'a' && *p <= L'z') digit = *p - L'a' + 10;
    else if (*p >= L'A' && *p <= L'Z') digit = *p - L'A' + 10;
    else break;
    if (digit >= base) break;
    const unsigned long next = value * static_cast<unsigned long>(base) + static_cast<unsigned long>(digit);
    value = next < value ? 0xFFFFFFFFul : next;
  }
  if (end) *end = const_cast<wchar_t*>(p == digitsStart ? text : p);
  if (negative) return value > 0x80000000ul ? static_cast<long>(-2147483647L - 1) : -static_cast<long>(value);
  return value > 0x7FFFFFFFul ? 2147483647L : static_cast<long>(value);
}

int __cdecl iswctype(wint_t character, wctype_t mask)
{
  wchar_t c = static_cast<wchar_t>(character);
  unsigned short types = 0;
  if (!GetStringTypeW(1 /* CT_CTYPE1 */, &c, 1, &types)) return 0;
  return types & mask;
}
int __cdecl iswspace(wint_t c) { return iswctype(c, 0x0008); }
int __cdecl iswdigit(wint_t c) { return c >= L'0' && c <= L'9'; }
int __cdecl iswalpha(wint_t c) { return iswctype(c, 0x0103); }
int __cdecl iswupper(wint_t c) { return iswctype(c, 0x0001); }
int __cdecl iswlower(wint_t c) { return iswctype(c, 0x0002); }
wint_t __cdecl towlower(wint_t c) { return static_cast<wint_t>(reinterpret_cast<size_t>(CharLowerW(reinterpret_cast<wchar_t*>(static_cast<size_t>(c))))); }
wint_t __cdecl towupper(wint_t c) { return static_cast<wint_t>(reinterpret_cast<size_t>(CharUpperW(reinterpret_cast<wchar_t*>(static_cast<size_t>(c))))); }

int __cdecl abs(int value) { return value < 0 ? -value : value; }
long __cdecl labs(long value) { return value < 0 ? -value : value; }

// ---------------------------------------------------------------------------
// Failure hooks the CRT headers and the STL reference.
// ---------------------------------------------------------------------------
int __cdecl _purecall(void) { mo_nocrt_fatal("pure virtual function call"); }
__declspec(noreturn) void __cdecl abort(void) { mo_nocrt_fatal("abort"); }
__declspec(noreturn) void __cdecl _invalid_parameter_noinfo_noreturn(void) { mo_nocrt_fatal("invalid parameter"); }
void __cdecl _invalid_parameter_noinfo(void) { mo_nocrt_fatal("invalid parameter"); }
void __cdecl _invalid_parameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, size_t) { mo_nocrt_fatal("invalid parameter"); }
__declspec(noreturn) void __cdecl _invoke_watson(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, size_t) { mo_nocrt_fatal("fatal runtime error"); }
int __cdecl _CrtDbgReport(int, const char*, int, const char*, const char*, ...) { return 0; }
int __cdecl _CrtDbgReportW(int, const wchar_t*, int, const wchar_t*, const wchar_t*, ...) { return 0; }

// ---------------------------------------------------------------------------
// Math. Accuracy is close to correctly rounded for the ranges the overlay uses
// (spectrum analysis, easing, hue rotation); special values follow C rules.
// ---------------------------------------------------------------------------
static inline u64 bitsOf(double value) { union { double d; u64 u; } v; v.d = value; return v.u; }
static inline double fromBits(u64 value) { union { double d; u64 u; } v; v.u = value; return v.d; }
static inline bool isNan(double value) { return value != value; }
static inline bool isInf(double value) { return (bitsOf(value) & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull; }
// Macros rather than globals so nothing here needs a dynamic initializer.
#define kInfinity fromBits(0x7FF0000000000000ull)
#define kNan fromBits(0x7FF8000000000000ull)
static const double kLn2Hi = 6.93147180369123816490e-01;
static const double kLn2Lo = 1.90821492927058770002e-10;

double __cdecl fabs(double value) { return fromBits(bitsOf(value) & 0x7FFFFFFFFFFFFFFFull); }

static double truncate(double value)
{
  if (isNan(value) || isInf(value) || fabs(value) >= 4503599627370496.0) return value; // >= 2^52 is already integral
  return static_cast<double>(static_cast<i64>(value));
}

double __cdecl floor(double value) { const double t = truncate(value); return t > value ? t - 1.0 : t; }
double __cdecl ceil(double value) { const double t = truncate(value); return t < value ? t + 1.0 : t; }

double __cdecl sqrt(double value)
{
  if (isNan(value) || value < 0) return value == 0 ? value : kNan;
  if (value == 0 || isInf(value)) return value;
  // Halve the exponent for a first guess, then refine with Newton's method.
  i64 bits = static_cast<i64>(bitsOf(value)) - 0x3FF0000000000000ll;
  bits >>= 1;
  double guess = fromBits(static_cast<u64>(bits + 0x3FF0000000000000ll));
  for (int i = 0; i < 7; ++i) guess = 0.5 * (guess + value / guess);
  return guess;
}

static double scale2(double value, int exponent)
{
  while (exponent > 1023) { value *= 0x1p1023; exponent -= 1023; }
  while (exponent < -1022) { value *= 0x1p-1022; exponent += 1022; }
  return value * fromBits(static_cast<u64>(exponent + 1023) << 52);
}

double __cdecl exp(double value)
{
  if (isNan(value)) return value;
  if (value > 709.782712893384) return kInfinity;
  if (value < -745.1332191019411) return 0.0;
  const int k = static_cast<int>(value * 1.44269504088896338700 + (value >= 0 ? 0.5 : -0.5));
  const double r = (value - k * kLn2Hi) - k * kLn2Lo;
  double term = 1.0, sum = 1.0;
  for (int i = 1; i <= 14; ++i) { term *= r / i; sum += term; }
  return scale2(sum, k);
}

double __cdecl log(double value)
{
  if (isNan(value)) return value;
  if (value < 0) return kNan;
  if (value == 0) return -kInfinity;
  if (isInf(value)) return value;
  u64 bits = bitsOf(value);
  int adjust = 0;
  if (((bits >> 52) & 0x7FF) == 0) { value *= 0x1p54; bits = bitsOf(value); adjust = 54; }
  int exponent = static_cast<int>((bits >> 52) & 0x7FF) - 1023 - adjust;
  double mantissa = fromBits((bits & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull);
  if (mantissa > 1.41421356237309504880) { mantissa *= 0.5; ++exponent; }
  const double f = (mantissa - 1.0) / (mantissa + 1.0), s = f * f;
  const double series = 1.0 + s * (1.0 / 3 + s * (1.0 / 5 + s * (1.0 / 7 + s * (1.0 / 9 + s * (1.0 / 11 + s * (1.0 / 13 + s * (1.0 / 15 + s * (1.0 / 17 + s * (1.0 / 19 + s * (1.0 / 21))))))))));
  return exponent * kLn2Hi + (exponent * kLn2Lo + 2.0 * f * series);
}

double __cdecl pow(double base, double exponent)
{
  if (exponent == 0 || base == 1) return 1.0;
  if (isNan(base) || isNan(exponent)) return kNan;
  bool negate = false;
  if (base < 0)
  {
    if (truncate(exponent) != exponent) return kNan;
    if (fabs(exponent) < 9007199254740992.0) negate = (static_cast<i64>(exponent) & 1) != 0;
    base = -base;
  }
  if (base == 0) return exponent > 0 ? (negate ? -0.0 : 0.0) : (negate ? -kInfinity : kInfinity);
  const double result = exp(exponent * log(base));
  return negate ? -result : result;
}

double __cdecl fmod(double x, double y)
{
  if (isNan(x) || isNan(y) || isInf(x) || y == 0) return kNan;
  if (isInf(y)) return x;
  const double ay = fabs(y);
  double r = fabs(x);
  if (r < ay) return x;
  // Exact remainder by subtracting power-of-two multiples of |y|.
  while (r >= ay)
  {
    double d = ay;
    while (d <= r * 0.5) d *= 2.0;
    r -= d;
  }
  return x < 0 ? -r : r;
}

static double sinKernel(double r)
{
  const double z = r * r;
  return r + r * z * (-1.66666666666666324348e-01 + z * (8.33333333332248946124e-03 + z * (-1.98412698298579493134e-04 + z * (2.75573137070700676789e-06 + z * (-2.50507602534068634195e-08 + z * 1.58969099521155010221e-10)))));
}
static double cosKernel(double r)
{
  const double z = r * r;
  return 1.0 - 0.5 * z + z * z * (4.16666666666666019037e-02 + z * (-1.38888888888741095749e-03 + z * (2.48015872894767294178e-05 + z * (-2.75573143513906633035e-07 + z * (2.08757232129817482790e-09 + z * -1.13596475577881948265e-11)))));
}
static int reduceQuarterTurns(double value, double& remainder)
{
  if (fabs(value) > 1.0e8) value = fmod(value, 6.28318530717958647692);
  const int k = static_cast<int>(value * 6.36619772367581382433e-01 + (value >= 0 ? 0.5 : -0.5));
  remainder = (value - k * 1.57079632673412561417e+00) - k * 6.07710050650619224932e-11;
  return k & 3;
}

double __cdecl sin(double value)
{
  if (isNan(value) || isInf(value)) return kNan;
  double r;
  switch (reduceQuarterTurns(value, r)) { case 0: return sinKernel(r); case 1: return cosKernel(r); case 2: return -sinKernel(r); default: return -cosKernel(r); }
}

double __cdecl cos(double value)
{
  if (isNan(value) || isInf(value)) return kNan;
  double r;
  switch (reduceQuarterTurns(value, r)) { case 0: return cosKernel(r); case 1: return -sinKernel(r); case 2: return -cosKernel(r); default: return sinKernel(r); }
}

// On x86 the CRT headers implement the float variants inline on top of the
// double functions; elsewhere they are real exports that must be provided.
#if !defined(_M_IX86)
#pragma function(sqrtf, sinf, cosf, expf, logf, powf, fmodf, floorf, ceilf)
float __cdecl sqrtf(float value) { return static_cast<float>(sqrt(value)); }
float __cdecl sinf(float value) { return static_cast<float>(sin(value)); }
float __cdecl cosf(float value) { return static_cast<float>(cos(value)); }
float __cdecl expf(float value) { return static_cast<float>(exp(value)); }
float __cdecl logf(float value) { return static_cast<float>(log(value)); }
float __cdecl powf(float base, float exponent) { return static_cast<float>(pow(base, exponent)); }
float __cdecl fmodf(float x, float y) { return static_cast<float>(fmod(x, y)); }
float __cdecl floorf(float value) { return static_cast<float>(floor(value)); }
float __cdecl ceilf(float value) { return static_cast<float>(ceil(value)); }
#endif
#if defined(_M_ARM64) || defined(_M_ARM64EC)
float __cdecl fabsf(float value) { return static_cast<float>(fabs(value)); }
#endif
}
