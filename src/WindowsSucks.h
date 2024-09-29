#ifdef WINDOWS_SUCKS_EXPORTS
#define WINDOWS_SUCKS_API __declspec(dllexport)
#else
#define WINDOWS_SUCKS_API __declspec(dllimport)
#endif

#include <windows.h>

// extern "C" LRESULT CALLBACK cbtProc(int nCode, WPARAM wParam, LPARAM lParam);
// extern "C" LRESULT CALLBACK shellProc(int nCode, WPARAM wParam, LPARAM lParam);
extern "C" LRESULT CALLBACK windowProc(int nCode, WPARAM wParam, LPARAM lParam);

