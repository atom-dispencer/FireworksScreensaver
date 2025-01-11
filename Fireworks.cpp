#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <chrono>
#include <iostream>
#include <random>
#include <math.h>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

//
//
// There's no seperate header file, so here are the declarations.
//
//

enum ParticleType {
    PT_SPARK,
    PT_SPARK_ROCKET,
    PT_HAZE
};

struct Particle {
    float pX = 0;
    float pY = 0;
    float vX = 0;
    float vY = 0;
    float aX = 0;
    float aY = 0;
    ParticleType type = PT_SPARK;
    bool isAlive = false;
    float remainingLife = 0;
    int radius = 0;
    Color color = Gdiplus::Color(0xff, 0xff, 0xff, 0xff);
    int children = 0;
    float timeSinceLastEmission = 0;
};

Color RandomBrightColour();
void PaintFireworks(HWND hWnd, HDC hdc);
int RandInRange(int lower, int upper);
void MoveParticles(HWND hWnd);
void DeleteParticle(Particle& p);

void MakePTSpark(Particle& p);
void MakePTSparkRocket(Particle& p);
void MakePTHaze(Particle& p);

void ProcessPTSpark(Particle& p, float dSecs);
void ProcessPTSparkRocket(Particle& p, float dSecs);
void ProcessPTHaze(Particle& p, float dSecs);

void KillPTSpark(Particle& p);
void KillPTSparkRocket(Particle& p);
void KillPTHaze(Particle& p);

//
//
// Implementations and stuff
//
//

const int MAX_ROCKETS = 1;
const int MAX_PARTICLES = 200;
Particle PARTICLES[MAX_PARTICLES];
int currentRockets = 0;
int currentParticles = 0;

HDC drawHdc = NULL;
HBITMAP hbmMem = NULL;
HANDLE hOld = NULL;
int window_width = 0;
int window_height = 0;

VOID PaintFireworks(HWND hWnd, HDC hdc)
{
    Graphics graphics(hdc);

    RECT rc;
    GetClientRect(hWnd, &rc);

    graphics.Clear(Gdiplus::Color::Black);
    for (Particle const &p : PARTICLES) {
        if (!p.isAlive) {
            continue;
        }
        
        Rect r = Rect(p.pX - p.radius, p.pY - p.radius, 2 * p.radius, 2 * p.radius);

        GraphicsPath path(Gdiplus::FillModeAlternate);
        path.AddEllipse(r);
        PathGradientBrush brush(&path);
        
        // The centre color is the outer color but 75% whiter
        ARGB argbCentre = p.color.GetValue();
        int red = argbCentre   & 0x00110000;
        int green = argbCentre & 0x00001100;
        int blue = argbCentre  & 0x00000011;
        if (red == 0) {
            // Red is 0
            argbCentre |= 0x00C00000;
        }
        else if (green == 0) {
            // Green is 0
            argbCentre |= 0x0000C000;
        }
        else if (blue == 0) {
            // Blue is 0
            argbCentre |= 0x000000C0;
        }
        Color centre = Color(argbCentre);

        // Impose a rough inverse square law
        Color quarter = Color(p.color.GetValue() & 0x40111111);

        Color colors[3] = { Color::Transparent, p.color, centre };
        float ratios[3] = { 0, 0.80, 1 };
        int COUNT = 3;
        brush.SetInterpolationColors(colors, ratios, COUNT);

        graphics.FillEllipse(&brush, r);
    }
}

int RandInRange(int lower, int upper) {
    int r = rand();

    if (upper < lower) {
        int temp = lower;
        lower = upper;
        upper = temp;
    }

    int diff = upper - lower;
    int n = lower + (r % diff);
    return n;
}

void DeleteParticle(Particle& p) {
    if (p.type == PT_SPARK_ROCKET) {
        currentRockets--;
    }

    p.isAlive = false;
    currentParticles--;
}

