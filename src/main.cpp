#include <stdio.h>
#include <windows.h>
#include <winuser.h>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include "WindowsSucks.h"
#include "globals.h"

#define MAX_ATTEMPTS 5
#define ATTEMPT_DELAY 100 // milliseconds

// might want to use GetWindow() to add raise and lower functionality
// https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindow

const char *unwantedClasses[] = {
    "Windows.UI.Core.CoreWindow", // to get rid of "Microsoft Text Input
                                  // Application"
};

BOOL isUnwantedClass(const char *className) {
  for (size_t i = 0; i < sizeof(unwantedClasses) / sizeof(unwantedClasses[0]);
       i++) {
    if (strcmp(className, unwantedClasses[i]) == 0) {
      return TRUE;
    }
  }
  return FALSE;
}

void printLastError() {
    DWORD errorMessageID = GetLastError();
    if (errorMessageID == 0) {
        return; // No error
    }

    LPSTR messageBuffer = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);

    fprintf(stderr, "Error: %s\n", messageBuffer);
    LocalFree(messageBuffer);
}

// an actual window is a window that's not stupid
BOOL isActualWindow(HWND hwnd) {
  // skip invisible windows
  if (!IsWindowVisible(hwnd)) {
    return FALSE;
  }

  // skip windows without any size
  RECT rect;
  if (!GetWindowRect(hwnd, &rect)) {
    return FALSE;
  }
  if (rect.right - rect.left <= 0 || rect.bottom - rect.top <= 0) {
    return FALSE;
  }

  // skip windows with certain styles (like tool windows)
  LONG style = GetWindowLong(hwnd, GWL_STYLE);
  LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
  // some system windows can be children even though its called through
  // EnumWindows
  if ((style & WS_CHILD) || (exStyle & WS_EX_TOOLWINDOW)) {
    return FALSE;
  }

  // get the window title
  char title[256];
  GetWindowTextA(hwnd, title, sizeof(title));

  // skip windows without a title
  if (strlen(title) == 0) {
    return FALSE;
  }

  char className[256];
  GetClassNameA(hwnd, className, sizeof(className));

  if (isUnwantedClass(className)) {
    return FALSE;
  }

  return TRUE;
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

  if (isUnwantedClass(className)) {
    return TRUE;
  }

  // likely a "real" window
  printf("Window Handle: %p, Title: %s, className: %s\n", hwnd, title,
         className);

  return TRUE;
}

// std::vector<HandleClass> v;

BOOL CALLBACK PopulateWinVec(HWND hwnd, LPARAM lParam) {
    if (isActualWindow(hwnd)) {
        auto v = reinterpret_cast<std::vector<HwndClass>*>(lParam);

        char className[256];
        GetClassNameA(hwnd, className, sizeof(className));

        struct HwndClass data = {.hwnd = hwnd, .className = className};
        v->push_back(data);

    }

    return TRUE;
}

// windows can be quite stubborn about focus management
// After a lot of trial and errors I found out that retrying is reliable
void raise(HWND hwnd) {

  if (hwnd == NULL) {
    hwnd = FindWindow("MozillaWindowClass", NULL);
  }

  if (hwnd == NULL) {
    fprintf(stderr, "Failed to find Firefox window handle\n");
    return;
  }

  DWORD currentThreadId = GetCurrentThreadId();
  DWORD firefoxThreadId = GetWindowThreadProcessId(hwnd, NULL);

  for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
    // attach threads
    AttachThreadInput(currentThreadId, firefoxThreadId, TRUE);

    // show and restore window
    ShowWindow(hwnd, SW_SHOW);
    if (IsIconic(hwnd)) {
      ShowWindow(hwnd, SW_RESTORE);
    }

    // bring window to foreground
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);

    // set focus
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    // detach threads
    AttachThreadInput(currentThreadId, firefoxThreadId, FALSE);

    // wait and check focus
    Sleep(ATTEMPT_DELAY);
    if (GetForegroundWindow() == hwnd) {
      printf("Successfully set focus to window on attempt %d\n", attempt + 1);
      return;
    }
  }

  fprintf(stderr, "Error: Failed to set focus to window after %d attempts\n",
          MAX_ATTEMPTS);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode < 0) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  // check for WM_KEYDOWN or WM_SYSKEYDOWN only, to avoid duplicate prints on
  // key release
  BOOL keyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
  if (nCode == HC_ACTION && keyDown) {
    tagKBDLLHOOKSTRUCT *kbd = (tagKBDLLHOOKSTRUCT *)lParam;
    if (kbd->vkCode == VK_F9) {
      raise(NULL);
      return 1; // swallow shortcut
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

LRESULT CALLBACK cbtProc(int nCode, WPARAM wParam, LPARAM lParam) {

    if (nCode != HCBT_CREATEWND || nCode != HCBT_DESTROYWND) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    HWND hwnd = (HWND) wParam;

    if (nCode == HCBT_CREATEWND) {
        char className[256];
        GetClassNameA(hwnd, className, sizeof(className));
        gwinVec->push_back(HwndClass {.hwnd = hwnd, .className = className});
    } else {
        std::remove_if(gwinVec->begin(), gwinVec->end(), [hwnd](HwndClass& hwndClass) {
            return hwndClass.hwnd == hwnd;
        });
    }

    printf("After CBTProc: \n");
    for (auto &item : *gwinVec) {
        printf("Handle: %p, class: %s\n", item.hwnd, item.className.c_str());
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

HHOOK g_hook;

// TODO: fix race conditions
void listenForWindows() {

    HMODULE dll = LoadLibraryA("WindowsSucks2.dll");

    if (dll == NULL) {
        fprintf(stderr, "failed to load dll: ");
        printLastError();
        return;
    }

    FARPROC proc = GetProcAddress(dll, "windowProc");
    if (proc == NULL) {
        fprintf(stderr, "failed to load proc: ");
        printLastError();
        return;
    }

    auto windowProc = reinterpret_cast<LRESULT (CALLBACK *)(int, WPARAM, LPARAM)>(proc);

    HHOOK g_hook = SetWindowsHookExA(WH_CALLWNDPROC, windowProc, dll, 0);
    if (g_hook == NULL) {
        fprintf(stderr, "failed to set hook: ");
        printLastError();
        return;
    }

    // MSG msg;
    // while (GetMessage(&msg, NULL, 0, 0)) {
    //     printf("Got message\n");
    //     TranslateMessage(&msg);
    //     DispatchMessage(&msg);
    // }

    getchar(); // Wait for user input

    UnhookWindowsHookEx(g_hook);
    FreeLibrary(dll);
}

int main() {
    gwinVec = std::make_unique<std::vector<HwndClass>>();
    EnumWindows(PopulateWinVec, reinterpret_cast<LPARAM>(gwinVec.get()));

    listenForWindows();

    return 0;
}
