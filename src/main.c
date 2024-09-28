#include <stdio.h>
#include <windows.h>
#include <winuser.h>

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // Check for WM_KEYDOWN or WM_SYSKEYDOWN only, to avoid duplicate prints on key release
    bool keyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    if (nCode == HC_ACTION && keyDown) {
        tagKBDLLHOOKSTRUCT *kbd = (tagKBDLLHOOKSTRUCT *)lParam;
        if (kbd->vkCode == VK_F9) {
            raise();
            return 1;
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main() {

    HHOOK hook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);

    if (hook == NULL) {
        fprintf(stderr, "failed to set hook: %ld", GetLastError());
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }


    UnhookWindowsHookEx(hook);
    return 0;
}