Color RandomBrightColour() {
    int offset = RandInRange(0, 3);
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

Particle& ReviveDeadParticle() {
    for (Particle& c : PARTICLES) {
        if (!c.isAlive) {
            c.isAlive = true;
            currentParticles++;
            return c;
        }
    }

    // I hope this never happens
    return PARTICLES[RandInRange(0, MAX_PARTICLES)];
}

void MakePTSparkRocket(Particle& p) {
    p.type = PT_SPARK_ROCKET;

    p.vX = RandInRange(-100, 100);
    p.vY = RandInRange(-400, -250);
    p.aX = 0;
    p.aY = 100;

    p.remainingLife = RandInRange(10, 40) / 10.0f;
    p.radius = 15;
    p.color = RandomBrightColour();
    p.children = RandInRange(5, 12);
}

void MakePTSpark(Particle& p) {
    p.type = PT_SPARK;

    p.vX = RandInRange(-200, 200);
    p.vY = RandInRange(-200, 200);
    p.aX = 0;
    p.aY = 100;

    p.remainingLife = 1.0f;
    p.radius = 9;
    p.children = 0;
}

void MakePTHaze(Particle& p) {
    p.type = PT_HAZE;

    p.vX = 0;
    p.vY = 0;
    p.aX = 0;
    p.aY = 8;

    p.remainingLife = 3.0f;
    p.radius = 5;
    p.children = 0;
}

void ProcessPTSparkRocket(Particle& rocket, float dSecs) {

    rocket.vX += RandInRange(-30, 30) / 10.0f;

    if (rocket.timeSinceLastEmission > 0.05f) {
        rocket.timeSinceLastEmission = 0;

        Particle& s = ReviveDeadParticle();
        MakePTHaze(s);
        s.pX = rocket.pX;
        s.pY = rocket.pY;
        s.vX = -0.75 * rocket.vX;
        s.vY = -0.75 * rocket.vY;
        s.aX = 0;
        s.aY = 0;
        s.color = rocket.color;
    }
    rocket.timeSinceLastEmission += dSecs;
}

void ProcessPTSpark(Particle& spark, float dSecs) {
    spark.aX = -1.6 * spark.vX;
    spark.aY = 60;

    if (spark.remainingLife < 0.5) {
        float factor = 2 * spark.remainingLife;

        // Remaining life is < 1
        BYTE bAlpha = (BYTE)(255 * factor * factor);
        int iAlpha = (int)bAlpha;
        int siAlpha = iAlpha << 24;
        ARGB _rgb = spark.color.GetValue() & 0x00ffffff;
        ARGB argb = _rgb | siAlpha;
        spark.color.SetValue(argb);
    }

    if (spark.timeSinceLastEmission > 0.1) {
        spark.timeSinceLastEmission = 0;
        Particle& haze = ReviveDeadParticle();
        MakePTHaze(haze);
        haze.pX = spark.pX;
        haze.pY = spark.pY;
        haze.color = spark.color;
    }

    spark.timeSinceLastEmission += dSecs;
}

void ProcessPTHaze(Particle& p, float dSecs) {
    float factor = p.remainingLife / 3.0f;

    BYTE bAlpha = (BYTE)(255 * 0.5f * factor * factor);
    int iAlpha = (int)bAlpha;
    int siAlpha = iAlpha << 24;
    ARGB _rgb = p.color.GetValue() & 0x00ffffff;
    ARGB argb = _rgb | siAlpha;
    p.color.SetValue(argb);
}

void KillPTSpark(Particle& p) {
}

void KillPTSparkRocket(Particle& rocket) {

    int xTotal = 0;
    int yTotal = 0;

    for (int i = 0; i < rocket.children; i++) {
        Particle& spark = ReviveDeadParticle();
        MakePTSpark(spark);

        // Always try to push the total towards zero to make more even
        // explosions.
        // If going in same x direction, flip vX
        if (xTotal * spark.vX > 0) {
            spark.vX *= -1;
        }
        // If going in same y direction, flip vY
        if (yTotal * spark.vY > 0) {
            spark.vY *= -1;
        }
        xTotal += spark.vX;
        yTotal += spark.vY;

        spark.color = rocket.color;
        spark.pX = rocket.pX;
        spark.pY = rocket.pY;

        spark.vX += 0.5f * rocket.vX;
        spark.vY += 0.5f * rocket.vY;
    }
}

void KillPTHaze(Particle& p) {
}

float timeSinceRocketCount = 0;
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

    // Sometimes the rocket count gets out of sync?
    // No idea how that happens, but here's a bodge for it
    int rocketCheck = 0;
    if (timeSinceRocketCount > 5.0f) {
        for (Particle& p : PARTICLES) {
            if (p.type == PT_SPARK_ROCKET && p.isAlive) {
                rocketCheck++;
            }
        }

        currentRockets = rocketCheck;
        timeSinceRocketCount = 0;
    }
    timeSinceRocketCount += dSecs;

    for (Particle &p : PARTICLES) {

        // Create new rockets
        if (!p.isAlive) {
            if (MAX_ROCKETS > currentRockets) {

                MakePTSparkRocket(p);

                p.pX = RandInRange(rc.left + 200, rc.right - 200);
                p.pY = rc.bottom + 50;

                p.isAlive = true;
                currentRockets += 1;
                currentParticles += 1;
            }

            continue;
        }

        // Make particles older
        p.remainingLife -= dSecs;

        if (p.pX < rc.left - 50 || p.pX > rc.right + 50 || p.pY < rc.top - 50 || p.pY > rc.bottom + 50) {
            DeleteParticle(p);
            continue;
        }

        // Kill old particles
        if (p.remainingLife <= 0) {
            switch (p.type) {
            case PT_SPARK:
                KillPTSpark(p);
                break;
            case PT_SPARK_ROCKET:
                KillPTSparkRocket(p);
                break;
            case PT_HAZE:
                KillPTHaze(p);
                break;
            }

            DeleteParticle(p);
            continue;
        }

        // Process different types of particle
        switch (p.type) {
        case PT_SPARK_ROCKET:
            ProcessPTSparkRocket(p, dSecs);
            break;
        case PT_SPARK:
            ProcessPTSpark(p, dSecs);
            break;
        case PT_HAZE:
            ProcessPTHaze(p, dSecs);
            break;
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
// WinAPI initialisation and maintenance.
//
// 

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, INT iCmdShow)
{
    HWND                hWnd;
    WNDCLASS            wndClass;
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;

    bool isPreview = true;
    if (strcmp(lpCmdLine, "/s") == 0) {
        isPreview = false;
    }
    else if (strcmp(lpCmdLine, "/c") == 0) {
        MessageBox(nullptr, L"Configuration options are not yet supported.\nUse the /? option for help.", L"Fireworks Screensaver Configuration", MB_OK);
        return 0;
    }
    else if (strcmp(lpCmdLine, "/p") == 0) {
        isPreview = true;
    }
    else if (strcmp(lpCmdLine, "/d") == 0) {
        isPreview = true;
    }
    else if (strcmp(lpCmdLine, "/?") == 0) {
        MessageBox(nullptr, L"/s - Run screensaver\n/c - Open configuration\n/p, /d - Preview/debug the screensaver\n/? - Show this message\n\nFireworks Screesaver, by Adam Spencer.\nhttps://github.com/atom-dispencer/FireworksScreensaver", L"Fireworks Screensaver", MB_OK);
        return 0;
    }


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
    
    

    if (isPreview) {
        // Standard window (preview window)
        hWnd = CreateWindow(
            TEXT("Fireworks"),      // window class name
            TEXT("Fireworks"),      // window caption
            WS_OVERLAPPEDWINDOW,    // window style
            CW_USEDEFAULT,          // initial x position
            CW_USEDEFAULT,          // initial y position
            CW_USEDEFAULT,          // initial x size
            CW_USEDEFAULT,          // initial y size
            NULL,                   // parent window handle
            NULL,                   // window menu handle
            hInstance,              // program instance handle
            NULL);                  // creation parametersWS_POPUP
    }
    else {
        // Full screen borderless (proper screensaver)
        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);
        hWnd = CreateWindow(
            TEXT("Fireworks"),      // window class name
            TEXT("Fireworks"),      // window caption
            WS_POPUP,               // window style
            0,                      // initial x position
            0,                      // initial y position
            w,                      // initial x size
            h,                      // initial y size
            NULL,                   // parent window handle
            NULL,                   // window menu handle
            hInstance,              // program instance handle
            NULL);                  // creation parameters
    }

    ShowCursor(false);
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

//
//
// Window management
//
//

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
        GetClientRect(hWnd, &rc);

        window_width = rc.right - rc.left;
        window_height = rc.bottom - rc.top;

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
        
        // In preview mode, if the PC sleeps here, this call hangs.
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