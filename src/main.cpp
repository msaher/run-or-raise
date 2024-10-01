#include <algorithm>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <winuser.h>
#include <ctype.h>
#define strequal(x, y) strcmp(x, y) == 0

#define MAX_ATTEMPTS 5
#define ATTEMPT_DELAY 50 // milliseconds

// TODO: disable logging or turn it on with -v

struct HwndClass {
    HWND hwnd;
    std::string className;
};

struct CmdClass {
    std::string cmdLine;
    std::string className;
};

enum KbdModifier {
    NONE = 0,
    CTRL = 1 << 0,
    SHIFT = 1 << 1,
    ALT = 1 << 2,
};

// overload operator==() so it plays nicely with maps
class KbdShortcut {
public:
    BYTE flags;
    DWORD key;

    bool operator==(const KbdShortcut &rhs) const;
};

bool KbdShortcut::operator==(const KbdShortcut &rhs) const {
    return this->flags == rhs.flags && this->key == rhs.key;
}

// specialize std::hash to please std::unordered_map
template <> struct std::hash<KbdShortcut> {
    std::size_t operator()(const KbdShortcut &k) const {
        // XOR and shift values to minimize collisions
        return hash<BYTE>()(k.flags) ^ (hash<int>()(k.key) << 1);
    }
};

const char *unwantedClasses[] = {
    "Windows.UI.Core.CoreWindow", // to get rid of "Microsoft Text Input
    // Application"
};

const int INVALID_SHORTCUT = 1;

std::unordered_map<KbdShortcut, CmdClass> g_keymaps;
std::vector<HwndClass> g_winVec;
HHOOK g_hook;

BYTE getActiveModifiers() {
    BYTE res = 0;
    if (GetKeyState(VK_CONTROL) < 0) {
        res |= CTRL;
    }

    if (GetKeyState(VK_SHIFT) < 0) {
        res |= SHIFT;
    }

    if (GetKeyState(VK_MENU) < 0) {
        res |= ALT;
    }

    return res;
}

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
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, errorMessageID,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
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

BOOL CALLBACK PopulateWinVec(HWND hwnd, LPARAM lParam) {
    if (isActualWindow(hwnd)) {
        auto v = reinterpret_cast<std::vector<HwndClass> *>(lParam);
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

    DWORD currentThreadId = GetCurrentThreadId();
    DWORD targetThreadId = GetWindowThreadProcessId(hwnd, NULL);

    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        // attach threads
        AttachThreadInput(currentThreadId, targetThreadId, TRUE);

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
        AttachThreadInput(currentThreadId, targetThreadId, FALSE);

        // wait and check focus
        Sleep(ATTEMPT_DELAY);
        if (GetForegroundWindow() == hwnd) {
            return;
        }
    }

    fprintf(stderr, "Error: Failed to set focus to window after %d attempts\n", MAX_ATTEMPTS);
}

void run(char *cmdLine) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // might extract this into a function
    int ok = CreateProcess(NULL, // use command line
                           cmdLine,
                           NULL,  // Process handle not inheritable
                           NULL,  // Thread handle not inheritable
                           FALSE, // Set handle inheritance to FALSE
                           NORMAL_PRIORITY_CLASS, // priority
                           NULL, // Use parent's environment block
                           NULL, // Use parent's starting directory
                           &si,  // Pointer to STARTUPINFO structure
                           &pi); // Pointer to PROCESS_INFORMATION structure

    if (!ok) {
        fprintf(stderr, "failed to create process %s", cmdLine);
        printLastError();
    }
}

void runOrRaise(std::vector<HwndClass> &v, LPSTR cmdLine, LPCSTR className) {
    auto currentHwnd = GetForegroundWindow();
    auto iter =
        std::find_if(g_winVec.begin(), g_winVec.end(),
                     [className, currentHwnd](const HwndClass &e) {
                     return e.className == className && e.hwnd != currentHwnd;
                     });

    if (iter == g_winVec.end()) {
        char currentClass[256];
        GetClassNameA(currentHwnd, currentClass, sizeof(currentClass));
        if (strcmp(currentClass, className) == 0) {
            return;
        } else {
            run(cmdLine);
        }
    } else {
        raise(iter->hwnd);
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode != HC_ACTION) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // check for WM_KEYDOWN or WM_SYSKEYDOWN only, to avoid duplicate prints on
    // key release
    BOOL keyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    if (!keyDown) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    tagKBDLLHOOKSTRUCT *kbd = (tagKBDLLHOOKSTRUCT *)lParam;
    auto currentShortcut = KbdShortcut{getActiveModifiers(), kbd->vkCode};
    auto iter = g_keymaps.find(currentShortcut);

    if (iter == g_keymaps.end()) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    } else {
        printf("flags: %x, key: %lx\n", currentShortcut.flags, currentShortcut.key);
        auto cmdClass = iter->second;

        // populate g_winVec
        g_winVec.clear();
        EnumWindows(PopulateWinVec, reinterpret_cast<LPARAM>(&g_winVec));

        runOrRaise(g_winVec, const_cast<char *>(cmdClass.cmdLine.c_str()),
                   cmdClass.className.c_str());
        return 1; // swallow
    }
}

