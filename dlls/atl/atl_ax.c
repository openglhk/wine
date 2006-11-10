/*
 * Active Template Library ActiveX functions (atl.dll)
 *
 * Copyright 2006 Andrey Turkin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <stdio.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winerror.h"
#include "winuser.h"
#include "wine/debug.h"
#include "objbase.h"
#include "objidl.h"
#include "ole2.h"
#include "exdisp.h"
#include "atlbase.h"
#include "atliface.h"
#include "atlwin.h"

#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(atl);

typedef struct IOCS {
    const IOleClientSiteVtbl *lpOleClientSiteVtbl;

    LONG ref;
    HWND hWnd;
    IOleObject *control;
    WNDPROC OrigWndProc;
} IOCS;

/**********************************************************************
 * AtlAxWin class window procedure
 */
static LRESULT CALLBACK AtlAxWin_wndproc( HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam )
{
    if ( wMsg == WM_CREATE )
    {
            DWORD len = GetWindowTextLengthW( hWnd ) + 1;
            WCHAR *ptr = HeapAlloc( GetProcessHeap(), 0, len*sizeof(WCHAR) );
            if (!ptr)
                return 1;
            GetWindowTextW( hWnd, ptr, len );
            AtlAxCreateControlEx( ptr, hWnd, NULL, NULL, NULL, NULL, NULL );
            HeapFree( GetProcessHeap(), 0, ptr );
            return 0;
    }
    return DefWindowProcW( hWnd, wMsg, wParam, lParam );
}

/***********************************************************************
 *           AtlAxWinInit          [ATL.@]
 * Initializes the control-hosting code: registering the AtlAxWin, 
 * AtlAxWin7 and AtlAxWinLic7 window classes and some messages.
 *
 * RETURNS
 *  TRUE or FALSE
 */
 
BOOL WINAPI AtlAxWinInit(void)
{
    WNDCLASSEXW wcex;
    const WCHAR AtlAxWin[] = {'A','t','l','A','x','W','i','n',0};

    FIXME("semi-stub\n");

    if ( FAILED( OleInitialize(NULL) ) )
        return FALSE;

    wcex.cbSize        = sizeof(wcex);
    wcex.style         = 0;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = GetModuleHandleW( NULL );
    wcex.hIcon         = NULL;
    wcex.hCursor       = NULL;
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName  = NULL;
    wcex.hIconSm       = 0;

    wcex.lpfnWndProc   = AtlAxWin_wndproc;
    wcex.lpszClassName = AtlAxWin;
    if ( !RegisterClassExW( &wcex ) )
        return FALSE;

    return TRUE;
}

/***********************************************************************
 *  Atl container component implementation
 */


static ULONG WINAPI IOCS_AddRef(IOCS *This)
{
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE( "(%p) : AddRef from %d\n", This, ref - 1 );

    return ref;
}

#define THIS2IOLECLIENTSITE(This) ((IOleClientSite*)&This->lpOleClientSiteVtbl)

static HRESULT WINAPI IOCS_QueryInterface(IOCS *This, REFIID riid, void **ppv)
{
    *ppv = NULL;

    if ( IsEqualIID( &IID_IUnknown, riid )
      || IsEqualIID( &IID_IOleClientSite, riid ) )
    {
        *ppv = THIS2IOLECLIENTSITE(This);
    }

    if (*ppv)
    {
        IOCS_AddRef( This );
        return S_OK;
    }
    
    WARN("unsupported interface %s\n", debugstr_guid( riid ) );
    *ppv = NULL;
    return E_NOINTERFACE;
}

static HRESULT IOCS_Detach( IOCS *This );
static ULONG WINAPI IOCS_Release(IOCS *This)
{
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE( "(%p) : ReleaseRef to %d\n", This, ref );

    if (!ref)
    {
        IOCS_Detach( This );
        HeapFree( GetProcessHeap(), 0, This );
    }

    return ref;
}

#define DEFINE_THIS(cls,ifc,iface) ((cls*)((BYTE*)(iface)-offsetof(cls,lp ## ifc ## Vtbl)))

