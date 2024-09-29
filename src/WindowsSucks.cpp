#include <windows.h>
#include <winuser.h>
#include <vector>
#include <algorithm>
#include "globals.h"
#include "HwndClass.h"
#include <stdio.h>

// extern "C" LRESULT CALLBACK cbtProc(int nCode, WPARAM wParam, LPARAM lParam) {
//
//   if (nCode != HCBT_CREATEWND || nCode != HCBT_DESTROYWND) {
//     return CallNextHookEx(NULL, nCode, wParam, lParam);
//   }
//
//   HWND hwnd = (HWND)wParam;
//
//   if (nCode == HCBT_CREATEWND) {
//     char className[256];
//     GetClassNameA(hwnd, className, sizeof(className));
//     gwinVec->push_back(HwndClass{.hwnd = hwnd, .className = className});
//   } else {
//     std::remove_if(
//         gwinVec->begin(), gwinVec->end(),
//         [hwnd](HwndClass &hwndClass) { return hwndClass.hwnd == hwnd; });
//   }
//
//   printf("After CBTProc: \n");
//   for (auto &item : *gwinVec) {
//     printf("Handle: %p, class: %s\n", item.hwnd, item.className.c_str());
//   }
//
//   return CallNextHookEx(NULL, nCode, wParam, lParam);
// }
//

HHOOK g_hook;

extern "C" LRESULT CALLBACK windowProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        CWPSTRUCT* pMsg = reinterpret_cast<CWPSTRUCT*>(lParam);

        switch (pMsg->message) {
            case WM_CREATE:
                *NULL;
                printf("WM_CREATE: HWND = %p", pMsg->hwnd);
                fflush(stdout);
                break;
            case WM_DESTROY:
                printf("WM_DESTROY: HWND = %p", pMsg->hwnd);
                fflush(stdout);
                break;
            case WM_SETFOCUS:
                printf("WM_SETFOCUS: HWND = %p", pMsg->hwnd);
                fflush(stdout);
                break;
        }
    }

    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            g_hook = NULL;
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