// NOTE: this runs on a seperate thread
BOOL WINAPI handlerRoutine(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        UnhookWindowsHookEx(g_hook);
    }

    // pass to next handler
    return FALSE;
}

// a little ugly but straight forward and gets the job done
int parseShortcut(char *str, KbdShortcut *kbd) {
    // win32 didn't bother defining a constant for A and 0 codes
    // These are from the docs
    // https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    const SHORT A_VK = 0x41;
    const SHORT ZERO_VK = 0x30;

    auto endStr = str+strlen(str);
    char *token = str;
    char *tokenEnd = strpbrk(str, "+");
    kbd->key = 0;
    kbd->flags = NONE;

    size_t tokenLen;
    while (TRUE) {
        if (tokenEnd == NULL) {
            tokenEnd = endStr;
        } else {
            *tokenEnd = '\0';
        }

        tokenLen = tokenEnd - token;

        if (strequal(token, "shift")) {
            kbd->flags |= SHIFT;
        } else if (strequal(token, "alt")) {
            kbd->flags |= ALT;
        } else if(strequal(token, "ctrl")) {
            kbd->flags |= CTRL;
        }

        else if (tokenLen == 1) {
            if (isdigit(token[0])) {
                BYTE num = atoi(token);
                kbd->key = ZERO_VK + num;
            } else if (isalpha(token[0])) {
                char alpha = toupper(token[0]);
                kbd->key = A_VK + (alpha - 'A');
            }
        } else if (tokenLen == 2) {
            if (toupper(token[0]) == 'F' && isdigit(token[1])) {
                BYTE num = atoi(token+1);
                if (num == 0) { // F0 is not a key
                    return INVALID_SHORTCUT;
                } else {
                    kbd->key = VK_F1-1 + atoi(token+1);
                }
            }
        } else {
            return INVALID_SHORTCUT;
        }

        if (tokenEnd == endStr) {
            break;
        } else {
            token = tokenEnd + 1;
            tokenEnd = strpbrk(token, "+");
        }
    }

    if (kbd->key == 0) {
        return INVALID_SHORTCUT;
    }

    return 0;
}

int main(int argc, char *argv[]) {

    char *arg = argv[0];
    if (arg[0] != '-' && argc == 3) {
        auto cmd = argv[1];
        auto className = argv[2];
        EnumWindows(PopulateWinVec, reinterpret_cast<LPARAM>(&g_winVec));
        runOrRaise(g_winVec, cmd, className);
        return 0;
    }

    int i = 1; // dont care about invokation name
    // avoid using GNU's getopt for portability
    while (i < argc) {
        if (strequal(argv[i], "--key")) {
            if (i + 3 >= argc) {
                fprintf(stderr, "--key requires 3 arguments\n");
                return 1;
            }
            KbdShortcut kbd;
            char shortcut[256];
            strcpy(shortcut, argv[i + 1]);
            auto err = parseShortcut(shortcut, &kbd);
            if (err == INVALID_SHORTCUT) {
                fprintf(stderr, "Invalid shortcut: %s\n", argv[i+1]);
                return 1;
            }
            const char *cmd = argv[i + 2];
            const char *className = argv[i + 3];
            g_keymaps[kbd] = CmdClass {cmd, className};

            i += 4;
        } else if (strequal(argv[i], "--help")) {
            const char *help = "%s\n"
                "--key [ctr|shift|alt]+KEY CMD WINDOWCLASS\n"
                "\tListen to KEY to run CMD or raise WINDOWCLASS\n"
                "--help show this help message\n";
            printf(help, argv[0]);
            return 0;
        } else {
            printf("Unknown argument %s\n", argv[i]);
            return 1;
        }
    }

    g_hook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (g_hook == NULL) {
        fprintf(stderr, "failed to set g_hook: %ld", GetLastError());
        return 1;
    }

    SetConsoleCtrlHandler(handlerRoutine, TRUE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
