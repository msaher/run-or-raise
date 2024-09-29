#include <string>
#include <windows.h>
#ifndef HWND_CLASS_H
#define HWND_CLASS_H

struct HwndClass {
  HWND hwnd;
  std::string className;
};

#endif
