// for some reason this seems to be applying "thick frame" instead of thin frame (intended)
// it is for this reason that I'm unable to get the cool mini window toolbar that og mania has :(
// tldr: microsoft sucks again

IDirect3D9 *RenderDevice::dx9Context;
IDirect3DDevice9 *RenderDevice::dx9Device;
UINT RenderDevice::dxAdapter;
IDirect3DVertexDeclaration9 *RenderDevice::dx9VertexDeclare;
IDirect3DVertexBuffer9 *RenderDevice::dx9VertexBuffer;
IDirect3DTexture9 *RenderDevice::screenTextures[SCREEN_COUNT];
IDirect3DTexture9 *RenderDevice::imageTexture;
D3DVIEWPORT9 RenderDevice::dx9ViewPort;

int32 RenderDevice::adapterCount = 0;
RECT RenderDevice::monitorDisplayRect;
GUID RenderDevice::deviceIdentifier;

uint32* RenderDevice::scratchBuffer = NULL;

bool RenderDevice::useFrequency = false;

LARGE_INTEGER RenderDevice::performanceCount;
LARGE_INTEGER RenderDevice::frequency;
LARGE_INTEGER RenderDevice::initialFrequency;
LARGE_INTEGER RenderDevice::curFrequency;

bool RenderDevice::Init()
{
#if _UNICODE
    // shoddy workaround to get the title into wide chars in UNICODE mode
    std::string str   = gameVerInfo.gameTitle;
    std::wstring temp = std::wstring(str.begin(), str.end());
    LPCWSTR gameTitle = temp.c_str();
#else
    std::string str  = gameVerInfo.gameTitle;
    LPCSTR gameTitle = str.c_str();
#endif

    if (!SetupRendering() || !AudioDevice::Init())
        return false;

    InitInputDevices();
    return true;
}

void RenderDevice::CopyFrameBuffer()
{
    dx9Device->SetTexture(0, NULL);

    for (int32 s = 0; s < videoSettings.screenCount; ++s) {
        D3DLOCKED_RECT rect;

        if (SUCCEEDED(screenTextures[s]->LockRect(0, &rect, NULL, 0))) {
            D3DSURFACE_DESC desc;
            screenTextures[s]->GetLevelDesc(0, &desc);

            XGTileSurface(
                rect.pBits,                 // Destination
                desc.Width,                 // Texture width (aligned)
                desc.Height,                // Texture height
                NULL,
                screens[s].frameBuffer,     // Source
                screens[s].pitch * 2,       // Source Pitch (Bytes)
                NULL,
                2                           // 16-bit
            );

            screenTextures[s]->UnlockRect(0);
        }
    }
}

void RenderDevice::FlipScreen()
{
    if (windowRefreshDelay > 0) {
        if (!--windowRefreshDelay)
            UpdateGameWindow();

        return;
    }

    dx9Device->SetViewport(&displayInfo.viewport);
    dx9Device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    dx9Device->SetViewport(&dx9ViewPort);

    if (SUCCEEDED(dx9Device->BeginScene())) {
        // reload shader if needed
        if (lastShaderID != videoSettings.shaderID) {
            lastShaderID = videoSettings.shaderID;

            if (shaderList[videoSettings.shaderID].linear) {
                dx9Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                dx9Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            }
            else {
                dx9Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                dx9Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            }

            if (videoSettings.shaderSupport) {
                dx9Device->SetVertexShader(shaderList[videoSettings.shaderID].vertexShaderObject);
                dx9Device->SetPixelShader(shaderList[videoSettings.shaderID].pixelShaderObject);
                dx9Device->SetVertexDeclaration(dx9VertexDeclare);
                dx9Device->SetStreamSource(0, dx9VertexBuffer, 0, sizeof(RenderVertex));
            }
            else {
                dx9Device->SetStreamSource(0, dx9VertexBuffer, 0, sizeof(RenderVertex));
                dx9Device->SetFVF(D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_XYZ);
            }
        }

        if (videoSettings.shaderSupport) {
            float2 screenDim = { videoSettings.dimMax * videoSettings.dimPercent, 0 };

            dx9Device->SetPixelShaderConstantF(0, &pixelSize.x, 1);   // pixelSize
            dx9Device->SetPixelShaderConstantF(1, &textureSize.x, 1); // textureSize
            dx9Device->SetPixelShaderConstantF(2, &viewSize.x, 1);    // viewSize
            dx9Device->SetPixelShaderConstantF(3, &screenDim.x, 1);   // screenDim
        }

        int32 startVert = 0;
        switch (videoSettings.screenCount) {
            default:
            case 0:
#if RETRO_REV02
                startVert = 54;
#else
                startVert = 18;
#endif
                dx9Device->SetTexture(0, imageTexture);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, startVert, 2);
                dx9Device->EndScene();
                break;

            case 1:
                dx9Device->SetTexture(0, screenTextures[0]);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                break;

            case 2:
#if RETRO_REV02
                startVert = startVertex_2P[0];
#else
                startVert = 6;
#endif
                dx9Device->SetTexture(0, screenTextures[0]);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, startVert, 2);

#if RETRO_REV02
                startVert = startVertex_2P[1];
#else
                startVert = 12;
#endif
                dx9Device->SetTexture(0, screenTextures[1]);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, startVert, 2);
                break;

