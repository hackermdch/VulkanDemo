#include "VulkanApp.h"

const wchar_t* VulkanApp::WndClsName = L"VulkanWindow";

int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	WNDCLASS cls{};
	cls.hInstance = hInstance;
	cls.lpszClassName = VulkanApp::WndClsName;
	cls.lpfnWndProc = VulkanApp::WndProcAlloter;
	RegisterClass(&cls);

	VulkanApp app;
	return app.Run();
}