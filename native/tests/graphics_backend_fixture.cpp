#include <windows.h>
// Identity-only fixture: deliberately has no graphics hooks or device creation.
// Registration must refuse before probing when a manual backend is present.
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
