#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d2d1_1.h>
#include <d2d1effects.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ui/d2d_renderer.h"
#include "core/win32_util.h"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_COLOR_NONE
#define DWMWA_COLOR_NONE 0xFFFFFFFEu
#endif

static inline void safe_release(IUnknown *p) {
    if (p) p->Release();
}

static inline D2D1_COLOR_F to_d2d_color(KsColor c) {
    D2D1_COLOR_F out;
    out.r = c.r;
    out.g = c.g;
    out.b = c.b;
    out.a = c.a;
    return out;
}

static const char *const kPresetFonts[] = {
    "JetBrains Mono",
    "Cascadia Code",
    "Consolas",
    "Segoe UI Variable",
    "Segoe UI",
    "Microsoft YaHei UI",
    "PingFang SC",
    "HarmonyOS Sans SC",
    "MiSans",
    "Noto Sans SC",
    "Source Han Sans SC",
    "Inter"
};

struct KsD2DRenderer {
    HWND hwnd;
    uint32_t width_px;
    uint32_t height_px;
    float dpi;

    ID3D11Device *d3d_device;
    ID3D11DeviceContext *d3d_ctx;
    IDXGIDevice *dxgi_device;
    IDXGISwapChain1 *swap_chain;
    IDCompositionDevice *dcomp_device;
    IDCompositionTarget *dcomp_target;
    IDCompositionVisual *dcomp_visual;

    ID2D1Factory1 *d2d_factory;
    ID2D1Device *d2d_device;
    ID2D1DeviceContext *d2d_ctx;
    ID2D1Bitmap1 *swap_bitmap;
    ID2D1Bitmap1 *scene_bitmap;
    ID2D1Bitmap1 *blur_src_bitmap;
    ID2D1Effect *blur_effect;
    ID2D1SolidColorBrush *solid_brush;

    IDWriteFactory *dwrite_factory;
    wchar_t font_family[128];
    uint8_t clip_stack[16];
    int clip_depth;
    bool in_frame;
    bool has_scene;
    bool glass_used_this_frame;
};

static bool ensure_scene_bitmap(KsD2DRenderer *r);
static bool ensure_blur_src_bitmap(KsD2DRenderer *r);
static void release_size_dependent_resources(KsD2DRenderer *r);

static bool create_device_resources(KsD2DRenderer *r) {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL chosen = D3D_FEATURE_LEVEL_11_0;
    /* Prefer WARP so we never load ~100MB+ discrete GPU user-mode drivers (nvgpucomp64/nvwgf2umx)
     * into a lightweight ~1.2 MB desktop keystroke utility. */
    HRESULT hr = D3D11CreateDevice(
        NULL, D3D_DRIVER_TYPE_WARP, NULL, flags,
        levels, (UINT)(sizeof(levels) / sizeof(levels[0])),
        D3D11_SDK_VERSION, &r->d3d_device, &chosen, &r->d3d_ctx);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
            levels, (UINT)(sizeof(levels) / sizeof(levels[0])),
            D3D11_SDK_VERSION, &r->d3d_device, &chosen, &r->d3d_ctx);
    }
    if (FAILED(hr)) return false;

    hr = r->d3d_device->QueryInterface(__uuidof(IDXGIDevice), (void **)&r->dxgi_device);
    if (FAILED(hr)) return false;

    IDXGIFactory2 *dxgi_factory = NULL;
    IDXGIAdapter *adapter = NULL;
    hr = r->dxgi_device->GetAdapter(&adapter);
    if (SUCCEEDED(hr)) {
        hr = adapter->GetParent(__uuidof(IDXGIFactory2), (void **)&dxgi_factory);
        adapter->Release();
    }
    if (FAILED(hr) || !dxgi_factory) return false;

    DXGI_SWAP_CHAIN_DESC1 scd;
    memset(&scd, 0, sizeof(scd));
    scd.Width = r->width_px > 0 ? r->width_px : 1;
    scd.Height = r->height_px > 0 ? r->height_px : 1;
    scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    scd.Scaling = DXGI_SCALING_STRETCH;

    hr = dxgi_factory->CreateSwapChainForComposition(r->dxgi_device, &scd, NULL, &r->swap_chain);
    dxgi_factory->Release();
    if (FAILED(hr)) return false;

    D2D1_FACTORY_OPTIONS fopts;
    memset(&fopts, 0, sizeof(fopts));
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                           __uuidof(ID2D1Factory1), &fopts, (void **)&r->d2d_factory);
    if (FAILED(hr)) return false;

    hr = r->d2d_factory->CreateDevice(r->dxgi_device, &r->d2d_device);
    if (FAILED(hr)) return false;

    hr = r->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &r->d2d_ctx);
    if (FAILED(hr)) return false;

    r->d2d_ctx->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    hr = r->d2d_ctx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &r->solid_brush);
    if (FAILED(hr)) return false;

    hr = r->d2d_ctx->CreateEffect(CLSID_D2D1GaussianBlur, &r->blur_effect);
    if (SUCCEEDED(hr) && r->blur_effect) {
        r->blur_effect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    }

    if (r->hwnd) {
        hr = DCompositionCreateDevice(r->dxgi_device, __uuidof(IDCompositionDevice), (void **)&r->dcomp_device);
        if (FAILED(hr)) return false;

        hr = r->dcomp_device->CreateTargetForHwnd(r->hwnd, TRUE, &r->dcomp_target);
        if (FAILED(hr)) return false;

        hr = r->dcomp_device->CreateVisual(&r->dcomp_visual);
        if (FAILED(hr)) return false;

        hr = r->dcomp_visual->SetContent(r->swap_chain);
        if (FAILED(hr)) return false;

        hr = r->dcomp_target->SetRoot(r->dcomp_visual);
        if (FAILED(hr)) return false;

        r->dcomp_device->Commit();
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory),
                             (IUnknown **)&r->dwrite_factory);
    if (FAILED(hr)) return false;

    return ensure_scene_bitmap(r);
}

