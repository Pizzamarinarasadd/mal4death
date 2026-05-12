/* Tutorial 1 sample: will be packed with UPX.
   Player runs "upx -l dummy.exe" and finds the packing algorithm. */
#include <windows.h>

int main(void) {
    MessageBoxA(NULL, "Hello from dummy!", "Dummy", MB_OK);
    return 0;
}
