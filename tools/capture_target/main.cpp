/// @file tools/capture_target/main.cpp
/// @brief 受控D3D11采集目标；主动注入长帧用于演示，不能据此推断真实游戏GPU瓶颈。
// 可重复采集目标：真实D3D11 Present，每60帧主动延迟70ms，便于验证长帧定位。
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <chrono>
using Microsoft::WRL::ComPtr;
/// 处理采集目标销毁消息，其余交给Win32默认窗口过程。
LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if(message==WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window,message,w,l);
}
/// 创建受控D3D11呈现目标并周期性注入70ms等待，用于帧采集，不代表真实游戏GPU瓶颈。
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    WNDCLASSW type{}; type.hInstance=instance; type.lpfnWndProc=windowProc; type.lpszClassName=L"GPUViewCaptureTarget";
    RegisterClassW(&type);
    HWND window=CreateWindowW(type.lpszClassName,L"GPUView capture target - closes after 20 seconds",WS_OVERLAPPEDWINDOW,
        100,100,640,360,nullptr,nullptr,instance,nullptr);
    if(!window) return 1;
    DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferCount=2; desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.OutputWindow=window; desc.SampleDesc.Count=1;
    desc.Windowed=TRUE; desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context; ComPtr<IDXGISwapChain> chain;
    if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &desc,&chain,&device,nullptr,&context))) return 2;
    ComPtr<ID3D11Texture2D> buffer; ComPtr<ID3D11RenderTargetView> view;
    if(FAILED(chain->GetBuffer(0,IID_PPV_ARGS(&buffer))) || FAILED(device->CreateRenderTargetView(buffer.Get(),nullptr,&view))) return 3;
    ShowWindow(window,SW_SHOWNOACTIVATE);
    const auto start=std::chrono::steady_clock::now(); unsigned frame=0; bool running=true;
    while(running && std::chrono::steady_clock::now()-start<std::chrono::seconds(20)) {
        MSG message{};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { if(message.message==WM_QUIT) running=false; TranslateMessage(&message); DispatchMessageW(&message); }
        if(!running) break;
        float color[4]={.1f, .2f + float(frame%60)/150.f, .35f, 1.f};
        context->ClearRenderTargetView(view.Get(),color);
        if(++frame%60==0) Sleep(70);
        if(FAILED(chain->Present(1,0))) return 4;
    }
    if(IsWindow(window)) DestroyWindow(window);
    return 0;
}