static void release_size_dependent_resources(KsD2DRenderer *r) {
    if (r->d2d_ctx) r->d2d_ctx->SetTarget(NULL);
    safe_release(r->blur_src_bitmap);
    r->blur_src_bitmap = NULL;
    safe_release(r->scene_bitmap);
    r->scene_bitmap = NULL;
    safe_release(r->swap_bitmap);
    r->swap_bitmap = NULL;
}

static bool ensure_swap_bitmap(KsD2DRenderer *r) {
    if (!r || !r->swap_chain || !r->d2d_ctx) return false;
    if (r->swap_bitmap) return true;

    IDXGISurface *surface = NULL;
    HRESULT hr = r->swap_chain->GetBuffer(0, __uuidof(IDXGISurface), (void **)&surface);
    if (FAILED(hr)) return false;

    D2D1_BITMAP_PROPERTIES1 swap_props;
    memset(&swap_props, 0, sizeof(swap_props));
    swap_props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    swap_props.dpiX = r->dpi;
    swap_props.dpiY = r->dpi;
    swap_props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    hr = r->d2d_ctx->CreateBitmapFromDxgiSurface(surface, &swap_props, &r->swap_bitmap);
    surface->Release();
    r->d2d_ctx->SetDpi(r->dpi, r->dpi);
    return SUCCEEDED(hr);
}

static bool ensure_scene_bitmap(KsD2DRenderer *r) {
    if (!ensure_swap_bitmap(r)) return false;
    if (r->scene_bitmap) return true;

    D2D1_SIZE_U sz = D2D1::SizeU(r->width_px, r->height_px);
    D2D1_BITMAP_PROPERTIES1 scene_props;
    memset(&scene_props, 0, sizeof(scene_props));
    scene_props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scene_props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    scene_props.dpiX = r->dpi;
    scene_props.dpiY = r->dpi;
    scene_props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;

    return SUCCEEDED(r->d2d_ctx->CreateBitmap(sz, NULL, 0, &scene_props, &r->scene_bitmap));
}

static bool ensure_blur_src_bitmap(KsD2DRenderer *r) {
    if (!r || !r->d2d_ctx) return false;
    if (r->blur_src_bitmap) return true;

    D2D1_SIZE_U sz = D2D1::SizeU(r->width_px, r->height_px);
    D2D1_BITMAP_PROPERTIES1 src_props;
    memset(&src_props, 0, sizeof(src_props));
    src_props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    src_props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    src_props.dpiX = r->dpi;
    src_props.dpiY = r->dpi;
    src_props.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;

    return SUCCEEDED(r->d2d_ctx->CreateBitmap(sz, NULL, 0, &src_props, &r->blur_src_bitmap));
}

KsD2DRenderer *ks_d2d_create(HWND hwnd) {
    KsD2DRenderer *r = (KsD2DRenderer *)calloc(1, sizeof(KsD2DRenderer));
    if (!r) return NULL;
    r->hwnd = hwnd;
    r->dpi = 96.0f;
    r->width_px = 1120;
    r->height_px = 680;
    wcscpy(r->font_family, L"Segoe UI Variable");

    if (hwnd) {
        RECT rc;
        if (GetClientRect(hwnd, &rc)) {
            uint32_t w = (uint32_t)(rc.right - rc.left);
            uint32_t h = (uint32_t)(rc.bottom - rc.top);
            if (w > 0) r->width_px = w;
            if (h > 0) r->height_px = h;
        }
        UINT dpi = GetDpiForWindow(hwnd);
        if (dpi >= 72) r->dpi = (float)dpi;
    }

    if (!create_device_resources(r)) {
        ks_d2d_destroy(r);
        return NULL;
    }
    return r;
}

void ks_d2d_destroy(KsD2DRenderer *r) {
    if (!r) return;
    release_size_dependent_resources(r);
    safe_release(r->dwrite_factory);
    safe_release(r->blur_effect);
    safe_release(r->solid_brush);
    safe_release(r->d2d_ctx);
    safe_release(r->d2d_device);
    safe_release(r->d2d_factory);
    safe_release(r->dcomp_visual);
    safe_release(r->dcomp_target);
    safe_release(r->dcomp_device);
    safe_release(r->swap_chain);
    safe_release(r->dxgi_device);
    safe_release(r->d3d_ctx);
    safe_release(r->d3d_device);
    free(r);
}

void ks_d2d_trim_memory(KsD2DRenderer *r, bool release_scene_buffers) {
    if (r) {
        if (r->d2d_ctx) r->d2d_ctx->SetTarget(NULL);
        safe_release(r->blur_src_bitmap);
        r->blur_src_bitmap = NULL;
        if (r->blur_effect) {
            r->blur_effect->SetInput(0, NULL);
        }
        if (release_scene_buffers) {
            safe_release(r->scene_bitmap);
            r->scene_bitmap = NULL;
            r->has_scene = false;
        }
        if (r->d2d_device) {
            r->d2d_device->ClearResources(0);
        }
        if (r->dxgi_device) {
            IDXGIDevice3 *dxgi3 = NULL;
            if (SUCCEEDED(r->dxgi_device->QueryInterface(__uuidof(IDXGIDevice3), (void **)&dxgi3)) && dxgi3) {
                dxgi3->Trim();
                dxgi3->Release();
            }
        }
    }
    SetProcessWorkingSetSizeEx(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1, 0);
}