#undef  IFACE2THIS
#define IFACE2THIS(iface) DEFINE_THIS(IOCS,OleClientSite, iface)
static HRESULT WINAPI OleClientSite_QueryInterface(IOleClientSite *iface, REFIID riid, void **ppv)
{
    IOCS *This = IFACE2THIS(iface);
    return IOCS_QueryInterface(This, riid, ppv);
}
static ULONG WINAPI OleClientSite_AddRef(IOleClientSite *iface)
{
    IOCS *This = IFACE2THIS(iface);
    return IOCS_AddRef(This);
}
static ULONG WINAPI OleClientSite_Release(IOleClientSite *iface)
{
    IOCS *This = IFACE2THIS(iface);
    return IOCS_Release(This);
}
static HRESULT WINAPI OleClientSite_SaveObject(IOleClientSite *iface)
{
    IOCS *This = IFACE2THIS(iface);
    FIXME( "(%p) - stub\n", This );
    return E_NOTIMPL;
}
static HRESULT WINAPI OleClientSite_GetMoniker(IOleClientSite *iface, DWORD dwAssign, DWORD dwWhichMoniker, IMoniker **ppmk)
{
    IOCS *This = IFACE2THIS(iface);

    FIXME( "(%p, 0x%x, 0x%x, %p)\n", This, dwAssign, dwWhichMoniker, ppmk );
    return E_NOTIMPL;
}
static HRESULT WINAPI OleClientSite_GetContainer(IOleClientSite *iface, IOleContainer **ppContainer)
{
    IOCS *This = IFACE2THIS(iface);
    FIXME( "(%p, %p)\n", This, ppContainer );
    return E_NOTIMPL;
}
static HRESULT WINAPI OleClientSite_ShowObject(IOleClientSite *iface)
{
    IOCS *This = IFACE2THIS(iface);
    FIXME( "(%p) - stub\n", This );
    return S_OK;
}
static HRESULT WINAPI OleClientSite_OnShowWindow(IOleClientSite *iface, BOOL fShow)
{
    IOCS *This = IFACE2THIS(iface);
    FIXME( "(%p, %s) - stub\n", This, fShow ? "TRUE" : "FALSE" );
    return E_NOTIMPL;
}
static HRESULT WINAPI OleClientSite_RequestNewObjectLayout(IOleClientSite *iface)
{
    IOCS *This = IFACE2THIS(iface);
    FIXME( "(%p) - stub\n", This );
    return E_NOTIMPL;
}
#undef IFACE2THIS



static const IOleClientSiteVtbl OleClientSite_vtbl = {
    OleClientSite_QueryInterface,
    OleClientSite_AddRef,
    OleClientSite_Release,
    OleClientSite_SaveObject,
    OleClientSite_GetMoniker,
    OleClientSite_GetContainer,
    OleClientSite_ShowObject,
    OleClientSite_OnShowWindow,
    OleClientSite_RequestNewObjectLayout
};

static HRESULT IOCS_Detach( IOCS *This ) /* remove subclassing */
{
    if ( This->hWnd )
    {
        SetWindowLongPtrW( This->hWnd, GWLP_WNDPROC, (ULONG_PTR) This->OrigWndProc );
        SetWindowLongPtrW( This->hWnd, GWLP_USERDATA, (LONG_PTR) NULL );
        This->hWnd = NULL;
    }
    if ( This->control )
    {
        IOleObject *control = This->control;
        
        This->control = NULL;
        IOleObject_SetClientSite( control, NULL );
        IOleObject_Release( control );
    }
    return S_OK;
}

static void IOCS_OnSize( IOCS* This, LPCRECT rect )
{
    SIZEL inPix, inHi;

    if ( !This->control )
        return;

    inPix.cx = rect->right - rect->left;
    inPix.cy = rect->bottom - rect->top;
    AtlPixelToHiMetric( &inPix, &inHi );
    IOleObject_SetExtent( This->control, DVASPECT_CONTENT, &inHi );
}

static LRESULT IOCS_OnWndProc( IOCS *This, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    WNDPROC OrigWndProc = This->OrigWndProc;

    switch( uMsg )
    {
        case WM_DESTROY:
            IOCS_Detach( This );
            break;
        case WM_SIZE:
            {
                RECT r;
                r.left = r.top = 0;
                r.right = LOWORD( lParam );
                r.bottom = HIWORD( lParam );
                IOCS_OnSize( This, &r );
            }
            break;
    }

    return OrigWndProc( hWnd, uMsg, wParam, lParam );
}

static LRESULT CALLBACK AtlHost_wndproc( HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam )
{
    IOCS *This = (IOCS*) GetWindowLongPtrW( hWnd, GWLP_USERDATA );
    return IOCS_OnWndProc( This, hWnd, wMsg, wParam, lParam );
}