#if RETRO_REV02
            case 3:
                dx9Device->SetTexture(0, screenTextures[0]);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, startVertex_3P[0], 2);

                dx9Device->SetTexture(0, screenTextures[1]);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, startVertex_3P[1], 2);

                dx9Device->SetTexture(0, screenTextures[2]);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, startVertex_3P[2], 2);
                break;

            case 4:
                dx9Device->SetTexture(0, screenTextures[0]);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, 30, 2);

                dx9Device->SetTexture(0, screenTextures[1]);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, 36, 2);

                dx9Device->SetTexture(0, screenTextures[2]);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, 42, 2);

                dx9Device->SetTexture(0, screenTextures[3]);
                dx9Device->DrawPrimitive(D3DPT_TRIANGLELIST, 48, 2);
                break;
#endif
        }

        dx9Device->EndScene();
    }

    if (FAILED(dx9Device->Present(NULL, NULL, NULL, NULL)))
        windowRefreshDelay = 8;
}

void RenderDevice::Release(bool32 isRefresh)
{
    if (videoSettings.shaderSupport) {
        for (int32 i = 0; i < shaderCount; ++i) {
            if (shaderList[i].vertexShaderObject)
                shaderList[i].vertexShaderObject->Release();
            shaderList[i].vertexShaderObject = NULL;

            if (shaderList[i].pixelShaderObject)
                shaderList[i].pixelShaderObject->Release();
            shaderList[i].pixelShaderObject = NULL;
        }

        shaderCount = 0;
#if RETRO_USE_MOD_LOADER
        userShaderCount = 0;
#endif
    }

    if (imageTexture) {
        imageTexture->Release();
        imageTexture = NULL;
    }

    for (int32 i = 0; i < SCREEN_COUNT; ++i) {
        if (screenTextures[i])
            screenTextures[i]->Release();

        screenTextures[i] = NULL;
    }

    if (!isRefresh && displayInfo.displays) {
        free(displayInfo.displays);
        displayInfo.displays = NULL;
    }

    if (dx9VertexBuffer) {
        dx9VertexBuffer->Release();
        dx9VertexBuffer = NULL;
    }

    if (isRefresh && dx9VertexDeclare) {
        dx9VertexDeclare->Release();
        dx9VertexDeclare = NULL;
    }

    if (dx9Device) {
        dx9Device->Release();
        dx9Device = NULL;
    }

    if (!isRefresh && dx9Context) {
        dx9Context->Release();
        dx9Context = NULL;
    }

    if (!isRefresh && scanlines) {
        free(scanlines);
        scanlines = NULL;
    }

	if (scratchBuffer) {
        free(scratchBuffer);
        scratchBuffer = NULL;
    }
}