bool ks_d2d_resize(KsD2DRenderer *r, uint32_t width_px, uint32_t height_px, float dpi) {
    if (!r || !r->swap_chain) return false;
    if (width_px == 0) width_px = 1;
    if (height_px == 0) height_px = 1;
    if (dpi < 72.0f) dpi = 96.0f;

    if (r->width_px == width_px && r->height_px == height_px && fabsf(r->dpi - dpi) < 0.1f && r->swap_bitmap) {
        return true;
    }

    r->width_px = width_px;
    r->height_px = height_px;
    r->dpi = dpi;

    release_size_dependent_resources(r);
    HRESULT hr = r->swap_chain->ResizeBuffers(2, width_px, height_px, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
    if (FAILED(hr)) return false;

    r->has_scene = false;
    return ensure_scene_bitmap(r);
}

bool ks_d2d_begin_draw(KsD2DRenderer *r, KsColor clear_color) {
    if (!r || !r->d2d_ctx) return false;
    if (!ensure_scene_bitmap(r)) return false;
    r->glass_used_this_frame = false;
    r->d2d_ctx->SetTarget(r->scene_bitmap);
    r->d2d_ctx->BeginDraw();
    r->d2d_ctx->SetTransform(D2D1::Matrix3x2F::Identity());
    r->d2d_ctx->Clear(to_d2d_color(clear_color));
    r->in_frame = true;
    return true;
}

bool ks_d2d_end_draw(KsD2DRenderer *r) {
    if (!r || !r->in_frame || !r->d2d_ctx || !r->swap_bitmap || !r->scene_bitmap) return false;
    r->in_frame = false;

    HRESULT hr = r->d2d_ctx->EndDraw();
    if (FAILED(hr)) return false;
    r->has_scene = true;

    r->d2d_ctx->SetTarget(r->swap_bitmap);
    r->d2d_ctx->BeginDraw();
    r->d2d_ctx->SetTransform(D2D1::Matrix3x2F::Identity());
    r->d2d_ctx->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    r->d2d_ctx->DrawImage(r->scene_bitmap,
                          D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                          D2D1_COMPOSITE_MODE_SOURCE_COPY);
    hr = r->d2d_ctx->EndDraw();
    if (FAILED(hr)) return false;

    if (!r->glass_used_this_frame && r->blur_src_bitmap) {
        if (r->blur_effect) r->blur_effect->SetInput(0, NULL);
        safe_release(r->blur_src_bitmap);
        r->blur_src_bitmap = NULL;
    }

    if (r->swap_chain) {
        r->swap_chain->Present(0, 0);
    }
    if (r->dcomp_device) {
        r->dcomp_device->Commit();
    }
    return true;
}

typedef LONG (WINAPI *KsRtlGetVersionFn)(OSVERSIONINFOW *);

bool ks_d2d_is_mica_supported(void) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    FARPROC proc = GetProcAddress(ntdll, "RtlGetVersion");
    if (!proc) return false;
    KsRtlGetVersionFn rtl_ver = NULL;
    memcpy(&rtl_ver, &proc, sizeof(rtl_ver));
    OSVERSIONINFOW osv;
    memset(&osv, 0, sizeof(osv));
    osv.dwOSVersionInfoSize = sizeof(osv);
    if (rtl_ver(&osv) != 0) return false;
    return osv.dwMajorVersion >= 10 && osv.dwBuildNumber >= 22621;
}

bool ks_d2d_apply_window_backdrop(HWND hwnd, bool dark_mode, bool *out_mica_active) {
    if (out_mica_active) *out_mica_active = false;
    if (!hwnd) return false;

    /* Zero non-client glass extension so DWM never draws a bright/stale outer glass rim around DComp. */
    MARGINS margins = { 0, 0, 0, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    BOOL dark = dark_mode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    DWORD corner = 2; /* DWMWCP_ROUND */
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    DWORD border_none = DWMWA_COLOR_NONE;
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border_none, sizeof(border_none));

    if (ks_d2d_is_mica_supported()) {
        DWORD backdrop = 1; /* DWMSBT_NONE: crisp opaque composition surface with zero white specular rim */
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    }
    return false;
}

void ks_d2d_set_font_family(KsD2DRenderer *r, const char *font_family_utf8) {
    if (!r) return;
    if (!font_family_utf8 || font_family_utf8[0] == '\0') {
        wcscpy(r->font_family, L"Segoe UI Variable");
        return;
    }
    if (!ks_utf8_to_wide_buf(font_family_utf8, r->font_family,
                             (int)(sizeof(r->font_family) / sizeof(r->font_family[0])))) {
        wcscpy(r->font_family, L"Segoe UI Variable");
    }
}

static int cmp_str_ci(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return _stricmp(sa, sb);
}

