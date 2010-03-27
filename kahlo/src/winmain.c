#include <Windows.h>
#include <tchar.h>

extern int main (int argc, char ** argv);

int APIENTRY _tWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPTSTR lpCmdLine, int nCmdShow)
{
  (void) hInstance;
  (void) hPrevInstance;
  (void) lpCmdLine;
  (void) nCmdShow;

  main (0, NULL);

  return 0;
}
