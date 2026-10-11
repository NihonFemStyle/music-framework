// 32-bit x86 compiler helpers for the no-CRT build.
//
// On x86, MSVC lowers 64-bit integer multiply/divide/shift and several
// floating-point conversions to calls into helper routines that normally live
// in the CRT. Windows does not export them for user mode, so they are all
// implemented here.
//
// Every routine works on 32-bit halves only: an unoptimized (Debug) build turns
// even constant 64-bit shifts into helper calls, so 64-bit arithmetic in this
// file would recurse into itself.
//
// Calling conventions (MSVC x86 helper ABI):
//   _allmul/_alldiv/_aulldiv/_allrem/_aullrem : two 64-bit stack args, callee pops 16 bytes, result in EDX:EAX
//   _alldvrm/_aulldvrm                        : as above, quotient in EDX:EAX, remainder in EBX:ECX
//   _allshl/_allshr/_aullshr                  : value in EDX:EAX, count in CL, result in EDX:EAX
//   _dtol3/_dtoul3/_dtoui3                    : double in XMM0 -> EDX:EAX (or EAX)
//   _ftol3/_ftoul3/_ftoui3                    : float in XMM0  -> EDX:EAX (or EAX)
//   _ltod3/_ultod3/_ltof3/_ultof3             : 64-bit in EDX:ECX -> XMM0
// The conversions use the x87 unit (fisttp requires SSE3, which every CPU
// capable of running Windows 10 provides).
#if defined(_M_IX86)

extern "C" unsigned __int64 __emulu(unsigned int a, unsigned int b);
#pragma intrinsic(__emulu)

namespace {
union Word64 { unsigned __int64 value; struct { unsigned int lo, hi; } half; };

void negate(unsigned int& lo, unsigned int& hi)
{
  lo = ~lo + 1u;
  hi = ~hi + (lo == 0u ? 1u : 0u);
}

// Restoring shift-subtract division on 32-bit halves.
void udivrem(unsigned int al, unsigned int ah, unsigned int bl, unsigned int bh,
             unsigned int& ql, unsigned int& qh, unsigned int& rl, unsigned int& rh)
{
  ql = qh = rl = rh = 0u;
  if (!bl && !bh) return; // division by zero: no CRT trap to raise, return zero
  if (!ah && !bh) { ql = al / bl; rl = al % bl; return; }
  for (int bit = 0; bit < 64; ++bit)
  {
    const unsigned int carry = rh >> 31;
    rh = (rh << 1) | (rl >> 31);
    rl = (rl << 1) | (ah >> 31);
    ah = (ah << 1) | (al >> 31);
    al <<= 1;
    qh = (qh << 1) | (ql >> 31);
    ql <<= 1;
    if (carry || rh > bh || (rh == bh && rl >= bl))
    {
      const unsigned int borrow = rl < bl ? 1u : 0u;
      rl -= bl;
      rh = rh - bh - borrow;
      ql |= 1u;
    }
  }
}

void sdivrem(Word64 a, Word64 b, Word64& q, Word64& r)
{
  const bool negativeA = (a.half.hi >> 31) != 0u, negativeB = (b.half.hi >> 31) != 0u;
  if (negativeA) negate(a.half.lo, a.half.hi);
  if (negativeB) negate(b.half.lo, b.half.hi);
  udivrem(a.half.lo, a.half.hi, b.half.lo, b.half.hi, q.half.lo, q.half.hi, r.half.lo, r.half.hi);
  if (negativeA != negativeB) negate(q.half.lo, q.half.hi);
  if (negativeA) negate(r.half.lo, r.half.hi); // remainder takes the dividend's sign
}
}