void ks_d2d_enumerate_fonts(KsD2DRenderer *r, KsFontList *out_list) {
    memset(out_list, 0, sizeof(*out_list));
    if (!r || !r->dwrite_factory) return;

    IDWriteFontCollection *collection = NULL;
    if (FAILED(r->dwrite_factory->GetSystemFontCollection(&collection, FALSE)) || !collection) {
        return;
    }

    UINT32 total = collection->GetFontFamilyCount();
    int preset_n = (int)(sizeof(kPresetFonts) / sizeof(kPresetFonts[0]));
    int rest_cap = (int)total + 8;
    int all_cap = rest_cap + preset_n + 8;
    char **all = (char **)calloc((size_t)all_cap, sizeof(char *));
    char **rest = (char **)calloc((size_t)rest_cap, sizeof(char *));
    if (!all || !rest) {
        free(all);
        free(rest);
        collection->Release();
        return;
    }

    int out_n = 0;
    int found_n = 0;
    for (UINT32 i = 0; i < total && found_n < rest_cap; ++i) {
        IDWriteFontFamily *fam = NULL;
        if (FAILED(collection->GetFontFamily(i, &fam)) || !fam) continue;
        IDWriteLocalizedStrings *names = NULL;
        if (SUCCEEDED(fam->GetFamilyNames(&names)) && names) {
            UINT32 idx = 0;
            BOOL exists = FALSE;
            if (FAILED(names->FindLocaleName(L"zh-cn", &idx, &exists)) || !exists) {
                if (FAILED(names->FindLocaleName(L"en-us", &idx, &exists)) || !exists) {
                    idx = 0;
                    exists = names->GetCount() > 0;
                }
            }
            if (exists) {
                UINT32 len = 0;
                if (SUCCEEDED(names->GetStringLength(idx, &len)) && len > 0 && len < 256) {
                    wchar_t wbuf[260];
                    if (SUCCEEDED(names->GetString(idx, wbuf, 260))) {
                        char *u8 = ks_wide_to_utf8_alloc(wbuf);
                        if (u8 && u8[0] != '\0') {
                            rest[found_n++] = u8;
                        } else {
                            free(u8);
                        }
                    }
                }
            }
            names->Release();
        }
        fam->Release();
    }
    collection->Release();

    for (int p = 0; p < preset_n && out_n < all_cap; ++p) {
        all[out_n++] = _strdup(kPresetFonts[p]);
    }
    out_list->preset_count = out_n;

    if (found_n > 1) {
        qsort(rest, (size_t)found_n, sizeof(char *), cmp_str_ci);
    }

    for (int i = 0; i < found_n; ++i) {
        bool is_dup = (i > 0 && _stricmp(rest[i], rest[i - 1]) == 0);
        if (!is_dup) {
            for (int p = 0; p < preset_n; ++p) {
                if (_stricmp(rest[i], kPresetFonts[p]) == 0) {
                    is_dup = true;
                    break;
                }
            }
        }
        if (is_dup || out_n >= all_cap) {
            free(rest[i]);
        } else {
            all[out_n++] = rest[i];
        }
    }
    free(rest);

    out_list->names = all;
    out_list->count = out_n;
}

void ks_d2d_free_font_list(KsFontList *list) {
    if (!list) return;
    for (int i = 0; i < list->count; ++i) {
        free(list->names[i]);
    }
    free(list->names);
    memset(list, 0, sizeof(*list));
}

void ks_d2d_push_clip(KsD2DRenderer *r, float x, float y, float w, float h, float radius) {
    if (!r || !r->d2d_ctx) return;
    if (radius > 0.5f) {
        ID2D1RoundedRectangleGeometry *geom = NULL;
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), radius, radius);
        if (SUCCEEDED(r->d2d_factory->CreateRoundedRectangleGeometry(&rr, &geom)) && geom) {
            D2D1_LAYER_PARAMETERS1 lp = D2D1::LayerParameters1();
            lp.contentBounds = rr.rect;
            lp.geometricMask = geom;
            lp.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
            r->d2d_ctx->PushLayer(&lp, NULL);
            geom->Release();
            if (r->clip_depth < 16) r->clip_stack[r->clip_depth++] = 1;
            return;
        }
    }
    r->d2d_ctx->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    if (r->clip_depth < 16) r->clip_stack[r->clip_depth++] = 0;
}

void ks_d2d_pop_clip(KsD2DRenderer *r) {
    if (!r || !r->d2d_ctx || r->clip_depth <= 0) return;
    uint8_t kind = r->clip_stack[--r->clip_depth];
    if (kind == 1) r->d2d_ctx->PopLayer();
    else r->d2d_ctx->PopAxisAlignedClip();
}

void ks_d2d_fill_rect(KsD2DRenderer *r, float x, float y, float w, float h, KsColor color) {
    if (!r || !r->d2d_ctx || !r->solid_brush || color.a <= 0.001f) return;
    r->solid_brush->SetColor(to_d2d_color(color));
    r->d2d_ctx->FillRectangle(D2D1::RectF(x, y, x + w, y + h), r->solid_brush);
}

void ks_d2d_fill_rounded_rect(KsD2DRenderer *r, float x, float y, float w, float h, float radius, KsColor color) {
    if (!r || !r->d2d_ctx || !r->solid_brush || color.a <= 0.001f) return;
    r->solid_brush->SetColor(to_d2d_color(color));
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), radius, radius);
    r->d2d_ctx->FillRoundedRectangle(&rr, r->solid_brush);
}

void ks_d2d_stroke_rounded_rect(KsD2DRenderer *r, float x, float y, float w, float h, float radius,
                                float stroke_width, KsColor color) {
    if (!r || !r->d2d_ctx || !r->solid_brush || color.a <= 0.001f) return;
    float inset = stroke_width * 0.5f;
    r->solid_brush->SetColor(to_d2d_color(color));
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
        D2D1::RectF(x + inset, y + inset, x + w - inset, y + h - inset),
        radius, radius);
    r->d2d_ctx->DrawRoundedRectangle(&rr, r->solid_brush, stroke_width);
}