void RenderDevice::RefreshWindow()
{
    videoSettings.windowState = WINDOWSTATE_UNINITIALIZED;

    Release(true);

    Sleep(250); // zzzz.. mimimimi..

    GetDisplays();

    if (!InitGraphicsAPI() || !InitShaders())
        return;

    videoSettings.windowState = WINDOWSTATE_ACTIVE;
}

void RenderDevice::InitFPSCap()
{
    if (QueryPerformanceFrequency(&frequency)) {
        useFrequency              = true;
        initialFrequency.QuadPart = frequency.QuadPart / videoSettings.refreshRate;
        QueryPerformanceCounter(&performanceCount);
    }
}
bool RenderDevice::CheckFPSCap()
{
    if (useFrequency)
        QueryPerformanceCounter(&curFrequency);

    if (curFrequency.QuadPart > performanceCount.QuadPart)
        return true;

    return false;
}
void RenderDevice::UpdateFPSCap() { performanceCount.QuadPart = curFrequency.QuadPart + initialFrequency.LowPart; }

void RenderDevice::InitVertexBuffer()
{
    RenderVertex vertBuffer[sizeof(rsdkVertexBuffer) / sizeof(RenderVertex)];
    memcpy(vertBuffer, rsdkVertexBuffer, sizeof(rsdkVertexBuffer));

    float x = 0.5 / (float)viewSize.x;
    float y = 0.5 / (float)viewSize.y;

    // ignore the last 6 verts, they're scaled to the 1024x512 textures already!
    int32 vertCount = (RETRO_REV02 ? 60 : 24) - 6;
    for (int32 v = 0; v < vertCount; ++v) {
        RenderVertex *vertex = &vertBuffer[v];
        vertex->pos.x        = vertex->pos.x - x;
        vertex->pos.y        = vertex->pos.y + y;

        if (vertex->tex.x)
            vertex->tex.x = screens[0].size.x * (1.0 / textureSize.x);

        if (vertex->tex.y)
            vertex->tex.y = screens[0].size.y * (1.0 / textureSize.y);
    }

    RenderVertex *vertBufferPtr;
    if (SUCCEEDED(dx9VertexBuffer->Lock(0, 0, (void **)&vertBufferPtr, 0))) {
        memcpy(vertBufferPtr, vertBuffer, sizeof(vertBuffer));
        dx9VertexBuffer->Unlock();
    }
}

