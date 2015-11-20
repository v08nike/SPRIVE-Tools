// Copyright (c) 2015 The Khronos Group Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and/or associated documentation files (the
// "Materials"), to deal in the Materials without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Materials, and to
// permit persons to whom the Materials are furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Materials.
//
// MODIFICATIONS TO THIS FILE MAY MEAN IT NO LONGER ACCURATELY REFLECTS
// KHRONOS STANDARDS. THE UNMODIFIED, NORMATIVE VERSIONS OF KHRONOS
// SPECIFICATIONS AND HEADER INFORMATION ARE LOCATED AT
//    https://www.khronos.org/registry/
//
// THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.

#include "print.h"

#if defined(SPIRV_LINUX) || defined(SPIRV_MAC)
namespace libspirv {

clr::reset::operator const char*() { return "\e[0m"; }

clr::grey::operator const char*() { return "\e[1;30m"; }

clr::red::operator const char*() { return "\e[31m"; }

clr::green::operator const char*() { return "\e[32m"; }

clr::yellow::operator const char*() { return "\e[33m"; }

clr::blue::operator const char*() { return "\e[34m"; }

}  // namespace libspirv
#elif defined(SPIRV_WINDOWS)
#include <Windows.h>

namespace libspirv {

clr::reset::operator const char*() {
  const DWORD color = 0Xf;
  HANDLE hConsole;
  hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  hConsole = GetStdHandle(STD_ERROR_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  return "";
}

clr::grey::operator const char*() {
  const DWORD color = 0x8;
  HANDLE hConsole;
  hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  hConsole = GetStdHandle(STD_ERROR_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  return "";
}

clr::red::operator const char*() {
  const DWORD color = 0x4;
  HANDLE hConsole;
  hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  hConsole = GetStdHandle(STD_ERROR_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  return "";
}

clr::green::operator const char*() {
  const DWORD color = 0x2;
  HANDLE hConsole;
  hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  hConsole = GetStdHandle(STD_ERROR_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  return "";
}

clr::yellow::operator const char*() {
  const DWORD color = 0x6;
  HANDLE hConsole;
  hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  hConsole = GetStdHandle(STD_ERROR_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  return "";
}

clr::blue::operator const char*() {
  const DWORD color = 0x1;
  HANDLE hConsole;
  hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  hConsole = GetStdHandle(STD_ERROR_HANDLE);
  SetConsoleTextAttribute(hConsole, color);
  return "";
}

}  // namespace libspirv
#endif