void ks_d2d_fill_rounded_gradient_v(KsD2DRenderer *r, float x, float y, float w, float h, float radius,
                                    KsColor top_color, KsColor bottom_color) {
    if (!r || !r->d2d_ctx) return;
    D2D1_GRADIENT_STOP stops[2];
    stops[0].position = 0.0f;
    stops[0].color = to_d2d_color(top_color);
    stops[1].position = 1.0f;
    stops[1].color = to_d2d_color(bottom_color);

    ID2D1GradientStopCollection *coll = NULL;
    if (FAILED(r->d2d_ctx->CreateGradientStopCollection(stops, 2, &coll)) || !coll) return;

    ID2D1LinearGradientBrush *grad = NULL;
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES lgp =
        D2D1::LinearGradientBrushProperties(D2D1::Point2F(x, y), D2D1::Point2F(x, y + h));
    if (SUCCEEDED(r->d2d_ctx->CreateLinearGradientBrush(&lgp, NULL, coll, &grad)) && grad) {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), radius, radius);
        r->d2d_ctx->FillRoundedRectangle(&rr, grad);
        grad->Release();
    }
    coll->Release();
}

void ks_d2d_fill_radial_glow(KsD2DRenderer *r, float cx, float cy, float rx, float ry, KsColor center_color) {
    if (!r || !r->d2d_ctx || center_color.a <= 0.001f || rx <= 1.0f || ry <= 1.0f) return;
    D2D1_GRADIENT_STOP stops[2];
    stops[0].position = 0.0f;
    stops[0].color = to_d2d_color(center_color);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(center_color.r, center_color.g, center_color.b, 0.0f);

    ID2D1GradientStopCollection *coll = NULL;
    if (FAILED(r->d2d_ctx->CreateGradientStopCollection(stops, 2, &coll)) || !coll) return;

    ID2D1RadialGradientBrush *rad = NULL;
    D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES rgp =
        D2D1::RadialGradientBrushProperties(D2D1::Point2F(cx, cy), D2D1::Point2F(0.0f, 0.0f), rx, ry);
    if (SUCCEEDED(r->d2d_ctx->CreateRadialGradientBrush(&rgp, NULL, coll, &rad)) && rad) {
        D2D1_ELLIPSE el = D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry);
        r->d2d_ctx->FillEllipse(&el, rad);
        rad->Release();
    }
    coll->Release();
}

void ks_d2d_fill_circle(KsD2DRenderer *r, float cx, float cy, float radius, KsColor color) {
    if (!r || !r->d2d_ctx || !r->solid_brush || color.a <= 0.001f) return;
    r->solid_brush->SetColor(to_d2d_color(color));
    D2D1_ELLIPSE el = D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius);
    r->d2d_ctx->FillEllipse(&el, r->solid_brush);
}

void ks_d2d_stroke_circle(KsD2DRenderer *r, float cx, float cy, float radius, float stroke_width, KsColor color) {
    if (!r || !r->d2d_ctx || !r->solid_brush || color.a <= 0.001f) return;
    r->solid_brush->SetColor(to_d2d_color(color));
    D2D1_ELLIPSE el = D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius);
    r->d2d_ctx->DrawEllipse(&el, r->solid_brush, stroke_width);
}

void ks_d2d_draw_line(KsD2DRenderer *r, float x1, float y1, float x2, float y2, float width, KsColor color) {
    if (!r || !r->d2d_ctx || !r->solid_brush || color.a <= 0.001f) return;
    r->solid_brush->SetColor(to_d2d_color(color));
    r->d2d_ctx->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), r->solid_brush, width);
}

void ks_d2d_draw_soft_shadow(KsD2DRenderer *r, float x, float y, float w, float h, float radius,
                              KsColor shadow_color, float spread, float offset_y) {
    if (!r || !r->d2d_ctx || shadow_color.a <= 0.001f) return;
    const int layers = 6;
    for (int i = layers - 1; i >= 0; --i) {
        float frac = (float)(i + 1) / (float)layers;
        float pad = spread * frac;
        float weight = (1.0f - frac);
        float alpha_scale = (weight * weight) * 0.22f;
        KsColor c = shadow_color;
        c.a = shadow_color.a * alpha_scale;
        ks_d2d_fill_rounded_rect(
            r,
            x - pad,
            y - pad * 0.4f + offset_y * frac,
            w + pad * 2.0f,
            h + pad * 1.4f,
            radius + pad * 0.6f,
            c);
    }
}

