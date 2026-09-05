#include "single_stage.h"
#include "single_stage_frame.h"
#include "ui_scale.h"
#include "single_stage_device.h"
#include "single_stage_pointer.h"
#include "console/windows_wrap.h"
#include <d3d9.h>
#include <ddraw.h>
#include <intrin.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>
#include "single_stage_shader.h"

namespace single_stage
{
    namespace
    {
        Frame building, published;
        std::mutex frame_mutex;
        std::recursive_mutex device_mutex;
        bool configured, enabled;
        bool smooth_world_edges;
        bool high_refresh_pointer, ui_captured;
        uint64_t serial;
        uintptr_t wrapper_start, wrapper_end;
        void **wrapper_device_slot;
        thread_local bool in_adapter;
        using DrawFn = HRESULT (WINAPI *)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, UINT, UINT);
        using ResetFn = HRESULT (WINAPI *)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
        using ReleaseFn = ULONG (WINAPI *)(IDirect3DDevice9 *);
        struct Table { void **slots; DrawFn draw; ResetFn reset; ReleaseFn release; };
        Table tables[4] = {};
        std::atomic<unsigned> table_count{0};
        IDirect3DDevice9 *owner;
        IDirect3DTexture9 *world_texture, *ui_texture, *palette_texture, *native_ui_texture;
        IDirect3DPixelShader9 *shader;
        IDirect3DStateBlock9 *saved_state;
        unsigned world_tw, world_th, ui_tw, ui_th;
        uint64_t uploaded;