static HRESULT IOCS_Attach( IOCS *This, HWND hWnd, IUnknown *pUnkControl ) /* subclass hWnd */
{
    This->hWnd = hWnd;
    IUnknown_QueryInterface( pUnkControl, &IID_IOleObject, (void**)&This->control );
    IOleObject_SetClientSite( This->control, THIS2IOLECLIENTSITE( This ) );
    This->OrigWndProc = (WNDPROC) GetWindowLongPtrW( This->hWnd, GWLP_WNDPROC );
    SetWindowLongPtrW( hWnd, GWLP_USERDATA, (ULONG_PTR) This );
    SetWindowLongPtrW( hWnd, GWLP_WNDPROC, (ULONG_PTR) AtlHost_wndproc );

    return S_OK;
}

static HRESULT IOCS_Init( IOCS *This )
{
    RECT rect;
    static const WCHAR AXWIN[] = {'A','X','W','I','N',0};

    IOleObject_SetHostNames( This->control, AXWIN, AXWIN );

    GetClientRect( This->hWnd, &rect );
    IOCS_OnSize( This, &rect );

    return S_OK;
}

/**********************************************************************
 * Create new instance of Atl host component and attach it to window  *
 */
static HRESULT IOCS_Create( HWND hWnd, IUnknown *pUnkControl, IOCS **ppSite )
{
    HRESULT hr;
    IOCS *This;
    
    *ppSite = NULL;
    This = HeapAlloc(GetProcessHeap(), 0, sizeof(IOCS));

    if (!This)
        return E_OUTOFMEMORY;

    This->lpOleClientSiteVtbl = &OleClientSite_vtbl;
    This->ref = 1;

    This->OrigWndProc = NULL;
    This->hWnd = NULL;

    hr = IOCS_Attach( This, hWnd, pUnkControl );
    if ( SUCCEEDED( hr ) )
        hr = IOCS_Init( This );
    if ( SUCCEEDED( hr ) )
        *ppSite = This;
    else
        IOCS_Release( This );

    return hr;
}


/***********************************************************************
 *           AtlAxCreateControl           [ATL.@]
 */
HRESULT WINAPI AtlAxCreateControl(LPCOLESTR lpszName, HWND hWnd,
        IStream *pStream, IUnknown **ppUnkContainer)
{
    return AtlAxCreateControlEx( lpszName, hWnd, pStream, ppUnkContainer,
            NULL, NULL, NULL );
}

/***********************************************************************
 *           AtlAxCreateControlEx            [ATL.@]
 *
 * REMARKS
 *   See http://www.codeproject.com/com/cwebpage.asp for some background
 *
 */
HRESULT WINAPI AtlAxCreateControlEx(LPCOLESTR lpszName, HWND hWnd,
        IStream *pStream, IUnknown **ppUnkContainer, IUnknown **ppUnkControl,
        REFIID iidSink, IUnknown *punkSink)
{
    CLSID controlId;
    HRESULT hRes;
    IOleObject *pControl;
    IUnknown *pUnkControl;
    IPersistStreamInit *pPSInit;
    IUnknown *pContainer;
    enum {IsGUID=0,IsHTML=1,IsURL=2} content;

    TRACE("(%s %p %p %p %p %p %p)\n", debugstr_w(lpszName), hWnd, pStream, 
            ppUnkContainer, ppUnkControl, iidSink, punkSink);

    hRes = CLSIDFromString( (LPOLESTR) lpszName, &controlId );
    if ( FAILED(hRes) )
        hRes = CLSIDFromProgID( lpszName, &controlId );
    if ( SUCCEEDED( hRes ) )
        content = IsGUID;
    else {
        /* FIXME - check for MSHTML: prefix! */
        content = IsURL;
        memcpy( &controlId, &CLSID_WebBrowser, sizeof(controlId) );
    }

    hRes = CoCreateInstance( &controlId, 0, CLSCTX_ALL, &IID_IOleObject, 
            (void**) &pControl );
    if ( FAILED( hRes ) )
    {
        WARN( "cannot create ActiveX control %s instance - error 0x%08x\n",
                debugstr_guid( &controlId ), hRes );
        return hRes;
    }

    hRes = IOleObject_QueryInterface( pControl, &IID_IPersistStreamInit, (void**) &pPSInit );
    if ( SUCCEEDED( hRes ) )
    {
        if (!pStream)
            IPersistStreamInit_InitNew( pPSInit );
        else
            IPersistStreamInit_Load( pPSInit, pStream );
        IPersistStreamInit_Release( pPSInit );
    } else
        WARN("cannot get IID_IPersistStreamInit out of control\n");

    IOleObject_QueryInterface( pControl, &IID_IUnknown, (void**) &pUnkControl );
    IOleObject_Release( pControl );
     

    hRes = AtlAxAttachControl( pUnkControl, hWnd, &pContainer );
    if ( FAILED( hRes ) )
        WARN("cannot attach control to window\n");

    if ( content == IsURL )
    {
        IWebBrowser2 *browser;

        hRes = IOleObject_QueryInterface( pControl, &IID_IWebBrowser2, (void**) &browser );
        if ( !browser )
            WARN( "Cannot query IWebBrowser2 interface: %08x\n", hRes );
        else {
            VARIANT url;
            
            IWebBrowser2_put_Visible( browser, VARIANT_TRUE ); /* it seems that native does this on URL (but do not on MSHTML:! why? */

            V_VT(&url) = VT_BSTR;
            V_BSTR(&url) = SysAllocString( lpszName );

            hRes = IWebBrowser2_Navigate2( browser, &url, NULL, NULL, NULL, NULL );
            if ( FAILED( hRes ) )
                WARN( "IWebBrowser2::Navigate2 failed: %08x\n", hRes );
            SysFreeString( V_BSTR(&url) );

            IWebBrowser2_Release( browser );
        }
    }

    if (ppUnkContainer)
    {
        *ppUnkContainer = pContainer;
        if ( pContainer )
            IUnknown_AddRef( pContainer );
    }
    if (ppUnkControl)
    {
        *ppUnkControl = pUnkControl;
        if ( pUnkControl )
            IUnknown_AddRef( pUnkControl );
    }

    IUnknown_Release( pUnkControl );
    if ( pContainer )
        IUnknown_Release( pContainer );

    return S_OK;
}