void ks_d2d_draw_glass_panel(KsD2DRenderer *r, float x, float y, float w, float h, float radius,
                             float blur_sigma, KsColor tint, KsColor border_color, KsColor shadow_color) {
    if (!r || !r->d2d_ctx) return;

    ks_d2d_draw_soft_shadow(r, x, y, w, h, radius, shadow_color, 18.0f, 8.0f);

    if (r->blur_effect && r->scene_bitmap && blur_sigma > 0.5f && ensure_blur_src_bitmap(r)) {
        r->glass_used_this_frame = true;
        float scale = r->dpi / 96.0f;
        float pad_dip = blur_sigma * 2.5f;
        int32_t px_x0 = (int32_t)floorf((x - pad_dip) * scale);
        int32_t px_y0 = (int32_t)floorf((y - pad_dip) * scale);
        int32_t px_x1 = (int32_t)ceilf((x + w + pad_dip) * scale);
        int32_t px_y1 = (int32_t)ceilf((y + h + pad_dip) * scale);
        if (px_x0 < 0) px_x0 = 0;
        if (px_y0 < 0) px_y0 = 0;
        if (px_x1 > (int32_t)r->width_px) px_x1 = (int32_t)r->width_px;
        if (px_y1 > (int32_t)r->height_px) px_y1 = (int32_t)r->height_px;

        if (px_x1 > px_x0 && px_y1 > px_y0) {
            r->d2d_ctx->Flush();
            D2D1_POINT_2U dst_pt = D2D1::Point2U((UINT32)px_x0, (UINT32)px_y0);
            D2D1_RECT_U src_rc = D2D1::RectU((UINT32)px_x0, (UINT32)px_y0, (UINT32)px_x1, (UINT32)px_y1);
            if (SUCCEEDED(r->blur_src_bitmap->CopyFromBitmap(&dst_pt, r->scene_bitmap, &src_rc))) {
                ID2D1RoundedRectangleGeometry *geom = NULL;
                D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), radius, radius);
                if (SUCCEEDED(r->d2d_factory->CreateRoundedRectangleGeometry(&rr, &geom)) && geom) {
                    D2D1_LAYER_PARAMETERS1 lp = D2D1::LayerParameters1();
                    lp.contentBounds = rr.rect;
                    lp.geometricMask = geom;
                    lp.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
                    r->d2d_ctx->PushLayer(&lp, NULL);

                    r->blur_effect->SetInput(0, r->blur_src_bitmap);
                    r->blur_effect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, blur_sigma);
                    D2D1_RECT_F region = D2D1::RectF(x, y, x + w, y + h);
                    r->d2d_ctx->DrawImage(r->blur_effect, D2D1::Point2F(x, y), region,
                                          D2D1_INTERPOLATION_MODE_LINEAR,
                                          D2D1_COMPOSITE_MODE_SOURCE_OVER);

                    r->d2d_ctx->PopLayer();
                    geom->Release();
                }
            }
        }
    }

    (void)border_color;
    ks_d2d_fill_rounded_rect(r, x, y, w, h, radius, tint);
}

void ks_d2d_draw_app_badge(KsD2DRenderer *r, float x, float y, float size, KsColor accent, KsColor icon_fg) {
    if (!r || !r->d2d_ctx) return;
    KsColor bg = accent;
    bg.a = 0.92f;
    ks_d2d_fill_rounded_rect(r, x, y, size, size, size * 0.28f, bg);

    /* Clean minimalist keycap glyph matching original App.svelte logo. */
    float inner = size * 0.44f;
    float ix = x + (size - inner) * 0.5f;
    float iy = y + (size - inner) * 0.5f;
    ks_d2d_fill_rounded_rect(r, ix, iy, inner, inner, 3.0f, icon_fg);
}

void ks_d2d_draw_chevron_down(KsD2DRenderer *r, float cx, float cy, float size, KsColor color, bool flipped) {
    float half = size * 0.35f;
    float dy = flipped ? -size * 0.16f : size * 0.16f;
    ks_d2d_draw_line(r, cx - half, cy - dy, cx, cy + dy, 1.5f, color);
    ks_d2d_draw_line(r, cx, cy + dy, cx + half, cy - dy, 1.5f, color);
}

void ks_d2d_draw_dots_vertical(KsD2DRenderer *r, float cx, float cy, float size, KsColor color) {
    float dot_r = 1.45f;
    float step = size * 0.28f;
    ks_d2d_fill_circle(r, cx, cy - step, dot_r, color);
    ks_d2d_fill_circle(r, cx, cy, dot_r, color);
    ks_d2d_fill_circle(r, cx, cy + step, dot_r, color);
}

void ks_d2d_draw_close_x(KsD2DRenderer *r, float cx, float cy, float size, KsColor color) {
    float h = size * 0.32f;
    ks_d2d_draw_line(r, cx - h, cy - h, cx + h, cy + h, 1.4f, color);
    ks_d2d_draw_line(r, cx + h, cy - h, cx - h, cy + h, 1.4f, color);
}

void ks_d2d_draw_checkmark(KsD2DRenderer *r, float cx, float cy, float size, KsColor color) {
    float w = size * 0.34f;
    ks_d2d_draw_line(r, cx - w, cy + w * 0.08f, cx - w * 0.2f, cy + w * 0.75f, 1.75f, color);
    ks_d2d_draw_line(r, cx - w * 0.2f, cy + w * 0.75f, cx + w, cy - w * 0.55f, 1.75f, color);
}

void ks_d2d_draw_minimize_dash(KsD2DRenderer *r, float cx, float cy, float size, KsColor color) {
    float h = size * 0.34f;
    ks_d2d_draw_line(r, cx - h, cy, cx + h, cy, 1.3f, color);
}

void ks_d2d_draw_maximize_box(KsD2DRenderer *r, float cx, float cy, float size, KsColor color, bool is_maximized) {
    float s = size * 0.58f;
    float x = cx - s * 0.5f;
    float y = cy - s * 0.5f;
    if (!is_maximized) {
        ks_d2d_stroke_rounded_rect(r, x, y, s, s, 1.5f, 1.2f, color);
    } else {
        float off = s * 0.24f;
        float sm = s - off;
        ks_d2d_stroke_rounded_rect(r, x + off, y, sm, sm, 1.0f, 1.1f, color);
        ks_d2d_stroke_rounded_rect(r, x, y + off, sm, sm, 1.0f, 1.1f, color);
    }
}

