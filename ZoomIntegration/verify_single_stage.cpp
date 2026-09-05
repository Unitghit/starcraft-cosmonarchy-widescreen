#define NOMINMAX
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <d3d9.h>
#include "../ZoomSource/Cosmonarchy-aidebug-resolution/src/single_stage_portable.cpp"
#include "../ZoomSource/Cosmonarchy-aidebug-resolution/src/single_stage_shader.h"
#include "../ZoomSource/Cosmonarchy-aidebug-resolution/src/single_stage_device.h"
#pragma comment(lib,"d3d9.lib")
namespace single_stage { bool TestAttach(IDirect3DDevice9 *,const Frame &); }
namespace single_stage { bool TestDiscover(HMODULE,const Frame &); }
namespace single_stage {
    void TestResetProducer();
    bool TestPointerSnapshot();
    bool TestPausedLayers();
    uint64_t TestUploadedSerial();
    size_t TestCaptureWorld(const uint8_t *,unsigned,unsigned,unsigned,unsigned);
}
static void Check(bool value,const char *message)
{ if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);} }
static single_stage::Frame MakeFrame()
{
    single_stage::Frame f; f.valid=true; f.width=1920;f.height=1080;
    f.world_width=1280;f.world_height=720;f.serial=1;
    f.world.resize(1280*720);f.ui.assign(1920*1080*2,0);
    for(unsigned y=0;y<720;++y)for(unsigned x=0;x<1280;++x)f.world[y*1280+x]=(x+19*y)%256;
    for(unsigned i=0;i<256;++i)f.palette[i]=0xff000000u|(i<<16)|((255-i)<<8)|((i*17)%256);
    for(unsigned y=1030;y<1070;++y)for(unsigned x=680;x<1240;++x)
    {size_t i=(y*1920+x)*2;f.ui[i]=0;f.ui[i+1]=255;} // Explicit opaque index zero.
    f.native_ui.assign(single_stage::NativeWidth*single_stage::NativeHeight*single_stage::NativeLayers*2,0);
    f.native_rects[0]={0,0,960,720};
    f.native_rects[1]={480,360,960,720};
    f.native_rects[2]={480,0,960,720};
    f.native_rects[3]={480,360,960,720};
    for(unsigned layer=0;layer<4;++layer)
    for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x)
    {
        bool ink=layer==0 ? y<35&&x>=320 : layer==1 ? y>=293 :
            layer==2 ? y<45&&x<400 : y>=250&&y<340&&x>=200&&x<400;
        if (!ink || (x%17==0 && y%13==0)) continue;
        const size_t i=((layer*480+y)*640+x)*2;
        f.native_ui[i]=(x*13+y*29+layer*31)%256;
        f.native_ui[i+1]=255;
    }
    return f;
}
static bool UiCovers(const single_stage::Frame &f,unsigned x,unsigned y,unsigned w,unsigned h)
{
    size_t i=(size_t(single_stage::Sample(y,f.height,h))*f.width+single_stage::Sample(x,f.width,w))*2;
    if(f.ui[i+1])return true;
    for(unsigned l=0;l<4;++l)
    {
        const auto&r=f.native_rects[l];
        int nx=single_stage::NativeSample(x,f.width,w,r.left,r.width,640);
        int ny=single_stage::NativeSample(y,f.height,h,r.top,r.height,480);
        if(nx>=0&&ny>=0&&!f.native_ui.empty()&&f.native_ui[((l*480+ny)*640+nx)*2+1])return true;
    }
    return false;
}
// Independent integer area-overlap oracle, not the shader's floating formula.
static uint32_t ReferenceColor(const single_stage::Frame &f,unsigned x,unsigned y,unsigned w,unsigned h)
{
    if(f.pointer.enabled&&single_stage::test_pointer_override)
    {
        const auto &p=*single_stage::test_pointer_override;
        if(p.shape[0]>0&&p.shape[1]>0)
        {
            int sx=int(std::floor((x-p.cursor[0]+0.5f)/p.cursor[2]));
            int sy=int(std::floor((y-p.cursor[1]+0.5f)/p.cursor[3]));
            if(sx>=0&&sy>=0&&sx<int(p.shape[0])&&sy<int(p.shape[1]))
            {
                size_t i=(sy*single_stage::CursorSize+sx)*2;
                if(f.pointer.pixels[i+1])return f.palette[f.pointer.pixels[i]]&0xffffff;
            }
        }
        if(p.shape[3]>0.5f)
        {
            int l=int(std::floor(p.selection[0])),t=int(std::floor(p.selection[1]));
            int r=int(std::floor(p.selection[2])),b=int(std::floor(p.selection[3]));
            int tx=std::max(1,int(float(w)/f.width+0.5f)),ty=std::max(1,int(float(h)/f.height+0.5f));
            if(int(x)>=l&&int(y)>=t&&int(x)<=r&&int(y)<=b&&
                (int(x)-l<tx||r-int(x)<tx||int(y)-t<ty||b-int(y)<ty))return f.palette[int(p.shape[2])]&0xffffff;
        }
    }
    if(!f.smooth_world_edges||w<f.world_width||h<f.world_height||UiCovers(f,x,y,w,h))
        return f.palette[single_stage::IndexAt(f,x,y,w,h)]&0xffffff;
    uint64_t left=uint64_t(x)*f.world_width,right=left+f.world_width;
    uint64_t top=uint64_t(y)*f.world_height,bottom=top+f.world_height;
    uint64_t sum[3]={};const uint64_t area=uint64_t(f.world_width)*f.world_height;
    for(unsigned sy=unsigned(top/h);sy<=unsigned((bottom-1)/h);++sy)
    for(unsigned sx=unsigned(left/w);sx<=unsigned((right-1)/w);++sx)
    {
        uint64_t weight=(std::min(right,uint64_t(sx+1)*w)-std::max(left,uint64_t(sx)*w))*
            (std::min(bottom,uint64_t(sy+1)*h)-std::max(top,uint64_t(sy)*h));
        uint32_t color=f.palette[f.world[size_t(sy)*f.world_width+sx]];
        for(unsigned c=0;c<3;++c)sum[c]+=((color>>(c*8))&255)*weight;
    }
    uint32_t result=0;for(unsigned c=0;c<3;++c)result|=uint32_t((sum[c]+area/2)/area)<<(c*8);
    return result;
}
static void CheckPixels(const single_stage::Frame &f,const uint32_t *pixels,unsigned pitch,unsigned w,unsigned h,bool flipped)
{
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)
    {
        unsigned actual=pixels[(flipped?h-y-1:y)*pitch+x]&0xffffff;
        unsigned expected=ReferenceColor(f,x,y,w,h);
        unsigned tolerance=f.smooth_world_edges&&!UiCovers(f,x,y,w,h)&&
            (w%f.world_width||h%f.world_height)?1:0;
        bool good=true;for(unsigned c=0;c<3;++c)good&=unsigned(std::abs(int((actual>>(c*8))&255)-int((expected>>(c*8))&255)))<=tolerance;
        if(!good){std::fprintf(stderr,"pixel (%u,%u) got %06X expected %06X\n",x,y,actual,expected);Check(false,"GPU sampling/coverage");}
    }
}
static const single_stage::Frame *gdi_frame;
static int WINAPI CaptureGdi(HDC,int,int,int w,int h,int,int,int sw,int sh,const void *bits,const BITMAPINFO *info,UINT,DWORD)
{
    Check(w==sw&&h==sh&&info->bmiHeader.biHeight==-h,"GDI single scaling and orientation");
    CheckPixels(*gdi_frame,static_cast<const uint32_t *>(bits),w,w,h,false);
    return h;
}
int main(int argc,char **argv)
{
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc==3&&!strcmp(argv[1],"--probe-wrapper"))
    {
        HMODULE wrapper=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        Check(wrapper!=nullptr,"map wrapper without executing it");
        auto slot=single_stage::FindWrapperDeviceSlot(wrapper);
        Check(slot!=nullptr,"unique real-wrapper device signature");
        std::printf("Wrapper device discovery PASS: %s device RVA=%08X\n",argv[2],
            unsigned(reinterpret_cast<uintptr_t>(slot)-reinterpret_cast<uintptr_t>(wrapper)));
        FreeLibrary(wrapper);return 0;
    }
    SetProcessDPIAware();
    auto f=MakeFrame();
    Check(single_stage::TestPointerSnapshot(),"clean pointer background retained and next-frame state reset");
    Check(single_stage::TestPausedLayers(),"pause retains crop, filter and native HUD with pointer on/off; invalid frame still falls back");
    std::puts("Paused producer layer preservation and fallback PASS");
    auto pointer_fixture=single_stage::PointerLayer{};
    pointer_fixture.enabled=pointer_fixture.dragging=true;pointer_fixture.width=pointer_fixture.height=12;
    pointer_fixture.hot_x=-2;pointer_fixture.hot_y=-3;pointer_fixture.anchor_x=100;pointer_fixture.anchor_y=80;
    pointer_fixture.selection_color=9;pointer_fixture.pixels.assign(single_stage::CursorSize*single_stage::CursorSize*2,0);
    for(unsigned y=0;y<12;++y)for(unsigned x=0;x<12;++x)
    { size_t i=(y*single_stage::CursorSize+x)*2;pointer_fixture.pixels[i]=(x+y)%8;pointer_fixture.pixels[i+1]=(x+y)%3?255:0; }
    auto pointer_draw=single_stage::PointerGeometry(pointer_fixture,1920,1080,3840,2160,800,600,true,true);
    Check(pointer_draw.cursor[0]==796&&pointer_draw.cursor[1]==594,"cursor hotspot independent of world zoom");
    Check(pointer_draw.selection[0]==200&&pointer_draw.selection[1]==160,"native drag anchor mapping");
    Check(single_stage::PointerGeometry(pointer_fixture,1920,1080,3840,2160,800,600,false,true).shape[3]==0,"release hides held-frame selection");
    Check(single_stage::PointerGeometry(pointer_fixture,1920,1080,3840,2160,800,600,true,false).shape[0]==0,"unfocused pointer hidden");
    for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b)
    {
        Check(!single_stage::NativeUiNeedsFallback(400,a,b),"opaque HUD boundary cannot reject animation");
        Check(!single_stage::NativeUiNeedsFallback(415,a,b),"animated portrait cannot reject final-output frame");
        Check(single_stage::NativeUiNeedsFallback(399,a,b)==(a!=b&&(a!=0||b!=255)),
            "unowned upper pixels retain conservative blending fallback");
    }
    std::puts("Opaque HUD animation versus transparency: all 65,536 palette pairs PASS");
    for(unsigned x=0;x<3840;++x)Check(single_stage::Sample(x,1280,3840)==x/3,"uniform 3x run");
    unsigned bad=0;
    for(unsigned x=0;x<3840;++x)bad+=single_stage::Sample(single_stage::Sample(x,1920,3840),1280,1920)!=x/3;
    Check(bad!=0,"baseline double resampling reproduced");
    std::printf("CPU direct 3x sampling PASS; old double-stage mismatches=%u columns\n",bad);
    unsigned hud_bad=0;
    for(unsigned p=0;p<1920;++p)
    {
        hud_bad+=single_stage::Sample(single_stage::Sample(p,960,1920),640,960)!=p/3;
        const size_t source=((480+400)*640+p/3)*2;
        for(unsigned dy=0;dy<3;++dy)
            Check(single_stage::IndexAt(f,960+p,1920+dy,3840,2160)==f.native_ui[source],
                "native HUD is uniform 3x in both axes, not alternating 2/4 blocks");
    }
    Check(hud_bad==320,"HUD double-stage baseline reproduced");
    std::printf("Native HUD 3x both axes PASS; old double-stage mismatches=%u columns\n",hud_bad);
    gdi_frame=&f;single_stage::original_stretch=&CaptureGdi;
    for(auto size:{std::pair{3840,2160},{1365,767}})
    {
        single_stage::GdiCall call={nullptr,0,0,size.first,size.second,1920,1080,0};
        Check(single_stage::DrawGdi(f,&call),"GDI adapter");
    }
    std::puts("GDI production composition exact world/UI pixels PASS");
    HWND window=CreateWindowExA(0,"STATIC","Offline single-stage test",WS_POPUP,0,0,3840,2160,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    Check(window!=nullptr,"hidden window");HDC dc=GetDC(window);
    PIXELFORMATDESCRIPTOR pfd={};pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cAlphaBits=8;
    int format=ChoosePixelFormat(dc,&pfd);Check(format&&SetPixelFormat(dc,format,&pfd),"OpenGL pixel format");
    HGLRC gl=wglCreateContext(dc);Check(gl&&wglMakeCurrent(dc,gl),"OpenGL context");
    if(argc>1&&!strcmp(argv[1],"--core"))
    {
        using CreateContextFn=HGLRC (WINAPI *)(HDC,HGLRC,const int *);
        auto create=reinterpret_cast<CreateContextFn>(wglGetProcAddress("wglCreateContextAttribsARB"));
        Check(create!=nullptr,"core context API");const int attributes[]={0x2091,3,0x2092,2,0x9126,1,0};
        HGLRC core=create(dc,nullptr,attributes);Check(core!=nullptr,"core context");
        wglMakeCurrent(nullptr,nullptr);wglDeleteContext(gl);gl=core;
        Check(wglMakeCurrent(dc,gl),"core current context");
    }
    std::printf("OpenGL driver: %s\n",glGetString(GL_VERSION));
    std::vector<uint32_t> pixels(3840*2160);
    for(auto size:{std::pair{3840u,2160u},{1920u,1080u},{1365u,767u}})
    {
        int ox=size.first==1365?13:0,oy=size.first==1365?7:0;
        glViewport(ox,oy,size.first,size.second);glClear(GL_COLOR_BUFFER_BIT);
        Check(single_stage::DrawGl(f,nullptr),"OpenGL presentation");
        glReadPixels(ox,oy,size.first,size.second,0x80E1,GL_UNSIGNED_BYTE,pixels.data());
        CheckPixels(f,pixels.data(),size.first,size.first,size.second,true);
        std::printf("OpenGL %ux%u exact world/UI pixels PASS\n",size.first,size.second);
    }
    f.smooth_world_edges=true;
    for(auto size:{std::pair{3840u,2160u},{3200u,1800u},{1365u,767u},{960u,540u}})
    {
        glViewport(0,0,size.first,size.second);
        Check(single_stage::DrawGl(f,nullptr),"filtered OpenGL presentation");
        glReadPixels(0,0,size.first,size.second,0x80E1,GL_UNSIGNED_BYTE,pixels.data());
        CheckPixels(f,pixels.data(),size.first,size.first,size.second,true);
        std::printf("OpenGL edge filter %ux%u integer/area coverage and unchanged HUD PASS\n",size.first,size.second);
    }
    f.smooth_world_edges=false;
    f.pointer=pointer_fixture;++f.serial;
    glViewport(0,0,3840,2160);
    for(auto position:{std::pair{800,600},{1400,1100},{0,0}})
    {
        pointer_draw=single_stage::PointerGeometry(f.pointer,f.width,f.height,3840,2160,float(position.first),float(position.second),true,true);
        single_stage::test_pointer_override=&pointer_draw;
        Check(single_stage::DrawGl(f,nullptr),"held-frame GL pointer draw");
        glReadPixels(0,0,3840,2160,0x80E1,GL_UNSIGNED_BYTE,pixels.data());
        CheckPixels(f,pixels.data(),3840,3840,2160,true);
        Check(single_stage::uploaded==f.serial,"GL pointer motion reuses uploaded serial");
    }
    single_stage::test_pointer_override=nullptr;f.pointer.enabled=false;
    std::puts("OpenGL held-frame cursor/selection motion, hotspot, clipping and no trails PASS");
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(gl);ReleaseDC(window,dc);DestroyWindow(window);
    if(argc>1&&!strcmp(argv[1],"--gl-only")) return 0;
    window=CreateWindowExA(0,"STATIC","Offline D3D9 test",WS_POPUP,0,0,3840,2160,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    IDirect3D9 *d3d=Direct3DCreate9(D3D_SDK_VERSION);Check(d3d!=nullptr,"D3D9 instance");
    D3DPRESENT_PARAMETERS pp={};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=3840;pp.BackBufferHeight=2160;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.hDeviceWindow=window;
    IDirect3DDevice9 *dev=nullptr;
    Check(SUCCEEDED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,
        D3DCREATE_MULTITHREADED|D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_PUREDEVICE|D3DCREATE_FPU_PRESERVE,
        &pp,&dev)),"cnc-ddraw-style pure hardware D3D9 device");
    IDirect3DTexture9 *world=nullptr,*ui=nullptr,*palette=nullptr,*native=nullptr;
    Check(SUCCEEDED(dev->CreateTexture(2048,2048,1,0,D3DFMT_L8,D3DPOOL_MANAGED,&world,nullptr)),"world texture");
    Check(SUCCEEDED(dev->CreateTexture(2048,2048,1,0,D3DFMT_A8L8,D3DPOOL_MANAGED,&ui,nullptr)),"UI texture");
    Check(SUCCEEDED(dev->CreateTexture(256,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&palette,nullptr)),"palette texture");
    Check(SUCCEEDED(dev->CreateTexture(1024,2048,1,0,D3DFMT_A8L8,D3DPOOL_MANAGED,&native,nullptr)),"native UI atlas");
    D3DLOCKED_RECT lock;
    world->LockRect(0,&lock,nullptr,0);for(unsigned y=0;y<720;++y)memcpy(static_cast<uint8_t *>(lock.pBits)+y*lock.Pitch,f.world.data()+y*1280,1280);world->UnlockRect(0);
    ui->LockRect(0,&lock,nullptr,0);for(unsigned y=0;y<1080;++y)memcpy(static_cast<uint8_t *>(lock.pBits)+y*lock.Pitch,f.ui.data()+y*1920*2,1920*2);ui->UnlockRect(0);
    palette->LockRect(0,&lock,nullptr,0);memcpy(lock.pBits,f.palette,sizeof(f.palette));palette->UnlockRect(0);
    native->LockRect(0,&lock,nullptr,0);for(unsigned y=0;y<1920;++y)memcpy(static_cast<uint8_t *>(lock.pBits)+y*lock.Pitch,f.native_ui.data()+y*640*2,640*2);native->UnlockRect(0);
    IDirect3DPixelShader9 *shader=nullptr;Check(SUCCEEDED(dev->CreatePixelShader(reinterpret_cast<const DWORD *>(single_stage_shader),&shader)),"shader");
    dev->SetTexture(1,palette);dev->SetTexture(2,world);dev->SetTexture(3,ui);dev->SetPixelShader(shader);
    dev->SetTexture(4,native);
    float rects[16];
    for(unsigned i=0;i<4;++i){const auto&r=f.native_rects[i];rects[4*i]=float(r.left);rects[4*i+1]=float(r.top);rects[4*i+2]=float(r.width);rects[4*i+3]=float(r.height);}
    dev->SetPixelShaderConstantF(3,rects,4);
    for(unsigned s=1;s<=4;++s){dev->SetSamplerState(s,D3DSAMP_MINFILTER,D3DTEXF_POINT);dev->SetSamplerState(s,D3DSAMP_MAGFILTER,D3DTEXF_POINT);}
    float c0[]={1920,1080,2048,2048},c1[]={1280,720,2048,2048},c2[]={2048,2048,0,0};
    dev->SetPixelShaderConstantF(0,c0,1);dev->SetPixelShaderConstantF(1,c1,1);dev->SetPixelShaderConstantF(2,c2,1);
    const float output_c7[]={0,3840,2160,0};dev->SetPixelShaderConstantF(7,output_c7,1);
    struct Vertex{float x,y,z,rhw,u,v;};
    Vertex vertices[]={{-.5f,-.5f,0,1,0,0},{3839.5f,-.5f,0,1,1920.f/2048,0},
        {-.5f,2159.5f,0,1,0,1080.f/2048},{3839.5f,2159.5f,0,1,1920.f/2048,1080.f/2048}};
    dev->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    dev->BeginScene();Check(SUCCEEDED(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex))),"D3D9 draw");dev->EndScene();
    IDirect3DSurface9 *target=nullptr,*copy=nullptr;dev->GetRenderTarget(0,&target);
    Check(SUCCEEDED(dev->CreateOffscreenPlainSurface(3840,2160,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&copy,nullptr)),"readback surface");
    Check(SUCCEEDED(dev->GetRenderTargetData(target,copy)),"D3D9 readback");copy->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    CheckPixels(f,static_cast<const uint32_t *>(lock.pBits),lock.Pitch/4,3840,2160,false);copy->UnlockRect();
    std::puts("D3D9 3840x2160 exact world/UI pixels PASS");
    IDirect3DTexture9 *wrapper_palette=nullptr;
    Check(SUCCEEDED(dev->CreateTexture(256,256,1,0,D3DFMT_X8R8G8B8,D3DPOOL_MANAGED,&wrapper_palette,nullptr)),"cnc-ddraw 256x256 palette storage");
    wrapper_palette->LockRect(0,&lock,nullptr,0);memcpy(lock.pBits,f.palette,sizeof(f.palette));wrapper_palette->UnlockRect(0);
    dev->SetTexture(0,world);dev->SetTexture(1,wrapper_palette);dev->SetTexture(2,nullptr);dev->SetTexture(3,nullptr);dev->SetTexture(4,nullptr);dev->SetPixelShader(nullptr);
    IDirect3DVertexBuffer9 *vb=nullptr;
    Check(SUCCEEDED(dev->CreateVertexBuffer(sizeof(vertices),0,D3DFVF_XYZRHW|D3DFVF_TEX1,D3DPOOL_MANAGED,&vb,nullptr)),"adapter vertex buffer");
    void *vertex_data=nullptr;vb->Lock(0,0,&vertex_data,0);memcpy(vertex_data,vertices,sizeof(vertices));vb->Unlock();
    dev->SetStreamSource(0,vb,0,sizeof(Vertex));
    using DrawFn=HRESULT (WINAPI *)(IDirect3DDevice9 *,D3DPRIMITIVETYPE,UINT,UINT);
    auto original_draw=reinterpret_cast<DrawFn>((*reinterpret_cast<void ***>(dev))[81]);
    // Real drivers/overlays can supply per-instance tables. The bootstrap
    // must discover this actual device instead of patching a dummy's table.
    void **driver_table=*reinterpret_cast<void ***>(dev);
    std::vector<void *> private_table(driver_table,driver_table+119);
    void **private_object=private_table.data();
    std::vector<uint8_t> wrapper_image(8192,0);
    auto fake=reinterpret_cast<HMODULE>(wrapper_image.data());
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(wrapper_image.data());
    dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32 *>(wrapper_image.data()+128);
    pe->Signature=IMAGE_NT_SIGNATURE;pe->FileHeader.NumberOfSections=2;
    pe->FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER32);
    pe->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;pe->OptionalHeader.SizeOfImage=8192;
    auto sections=IMAGE_FIRST_SECTION(pe);
    sections[0].VirtualAddress=1024;sections[0].Misc.VirtualSize=512;sections[0].Characteristics=IMAGE_SCN_MEM_EXECUTE;
    sections[1].VirtualAddress=4096;sections[1].Misc.VirtualSize=512;sections[1].Characteristics=IMAGE_SCN_MEM_WRITE;
    uint8_t sequence[]={0xa1,0,0,0,0,0x6a,2,0x6a,0,0x6a,5,0x8b,8,0x50,
        0x8b,0x81,0x44,1,0,0,0xff,0xd0,0xa1,0,0,0,0,0x50,0x8b,8,0x8b,0x81,0xa8,0,0,0,0xff,0xd0};
    void **slot=reinterpret_cast<void **>(wrapper_image.data()+4096);
    memcpy(sequence+1,&slot,4);memcpy(sequence+23,&slot,4);
    memcpy(wrapper_image.data()+1024,sequence,sizeof(sequence));
    Check(single_stage::FindWrapperDeviceSlot(fake)==slot,"wrapper global signature");
    wrapper_image[1024+6]=3;
    Check(!single_stage::FindWrapperDeviceSlot(fake),"unknown primitive signature rejected");
    wrapper_image[1024+6]=2;
    memcpy(wrapper_image.data()+1100,sequence,sizeof(sequence));
    Check(!single_stage::FindWrapperDeviceSlot(fake),"ambiguous signature rejected");
    memset(wrapper_image.data()+1100,0,sizeof(sequence));
    *slot=&private_object;
    Check(single_stage::TestDiscover(fake,f),"actual wrapper private-device discovery");
    Check(private_table[81]!=driver_table[81],"only actual private table receives hook");
    *slot=dev;
    Check(single_stage::TestDiscover(fake,f),"real GPU wrapper device discovery");
    std::puts("D3D9 wrapper device discovery with separate private table PASS");
    dev->BeginScene();Check(SUCCEEDED(dev->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2)),"real adapter draw");dev->EndScene();
    Check(SUCCEEDED(dev->GetRenderTargetData(target,copy)),"adapter readback");copy->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    CheckPixels(f,static_cast<const uint32_t *>(lock.pBits),lock.Pitch/4,3840,2160,false);copy->UnlockRect();
    IDirect3DPixelShader9 *after_shader=nullptr;dev->GetPixelShader(&after_shader);Check(!after_shader,"adapter shader state restored");
    IDirect3DBaseTexture9 *after_texture=nullptr;dev->GetTexture(2,&after_texture);Check(!after_texture,"adapter texture state restored");
    dev->GetTexture(4,&after_texture);Check(!after_texture,"native UI sampler state restored");
    std::puts("D3D9 production adapter pixel output and state restoration PASS");
    f.pointer=pointer_fixture;++f.serial;single_stage::TestAttach(dev,f);
    for(auto position:{std::pair{800,600},{1400,1100},{0,0}})
    {
        pointer_draw=single_stage::PointerGeometry(f.pointer,f.width,f.height,3840,2160,float(position.first),float(position.second),true,true);
        single_stage::test_pointer_override=&pointer_draw;
        dev->BeginScene();dev->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2);dev->EndScene();
        Check(SUCCEEDED(dev->GetRenderTargetData(target,copy)),"held-frame D3D9 pointer readback");copy->LockRect(&lock,nullptr,D3DLOCK_READONLY);
        CheckPixels(f,static_cast<const uint32_t *>(lock.pBits),lock.Pitch/4,3840,2160,false);copy->UnlockRect();
        Check(single_stage::TestUploadedSerial()==f.serial,"D3D9 pointer motion reuses uploaded serial");
    }
    single_stage::test_pointer_override=nullptr;f.pointer.enabled=false;
    std::puts("D3D9 held-frame cursor/selection motion, hotspot, clipping and no trails PASS");
    f.smooth_world_edges=true;
    for(auto crop:{std::pair{1280u,720u},{1536u,864u},{1919u,1079u}})
    {
        f.world_width=crop.first;f.world_height=crop.second;++f.serial;
        f.world.resize(size_t(f.world_width)*f.world_height);
        for(unsigned y=0;y<f.world_height;++y)for(unsigned x=0;x<f.world_width;++x)
            f.world[y*f.world_width+x]=(x+19*y)%256;
        Check(single_stage::TestAttach(dev,f),"filtered frame publication");
        dev->BeginScene();dev->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2);dev->EndScene();
        Check(SUCCEEDED(dev->GetRenderTargetData(target,copy)),"filtered adapter readback");copy->LockRect(&lock,nullptr,D3DLOCK_READONLY);
        CheckPixels(f,static_cast<const uint32_t *>(lock.pBits),lock.Pitch/4,3840,2160,false);copy->UnlockRect();
        std::printf("D3D9 edge filter %ux%u crop to 4K coverage and unchanged HUD PASS\n",crop.first,crop.second);
    }
    // Offset/aspect-fit quad, while the device viewport remains full-screen.
    Vertex inset[]={{12.5f,6.5f,0,1,0,0},{1377.5f,6.5f,0,1,1920.f/2048,0},
        {12.5f,773.5f,0,1,0,1080.f/2048},{1377.5f,773.5f,0,1,1920.f/2048,1080.f/2048}};
    vb->Lock(0,0,&vertex_data,0);memcpy(vertex_data,inset,sizeof(inset));vb->Unlock();
    dev->BeginScene();dev->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2);dev->EndScene();
    Check(SUCCEEDED(dev->GetRenderTargetData(target,copy)),"inset quad readback");copy->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    CheckPixels(f,reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(lock.pBits)+7*lock.Pitch)+13,
        lock.Pitch/4,1365,767,false);copy->UnlockRect();
    std::puts("D3D9 actual inset quad geometry, not full backbuffer size PASS");
    vb->Lock(0,0,&vertex_data,0);memcpy(vertex_data,vertices,sizeof(vertices));vb->Unlock();
    // Bounded offline timings, including normal hook/state/geometry overhead.
    // No timing probes are included in the production build.
    IDirect3DQuery9 *event=nullptr;
    Check(SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_EVENT,&event)),"benchmark GPU fence");
    LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
    for(bool filtered:{false,true})
    {
        f.smooth_world_edges=filtered;single_stage::TestAttach(dev,f);
        for(unsigned i=0;i<5;++i){dev->BeginScene();dev->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2);dev->EndScene();}
        event->Issue(D3DISSUE_END);while(event->GetData(nullptr,0,D3DGETDATA_FLUSH)==S_FALSE)Sleep(0);
        LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);
        for(unsigned i=0;i<40;++i){dev->BeginScene();dev->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2);dev->EndScene();}
        event->Issue(D3DISSUE_END);while(event->GetData(nullptr,0,D3DGETDATA_FLUSH)==S_FALSE)Sleep(0);
        QueryPerformanceCounter(&end);
        std::printf("4K final presentation %s: %.3f ms/draw (40 warmed draws, no upload)\n",filtered?"smooth edges":"nearest",
            1000.0*(end.QuadPart-begin.QuadPart)/frequency.QuadPart/40);
    }
    event->Release();
    f.valid=false;single_stage::TestAttach(dev,f);
    dev->BeginScene();original_draw(dev,D3DPT_TRIANGLESTRIP,0,2);dev->EndScene();
    Check(SUCCEEDED(dev->GetRenderTargetData(target,copy)),"fallback baseline readback");copy->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    for(unsigned y=0;y<2160;++y)memcpy(pixels.data()+y*3840,static_cast<uint8_t *>(lock.pBits)+y*lock.Pitch,3840*4);
    copy->UnlockRect();dev->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);
    dev->BeginScene();dev->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2);dev->EndScene();
    Check(SUCCEEDED(dev->GetRenderTargetData(target,copy)),"fallback adapter readback");copy->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    for(unsigned y=0;y<2160;++y)Check(!memcmp(pixels.data()+y*3840,static_cast<uint8_t *>(lock.pBits)+y*lock.Pitch,3840*4),"invalid frame unchanged fallback");
    copy->UnlockRect();std::puts("D3D9 invalid frame exact fallback PASS");
    vb->Release();copy->Release();target->Release();shader->Release();world->Release();ui->Release();palette->Release();wrapper_palette->Release();native->Release();
    Check(SUCCEEDED(dev->Reset(&pp)),"production device reset");
    std::puts("D3D9 production device reset PASS");dev->Release();d3d->Release();DestroyWindow(window);
    single_stage::TestResetProducer();
    std::vector<uint8_t> source(1920*1080,1);
    for(size_t i=0;i<source.size();++i) source[i]=static_cast<uint8_t>((i+i/1920)%251);
    single_stage::TestCaptureWorld(source.data(),1920,1080,1280,720);
    single_stage::TestCaptureWorld(source.data(),1920,1080,1280,720);
    size_t grows=0;
    for(unsigned width=1281;width<=1920;++width)
        grows+=single_stage::TestCaptureWorld(source.data(),1920,1080,width,width*1080/1920);
    std::printf("World-buffer capacity growths during warmed 150%% to 100%% zoom: %zu\n",grows);
    Check(grows==0,"zoom motion must not allocate a larger world buffer");
}