        Table *Find(IDirect3DDevice9 *device)
        {
            void **slots = *reinterpret_cast<void ***>(device);
            for (unsigned i = 0; i < table_count.load(); ++i)
                if (tables[i].slots == slots) return &tables[i];
            return nullptr;
        }
        void FreeResources()
        {
            if (saved_state) saved_state->Release();
            if (shader) shader->Release();
            if (world_texture) world_texture->Release();
            if (ui_texture) ui_texture->Release();
            if (palette_texture) palette_texture->Release();
            if (native_ui_texture) native_ui_texture->Release();
            saved_state = nullptr; shader = nullptr;
            world_texture = ui_texture = nullptr;
            palette_texture = nullptr;
            native_ui_texture = nullptr;
            owner = nullptr; uploaded = 0;
        }
        unsigned Pow2(unsigned value)
        {
            unsigned result = 1;
            while (result < value) result *= 2;
            return result;
        }
        bool Resources(IDirect3DDevice9 *device, const Frame &f)
        {
            // Allocate to the logical maximum once, not at each zoom step.
            const unsigned w = Pow2(f.width), h = Pow2(f.height);
            if (owner == device && world_texture && ui_texture && palette_texture && native_ui_texture && shader &&
                saved_state && ui_tw == w && ui_th == h) return true;
            FreeResources();
            owner = device;
            world_tw = ui_tw = w; world_th = ui_th = h;
            if (FAILED(device->CreateTexture(w, h, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8,
                    D3DPOOL_DEFAULT, &world_texture, nullptr)) ||
                FAILED(device->CreateTexture(w, h, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8L8,
                    D3DPOOL_DEFAULT, &ui_texture, nullptr)) ||
                FAILED(device->CreateTexture(256, 1, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                    D3DPOOL_DEFAULT, &palette_texture, nullptr)) ||
                FAILED(device->CreateTexture(AtlasWidth, AtlasHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8L8,
                    D3DPOOL_DEFAULT, &native_ui_texture, nullptr)) ||
                FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD *>(single_stage_shader), &shader)) ||
                FAILED(device->CreateStateBlock(D3DSBT_ALL, &saved_state)))
            { FreeResources(); return false; }
            return true;
        }
        bool Upload(const Frame &f)
        {
            if (uploaded == f.serial) return true;
            D3DLOCKED_RECT lock;
            if (FAILED(world_texture->LockRect(0, &lock, nullptr, D3DLOCK_DISCARD))) return false;
            for (unsigned y = 0; y < f.world_height; ++y)
                memcpy(static_cast<uint8_t *>(lock.pBits) + static_cast<size_t>(y) * lock.Pitch,
                    f.world.data() + static_cast<size_t>(y) * f.world_width, f.world_width);
            world_texture->UnlockRect(0);
            if (FAILED(ui_texture->LockRect(0, &lock, nullptr, D3DLOCK_DISCARD))) return false;
            for (unsigned y = 0; y < f.height; ++y)
                memcpy(static_cast<uint8_t *>(lock.pBits) + static_cast<size_t>(y) * lock.Pitch,
                    f.ui.data() + static_cast<size_t>(y) * f.width * 2, f.width * 2);
            ui_texture->UnlockRect(0);
            if (!f.native_ui.empty() || f.pointer.enabled)
            {
                if (FAILED(native_ui_texture->LockRect(0, &lock, nullptr, D3DLOCK_DISCARD))) return false;
                if(!f.native_ui.empty()) for (unsigned y = 0; y < NativeHeight * NativeLayers; ++y)
                    memcpy(static_cast<uint8_t *>(lock.pBits) + static_cast<size_t>(y) * lock.Pitch,
                        f.native_ui.data() + static_cast<size_t>(y) * NativeWidth * 2, NativeWidth * 2);
                if(f.pointer.enabled && f.pointer.pixels.size()==CursorSize*CursorSize*2)
                    for(unsigned y=0;y<CursorSize;++y)
                        memcpy(static_cast<uint8_t *>(lock.pBits)+size_t(y+CursorAtlasY)*lock.Pitch,
                            f.pointer.pixels.data()+y*CursorSize*2,CursorSize*2);
                native_ui_texture->UnlockRect(0);
            }
            if (FAILED(palette_texture->LockRect(0, &lock, nullptr, D3DLOCK_DISCARD))) return false;
            memcpy(lock.pBits, f.palette, sizeof(f.palette));
            palette_texture->UnlockRect(0);
            uploaded = f.serial;
            return true;
        }
        bool OutputSize(IDirect3DDevice9 *device, float (&size)[4])
        {
            // cnc-ddraw uses an XYZRHW quad with aspect bars baked into its
            // vertices, not SetViewport. Read its 96-byte managed CPU buffer;
            // never infer output dimensions from noisy fragment derivatives.
            IDirect3DVertexBuffer9 *buffer = nullptr;
            UINT offset=0,stride=0;
            if (FAILED(device->GetStreamSource(0,&buffer,&offset,&stride)) || !buffer) return false;
            D3DVERTEXBUFFER_DESC desc={};void *data=nullptr;bool good=false;
            struct Vertex { float x,y,z,rhw,u,v; } vertices[4];
            if (stride==sizeof(Vertex) && SUCCEEDED(buffer->GetDesc(&desc)) &&
                desc.Pool==D3DPOOL_MANAGED && !(desc.Usage&D3DUSAGE_WRITEONLY) &&
                offset<=desc.Size && sizeof(vertices)<=desc.Size-offset &&
                SUCCEEDED(buffer->Lock(offset,sizeof(vertices),&data,D3DLOCK_READONLY)))
            {
                memcpy(vertices,data,sizeof(vertices));buffer->Unlock();
                float minx=vertices[0].x,maxx=minx,miny=vertices[0].y,maxy=miny;
                for(const auto &v:vertices)
                { minx=(std::min)(minx,v.x);maxx=(std::max)(maxx,v.x);miny=(std::min)(miny,v.y);maxy=(std::max)(maxy,v.y); }
                size[0]=maxx-minx;size[1]=maxy-miny;
                size[2]=minx+0.5f;size[3]=miny+0.5f;
                good=size[0]>=1&&size[1]>=1&&size[0]<=16384&&size[1]<=16384;
            }
            buffer->Release();return good;
        }
        HRESULT WINAPI Draw(IDirect3DDevice9 *device, D3DPRIMITIVETYPE type, UINT start, UINT count)
        {
            Table *table = Find(device);
            if (!table) return D3DERR_INVALIDCALL;
            const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
            if (in_adapter || caller < wrapper_start || caller >= wrapper_end ||
                type != D3DPT_TRIANGLESTRIP || start != 0 || count != 2)
                return table->draw(device, type, start, count);
            std::lock_guard<std::recursive_mutex> guard(device_mutex);
            in_adapter = true;
            bool active = false;
            IDirect3DBaseTexture9 *base = nullptr, *palette = nullptr;
            D3DSURFACE_DESC source_desc = {}, palette_desc = {};
            if (SUCCEEDED(device->GetTexture(0, &base)) && base && base->GetType() == D3DRTYPE_TEXTURE &&
                SUCCEEDED(device->GetTexture(1, &palette)) && palette && palette->GetType() == D3DRTYPE_TEXTURE &&
                SUCCEEDED(static_cast<IDirect3DTexture9 *>(base)->GetLevelDesc(0, &source_desc)) &&
                SUCCEEDED(static_cast<IDirect3DTexture9 *>(palette)->GetLevelDesc(0, &palette_desc)) &&
                // cnc-ddraw allocates a square palette texture and updates
                // only its first row. Our own palette texture is 256x1; do
                // not confuse that private upload format with the wrapper's.
                palette_desc.Width == 256 &&
                (palette_desc.Height == 256 || palette_desc.Height == 1))
            {
                std::lock_guard<std::mutex> frames(frame_mutex);
                const Frame &f = published;
                float output_size[4];
                if (f.valid && source_desc.Width >= f.width && source_desc.Height >= f.height &&
                    OutputSize(device,output_size) && Resources(device, f) && Upload(f) && SUCCEEDED(saved_state->Capture()))
                {
                    const float c0[] = {float(f.width), float(f.height), float(source_desc.Width), float(source_desc.Height)};
                    const float c1[] = {float(f.world_width), float(f.world_height), float(world_tw), float(world_th)};
                    const float c2[] = {float(ui_tw), float(ui_th), 0, 0};
                    const float c7[] = {f.smooth_world_edges ? 1.0f : 0.0f,
                        output_size[0], output_size[1], 0};
                    const auto pointer=PollPointer(f.pointer,f.width,f.height,output_size[2],output_size[3],output_size[0],output_size[1]);
                    float native_rects[NativeLayers * 4];
                    for (unsigned i = 0; i < NativeLayers; ++i)
                    {
                        const auto &r = f.native_rects[i];
                        native_rects[4*i] = float(r.left); native_rects[4*i+1] = float(r.top);
                        native_rects[4*i+2] = float(r.width); native_rects[4*i+3] = float(r.height);
                    }
                    active = SUCCEEDED(device->SetPixelShader(shader)) &&
                        SUCCEEDED(device->SetTexture(1, palette_texture)) &&
                        SUCCEEDED(device->SetTexture(2, world_texture)) &&
                        SUCCEEDED(device->SetTexture(3, ui_texture)) &&
                        SUCCEEDED(device->SetTexture(4, native_ui_texture)) &&
                        SUCCEEDED(device->SetPixelShaderConstantF(0, c0, 1)) &&
                        SUCCEEDED(device->SetPixelShaderConstantF(1, c1, 1)) &&
                        SUCCEEDED(device->SetPixelShaderConstantF(2, c2, 1)) &&
                        SUCCEEDED(device->SetPixelShaderConstantF(3, native_rects, NativeLayers)) &&
                        SUCCEEDED(device->SetPixelShaderConstantF(7, c7, 1)) &&
                        SUCCEEDED(device->SetPixelShaderConstantF(8,pointer.cursor,1)) &&
                        SUCCEEDED(device->SetPixelShaderConstantF(9,pointer.shape,1)) &&
                        SUCCEEDED(device->SetPixelShaderConstantF(10,pointer.selection,1));
                    for (unsigned sampler = 1; sampler <= 4; ++sampler)
                    {
                        active &= SUCCEEDED(device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_POINT));
                        active &= SUCCEEDED(device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
                        active &= SUCCEEDED(device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE));
                        active &= SUCCEEDED(device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
                        active &= SUCCEEDED(device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
                    }
                    if (!active) saved_state->Apply();
                }
            }
            if (base) base->Release();
            if (palette) palette->Release();
            const HRESULT result = table->draw(device, type, start, count);
            if (active) saved_state->Apply();
            in_adapter = false;
            return result;
        }
        HRESULT WINAPI Reset(IDirect3DDevice9 *device, D3DPRESENT_PARAMETERS *parameters)
        {
            Table *table = Find(device);
            if (!table) return D3DERR_INVALIDCALL;
            std::lock_guard<std::recursive_mutex> guard(device_mutex);
            const bool nested = in_adapter; in_adapter = true;
            if (owner == device) FreeResources();
            HRESULT result = table->reset(device, parameters);
            in_adapter = nested;
            return result;
        }
        ULONG WINAPI Release(IDirect3DDevice9 *device)
        {
            Table *table = Find(device);
            if (!table) return 0;
            if (in_adapter) return table->release(device);
            std::lock_guard<std::recursive_mutex> guard(device_mutex);
            in_adapter = true;
            if (owner == device) FreeResources();
            ULONG result = table->release(device);
            in_adapter = false;
            return result;
        }
        bool InstallTable(IDirect3DDevice9 *device)
        {
            if (Find(device)) return true;
            unsigned index = table_count.load();
            if (index == 4) return false;
            void **v = *reinterpret_cast<void ***>(device);
            DWORD old;
            if (!VirtualProtect(v, 119 * sizeof(void *), PAGE_READWRITE, &old)) return false;
            tables[index] = {v, reinterpret_cast<DrawFn>(v[81]),
                reinterpret_cast<ResetFn>(v[16]), reinterpret_cast<ReleaseFn>(v[2])};
            table_count.store(index + 1);
            InterlockedExchangePointer(v + 2, reinterpret_cast<void *>(&Release));
            InterlockedExchangePointer(v + 16, reinterpret_cast<void *>(&Reset));
            InterlockedExchangePointer(v + 81, reinterpret_cast<void *>(&Draw));
            DWORD ignored; VirtualProtect(v, 119 * sizeof(void *), old, &ignored);
            return true;
        }
        void AttachWrapperDevice()
        {
            // The wrapper may create its device after our initial setup.
            // One pointer read per frame, with no repeated module scan.
            if (!wrapper_device_slot) return;
            auto device = static_cast<IDirect3DDevice9 *>(*wrapper_device_slot);
            if (!device || Find(device)) return;
            std::lock_guard<std::recursive_mutex> guard(device_mutex);
            enabled |= InstallTable(device);
        }
        void Configure()
        {
            if (configured) return;
            configured = true;
            char path[MAX_PATH], value[64];
            if (!GetFullPathNameA("cosmonarchy_viewport.ini", MAX_PATH, path, nullptr)) return;
            GetPrivateProfileStringA("presentation", "world_filter", "sharp_edges", value, sizeof(value), path);
            smooth_world_edges = _stricmp(value, "sharp_edges") == 0;
            high_refresh_pointer=GetPrivateProfileIntA("presentation","high_refresh_pointer",0,path)!=0;
            // Resized UI also requires final-output sampling with world zoom off.
            if (!GetPrivateProfileIntA("world_zoom", "enabled", 0, path) &&
                !ui_scale::hud.Scaled() && !ui_scale::objectives.Scaled() && !smooth_world_edges && !high_refresh_pointer) return;
            GetPrivateProfileStringA("presentation", "backend", "single_stage", value, sizeof(value), path);
            if (_stricmp(value, "single_stage") != 0) { smooth_world_edges = high_refresh_pointer = false; return; }
            HMODULE wrapper = GetModuleHandleA("ddraw.dll");
            if (!wrapper || !GetProcAddress(wrapper, "DDIsWindowed")) return;
            wrapper_start = reinterpret_cast<uintptr_t>(wrapper);
            auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(wrapper);
            auto nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(wrapper_start + dos->e_lfanew);
            wrapper_end = wrapper_start + nt->OptionalHeader.SizeOfImage;
            enabled = InstallPortableAdapters(wrapper);
            wrapper_device_slot = FindWrapperDeviceSlot(wrapper);
        }
    }
    bool WithFrame(bool (*callback)(const Frame &, void *), void *context)
    {
        std::lock_guard<std::mutex> guard(frame_mutex);
        return published.valid && callback(published, context);
    }
