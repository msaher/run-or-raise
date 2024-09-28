#include <stdio.h>
#include <windows.h>
#include <winuser.h>

#define MAX_ATTEMPTS 5
#define ATTEMPT_DELAY 100 // milliseconds

BOOL IsUnwantedClass(const char *className) {
    for (size_t i = 0; i < sizeof(unwantedClasses) / sizeof(unwantedClasses[0]); i++) {
        if (strcmp(className, unwantedClasses[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}


BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    // skip invisible windows
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    // skip windows without any size
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        return TRUE;
    }
    if (rect.right - rect.left <= 0 || rect.bottom - rect.top <= 0) {
        return TRUE;
    }

    // skip windows with certain styles (like tool windows)
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    // some system windows can be children even though its called through
    // EnumWindows
    if ((style & WS_CHILD) || (exStyle & WS_EX_TOOLWINDOW)) {
        return TRUE;
    }

    // get the window title
    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));

    // skip windows without a title
    if (strlen(title) == 0) {
        return TRUE;
    }

    char className[256];
    GetClassNameA(hwnd, className, sizeof(className));

    if (IsUnwantedClass(className)) {
        return TRUE;
    }


    // likely a "real" window
    printf("Window Handle: %p, Title: %s, className: %s\n", hwnd, title, className);

    return TRUE;
}

// windows can be quite stubborn about focus management
// After a lot of trial and errors I found out that retrying is reliable
void raise() {
    HWND hwnd = FindWindow("MozillaWindowClass", NULL);
    if (hwnd == NULL) {
        fprintf(stderr, "Failed to find Firefox window handle\n");
        return;
    }

    DWORD currentThreadId = GetCurrentThreadId();
    DWORD firefoxThreadId = GetWindowThreadProcessId(hwnd, NULL);

    // Force our window to the foreground first
    // EnumWindows(EnumWindowsProc, 0);

    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        // Attach threads
        AttachThreadInput(currentThreadId, firefoxThreadId, TRUE);

        // Show and restore window
        ShowWindow(hwnd, SW_SHOW);
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }

        // Bring window to foreground
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);

        // Set focus
        SetActiveWindow(hwnd);
        SetFocus(hwnd);

        // RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

        // Detach threads
        AttachThreadInput(currentThreadId, firefoxThreadId, FALSE);

        // Wait and check focus
        Sleep(ATTEMPT_DELAY);
        if (GetForegroundWindow() == hwnd) {
            printf("Successfully set focus to Firefox window on attempt %d\n", attempt + 1);
            return;
        }
    }

    fprintf(stderr, "Warning: Failed to set focus to Firefox window after %d attempts\n", MAX_ATTEMPTS);
}

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

void messageLoop() {
    HHOOK hook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (hook == NULL) {
        fprintf(stderr, "failed to set hook: %ld", GetLastError());
        return;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hook);
}

int main() {

    messageLoop();

    return 0;
}