bool RenderDevice::InitGraphicsAPI()
{
    videoSettings.shaderSupport = false;

    D3DCAPS9 pCaps;
    if (SUCCEEDED(dx9Context->GetDeviceCaps(0, D3DDEVTYPE_HAL, &pCaps)) && (pCaps.PixelShaderVersion & 0xFF00) >= 0x300)
        videoSettings.shaderSupport = true;

    viewSize.x = 0;
    viewSize.y = 0;

    D3DPRESENT_PARAMETERS presentParams;
    ZeroMemory(&presentParams, sizeof(presentParams));
    int32 bufferWidth  = videoSettings.fsWidth;
    int32 bufferHeight = videoSettings.fsHeight;
    if (videoSettings.fsWidth <= 0 || videoSettings.fsHeight <= 0) {
        bufferWidth  = displayWidth[dxAdapter];
        bufferHeight = displayHeight[dxAdapter];
    }

	presentParams.DisableAutoBackBuffer = false;
	//presentParams.DisableAutoFrontBuffer = false;

    presentParams.BackBufferWidth  = bufferWidth;
    presentParams.BackBufferHeight = bufferHeight;
    // for some reason this seems to force the window to have permanent top focus after coming out of fullscreen
    // despite this being 1:1 with the original code and it not having that behaviour
    // tldr: microsoft sucks
    presentParams.BackBufferFormat = D3DFMT_X8R8G8B8;
    presentParams.BackBufferCount  = videoSettings.tripleBuffered ? 2 : 1;

    presentParams.MultiSampleType    = D3DMULTISAMPLE_NONE;
    presentParams.MultiSampleQuality = 0;

    presentParams.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    presentParams.Windowed               = false;
    presentParams.EnableAutoDepthStencil = false;
    presentParams.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    presentParams.Flags                  = 0;

    presentParams.FullScreen_RefreshRateInHz = 0;
    presentParams.PresentationInterval       = videoSettings.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;


    viewSize.x = (float)bufferWidth;
    viewSize.y = (float)bufferHeight;

    if (FAILED(dx9Context->CreateDevice(dxAdapter, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &presentParams, &dx9Device)))
        return false;

    if (videoSettings.shaderSupport) {
        D3DVERTEXELEMENT9 elements[4];

        elements[0].Type       = D3DDECLTYPE_FLOAT3;
        elements[0].Method     = 0;
        elements[0].Stream     = 0;
        elements[0].Offset     = 0;
        elements[0].Usage      = D3DDECLUSAGE_POSITION;
        elements[0].UsageIndex = 0;

        elements[1].Type       = D3DDECLTYPE_D3DCOLOR;
        elements[1].Method     = 0;
        elements[1].Stream     = 0;
        elements[1].Offset     = offsetof(RenderVertex, color);
        elements[1].Usage      = D3DDECLUSAGE_COLOR;
        elements[1].UsageIndex = 0;

        elements[2].Type       = D3DDECLTYPE_FLOAT2;
        elements[2].Method     = 0;
        elements[2].Stream     = 0;
        elements[2].Offset     = offsetof(RenderVertex, tex);
        elements[2].Usage      = D3DDECLUSAGE_TEXCOORD;
        elements[2].UsageIndex = 0;

        elements[3].Type       = D3DDECLTYPE_UNUSED;
        elements[3].Method     = 0;
        elements[3].Stream     = 0xFF;
        elements[3].Offset     = 0;
        elements[3].Usage      = 0;
        elements[3].UsageIndex = 0;

        if (FAILED(dx9Device->CreateVertexDeclaration(elements, &dx9VertexDeclare)))
            return false;

        if (FAILED(dx9Device->CreateVertexBuffer(sizeof(rsdkVertexBuffer), 0, 0, D3DPOOL_DEFAULT, &dx9VertexBuffer, NULL)))
            return false;
    }
    else {
        if (FAILED(dx9Device->CreateVertexBuffer(sizeof(rsdkVertexBuffer), 0, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, D3DPOOL_DEFAULT,
                                                 &dx9VertexBuffer, NULL)))
            return false;
    }

    int32 maxPixHeight = 0;
#if !RETRO_USE_ORIGINAL_CODE
    int32 screenWidth = 0;
#endif
    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        if (videoSettings.pixHeight > maxPixHeight)
            maxPixHeight = videoSettings.pixHeight;

        screens[s].size.y = videoSettings.pixHeight;

        float viewAspect = viewSize.x / viewSize.y;
#if !RETRO_USE_ORIGINAL_CODE
        screenWidth = (int32)((viewAspect * videoSettings.pixHeight) + 3) & 0xFFFFFFFC;
#else
        int32 screenWidth = (int32)((viewAspect * videoSettings.pixHeight) + 3) & 0xFFFFFFFC;
#endif
        if (screenWidth < videoSettings.pixWidth)
            screenWidth = videoSettings.pixWidth;

#if !RETRO_USE_ORIGINAL_CODE
        if (customSettings.maxPixWidth && screenWidth > customSettings.maxPixWidth)
            screenWidth = customSettings.maxPixWidth;
#else
        if (screenWidth > DEFAULT_PIXWIDTH)
            screenWidth = DEFAULT_PIXWIDTH;
#endif

        memset(&screens[s].frameBuffer, 0, sizeof(screens[s].frameBuffer));
        SetScreenSize(s, screenWidth, screens[s].size.y);
    }

    pixelSize.x     = (float)screens[0].size.x;
    pixelSize.y     = (float)screens[0].size.y;
    float pixAspect = pixelSize.x / pixelSize.y;

    dx9Device->GetViewport(&displayInfo.viewport);
    dx9ViewPort = displayInfo.viewport;

    if ((viewSize.x / viewSize.y) <= ((pixelSize.x / pixelSize.y) + 0.1)) {
        if ((pixAspect - 0.1) > (viewSize.x / viewSize.y)) {
            viewSize.y         = (pixelSize.y / pixelSize.x) * viewSize.x;
            dx9ViewPort.Y      = (DWORD)((displayInfo.viewport.Height >> 1) - (viewSize.y * 0.5));
            dx9ViewPort.Height = (DWORD)viewSize.y;

            dx9Device->SetViewport(&dx9ViewPort);
        }
    }
    else {
        viewSize.x        = pixAspect * viewSize.y;
        dx9ViewPort.X     = (DWORD)((displayInfo.viewport.Width >> 1) - (viewSize.x * 0.5));
        dx9ViewPort.Width = (DWORD)viewSize.x;

        dx9Device->SetViewport(&dx9ViewPort);
    }

