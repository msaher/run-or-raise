#ifdef WINDOWS_SUCKS_EXPORTS
#define WINDOWS_SUCKS_API __declspec(dllexport)
#else
#define WINDOWS_SUCKS_API __declspec(dllimport)
#endif

extern "C" WINDOWS_SUCKS_API int add(int a, int b);