#ifdef SINGLE_STAGE_TEST
    void TestResetProducer()
    { building = Frame{}; published = Frame{}; configured = enabled = true; wrapper_device_slot = nullptr; }
    size_t TestCaptureWorld(const uint8_t *pixels,unsigned width,unsigned height,unsigned cw,unsigned ch)
    {
        const size_t before=building.world.capacity();
        Begin(true,width,height); World(pixels,width,0,0,cw,ch);
        const size_t grows=building.world.capacity()!=before;
        for(unsigned y=0;y<ch;++y)
            if(memcmp(building.world.data()+size_t(y)*cw,pixels+size_t(y)*width,cw)!=0)
                return SIZE_MAX;
        std::swap(building,published);
        return grows;
    }
    bool TestAttach(IDirect3DDevice9 *device, const Frame &frame)
    {
        // Offline host only: exercise the real adapter and state restoration.
        published = frame; wrapper_start = 0; wrapper_end = UINTPTR_MAX;
        return InstallTable(device);
    }
    bool TestDiscover(HMODULE wrapper, const Frame &frame)
    {
        published = frame; wrapper_start = 0; wrapper_end = UINTPTR_MAX;
        wrapper_device_slot = FindWrapperDeviceSlot(wrapper);
        AttachWrapperDevice();
        return wrapper_device_slot && Find(static_cast<IDirect3DDevice9 *>(*wrapper_device_slot));
    }
    uint64_t TestUploadedSerial() { return uploaded; }
    bool TestPointerSnapshot()
    {
        configured=enabled=high_refresh_pointer=true;wrapper_device_slot=nullptr;
        std::vector<uint8_t> clean(640*480,17);
        Begin(true,640,480);World(clean.data(),640,0,0,640,480);Opaque(0,0,10,10);
        PointerBackground(clean.data(),640,480,0,false);
        clean[0]=99; // The ordinary fallback cursor is drawn after the snapshot.
        bool good=PointerCapturing()&&ui_captured&&building.ui[0]==17&&building.ui[1]==255;
        Begin(true,640,480);
        good&=!PointerCapturing()&&!ui_captured&&!building.pointer.dragging;
        high_refresh_pointer=false;
        return good;
    }
    bool TestPausedLayers()
    {
        configured=enabled=smooth_world_edges=true;wrapper_device_slot=nullptr;
        ui_scale::Configure(1920,1080,720,720,false);
        std::vector<uint8_t> clean(1920*1080,17);
        bool good=true;
        for(bool pointer : {false,true})
        {
            high_refresh_pointer=pointer;
            Begin(true,1920,1080); World(clean.data(),1920,320,180,1280,720);
            NativeUiPixel(1,ui_scale::hud,25,415,99);
            Opaque(800,350,200,160); // Only the preblended dialog is logical UI.
            const auto native=building.native_ui;
            PointerBackground(clean.data(),1920,1080,0,true);
            good &= building.valid && building.smooth_world_edges &&
                building.world_width==1280 && building.world_height==720 &&
                building.native_ui==native && building.mask[0]==0 &&
                building.mask[350*1920+800]==255 &&
                building.pointer.enabled==pointer;
        }
        Reject(); PointerBackground(clean.data(),1920,1080,0,true);
        good &= building.valid && !building.smooth_world_edges &&
            building.world_width==1920 && building.native_ui.empty() && building.mask[0]==255;
        high_refresh_pointer=smooth_world_edges=false;
        ui_scale::Configure(1920,1080,0,0,false);
        return good;
    }