#if !RETRO_USE_ORIGINAL_CODE
    if (screenWidth <= 512 && maxPixHeight <= 256) {
#else
    if (maxPixHeight <= 256) {
#endif
        textureSize.x = 512.0;
        textureSize.y = 256.0;
    }
    else {
        textureSize.x = 1024.0;
        textureSize.y = 512.0;
    }

    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        if (FAILED(dx9Device->CreateTexture((UINT)textureSize.x, (UINT)textureSize.y, 1, 0, D3DFMT_R5G6B5, D3DPOOL_MANAGED,
                                            &screenTextures[s], NULL)))
            return false;
    }

    if (FAILED(dx9Device->CreateTexture(RETRO_VIDEO_TEXTURE_W, RETRO_VIDEO_TEXTURE_H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                        &imageTexture, NULL)))
        return false;

    lastShaderID = -1;
    InitVertexBuffer();
    engine.inFocus          = 1;
    videoSettings.viewportX = (float)dx9ViewPort.X;
    videoSettings.viewportY = (float)dx9ViewPort.Y;
    videoSettings.viewportW = 1.0 / viewSize.x;
    videoSettings.viewportH = 1.0 / viewSize.y;

    return true;
}

void RenderDevice::LoadShader(const char *fileName, bool32 linear)
{
    char fullFilePath[0x100];
    FileInfo info;

    for (int32 i = 0; i < shaderCount; ++i) {
        if (strcmp(shaderList[i].name, fileName) == 0)
            return;
    }

    if (shaderCount == SHADER_COUNT)
        return;

    ShaderEntry *shader = &shaderList[shaderCount];
    shader->linear      = linear;
    sprintf_s(shader->name, sizeof(shader->name), "%s", fileName);

    // if the vertex shader source doesn't exist, fall back and try to load the vertex shader bytecode
    sprintf_s(fullFilePath, sizeof(fullFilePath), "game:\\Shaders\\X360\\%s.vso", fileName);
    
    FILE* fHandle = fopen(fullFilePath, "rb");
    if (fHandle) {
        fseek(fHandle, 0, SEEK_END);
        long fileSize = ftell(fHandle);
        fseek(fHandle, 0, SEEK_SET);

        uint8* fileData = (uint8*)malloc(fileSize);
        fread(fileData, 1, fileSize, fHandle);
        fclose(fHandle);

        if (FAILED(dx9Device->CreateVertexShader((DWORD *)fileData, &shader->vertexShaderObject))) {
            if (shader->vertexShaderObject) {
                shader->vertexShaderObject->Release();
                shader->vertexShaderObject = NULL;
            }
            free(fileData);
            return;
        }

        free(fileData);
    }

    // if the pixel shader source doesn't exist, fall back and try to load the pixel shader bytecode
    sprintf_s(fullFilePath, sizeof(fullFilePath), "game:\\Shaders\\X360\\%s.fso", fileName);
    
    fHandle = fopen(fullFilePath, "rb");
    if (fHandle) {
        fseek(fHandle, 0, SEEK_END);
        long fileSize = ftell(fHandle);
        fseek(fHandle, 0, SEEK_SET);

        uint8* fileData = (uint8*)malloc(fileSize);
        fread(fileData, 1, fileSize, fHandle);
        fclose(fHandle);

        if (FAILED(dx9Device->CreatePixelShader((DWORD *)fileData, &shader->pixelShaderObject))) {
            if (shader->pixelShaderObject) {
                shader->pixelShaderObject->Release();
                shader->pixelShaderObject = NULL;
            }
            free(fileData); // CRITICAL FIX: Use free()
            return;
        }

        free(fileData);
    }

    shaderCount++;
}