void ks_d2d_draw_menu_icon(KsD2DRenderer *r, int icon_kind, float cx, float cy, float size, KsColor color) {
    float h = size * 0.42f;
    switch (icon_kind) {
    case 0: /* Reset / trash or rotate-ccw glyph */
        ks_d2d_stroke_circle(r, cx, cy, h * 0.85f, 1.35f, color);
        ks_d2d_draw_line(r, cx - h * 0.85f, cy - h * 0.35f, cx - h * 0.85f, cy + h * 0.1f, 1.5f, color);
        ks_d2d_draw_line(r, cx - h * 0.85f, cy + h * 0.1f, cx - h * 0.35f, cy + h * 0.1f, 1.5f, color);
        break;
    case 1: /* Status / activity pulse line */
        ks_d2d_draw_line(r, cx - h, cy, cx - h * 0.4f, cy, 1.35f, color);
        ks_d2d_draw_line(r, cx - h * 0.4f, cy, cx - h * 0.15f, cy - h * 0.7f, 1.35f, color);
        ks_d2d_draw_line(r, cx - h * 0.15f, cy - h * 0.7f, cx + h * 0.2f, cy + h * 0.65f, 1.35f, color);
        ks_d2d_draw_line(r, cx + h * 0.2f, cy + h * 0.65f, cx + h * 0.45f, cy, 1.35f, color);
        ks_d2d_draw_line(r, cx + h * 0.45f, cy, cx + h, cy, 1.35f, color);
        break;
    case 2: /* Settings / slider controls */
        ks_d2d_draw_line(r, cx - h, cy - h * 0.45f, cx + h, cy - h * 0.45f, 1.25f, color);
        ks_d2d_fill_circle(r, cx - h * 0.25f, cy - h * 0.45f, 2.1f, color);
        ks_d2d_draw_line(r, cx - h, cy + h * 0.45f, cx + h, cy + h * 0.45f, 1.25f, color);
        ks_d2d_fill_circle(r, cx + h * 0.35f, cy + h * 0.45f, 2.1f, color);
        break;
    case 3: /* Minimize to tray */
        ks_d2d_draw_line(r, cx - h * 0.75f, cy + h * 0.5f, cx + h * 0.75f, cy + h * 0.5f, 1.4f, color);
        ks_d2d_draw_line(r, cx, cy - h * 0.75f, cx, cy + h * 0.2f, 1.35f, color);
        ks_d2d_draw_line(r, cx - h * 0.35f, cy - h * 0.15f, cx, cy + h * 0.22f, 1.35f, color);
        ks_d2d_draw_line(r, cx + h * 0.35f, cy - h * 0.15f, cx, cy + h * 0.22f, 1.35f, color);
        break;
    case 4: /* Quit / exit */
    default:
        ks_d2d_stroke_circle(r, cx, cy, h * 0.72f, 1.35f, color);
        ks_d2d_draw_line(r, cx, cy - h * 0.95f, cx, cy - h * 0.1f, 1.5f, color);
        break;
    }
}

static DWRITE_FONT_WEIGHT to_dwrite_weight(KsFontWeight w) {
    switch (w) {
    case KS_FONT_MEDIUM: return DWRITE_FONT_WEIGHT_MEDIUM;
    case KS_FONT_SEMIBOLD: return DWRITE_FONT_WEIGHT_SEMI_BOLD;
    case KS_FONT_BOLD: return DWRITE_FONT_WEIGHT_BOLD;
    default: return DWRITE_FONT_WEIGHT_NORMAL;
    }
}