/***********************************************************************
 *           AtlAxAttachControl           [ATL.@]
 */
HRESULT WINAPI AtlAxAttachControl(IUnknown* pControl, HWND hWnd, IUnknown** ppUnkContainer)
{
    FIXME( "(%p %p %p) - stub\n", pControl, hWnd, ppUnkContainer );
    return E_NOTIMPL;
}

/**********************************************************************
 * Helper function for AX_ConvertDialogTemplate
 */
static inline BOOL advance_array(WORD **pptr, DWORD *palloc, DWORD *pfilled, const WORD *data, DWORD size)
{
    if ( (*pfilled + size) > *palloc )
    {
        *palloc = ((*pfilled+size) + 0xFF) & ~0xFF;
        *pptr = HeapReAlloc( GetProcessHeap(), 0, *pptr, *palloc * sizeof(WORD) );
        if (!*pptr)
            return FALSE;
    }
    RtlMoveMemory( *pptr+*pfilled, data, size * sizeof(WORD) );
    *pfilled += size;
    return TRUE;
}

/**********************************************************************
 * Convert ActiveX control templates to AtlAxWin class instances
 */
static LPDLGTEMPLATEW AX_ConvertDialogTemplate(LPCDLGTEMPLATEW src_tmpl)
{
#define GET_WORD(x)  (*(const  WORD *)(x))
#define GET_DWORD(x) (*(const DWORD *)(x))
#define PUT_BLOCK(x,y) do {if (!advance_array(&output, &allocated, &filled, (x), (y))) return NULL;} while (0)
#define PUT_WORD(x)  do {WORD w = (x);PUT_BLOCK(&w, 1);} while(0)
#define PUT_DWORD(x)  do {DWORD w = (x);PUT_BLOCK(&w, 2);} while(0)
    const WORD *tmp, *src = (const WORD *)src_tmpl;
    WORD *output; 
    DWORD allocated, filled; /* in WORDs */
    BOOL ext;
    WORD signature, dlgver, rescount;
    DWORD style;

    filled = 0; allocated = 256;
    output = HeapAlloc( GetProcessHeap(), 0, allocated * sizeof(WORD) );
    if (!output)
        return NULL;
    
    /* header */
    tmp = src;
    signature = GET_WORD(src);
    dlgver = GET_WORD(src + 1);
    if (signature == 1 && dlgver == 0xFFFF)
    {
        ext = TRUE;
        src += 6;
        style = GET_DWORD(src);
        src += 2;
        rescount = GET_WORD(src++);
        src += 4;
        if ( GET_WORD(src) == 0xFFFF ) /* menu */
            src += 2;
        else
            src += strlenW(src) + 1;
        if ( GET_WORD(src) == 0xFFFF ) /* class */
            src += 2;
        else
            src += strlenW(src) + 1;
        src += strlenW(src) + 1; /* title */
        if ( style & (DS_SETFONT | DS_SHELLFONT) )
        {
            src += 3;
            src += strlenW(src) + 1;
        }
    } else {
        ext = FALSE;
        style = GET_DWORD(src);
        src += 4;
        rescount = GET_WORD(src++);
        src += 4;
        if ( GET_WORD(src) == 0xFFFF ) /* menu */
            src += 2;
        else
            src += strlenW(src) + 1;
        if ( GET_WORD(src) == 0xFFFF ) /* class */
            src += 2;
        else
            src += strlenW(src) + 1;
        src += strlenW(src) + 1; /* title */
        if ( style & DS_SETFONT )
        {
            src++;
            src += strlenW(src) + 1;
        }
    }
    PUT_BLOCK(tmp, src-tmp);

    while(rescount--)
    {
        src = (const WORD *)( ( ((ULONG)src) + 3) & ~3); /* align on DWORD boundary */
                filled = (filled + 1) & ~1; /* depends on DWORD-aligned allocation unit */

        tmp = src;
        if (ext)
            src += 11;
        else
            src += 9;
        PUT_BLOCK(tmp, src-tmp);

        tmp = src;
        if ( GET_WORD(src) == 0xFFFF ) /* class */
        {
            src += 2;
        } else
        {
            src += strlenW(src) + 1;
        }
        src += strlenW(src) + 1; /* title */
        if ( GET_WORD(tmp) == '{' ) /* all this mess created because of this line */
        {
            const WCHAR AtlAxWin[9]={'A','t','l','A','x','W','i','n',0};
            PUT_BLOCK(AtlAxWin, sizeof(AtlAxWin)/sizeof(WORD));
            PUT_BLOCK(tmp, strlenW(tmp)+1);
        } else
            PUT_BLOCK(tmp, src-tmp);

        if ( GET_WORD(src) )
        {
            WORD size = (GET_WORD(src)+sizeof(WORD)-1) / sizeof(WORD); /* quite ugly :( Maybe use BYTE* instead of WORD* everywhere ? */
            PUT_BLOCK(src, size);
            src+=size;
        }
        else
        {
            PUT_WORD(0);
            src++;
        }
    }
    return (LPDLGTEMPLATEW) output;
}

