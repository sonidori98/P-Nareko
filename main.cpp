#include "Renderer.h"

Renderer g_renderer;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        g_renderer.Render();
        ValidateRect(hWnd, nullptr);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        return E_FAIL;
    // 1. �E�B���h�E�N���X�̓o�^
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"DirectX11WindowClass";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
        return E_FAIL;
    }

    // 2. �E�B���h�E�̍쐬
    HWND hWnd = CreateWindow(
        L"DirectX11WindowClass",
        L"P_Nareko",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd)
    {
        return E_FAIL;
    }

    // 3. Renderer�̏�����
    if (FAILED(g_renderer.Init(hWnd)))
    {
        g_renderer.Cleanup();
        return E_FAIL;
    }

    // 4. �E�B���h�E�̕\���ƍX�V
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 5. ���b�Z�[�W���[�v
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            g_renderer.Update();
            g_renderer.Render();
        }
    }

    // 6. �I������
    g_renderer.Cleanup();

    return (int)msg.wParam;
}