static IDWriteTextFormat *create_text_format(KsD2DRenderer *r, const char *override_font_utf8,
                                             float font_size, KsFontWeight weight,
                                             KsTextAlign h_align, bool v_center, bool ellipsis) {
    if (!r || !r->dwrite_factory) return NULL;

    wchar_t fam_w[128];
    const wchar_t *fam = r->font_family;
    if (override_font_utf8 && override_font_utf8[0] != '\0') {
        if (ks_utf8_to_wide_buf(override_font_utf8, fam_w, 128)) {
            fam = fam_w;
        }
    }

    IDWriteTextFormat *fmt = NULL;
    HRESULT hr = r->dwrite_factory->CreateTextFormat(
        fam, NULL, to_dwrite_weight(weight),
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        font_size, L"", &fmt);
    if (FAILED(hr) || !fmt) return NULL;

    DWRITE_TEXT_ALIGNMENT ta = DWRITE_TEXT_ALIGNMENT_LEADING;
    if (h_align == KS_ALIGN_CENTER) ta = DWRITE_TEXT_ALIGNMENT_CENTER;
    else if (h_align == KS_ALIGN_RIGHT) ta = DWRITE_TEXT_ALIGNMENT_TRAILING;
    fmt->SetTextAlignment(ta);
    fmt->SetParagraphAlignment(v_center ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    if (ellipsis) {
        fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        IDWriteInlineObject *trim = NULL;
        if (SUCCEEDED(r->dwrite_factory->CreateEllipsisTrimmingSign(fmt, &trim)) && trim) {
            DWRITE_TRIMMING tr;
            memset(&tr, 0, sizeof(tr));
            tr.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
            fmt->SetTrimming(&tr, trim);
            trim->Release();
        }
    } else {
        fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }
    return fmt;
}

static void strip_emojis_inplace(wchar_t *s) {
    if (!s) return;
    size_t r = 0, w = 0;
    while (s[r] != L'\0') {
        wchar_t ch = s[r];
        /* Supplemental plane emojis (U+1F000..U+1FBFF) occupy high surrogates 0xD83C..0xD83E */
        if (ch >= 0xD83C && ch <= 0xD83E && s[r + 1] != L'\0') {
            r += 2;
            continue;
        }
        /* Misc symbols, dingbats, variation selectors, ZWJ, keycap combining */
        if ((ch >= 0x2600 && ch <= 0x27BF) || ch == 0xFE0E || ch == 0xFE0F ||
            ch == 0x200D || ch == 0x20E3) {
            r += 1;
            continue;
        }
        if (w == 0 && ch == L' ') {
            r += 1;
            continue;
        }
        s[w++] = ch;
        r += 1;
    }
    while (w > 0 && s[w - 1] == L' ') w--;
    s[w] = L'\0';
}

void ks_d2d_draw_text(KsD2DRenderer *r, const char *utf8_text, float x, float y, float w, float h,
                      float font_size, KsFontWeight weight, KsColor color,
                      KsTextAlign h_align, bool v_center, bool ellipsis, const char *override_font_utf8) {
    if (!r || !r->d2d_ctx || !r->solid_brush || !utf8_text || utf8_text[0] == '\0' || color.a <= 0.001f) {
        return;
    }

    wchar_t *wtext = ks_utf8_to_wide_alloc(utf8_text);
    if (!wtext) return;
    strip_emojis_inplace(wtext);

    IDWriteTextFormat *fmt = create_text_format(r, override_font_utf8, font_size, weight,
                                                h_align, v_center, ellipsis);
    if (fmt && wtext[0] != L'\0') {
        r->solid_brush->SetColor(to_d2d_color(color));
        UINT32 len = (UINT32)wcslen(wtext);
        D2D1_RECT_F rc = D2D1::RectF(x, y, x + w, y + h);
        r->d2d_ctx->DrawText(wtext, len, fmt, &rc, r->solid_brush,
                             D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    if (fmt) fmt->Release();
    free(wtext);
}

float ks_d2d_measure_text_width(KsD2DRenderer *r, const char *utf8_text, float font_size,
                                KsFontWeight weight, const char *override_font_utf8) {
    if (!r || !r->dwrite_factory || !utf8_text || utf8_text[0] == '\0') return 0.0f;
    wchar_t *wtext = ks_utf8_to_wide_alloc(utf8_text);
    if (!wtext) return 0.0f;
    strip_emojis_inplace(wtext);

    float width = 0.0f;
    IDWriteTextFormat *fmt = create_text_format(r, override_font_utf8, font_size, weight,
                                                KS_ALIGN_LEFT, false, false);
    if (fmt && wtext[0] != L'\0') {
        IDWriteTextLayout *layout = NULL;
        if (SUCCEEDED(r->dwrite_factory->CreateTextLayout(wtext, (UINT32)wcslen(wtext),
                                                          fmt, 4096.0f, 512.0f, &layout)) && layout) {
            DWRITE_TEXT_METRICS m;
            if (SUCCEEDED(layout->GetMetrics(&m))) {
                width = m.widthIncludingTrailingWhitespace;
            }
            layout->Release();
        }
    }
    if (fmt) fmt->Release();
    free(wtext);
    return width;
}

bool ks_d2d_save_snapshot_png_w(KsD2DRenderer *r, const wchar_t *path_w) {
    if (!r || !r->d2d_ctx || !r->scene_bitmap || !r->has_scene || !path_w) return false;

    D2D1_BITMAP_PROPERTIES1 cpu_props;
    memset(&cpu_props, 0, sizeof(cpu_props));
    cpu_props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    cpu_props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    cpu_props.dpiX = r->dpi;
    cpu_props.dpiY = r->dpi;
    cpu_props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    ID2D1Bitmap1 *cpu_bmp = NULL;
    D2D1_SIZE_U sz = D2D1::SizeU(r->width_px, r->height_px);
    if (FAILED(r->d2d_ctx->CreateBitmap(sz, NULL, 0, &cpu_props, &cpu_bmp)) || !cpu_bmp) {
        return false;
    }

    D2D1_POINT_2U dst_pt = D2D1::Point2U(0, 0);
    D2D1_RECT_U src_rc = D2D1::RectU(0, 0, r->width_px, r->height_px);
    bool ok = false;
    if (SUCCEEDED(cpu_bmp->CopyFromBitmap(&dst_pt, r->scene_bitmap, &src_rc))) {
        D2D1_MAPPED_RECT mapped;
        if (SUCCEEDED(cpu_bmp->Map(D2D1_MAP_OPTIONS_READ, &mapped))) {
            IWICImagingFactory *wic = NULL;
            if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                           __uuidof(IWICImagingFactory), (void **)&wic)) && wic) {
                IWICStream *stream = NULL;
                IWICBitmapEncoder *enc = NULL;
                IWICBitmapFrameEncode *frame = NULL;
                if (SUCCEEDED(wic->CreateStream(&stream)) &&
                    SUCCEEDED(stream->InitializeFromFilename(path_w, GENERIC_WRITE)) &&
                    SUCCEEDED(wic->CreateEncoder(GUID_ContainerFormatPng, NULL, &enc)) &&
                    SUCCEEDED(enc->Initialize(stream, WICBitmapEncoderNoCache)) &&
                    SUCCEEDED(enc->CreateNewFrame(&frame, NULL)) &&
                    SUCCEEDED(frame->Initialize(NULL)) &&
                    SUCCEEDED(frame->SetSize(r->width_px, r->height_px))) {
                    WICPixelFormatGUID pf = GUID_WICPixelFormat32bppPBGRA;
                    if (SUCCEEDED(frame->SetPixelFormat(&pf)) &&
                        SUCCEEDED(frame->WritePixels(r->height_px, mapped.pitch,
                                                     mapped.pitch * r->height_px, mapped.bits)) &&
                        SUCCEEDED(frame->Commit()) &&
                        SUCCEEDED(enc->Commit())) {
                        ok = true;
                    }
                }
                safe_release(frame);
                safe_release(enc);
                safe_release(stream);
                wic->Release();
            }
            cpu_bmp->Unmap();
        }
    }

    cpu_bmp->Release();
    return ok;
}
