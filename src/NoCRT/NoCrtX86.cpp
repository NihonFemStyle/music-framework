// 32-bit x86 compiler helpers for the no-CRT build.
//
// MSVC lowers some 64-bit and floating-point conversions on x86 to calls into
// helper routines that normally live in the CRT. The 64-bit integer helpers
// (_alldiv, _allmul, _aulldiv, _allshl, ...) are linked from ntdll.lib, which
// exports them as part of the operating system. The SSE2 conversion helpers
// introduced in Visual Studio 2019 are not exported by Windows, so they are
// implemented here using the x87 unit (fisttp requires SSE3, which every CPU
// capable of running Windows 10 provides).
//
// Register contract (MSVC x86):
//   _dtol3/_dtoul3/_dtoui3 : double in XMM0      -> EDX:EAX (or EAX)
//   _ftol3/_ftoul3/_ftoui3 : float  in XMM0      -> EDX:EAX (or EAX)
//   _ltod3/_ultod3         : 64-bit in EDX:ECX   -> double in XMM0
#if defined(_M_IX86)

extern "C" {
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
}

#endif
