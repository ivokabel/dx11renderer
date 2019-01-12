#include "renderer.hpp"


SimpleDirectX11Renderer::SimpleDirectX11Renderer()
{
}


SimpleDirectX11Renderer::~SimpleDirectX11Renderer()
{
    DestroyWindow();
    // TODO: Destroy device, etc.
}


bool SimpleDirectX11Renderer::IsValid() const
{
    return g_hWnd
        && (mWndWidth > 0u)
        && (mWndHeight > 0u);
}


bool SimpleDirectX11Renderer::Init(HINSTANCE hInstance, int nCmdShow,
                                   int32_t wndWidth, int32_t wndHeight)
{
    mWndWidth = wndWidth;
    mWndHeight = wndHeight;

    if (!InitWindow(hInstance, nCmdShow))
        return false;

    // TODO: init device, etc

    return true;
}


bool SimpleDirectX11Renderer::InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = nullptr; // TODO: LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = mWndClassName;
    wcex.hIconSm = nullptr; // TODO: LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
    if (!RegisterClassEx(&wcex))
        return false;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, mWndWidth, mWndHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow(mWndClassName, mWndName,
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          rc.right - rc.left, rc.bottom - rc.top,
                          nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd)
        return false;

    ShowWindow(g_hWnd, nCmdShow);

    return true;
}


void SimpleDirectX11Renderer::DestroyWindow()
{
    ::DestroyWindow(g_hWnd);
    g_hWnd = nullptr;
    g_hInst = nullptr;
}


LRESULT CALLBACK SimpleDirectX11Renderer::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}


int SimpleDirectX11Renderer::Run()
{
    // Main message loop
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            //Render();
        }
    }

    return (int)msg.wParam;
}