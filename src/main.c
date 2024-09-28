#include <stdio.h>
#include <windows.h>
#include <winuser.h>

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // we dont care about the alt key information from wParam

    if (nCode == HC_ACTION ) {
        tagKBDLLHOOKSTRUCT *kbd = (tagKBDLLHOOKSTRUCT *) lParam;
        if (kbd->vkCode == VK_TAB) {
            printf("You pressed tab\n");
        } else {
            printf("you pressed something that has value %x\n", wParam);
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}


int main() {

    HHOOK hook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);

    if (hook == NULL) {
        fprintf(stderr, "failed to set hook: %s", GetLastError());
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
