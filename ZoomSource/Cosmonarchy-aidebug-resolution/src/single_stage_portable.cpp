#include "single_stage_frame.h"
#include "single_stage_pointer.h"
#include "console/windows_wrap.h"
#include <gl/GL.h>
#include <cstring>
#include <cstdio>
#include <string>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")

namespace single_stage
{
    namespace
    {
        using SwapFn = BOOL (WINAPI *)(HDC);
        using StretchFn = int (WINAPI *)(HDC,int,int,int,int,int,int,int,int,const void *,const BITMAPINFO *,UINT,DWORD);
        SwapFn original_swap;
        StretchFn original_stretch;
        void **Import(HMODULE module, const char *name)
        {
            auto base = reinterpret_cast<uint8_t *>(module);
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
            auto pe = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
            auto directory = pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (!directory.VirtualAddress) return nullptr;
            auto desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base + directory.VirtualAddress);
            for (; desc->Name; ++desc)
            {
                if (!desc->OriginalFirstThunk) continue;
                auto names = reinterpret_cast<IMAGE_THUNK_DATA *>(base + desc->OriginalFirstThunk);
                auto slots = reinterpret_cast<void **>(base + desc->FirstThunk);
                for (unsigned i = 0; names[i].u1.AddressOfData; ++i)
                    if (!IMAGE_SNAP_BY_ORDINAL(names[i].u1.Ordinal) && strcmp(name,
                        reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base + names[i].u1.AddressOfData)->Name) == 0)
                        return slots + i;
            }
            return nullptr;
        }
        bool Patch(void **slot, void *replacement)
        {
            DWORD old;
            if (!slot || !VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old)) return false;
            InterlockedExchangePointer(slot, replacement);
            DWORD ignored; VirtualProtect(slot, sizeof(void *), old, &ignored);
            return true;
        }
        void *Proc(const char *name)
        {
            PROC p = wglGetProcAddress(name);
            if (reinterpret_cast<uintptr_t>(p) <= 3 || p == reinterpret_cast<PROC>(-1))
                p = GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
            return reinterpret_cast<void *>(p);
        }
