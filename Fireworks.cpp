#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <chrono>
#include <iostream>
#include <random>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

enum ParticleType {
    PT_SPARK,
    PT_SPARK_ROCKET,
};

struct Particle {
    float pX;
    float pY;
    float vX;
    float vY;
    float aX;
    float aY;
    ParticleType type = PT_SPARK;
    bool isAlive = false;
    float remainingLife = 0;
    int radius = 0;
    Color color = Color(0xff, 0xff, 0xff, 0xff);
    int children = 0;
};

const int MAX_PARTICLES = 50;
Particle PARTICLES[MAX_PARTICLES];
int currentParticles = 0;


HDC drawHdc = NULL;
HBITMAP hbmMem = NULL;
HANDLE hOld = NULL;
int window_width = 0;
int window_height = 0;


VOID PaintFireworks(HWND hWnd, HDC hdc)
{
    Graphics graphics(hdc);

    graphics.Clear(Color::Black);
    for (Particle const &p : PARTICLES) {
        if (!p.isAlive) {
            continue;
        }

        Pen pen(p.color);
        graphics.DrawEllipse(&pen, Rect(p.pX - p.radius, p.pY - p.radius, 10 + p.radius, 10 + p.radius));
    }
}

int RandInRange(int lower, int upper) {
    return lower + (rand() % (lower - upper));
}

Color RandomBrightColour() {
    int offset = RandInRange(0, 2);
    int channels[3] = { 0xff, 0xff, 0xff };

    for (int i = 0; i < 3; i++) {
        int index = (offset + i) % 3;

        switch (index) {
        case 0:
            channels[i] = 0;
            break;
        case 1:
            channels[i] = 255;
            break;
        case 2:
            channels[i] = RandInRange(0,255);
            break;
        }
    }

    
    return Color(0xff, channels[0], channels[1], channels[2]);
}

auto lastStep = std::chrono::high_resolution_clock::now();
void MoveParticles(HWND hWnd) {
    auto thisStep = std::chrono::high_resolution_clock::now();
    float dSecs = std::chrono::duration_cast<std::chrono::nanoseconds>(thisStep - lastStep).count() * 1.0e-9;
    lastStep = thisStep;

    RECT rc;
    GetClientRect(
        hWnd,
        &rc
    );

    for (Particle &p : PARTICLES) {

        // Make particles older
        if (p.isAlive) {
            p.remainingLife -= dSecs;
        }

        // Kill old particles and spawn children
        if (p.isAlive && p.remainingLife <= 0) {
            p.isAlive = false;
            currentParticles--;

            for (int i = 0; i < p.children; i++) {
                for (Particle& c : PARTICLES) {
                    if (!c.isAlive) {
                        // No need to increment currentParticles
                        // because the space is already allocated

                        c.pX = p.pX;
                        c.pY = p.pY;
                        c.vX = RandInRange(-200, 200);
                        c.vY = RandInRange(-200, 200);
                        c.aX = 0;
                        c.aY = 100;

                        c.type = PT_SPARK;
                        c.isAlive = true;
                        c.remainingLife = 1.0f;
                        c.radius = 5;
                        c.color = p.color;
                        c.children = 0;

                        break;
                    }
                }
            }
            continue;
        }

        // Kill out of bounds particles
        /*if (p.x < rc.left || p.x > rc.right || p.y < rc.top || p.y > rc.bottom) {
            p.isAlive = false;
            continue;
        }*/

        // Create new rockets
        if (!p.isAlive && MAX_PARTICLES > currentParticles) {
            currentParticles++;

            p.pX = RandInRange(rc.left, rc.right);
            p.pY = rc.bottom-50;
            p.vX = RandInRange(-50, 50);
            p.vY = RandInRange(-400, -600);
            p.aX = 0;
            p.aY = 100;

            p.type = PT_SPARK_ROCKET;
            p.isAlive = true;
            p.remainingLife = 4.0f;
            p.radius = 10;
            p.color = RandomBrightColour();
            p.children = RandInRange(3, 6);

            currentParticles += p.children;
        }
        
        // 
        p.pX += p.vX * dSecs;
        p.pY += p.vY * dSecs;
        p.vX += p.aX * dSecs;
        p.vY += p.aY * dSecs;
    }

    InvalidateRect(hWnd, NULL, NULL);
}

//
//
// WinAPI and Window maintenance.
//
// 

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, INT iCmdShow)
{
    HWND                hWnd;
    WNDCLASS            wndClass;
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;

    // Initialize GDI+.
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hInstance;
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = TEXT("Fireworks");

    RegisterClass(&wndClass);

    hWnd = CreateWindow(
        TEXT("Fireworks"),   // window class name
        TEXT("Fireworks"),  // window caption
        WS_OVERLAPPEDWINDOW,      // window style
        CW_USEDEFAULT,            // initial x position
        CW_USEDEFAULT,            // initial y position
        CW_USEDEFAULT,            // initial x size
        CW_USEDEFAULT,            // initial y size
        NULL,                     // parent window handle
        NULL,                     // window menu handle
        hInstance,                // program instance handle
        NULL);                    // creation parameters

    ShowWindow(hWnd, iCmdShow);
    UpdateWindow(hWnd);

    srand(time(0));

    // Generic message queue
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            MoveParticles(hWnd);
        }
    }

    GdiplusShutdown(gdiplusToken);
    return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC          renderHdc;
    PAINTSTRUCT  ps;

    switch (message)
    {

    // Queue exit if any key pressed
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        PostMessage(hWnd, WM_CLOSE, 0, 0);
        break;

    case WM_PAINT:
        // Get a handle for a new rendering session
        renderHdc = BeginPaint(hWnd, &ps);

        RECT rc;
        GetClientRect(
            hWnd,
            &rc
        );

        // Update the window size.
        // It's a screensaver so this shouldn't change
        if (window_width == 0) {
            window_width = rc.right - rc.left;
        }
        if (window_height == 0) {
            window_height = rc.bottom - rc.top;
        }

        // Get handles for a swap buffer and bitmap if they don't exist
        if (drawHdc == NULL) {
            drawHdc = CreateCompatibleDC(renderHdc);
        }
        if (hbmMem == NULL) {
            hbmMem = CreateCompatibleBitmap(renderHdc, window_width, window_height);
        }
        hOld = SelectObject(drawHdc, hbmMem);

        PaintFireworks(hWnd, drawHdc);

        // Copy the drawing buffer into the render buffer
        BitBlt(renderHdc, 0, 0, window_width, window_height, drawHdc, 0, 0, SRCCOPY);
        SelectObject(drawHdc, hOld);

        EndPaint(hWnd, &ps);
        return 0;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_DESTROY:

        // Delete handles on exit
        if (hbmMem != NULL) {
            DeleteObject(hbmMem);
        }
        if (drawHdc != NULL) {
            DeleteDC(drawHdc);
        }

        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}