bool RenderDevice::InitShaders()
{
    dx9Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dx9Device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dx9Device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    int32 maxShaders = 0;
#if RETRO_USE_MOD_LOADER
    // this causes small memleaks here and in other render devices, as we never close the existing shaders
    // TODO: fix? 🤨
    shaderCount = 0;
#endif
    if (videoSettings.shaderSupport) {
        LoadShader("None", false);
        LoadShader("Clean", true);
        LoadShader("CRT-Yeetron", true);
        LoadShader("CRT-Yee64", true);

#if RETRO_USE_MOD_LOADER
        // a place for mods to load custom shaders
        RunModCallbacks(MODCB_ONSHADERLOAD, NULL);
        userShaderCount = shaderCount;
#endif

        LoadShader("YUV-420", true);
        LoadShader("YUV-422", true);
        LoadShader("YUV-444", true);
        LoadShader("RGB-Image", true);
        maxShaders = shaderCount;
    }
    else {
        for (int32 s = 0; s < SHADER_COUNT; ++s) shaderList[s].linear = true;

        shaderList[0].linear = shaderList[0].linear;
        maxShaders           = 1;
        shaderCount          = 1;
    }

    // no shaders == no support
    if (!maxShaders) {
        videoSettings.shaderSupport = false;

        for (int32 s = 0; s < SHADER_COUNT; ++s) shaderList[s].linear = true;

        shaderList[0].linear = shaderList[0].linear;
        maxShaders           = 1;
        shaderCount          = 1;
    }

    videoSettings.shaderID = videoSettings.shaderID >= maxShaders ? 0 : videoSettings.shaderID;

    if (shaderList[videoSettings.shaderID].linear || videoSettings.screenCount > 1) {
        dx9Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        dx9Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    }
    else {
        dx9Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        dx9Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    }

    dx9Device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

    return true;
}

bool RenderDevice::SetupRendering()
{
    dx9Context = Direct3DCreate9(D3D_SDK_VERSION);
    if (!dx9Context)
        return false;

    ZeroMemory(&deviceIdentifier, sizeof(deviceIdentifier));

    GetDisplays();

    if (!InitGraphicsAPI() || !InitShaders())
        return false;

    int32 size = videoSettings.pixWidth >= SCREEN_YSIZE ? videoSettings.pixWidth : SCREEN_YSIZE;
    scanlines  = (ScanlineInfo *)malloc(size * sizeof(ScanlineInfo));
    memset(scanlines, 0, size * sizeof(ScanlineInfo));

    videoSettings.windowState = WINDOWSTATE_ACTIVE;
    videoSettings.dimMax      = 1.0;
    videoSettings.dimPercent  = 1.0;

    return true;
}

void RenderDevice::GetDisplays()
{
    dxAdapter = 0;
    adapterCount = 1; 
	
    XVIDEO_MODE videoMode;
    XGetVideoMode(&videoMode);

	displayWidth[0]  = videoMode.dwDisplayWidth;
    displayHeight[0] = videoMode.dwDisplayHeight;

	if (dx9Context) {
        D3DADAPTER_IDENTIFIER9 adapterIdentifier;
        ZeroMemory(&adapterIdentifier, sizeof(adapterIdentifier));
        dx9Context->GetAdapterIdentifier(dxAdapter, 0, &adapterIdentifier);
        deviceIdentifier = adapterIdentifier.DeviceIdentifier;
    }

	if (displayInfo.displays)
        free(displayInfo.displays);

	displayCount = 1;
    displayInfo.displays = (decltype(displayInfo.displays))malloc(sizeof(RenderDevice::WindowInfo));
    
	displayInfo.displays[0].width        = videoMode.dwDisplayWidth;
    displayInfo.displays[0].height       = videoMode.dwDisplayHeight;
    displayInfo.displays[0].refresh_rate = 60;
    displayInfo.displays[0].internal.Format = D3DFMT_A8R8G8B8;

    videoSettings.fsWidth     = videoMode.dwDisplayWidth;
    videoSettings.fsHeight    = videoMode.dwDisplayHeight;
    videoSettings.refreshRate = 60;
}