extern "C" {
unsigned __int64 __stdcall mo_x86_allmul(unsigned __int64 a, unsigned __int64 b)
{
  Word64 x, y, product;
  x.value = a; y.value = b;
  product.value = __emulu(x.half.lo, y.half.lo);
  product.half.hi += x.half.lo * y.half.hi + x.half.hi * y.half.lo;
  return product.value;
}
unsigned __int64 __stdcall mo_x86_aulldiv(unsigned __int64 a, unsigned __int64 b)
{
  Word64 x, y, q, r;
  x.value = a; y.value = b;
  udivrem(x.half.lo, x.half.hi, y.half.lo, y.half.hi, q.half.lo, q.half.hi, r.half.lo, r.half.hi);
  return q.value;
}
unsigned __int64 __stdcall mo_x86_aullrem(unsigned __int64 a, unsigned __int64 b)
{
  Word64 x, y, q, r;
  x.value = a; y.value = b;
  udivrem(x.half.lo, x.half.hi, y.half.lo, y.half.hi, q.half.lo, q.half.hi, r.half.lo, r.half.hi);
  return r.value;
}
__int64 __stdcall mo_x86_alldiv(__int64 a, __int64 b)
{
  Word64 x, y, q, r;
  x.value = static_cast<unsigned __int64>(a); y.value = static_cast<unsigned __int64>(b);
  sdivrem(x, y, q, r);
  return static_cast<__int64>(q.value);
}
__int64 __stdcall mo_x86_allrem(__int64 a, __int64 b)
{
  Word64 x, y, q, r;
  x.value = static_cast<unsigned __int64>(a); y.value = static_cast<unsigned __int64>(b);
  sdivrem(x, y, q, r);
  return static_cast<__int64>(r.value);
}
void __stdcall mo_x86_udivrem(unsigned __int64 a, unsigned __int64 b, unsigned __int64* quotient, unsigned __int64* remainder)
{
  Word64 x, y, q, r;
  x.value = a; y.value = b;
  udivrem(x.half.lo, x.half.hi, y.half.lo, y.half.hi, q.half.lo, q.half.hi, r.half.lo, r.half.hi);
  *quotient = q.value; *remainder = r.value;
}
void __stdcall mo_x86_sdivrem(unsigned __int64 a, unsigned __int64 b, unsigned __int64* quotient, unsigned __int64* remainder)
{
  Word64 x, y, q, r;
  x.value = a; y.value = b;
  sdivrem(x, y, q, r);
  *quotient = q.value; *remainder = r.value;
}

// The stack-argument helpers have exactly the __stdcall layout (callee pops
// 16 bytes, result in EDX:EAX); thin naked entry points give them the
// undecorated names the compiler references.
__declspec(naked) void _allmul() { __asm jmp mo_x86_allmul }
__declspec(naked) void _aulldiv() { __asm jmp mo_x86_aulldiv }
__declspec(naked) void _aullrem() { __asm jmp mo_x86_aullrem }
__declspec(naked) void _alldiv() { __asm jmp mo_x86_alldiv }
__declspec(naked) void _allrem() { __asm jmp mo_x86_allrem }

// Quotient in EDX:EAX, remainder in EBX:ECX.
__declspec(naked) void _aulldvrm()
{
  __asm {
    sub esp, 16                // [esp] = quotient, [esp+8] = remainder
    lea eax, [esp + 8]
    push eax                   // &remainder
    lea eax, [esp + 4]
    push eax                   // &quotient
    push dword ptr [esp + 40]  // divisor high
    push dword ptr [esp + 40]  // divisor low
    push dword ptr [esp + 40]  // dividend high
    push dword ptr [esp + 40]  // dividend low
    call mo_x86_udivrem        // pops 24 bytes
    mov eax, dword ptr [esp]
    mov edx, dword ptr [esp + 4]
    mov ecx, dword ptr [esp + 8]
    mov ebx, dword ptr [esp + 12]
    add esp, 16
    ret 16
  }
}
__declspec(naked) void _alldvrm()
{
  __asm {
    sub esp, 16
    lea eax, [esp + 8]
    push eax
    lea eax, [esp + 4]
    push eax
    push dword ptr [esp + 40]
    push dword ptr [esp + 40]
    push dword ptr [esp + 40]
    push dword ptr [esp + 40]
    call mo_x86_sdivrem
    mov eax, dword ptr [esp]
    mov edx, dword ptr [esp + 4]
    mov ecx, dword ptr [esp + 8]
    mov ebx, dword ptr [esp + 12]
    add esp, 16
    ret 16
  }
}

__declspec(naked) void _allshl()
{
  __asm {
    cmp cl, 64
    jae all_zero
    cmp cl, 32
    jae over_32
    shld edx, eax, cl
    shl eax, cl
    ret
  over_32:
    mov edx, eax
    xor eax, eax
    and cl, 31
    shl edx, cl
    ret
  all_zero:
    xor eax, eax
    xor edx, edx
    ret
  }
}
__declspec(naked) void _allshr()
{
  __asm {
    cmp cl, 64
    jae sign_fill
    cmp cl, 32
    jae over_32
    shrd eax, edx, cl
    sar edx, cl
    ret
  over_32:
    mov eax, edx
    sar edx, 31
    and cl, 31
    sar eax, cl
    ret
  sign_fill:
    sar edx, 31
    mov eax, edx
    ret
  }
}
__declspec(naked) void _aullshr()
{
  __asm {
    cmp cl, 64
    jae all_zero
    cmp cl, 32
    jae over_32
    shrd eax, edx, cl
    shr edx, cl
    ret
  over_32:
    mov eax, edx
    xor edx, edx
    and cl, 31
    shr eax, cl
    ret
  all_zero:
    xor eax, eax
    xor edx, edx
    ret
  }
}

static const double kTwoPow63 = 9223372036854775808.0;
static const double kTwoPow64 = 18446744073709551616.0;

__declspec(naked) void _dtol3()
{
  __asm {
    sub esp, 8
    movsd qword ptr [esp], xmm0
    fld qword ptr [esp]
    fisttp qword ptr [esp]
    mov eax, dword ptr [esp]
    mov edx, dword ptr [esp + 4]
    add esp, 8
    ret
  }
}

__declspec(naked) void _dtoui3()
{
  __asm {
    sub esp, 8
    movsd qword ptr [esp], xmm0
    fld qword ptr [esp]
    fisttp qword ptr [esp]
    mov eax, dword ptr [esp]
    add esp, 8
    ret
  }
}

__declspec(naked) void _dtoul3()
{
  __asm {
    sub esp, 8
    movsd qword ptr [esp], xmm0
    fld qword ptr [esp]
    fld qword ptr [kTwoPow63]
    fcomip st(0), st(1)
    ja below_two_pow_63
    fsub qword ptr [kTwoPow63]
    fisttp qword ptr [esp]
    mov eax, dword ptr [esp]
    mov edx, dword ptr [esp + 4]
    xor edx, 80000000h
    add esp, 8
    ret
  below_two_pow_63:
    fisttp qword ptr [esp]
    mov eax, dword ptr [esp]
    mov edx, dword ptr [esp + 4]
    add esp, 8
    ret
  }
}

__declspec(naked) void _ftol3()
{
  __asm {
    sub esp, 8
    movss dword ptr [esp], xmm0
    fld dword ptr [esp]
    fisttp qword ptr [esp]
    mov eax, dword ptr [esp]
    mov edx, dword ptr [esp + 4]
    add esp, 8
    ret
  }
}

__declspec(naked) void _ftoui3()
{
  __asm {
    sub esp, 8
    movss dword ptr [esp], xmm0
    fld dword ptr [esp]
    fisttp qword ptr [esp]
    mov eax, dword ptr [esp]
    add esp, 8
    ret
  }
}

__declspec(naked) void _ftoul3()
{
  __asm {
    sub esp, 8
    movss dword ptr [esp], xmm0
    fld dword ptr [esp]
    fld qword ptr [kTwoPow63]
    fcomip st(0), st(1)
    ja below_two_pow_63
    fsub qword ptr [kTwoPow63]
    fisttp qword ptr [esp]
    mov eax, dword ptr [esp]
    mov edx, dword ptr [esp + 4]
    xor edx, 80000000h
    add esp, 8
    ret
  below_two_pow_63:
    fisttp qword ptr [esp]
    mov eax, dword ptr [esp]
    mov edx, dword ptr [esp + 4]
    add esp, 8
    ret
  }
}

__declspec(naked) void _ltod3()
{
  __asm {
    push edx
    push ecx
    fild qword ptr [esp]
    fstp qword ptr [esp]
    movsd xmm0, qword ptr [esp]
    add esp, 8
    ret
  }
}

__declspec(naked) void _ultod3()
{
  __asm {
    push edx
    push ecx
    fild qword ptr [esp]
    test edx, edx
    jns not_negative
    fadd qword ptr [kTwoPow64]
  not_negative:
    fstp qword ptr [esp]
    movsd xmm0, qword ptr [esp]
    add esp, 8
    ret
  }
}

__declspec(naked) void _ltof3()
{
  __asm {
    push edx
    push ecx
    fild qword ptr [esp]
    fstp dword ptr [esp]
    movss xmm0, dword ptr [esp]
    add esp, 8
    ret
  }
}

__declspec(naked) void _ultof3()
{
  __asm {
    push edx
    push ecx
    fild qword ptr [esp]
    test edx, edx
    jns not_negative
    fadd qword ptr [kTwoPow64]
  not_negative:
    fstp dword ptr [esp]
    movss xmm0, dword ptr [esp]
    add esp, 8
    ret
  }
}
}

#endif