#define GLPROC(ret, name, ...) using name##Fn = ret (APIENTRY *)(__VA_ARGS__); name##Fn name = nullptr
        GLPROC(GLuint, CreateShader, GLenum);
        GLPROC(void, ShaderSource, GLuint, GLsizei, const char *const *, const GLint *);
        GLPROC(void, CompileShader, GLuint);
        GLPROC(void, GetShaderiv, GLuint, GLenum, GLint *);
        GLPROC(GLuint, CreateProgram, void);
        GLPROC(void, AttachShader, GLuint, GLuint);
        GLPROC(void, BindAttribLocation, GLuint, GLuint, const char *);
        GLPROC(void, LinkProgram, GLuint);
        GLPROC(void, GetProgramiv, GLuint, GLenum, GLint *);
        GLPROC(void, DeleteShader, GLuint);
        GLPROC(void, DeleteProgram, GLuint);
        GLPROC(void, UseProgram, GLuint);
        GLPROC(GLint, GetUniformLocation, GLuint, const char *);
        GLPROC(void, Uniform1i, GLint, GLint);
        GLPROC(void, Uniform2f, GLint, GLfloat, GLfloat);
        GLPROC(void, Uniform4fv, GLint, GLsizei, const GLfloat *);
        GLPROC(void, ActiveTexture, GLenum);
        GLPROC(void, GenBuffers, GLsizei, GLuint *);
        GLPROC(void, BindBuffer, GLenum, GLuint);
        GLPROC(void, BufferData, GLenum, ptrdiff_t, const void *, GLenum);
        GLPROC(void, VertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
        GLPROC(void, EnableVertexAttribArray, GLuint);
        GLPROC(void, DisableVertexAttribArray, GLuint);
        GLPROC(void, GetVertexAttribiv, GLuint, GLenum, GLint *);
        GLPROC(void, GetVertexAttribPointerv, GLuint, GLenum, void **);
        GLPROC(void, GenVertexArrays, GLsizei, GLuint *);
        GLPROC(void, BindVertexArray, GLuint);
#undef GLPROC
        HGLRC context;
        GLuint program, textures[4], buffer, vao;
        unsigned tw, th;
        bool ready, modern;
        bool geometry_uploaded;
        GLint uniform_world, uniform_ui, uniform_palette, uniform_logical, uniform_crop, uniform_storage;
        GLint uniform_native, uniform_native_rects, uniform_world_filter, uniform_output;
        GLint uniform_pointer_rect,uniform_pointer_shape,uniform_pointer_selection;
        uint64_t uploaded;
        GLuint Compile(GLenum type, const std::string &source)
        {
            GLuint shader = CreateShader(type);
            const char *text = source.c_str(); ShaderSource(shader, 1, &text, nullptr); CompileShader(shader);
            GLint success = 0; GetShaderiv(shader, 0x8B81, &success);
            if (!success) { DeleteShader(shader); return 0; }
            return shader;
        }
        bool Setup()
        {
            HGLRC current = wglGetCurrentContext();
            if (!current) return false;
            if (current == context) return ready;
            // GL owns resource lifetime with its context. Never delete names
            // belonging to another context or use a cached dispatch on it.
            context = current; program = buffer = vao = 0; tw = th = 0; uploaded = 0; ready = false;
            geometry_uploaded = false;
            memset(textures, 0, sizeof(textures));
#define LOAD(name) name = reinterpret_cast<name##Fn>(Proc("gl" #name)); if (!name) return false
            LOAD(CreateShader); LOAD(ShaderSource); LOAD(CompileShader); LOAD(GetShaderiv);
            LOAD(CreateProgram); LOAD(AttachShader); LOAD(BindAttribLocation); LOAD(LinkProgram);
            LOAD(GetProgramiv); LOAD(DeleteShader); LOAD(DeleteProgram); LOAD(UseProgram);
            LOAD(GetUniformLocation); LOAD(Uniform1i); LOAD(Uniform2f); LOAD(ActiveTexture);
            LOAD(Uniform4fv);
            LOAD(GenBuffers); LOAD(BindBuffer); LOAD(BufferData); LOAD(VertexAttribPointer);
            LOAD(EnableVertexAttribArray); LOAD(DisableVertexAttribArray);
            LOAD(GetVertexAttribiv); LOAD(GetVertexAttribPointerv);
#undef LOAD
            GenVertexArrays = reinterpret_cast<GenVertexArraysFn>(Proc("glGenVertexArrays"));
            BindVertexArray = reinterpret_cast<BindVertexArrayFn>(Proc("glBindVertexArray"));
            const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
            modern = version && version[0] >= '3';
            if (modern && (!GenVertexArrays || !BindVertexArray)) return false;
            std::string prefix = modern ? "#version 130\n" : "#version 120\n";
            const std::string vs = prefix + (modern ? "in vec2 pos; out vec2 uv;" : "attribute vec2 pos; varying vec2 uv;") +
                "void main(){gl_Position=vec4(pos,0,1);uv=vec2((pos.x+1.0)*0.5,(1.0-pos.y)*0.5);}";
            const std::string fs = prefix + (modern ? "in vec2 uv; out vec4 result;\n#define SAMPLE texture\n#define RESULT result\n" :
                "varying vec2 uv;\n#define SAMPLE texture2D\n#define RESULT gl_FragColor\n") +
                "uniform sampler2D world,ui,palette,nativeUi; uniform vec2 logical,crop,storage,outSize; uniform vec4 nativeRects[4];uniform int worldFilter;"
                "uniform vec4 pointerRect,pointerShape,pointerSelection;"
                "vec2 divideFloor(vec2 n,vec2 d){vec2 q=floor(n/d);return q+vec2(greaterThanEqual(n,(q+1.0)*d))-vec2(lessThan(n,q*d));}"
                "vec4 worldColor(vec2 cell){float i=SAMPLE(world,(cell+0.5)/storage).r;return SAMPLE(palette,vec2((i*255.0+0.5)/256.0,0.5));}"
                "void main(){vec2 p=floor(uv*outSize);"
                "if(pointerShape.x>0.0&&pointerShape.y>0.0){vec2 n=floor((p-pointerRect.xy+0.5)/max(pointerRect.zw,vec2(0.001)));"
                "if(all(greaterThanEqual(n,vec2(0)))&&all(lessThan(n,pointerShape.xy))){"
                "vec4 ink=SAMPLE(nativeUi,(n+vec2(0,1920)+0.5)/vec2(1024,2048));if("+
                (modern?std::string("ink.g"):std::string("ink.a"))+">0.5){RESULT=SAMPLE(palette,vec2((ink.r*255.0+0.5)/256.0,0.5));return;}}}"
                "if(pointerShape.w>0.5){vec2 lo=floor(pointerSelection.xy),hi=floor(pointerSelection.zw),thickness=max(vec2(1),floor(outSize/logical+0.5));"
                "if(all(greaterThanEqual(p,lo))&&all(lessThanEqual(p,hi))&&(any(lessThan(p-lo,thickness))||any(lessThan(hi-p,thickness)))){"
                "RESULT=SAMPLE(palette,vec2((pointerShape.z+0.5)/256.0,0.5));return;}}"
                "vec2 wp=min(divideFloor((2.0*p+1.0)*crop,2.0*outSize),crop-1.0);"
                "vec2 up=min(divideFloor((2.0*p+1.0)*logical,2.0*outSize),logical-1.0);"
                "vec4 u=SAMPLE(ui,(up+0.5)/storage);float i=u.r;bool covered=" + (modern ? std::string("u.g") : std::string("u.a")) + ">0.5;"
                "if(" + (modern ? std::string("u.g") : std::string("u.a")) + "<=0.5){for(int layer=0;layer<4;++layer){"
                "vec4 r=nativeRects[layer];vec2 n=divideFloor(((2.0*p+1.0)*logical-2.0*outSize*r.xy)*vec2(640,480),2.0*outSize*max(r.zw,vec2(1)));"
                "if(r.z>0.0&&r.w>0.0&&all(greaterThanEqual(n,vec2(0)))&&all(lessThan(n,vec2(640,480)))){"
                "vec4 ink=SAMPLE(nativeUi,(n+vec2(0,480*layer)+0.5)/vec2(1024,2048));if(" +
                (modern ? std::string("ink.g") : std::string("ink.a")) + ">0.5){i=ink.r;covered=true;}}}}"
                "if(covered){RESULT=SAMPLE(palette,vec2((i*255.0+0.5)/256.0,0.5));return;}"
                "bvec2 exact=equal(outSize,floor(outSize/crop+0.5)*crop);"
                "if(worldFilter!=0&&all(greaterThanEqual(outSize,crop))&&!all(exact)){"
                "vec2 lo=divideFloor(p*crop,outSize),end=(p+1.0)*crop,hi=min(divideFloor(end-1.0,outSize),crop-1.0);"
                "lo=vec2(exact.x?wp.x:lo.x,exact.y?wp.y:lo.y);hi=vec2(exact.x?wp.x:hi.x,exact.y?wp.y:hi.y);"
                "if(any(notEqual(lo,hi))){vec2 weight=clamp((end-hi*outSize)/crop,0.0,1.0);"
                "vec4 a=worldColor(lo),b=lo.x==hi.x?a:worldColor(vec2(hi.x,lo.y));vec4 top=mix(a,b,weight.x);"
                "if(lo.y==hi.y){RESULT=top;return;}vec4 c=worldColor(vec2(lo.x,hi.y)),d=lo.x==hi.x?c:worldColor(hi);"
                "RESULT=mix(top,mix(c,d,weight.x),weight.y);return;}}"
                "RESULT=worldColor(wp);}";
            GLuint vertex = Compile(0x8B31, vs), fragment = Compile(0x8B30, fs);
            if (!vertex || !fragment) { if (vertex) DeleteShader(vertex); if (fragment) DeleteShader(fragment); return false; }
            program = CreateProgram(); AttachShader(program, vertex); AttachShader(program, fragment);
            BindAttribLocation(program, 0, "pos"); LinkProgram(program);
            DeleteShader(vertex); DeleteShader(fragment);
            GLint linked = 0; GetProgramiv(program, 0x8B82, &linked);
            if (!linked) { DeleteProgram(program); program = 0; return false; }
            uniform_world = GetUniformLocation(program,"world");
            uniform_ui = GetUniformLocation(program,"ui");
            uniform_palette = GetUniformLocation(program,"palette");
            uniform_logical = GetUniformLocation(program,"logical");
            uniform_crop = GetUniformLocation(program,"crop");
            uniform_storage = GetUniformLocation(program,"storage");
            uniform_native = GetUniformLocation(program,"nativeUi");
            uniform_native_rects = GetUniformLocation(program,"nativeRects[0]");
            uniform_world_filter = GetUniformLocation(program,"worldFilter");
            uniform_output = GetUniformLocation(program,"outSize");
            uniform_pointer_rect=GetUniformLocation(program,"pointerRect");
            uniform_pointer_shape=GetUniformLocation(program,"pointerShape");
            uniform_pointer_selection=GetUniformLocation(program,"pointerSelection");
            glGenTextures(4, textures); GenBuffers(1, &buffer);
            if (GenVertexArrays && BindVertexArray) GenVertexArrays(1, &vao);
            ready = true; return true;
        }
        bool DrawGl(const Frame &f, void *)
        {
            if (!Setup()) return false;
            GLint old_program, active, binding[4], array, old_vao = 0, alignment;
            GLint row_length, skip_rows, skip_pixels, unpack_buffer = 0;
            GLint maximum_texture = 0;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum_texture);
            if (f.width > static_cast<unsigned>(maximum_texture) || f.height > static_cast<unsigned>(maximum_texture) ||
                AtlasHeight > static_cast<unsigned>(maximum_texture)) return false;
            glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
            glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);
            glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);
            if (modern) { glGetIntegerv(0x88EF, &unpack_buffer); BindBuffer(0x88EC, 0); }
            GLint enabled_attr = 0, attr_size = 4, attr_type = GL_FLOAT, attr_norm = 0, attr_stride = 0, attr_buffer = 0;
            void *attr_pointer = nullptr;
            glGetIntegerv(0x8B8D, &old_program); glGetIntegerv(0x84E0, &active);
            glGetIntegerv(0x8894, &array); glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
            if (vao) glGetIntegerv(0x85B5, &old_vao);
            else
            {
                GetVertexAttribiv(0, 0x8622, &enabled_attr); GetVertexAttribiv(0, 0x8623, &attr_size);
                GetVertexAttribiv(0, 0x8625, &attr_type); GetVertexAttribiv(0, 0x886A, &attr_norm);
                GetVertexAttribiv(0, 0x8624, &attr_stride); GetVertexAttribiv(0, 0x889F, &attr_buffer);
                GetVertexAttribPointerv(0, 0x8645, &attr_pointer);
            }
            const GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST), scissor = glIsEnabled(GL_SCISSOR_TEST), cull = glIsEnabled(GL_CULL_FACE);
            GLboolean color_mask[4]; glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
            for (unsigned i = 0; i < 4; ++i)
            { ActiveTexture(0x84C0 + i); glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding[i]); glBindTexture(GL_TEXTURE_2D, textures[i]); }
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); glPixelStorei(GL_UNPACK_SKIP_ROWS, 0); glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            unsigned w = 1, h = 1; while (w < f.width) w *= 2; while (h < f.height) h *= 2;
            if (tw != w || th != h)
            {
                tw = w; th = h; uploaded = 0;
                for (unsigned i = 0; i < 4; ++i)
                {
                    ActiveTexture(0x84C0 + i);
                    const GLint format = i == 2 ? GL_RGBA : (i == 1 || i == 3) ? (modern ? 0x8227 : GL_LUMINANCE_ALPHA) : (modern ? 0x1903 : GL_LUMINANCE);
                    glTexImage2D(GL_TEXTURE_2D, 0, format, i == 2 ? 256 : i == 3 ? AtlasWidth : tw,
                        i == 2 ? 1 : i == 3 ? AtlasHeight : th, 0, format, GL_UNSIGNED_BYTE, nullptr);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
                }
            }
            if (uploaded != f.serial)
            {
                ActiveTexture(0x84C0); glTexSubImage2D(GL_TEXTURE_2D,0,0,0,f.world_width,f.world_height,modern ? 0x1903 : GL_LUMINANCE,GL_UNSIGNED_BYTE,f.world.data());
                ActiveTexture(0x84C1); glTexSubImage2D(GL_TEXTURE_2D,0,0,0,f.width,f.height,modern ? 0x8227 : GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,f.ui.data());
                ActiveTexture(0x84C2); glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,1,0x80E1,GL_UNSIGNED_BYTE,f.palette);
                if (!f.native_ui.empty())
                {
                    ActiveTexture(0x84C3); glTexSubImage2D(GL_TEXTURE_2D,0,0,0,NativeWidth,NativeHeight*NativeLayers,
                        modern ? 0x8227 : GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,f.native_ui.data());
                }
                if(f.pointer.enabled&&f.pointer.pixels.size()==CursorSize*CursorSize*2)
                {
                    ActiveTexture(0x84C3);glTexSubImage2D(GL_TEXTURE_2D,0,0,CursorAtlasY,CursorSize,CursorSize,
                        modern?0x8227:GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,f.pointer.pixels.data());
                }
                uploaded = f.serial;
            }
            UseProgram(program);
            Uniform1i(uniform_world,0); Uniform1i(uniform_ui,1); Uniform1i(uniform_palette,2);
            Uniform1i(uniform_native,3);
            Uniform1i(uniform_world_filter,f.smooth_world_edges ? 1 : 0);
            GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
            Uniform2f(uniform_output,float(viewport[2]),float(viewport[3]));
            RECT client={};
            using RectFn=BOOL(WINAPI *)(HWND,LPRECT);
            static auto client_rect=reinterpret_cast<RectFn>(GetProcAddress(GetModuleHandleA("user32.dll"),"GetClientRect"));
            if(f.pointer.enabled&&client_rect)client_rect(reinterpret_cast<HWND>(f.pointer.window),&client);
            const auto pointer=PollPointer(f.pointer,f.width,f.height,float(viewport[0]),float(client.bottom-viewport[1]-viewport[3]),float(viewport[2]),float(viewport[3]));
            Uniform4fv(uniform_pointer_rect,1,pointer.cursor);Uniform4fv(uniform_pointer_shape,1,pointer.shape);
            Uniform4fv(uniform_pointer_selection,1,pointer.selection);
            float native_rects[NativeLayers * 4];
            for (unsigned i = 0; i < NativeLayers; ++i)
            {
                const auto &r = f.native_rects[i];
                native_rects[4*i] = float(r.left); native_rects[4*i+1] = float(r.top);
                native_rects[4*i+2] = float(r.width); native_rects[4*i+3] = float(r.height);
            }
            Uniform4fv(uniform_native_rects,NativeLayers,native_rects);
            Uniform2f(uniform_logical,float(f.width),float(f.height));
            Uniform2f(uniform_crop,float(f.world_width),float(f.world_height));
            Uniform2f(uniform_storage,float(tw),float(th));
            if (vao) BindVertexArray(vao);
            BindBuffer(0x8892, buffer);
            if (!geometry_uploaded)
            {
                const float vertices[] = {-1,-1,3,-1,-1,3};
                BufferData(0x8892, sizeof(vertices), vertices, 0x88E4);
                geometry_uploaded = true;
            }
            EnableVertexAttribArray(0); VertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr);
            glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST);
            glDisable(GL_CULL_FACE); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            const bool good = glGetError() == GL_NO_ERROR;
            if (!good) ready = false; // Preserve the wrapper frame after a failed upload/setup.
            if (good) glDrawArrays(GL_TRIANGLES,0,3);
            if (vao) BindVertexArray(old_vao);
            else
            {
                BindBuffer(0x8892, attr_buffer); VertexAttribPointer(0,attr_size,attr_type,static_cast<GLboolean>(attr_norm),attr_stride,attr_pointer);
                if (!enabled_attr) DisableVertexAttribArray(0);
            }
            BindBuffer(0x8892,array); UseProgram(old_program);
            if (blend) glEnable(GL_BLEND); if (depth) glEnable(GL_DEPTH_TEST); if (scissor) glEnable(GL_SCISSOR_TEST);
            if (cull) glEnable(GL_CULL_FACE); glColorMask(color_mask[0],color_mask[1],color_mask[2],color_mask[3]);
            glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);
            glPixelStorei(GL_UNPACK_ROW_LENGTH,row_length); glPixelStorei(GL_UNPACK_SKIP_ROWS,skip_rows); glPixelStorei(GL_UNPACK_SKIP_PIXELS,skip_pixels);
            if (modern) BindBuffer(0x88EC,unpack_buffer);
            for (unsigned i=0;i<4;++i) { ActiveTexture(0x84C0+i); glBindTexture(GL_TEXTURE_2D,binding[i]); }
            ActiveTexture(active);
            return good;
        }
        BOOL WINAPI Swap(HDC dc)
        { WithFrame(&DrawGl, nullptr); return original_swap(dc); }
        struct GdiCall { HDC dc; int x,y,w,h,sw,sh; int result; };
        bool DrawGdi(const Frame &f, void *context)
        {
            if(f.pointer.enabled)return false; // Complete stock frame retains the ordinary pointer in software fallback.
            auto &call = *static_cast<GdiCall *>(context);
            if (call.sw != static_cast<int>(f.width) || call.sh != static_cast<int>(f.height)) return false;
            if (call.w <= 0 || call.h <= 0 || uint64_t(call.w) * call.h > 3840u*2160u) return false;
            static std::vector<uint32_t> pixels;
            static std::vector<unsigned> world_x, ui_x;
            pixels.resize(static_cast<size_t>(call.w)*call.h);
            world_x.resize(call.w); ui_x.resize(call.w);
            for (int x=0;x<call.w;++x)
            { world_x[x]=Sample(x,f.world_width,call.w); ui_x[x]=Sample(x,f.width,call.w)*2; }
            for (int y=0;y<call.h;++y)
            {
                const auto world_row=f.world.data()+size_t(Sample(y,f.world_height,call.h))*f.world_width;
                const auto ui_row=f.ui.data()+size_t(Sample(y,f.height,call.h))*f.width*2;
                auto output=pixels.data()+size_t(y)*call.w;
                for(int x=0;x<call.w;++x)
                    output[x]=f.palette[ui_row[ui_x[x]+1] ? ui_row[ui_x[x]] : world_row[world_x[x]]];
            }
            // Precompute horizontal native samples once per layer. Do not run
            // four 64-bit coordinate divisions for every output pixel in GDI.
            static std::vector<int> native_x;
            native_x.resize(call.w);
            if (!f.native_ui.empty()) for (unsigned layer = 0; layer < NativeLayers; ++layer)
            {
                const auto &r = f.native_rects[layer];
                if (r.width <= 0 || r.height <= 0) continue;
                for (int x = 0; x < call.w; ++x)
                    native_x[x] = NativeSample(x,f.width,call.w,r.left,r.width,NativeWidth);
                for (int y = 0; y < call.h; ++y)
                {
                    const int ny = NativeSample(y,f.height,call.h,r.top,r.height,NativeHeight);
                    if (ny < 0) continue;
                    const auto ink = f.native_ui.data() + (layer * NativeHeight + ny) * NativeWidth * 2;
                    const auto ui_row = f.ui.data()+size_t(Sample(y,f.height,call.h))*f.width*2;
                    auto output = pixels.data()+size_t(y)*call.w;
                    for (int x = 0; x < call.w; ++x)
                        if (native_x[x] >= 0 && !ui_row[ui_x[x]+1] && ink[native_x[x]*2+1])
                            output[x] = f.palette[ink[native_x[x]*2]];
                }
            }
            BITMAPINFO bmi={}; bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth=call.w; bmi.bmiHeader.biHeight=-call.h;
            bmi.bmiHeader.biPlanes=1; bmi.bmiHeader.biBitCount=32;
            call.result=original_stretch(call.dc,call.x,call.y,call.w,call.h,0,0,call.w,call.h,pixels.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
            return call.result != 0 && call.result != GDI_ERROR;
        }
        int WINAPI Stretch(HDC dc,int x,int y,int w,int h,int sx,int sy,int sw,int sh,const void *bits,const BITMAPINFO *info,UINT usage,DWORD rop)
        {
            GdiCall call={dc,x,y,w,h,sw,sh,0};
            if (info && info->bmiHeader.biBitCount==8 && sx==0 && sy==0 && rop==SRCCOPY && WithFrame(&DrawGdi,&call)) return call.result;
            return original_stretch(dc,x,y,w,h,sx,sy,sw,sh,bits,info,usage,rop);
        }
    }
    bool InstallPortableAdapters(void *module)
    {
        bool installed=false;
        auto wrapper=static_cast<HMODULE>(module);
        if (auto slot=Import(wrapper,"SwapBuffers"))
        { original_swap=reinterpret_cast<SwapFn>(*slot); installed |= Patch(slot,reinterpret_cast<void *>(&Swap)); }
        if (auto slot=Import(wrapper,"StretchDIBits"))
        { original_stretch=reinterpret_cast<StretchFn>(*slot); installed |= Patch(slot,reinterpret_cast<void *>(&Stretch)); }
        return installed;
    }
}