void RenderDevice::GetWindowSize(int32 *width, int32 *height)
{
    XVIDEO_MODE videoMode;
    XGetVideoMode(&videoMode);

    if (width)
        *width = videoMode.dwDisplayWidth;

    if (height)
        *height = videoMode.dwDisplayHeight;
}

bool RenderDevice::ProcessEvents()
{
	SKU::InitXInputAPI();
	SKU::UpdateXInputDevices();

    return isRunning;
}

void RenderDevice::SetupImageTexture(int32 width, int32 height, uint8 *imagePixels)
{
    if (!imagePixels)
        return;

    dx9Device->SetTexture(0, NULL);

    if (!scratchBuffer) {
        scratchBuffer = (uint32*)malloc(1280 * 720 * 4); 
    }

    D3DLOCKED_RECT rect;
    if (SUCCEEDED(imageTexture->LockRect(0, &rect, NULL, 0))) {
        D3DSURFACE_DESC desc;
        imageTexture->GetLevelDesc(0, &desc);

        memcpy(scratchBuffer, imagePixels, width * height * 4);

        RECT sourceRect = { 0, 0, (LONG)width, (LONG)height };
        
		XGTileSurface(
            rect.pBits,       
            desc.Width,       
            desc.Height,      
            NULL,             
            scratchBuffer,    
            width * 4,
            &sourceRect,
            4                 
        );

        imageTexture->UnlockRect(0);
    }
}