/***********************************************************************
 *           AtlAxCreateDialogA           [ATL.@]
 *
 * Creates a dialog window
 *
 * PARAMS
 *  hInst   [I] Application instance
 *  name    [I] Dialog box template name
 *  owner   [I] Dialog box parent HWND
 *  dlgProc [I] Dialog box procedure
 *  param   [I] This value will be passed to dlgProc as WM_INITDIALOG's message lParam 
 *
 * RETURNS
 *  Window handle of dialog window.
 */
HWND WINAPI AtlAxCreateDialogA(HINSTANCE hInst, LPCSTR name, HWND owner, DLGPROC dlgProc ,LPARAM param)
{
    HWND res = NULL;
    int length;
    WCHAR *nameW;

    if ( HIWORD(name) == 0 )
        return AtlAxCreateDialogW( hInst, (LPCWSTR) name, owner, dlgProc, param );

    length = MultiByteToWideChar( CP_ACP, 0, name, -1, NULL, 0 );
    nameW = HeapAlloc( GetProcessHeap(), 0, length * sizeof(WCHAR) );
    if (nameW)
    {
        MultiByteToWideChar( CP_ACP, 0, name, -1, nameW, length );
        res = AtlAxCreateDialogW( hInst, nameW, owner, dlgProc, param );
        HeapFree( GetProcessHeap(), 0, nameW );
    }
    return res;
}

/***********************************************************************
 *           AtlAxCreateDialogW           [ATL.@]
 *
 * See AtlAxCreateDialogA
 *
 */
HWND WINAPI AtlAxCreateDialogW(HINSTANCE hInst, LPCWSTR name, HWND owner, DLGPROC dlgProc ,LPARAM param)
{
    HRSRC hrsrc;
    HGLOBAL hgl;
    LPCDLGTEMPLATEW ptr;
    LPDLGTEMPLATEW newptr;
    HWND res;

    FIXME("(%p %s %p %p %lx) - not tested\n", hInst, debugstr_w(name), owner, dlgProc, param);

    hrsrc = FindResourceW( hInst, name, (LPWSTR)RT_DIALOG );
    if ( !hrsrc )
        return NULL;
    hgl = LoadResource (hInst, hrsrc);
    if ( !hgl )
        return NULL;
    ptr = (LPCDLGTEMPLATEW)LockResource ( hgl );
    if (!ptr)
    {
        FreeResource( hgl );
        return NULL;
    }
    newptr = AX_ConvertDialogTemplate( ptr );
    if ( newptr )
    {
            res = CreateDialogIndirectParamW( hInst, newptr, owner, dlgProc, param );
            HeapFree( GetProcessHeap(), 0, newptr );
    } else
        res = NULL;
    FreeResource ( hrsrc );
    return res;
}