#endif
    void Begin(bool eligible, unsigned width, unsigned height)
    {
        Configure();
        AttachWrapperDevice();
        ui_captured=false;
        building.pointer.enabled=building.pointer.dragging=false;
        building.pointer.width=building.pointer.height=0;
        building.valid = enabled && eligible && width >= 640 && height >= 480 && width <= 3840 && height <= 2160;
        if (!building.valid) return;
        building.width = width; building.height = height;
        building.smooth_world_edges = smooth_world_edges;
        building.world_width = building.world_height = 0;
        // Keep full logical storage in both alternating frames. Only the packed
        // crop prefix is valid; its dimensions, not vector size, bound uploads.
        // Initial allocation happens at first use, never while changing crops.
        building.world.resize(static_cast<size_t>(width) * height);
        building.mask.assign(static_cast<size_t>(width) * height, 0);
        for (auto &rect : building.native_rects) rect = UiRect{};
        if (ui_scale::hud.Scaled() || ui_scale::objectives.Scaled())
            building.native_ui.assign(NativeWidth * NativeHeight * NativeLayers * 2, 0);
        else building.native_ui.clear();
    }
    bool Capturing() { return building.valid; }
    bool Filtering() { Configure(); return smooth_world_edges; }
    bool HighRefreshPointer() { Configure(); return high_refresh_pointer; }
    bool PointerCapturing() { return building.valid && building.pointer.enabled; }
    void PointerBackground(const uint8_t *pixels,unsigned width,unsigned height,uintptr_t window,bool flat)
    {
        if(!HighRefreshPointer())return;
        // A modal dialog no longer invalidates the world/native-UI planes.
        // Flatten only the fallback, never overwrite a valid layered frame.
        if(flat && !building.valid)
        {
            Begin(true,width,height);
            if(!building.valid)return;
            World(pixels,width,0,0,width,height);
            building.smooth_world_edges=false;building.native_ui.clear();
            Opaque(0,0,width,height);
        }
        if(!building.valid)return;
        building.ui.resize(size_t(width)*height*2);
        for(size_t i=0;i<building.mask.size();++i)
        { building.ui[i*2]=pixels[i];building.ui[i*2+1]=building.mask[i]; }
        ui_captured=true;building.pointer.enabled=true;building.pointer.window=window;
        building.pointer.pixels.assign(CursorSize*CursorSize*2,0);
    }
    void PointerCursor(const uint8_t *zero,const uint8_t *full,unsigned width,unsigned height,int hot_x,int hot_y)
    {
        if(!PointerCapturing())return;
        if(width>CursorSize||height>CursorSize){Reject();return;}
        auto &cursor=building.pointer;cursor.width=width;cursor.height=height;cursor.hot_x=hot_x;cursor.hot_y=hot_y;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)
        {
            size_t i=y*CursorSize+x;
            if(zero[i]==full[i]){cursor.pixels[i*2]=zero[i];cursor.pixels[i*2+1]=255;}
            else if(zero[i]!=0||full[i]!=255){Reject();return;}
        }
    }
    void PointerSelection(int x,int y,uint8_t color)
    {
        if(!PointerCapturing())return;
        building.pointer.dragging=true;building.pointer.anchor_x=x;building.pointer.anchor_y=y;building.pointer.selection_color=color;
    }
    bool NativeUiCapturing() { return building.valid && !building.native_ui.empty(); }
    bool NativeUiPixel(unsigned layer, const ui_scale::Geometry &geometry,
                       unsigned x, unsigned y, uint8_t color)
    {
        if (!NativeUiCapturing() || layer >= NativeLayers || x >= NativeWidth || y >= NativeHeight) return false;
        building.native_rects[layer] = {geometry.left, geometry.top, geometry.width, geometry.height};
        const size_t index = ((layer * NativeHeight + y) * NativeWidth + x) * 2;
        building.native_ui[index] = color; building.native_ui[index + 1] = 255;
        return true;
    }
    void Reject() { building.valid = false; }
    void World(const uint8_t *pixels, unsigned pitch, unsigned left, unsigned top, unsigned width, unsigned height)
    {
        if (!building.valid) return;
        if (!width || !height || left + width > building.width || top + height > building.height)
        { Reject(); return; }
        building.world_width = width; building.world_height = height;
        for (unsigned y = 0; y < height; ++y)
            memcpy(building.world.data() + static_cast<size_t>(y) * width,
                pixels + static_cast<size_t>(top + y) * pitch + left, width);
    }
    void Opaque(unsigned x, unsigned y, unsigned width, unsigned height)
    {
        if (!building.valid || x >= building.width || y >= building.height) return;
        width = (std::min)(width, building.width - x); height = (std::min)(height, building.height - y);
        for (unsigned row = 0; row < height; ++row)
            memset(building.mask.data() + static_cast<size_t>(y + row) * building.width + x, 255, width);
    }
    void Submit(const uint8_t *completed)
    {
        if (!enabled) return;
        building.valid &= building.world_width != 0 && building.world_height != 0;
        if (building.valid)
        {
            using ObjectsFn = BOOL (WINAPI *)(IDirectDraw **, IDirectDrawSurface **,
                IDirectDrawSurface **, IDirectDrawSurface **, IDirectDrawSurface **,
                IDirectDrawPalette **, HPALETTE *);
            static auto objects = reinterpret_cast<ObjectsFn>(GetProcAddress(
                GetModuleHandleA("storm.dll"), MAKEINTRESOURCEA(347)));
            IDirectDrawPalette *palette = nullptr;
            PALETTEENTRY colors[256] = {};
            if (!objects || !objects(nullptr, nullptr, nullptr, nullptr, nullptr, &palette, nullptr) ||
                !palette || FAILED(palette->GetEntries(0, 0, 256, colors)))
                building.valid = false;
            for (unsigned i = 0; i < 256; ++i)
                building.palette[i] = 0xff000000u | (unsigned(colors[i].peRed) << 16) |
                    (unsigned(colors[i].peGreen) << 8) | colors[i].peBlue;
            if(!ui_captured)
            {
                building.ui.resize(static_cast<size_t>(building.width) * building.height * 2);
                for (size_t i = 0; i < building.mask.size(); ++i)
                { building.ui[2*i] = completed[i]; building.ui[2*i+1] = building.mask[i]; }
            }
        }
        building.serial = ++serial;
        std::lock_guard<std::mutex> guard(frame_mutex);
        std::swap(published, building);
        building.valid = false;
    }
}