void RenderDevice::SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU, int32 strideV)
{
    dx9Device->SetTexture(0, NULL);

    if (!scratchBuffer) {
        scratchBuffer = (uint32*)malloc(1280 * 720 * 4); 
    }

    D3DLOCKED_RECT rect;
    if (SUCCEEDED(imageTexture->LockRect(0, &rect, NULL, 0))) {
        D3DSURFACE_DESC desc;
        imageTexture->GetLevelDesc(0, &desc);

        uint32* dst = scratchBuffer;

        if (videoSettings.shaderSupport) {
			for (int32 y = 0; y < height; ++y) {
				uint8 *rowY = yPlane + (y * strideY);

				bool inChromaY = (y < (height / 2));
				uint8 *rowU = inChromaY ? (uPlane + (y * strideU)) : nullptr;
				uint8 *rowV = inChromaY ? (vPlane + (y * strideV)) : nullptr;

				for (int32 x = 0; x < width; ++x) {
					uint8 yVal = rowY[x];

					uint8 uVal = 0x80;
					uint8 vVal = 0x80;
            
					if (inChromaY && (x < (width / 2))) {
						uVal = rowU[x];
						vVal = rowV[x];
					}

					*dst++ = 0xFF000000 | (yVal << 16) | (uVal << 8) | (vVal);
				}
			}
		}
        else {
            for (int32 y = 0; y < height; ++y) {
                uint8 *rowY = yPlane + (y * strideY);
                for (int32 x = 0; x < width; ++x) {
                    uint8 val = rowY[x];
                    *dst++ = 0xFF000000 | (val << 16) | (val << 8) | val;
                }
            }
        }

        RECT sourceRect = { 0, 0, (LONG)width, (LONG)height };
        
        XGTileSurface(
            rect.pBits,          
            desc.Width,          
            desc.Height,         
            NULL,                
            scratchBuffer,       
            width * 4,
            &sourceRect,
            4                    
        );

        imageTexture->UnlockRect(0);
    }
}
void RenderDevice::SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU, int32 strideV)
{
    dx9Device->SetTexture(0, NULL);

    if (!scratchBuffer) {
        scratchBuffer = (uint32*)malloc(1280 * 720 * 4); 
    }

    D3DLOCKED_RECT rect;
    if (SUCCEEDED(imageTexture->LockRect(0, &rect, NULL, 0))) {
        D3DSURFACE_DESC desc;
        imageTexture->GetLevelDesc(0, &desc);

        uint32* dst = scratchBuffer;

        if (videoSettings.shaderSupport) {
            for (int32 y = 0; y < height; ++y) {
                uint8 *rowY = yPlane + (y * strideY);

                bool inChromaY = (y < (height / 2));
                
                uint8 *rowU = inChromaY ? (uPlane + ((y * 2) * strideU)) : nullptr;
                uint8 *rowV = inChromaY ? (vPlane + ((y * 2) * strideV)) : nullptr;

                for (int32 x = 0; x < width; ++x) {
                    uint8 yVal = rowY[x];
                    uint8 uVal = 0x80;
                    uint8 vVal = 0x80;

                    if (inChromaY && (x < (width / 2))) {
                        uVal = rowU[x];
                        vVal = rowV[x];
                    }

                    *dst++ = 0xFF000000 | (yVal << 16) | (uVal << 8) | (vVal);
                }
            }
        }
        else {
            for (int32 y = 0; y < height; ++y) {
                uint8 *rowY = yPlane + (y * strideY);
                for (int32 x = 0; x < width; ++x) {
                    uint8 val = rowY[x];
                    *dst++ = 0xFF000000 | (val << 16) | (val << 8) | val;
                }
            }
        }

        RECT sourceRect = { 0, 0, (LONG)width, (LONG)height };
        XGTileSurface(
            rect.pBits,       
            desc.Width,       
            desc.Height,      
            NULL,             
            scratchBuffer,    
            width * 4,
            &sourceRect,
            4                 
        );

        imageTexture->UnlockRect(0);
    }
}
void RenderDevice::SetupVideoTexture_YUV444(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU, int32 strideV)
{
    dx9Device->SetTexture(0, NULL);

    if (!scratchBuffer) {
        scratchBuffer = (uint32*)malloc(1280 * 720 * 4); 
    }

    D3DLOCKED_RECT rect;
    if (SUCCEEDED(imageTexture->LockRect(0, &rect, NULL, 0))) {
        D3DSURFACE_DESC desc;
        imageTexture->GetLevelDesc(0, &desc);

        uint32* dst = scratchBuffer;

        if (videoSettings.shaderSupport) {
            for (int32 y = 0; y < height; ++y) {
                uint8 *rowY = yPlane + (y * strideY);

                bool inChromaY = (y < (height / 2));
                
                uint8 *rowU = inChromaY ? (uPlane + ((y * 2) * strideU)) : nullptr;
                uint8 *rowV = inChromaY ? (vPlane + ((y * 2) * strideV)) : nullptr;

                for (int32 x = 0; x < width; ++x) {
                    uint8 yVal = rowY[x];
                    uint8 uVal = 0x80;
                    uint8 vVal = 0x80;

                    if (inChromaY && (x < (width / 2))) {
                        uVal = rowU[x * 2];
                        vVal = rowV[x * 2];
                    }

                    *dst++ = 0xFF000000 | (yVal << 16) | (uVal << 8) | (vVal);
                }
            }
        }
        else {
            for (int32 y = 0; y < height; ++y) {
                uint8 *rowY = yPlane + (y * strideY);
                for (int32 x = 0; x < width; ++x) {
                    uint8 val = rowY[x];
                    *dst++ = 0xFF000000 | (val << 16) | (val << 8) | val;
                }
            }
        }

        RECT sourceRect = { 0, 0, (LONG)width, (LONG)height };
        XGTileSurface(
            rect.pBits,       
            desc.Width,       
            desc.Height,      
            NULL,             
            scratchBuffer,    
            width * 4,
            &sourceRect,
            4                 
        );

        imageTexture->UnlockRect(0);
    }
}