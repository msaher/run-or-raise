#include <windows.h>
#include <winuser.h>
#include <vector>
#include "globals.h"
#include "HwndClass.h"
#include <algorithm>

extern "C" LRESULT CALLBACK cbtProc(int nCode, WPARAM wParam, LPARAM lParam) {

  if (nCode != HCBT_CREATEWND || nCode != HCBT_DESTROYWND) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  HWND hwnd = (HWND)wParam;

  if (nCode == HCBT_CREATEWND) {
    char className[256];
    GetClassNameA(hwnd, className, sizeof(className));
    gwinVec->push_back(HwndClass{.hwnd = hwnd, .className = className});
  } else {
    std::remove_if(
        gwinVec->begin(), gwinVec->end(),
        [hwnd](HwndClass &hwndClass) { return hwndClass.hwnd == hwnd; });
  }

  printf("After CBTProc: \n");
  for (auto &item : *gwinVec) {
    printf("Handle: %p, class: %s\n", item.hwnd, item.className.c_str());
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}
