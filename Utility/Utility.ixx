/****************************************************************
* Copyright © 2025-present Jovibor https://github.com/jovibor/  *
* Helpful Windows Utilities.                                    *
* This software is available under "The MIT License"            *
****************************************************************/
module;
#include <Windows.h>
#include <Shlwapi.h> //SHCreateMemStream.
#include <ShObjIdl.h> //IFileOpenDialog and related.
#include <commctrl.h>
#include <d2d1_3.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dwrite_3.h>
#include <algorithm>
#include <cassert>
#include <format>
#include <locale>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
export module Utility;

export using ::COMDLG_FILTERSPEC;      //Used in the OpenFileDlg func.
export using ::IFileOpenDialog;        //Used in the OpenFileDlg func.
export using ::_FILEOPENDIALOGOPTIONS; //Used in the OpenFileDlg func.

#pragma comment(lib, "d2d1")
#pragma comment (lib, "d3d11") 
#pragma comment(lib, "dwrite")
#pragma comment(lib, "MSImg32")  //AlphaBlend.
#pragma comment(lib, "Shlwapi")  //SHCreateMemStream.
#pragma comment(lib, "Comctl32") //SetWindowSubclass

//Setting a manifest for the ComCtl32.dll version 6.
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' \
version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

export namespace ut {
	[[nodiscard]] auto StrToWstr(std::string_view sv, UINT uCodePage = CP_UTF8) -> std::wstring {
		const auto iSize = ::MultiByteToWideChar(uCodePage, 0, sv.data(), static_cast<int>(sv.size()), nullptr, 0);
		std::wstring wstr(iSize, 0);
		::MultiByteToWideChar(uCodePage, 0, sv.data(), static_cast<int>(sv.size()), wstr.data(), iSize);
		return wstr;
	}

	[[nodiscard]] auto WstrToStr(std::wstring_view wsv, UINT uCodePage = CP_UTF8) -> std::string {
		const auto iSize = ::WideCharToMultiByte(uCodePage, 0, wsv.data(), static_cast<int>(wsv.size()), nullptr, 0, nullptr, nullptr);
		std::string str(iSize, 0);
		::WideCharToMultiByte(uCodePage, 0, wsv.data(), static_cast<int>(wsv.size()), str.data(), iSize, nullptr, nullptr);
		return str;
	}

	//Returns a HINSTANCE of the current module, regardless of whether it is an .exe or .dll.
	[[nodiscard]] auto GetCurrModuleHinst() -> HINSTANCE {
		HINSTANCE hInst { };
		::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&GetCurrModuleHinst), &hInst);
		return hInst;
	};

	//Replicates GET_X_LPARAM macro from windowsx.h.
	[[nodiscard]] constexpr int GetXLPARAM(LPARAM lParam) {
		return static_cast<int>(static_cast<short>(static_cast<DWORD_PTR>(lParam) & 0xFFFFU));
	}

	//Replicates GET_Y_LPARAM macro from windowsx.h.
	[[nodiscard]] constexpr int GetYLPARAM(LPARAM lParam) {
		return GetXLPARAM(static_cast<DWORD_PTR>(lParam) >> 16);
	}
}

export namespace DXUT {
	template<typename TCom> requires requires(TCom* pTCom, const IID& iid, void** ppv) {
		pTCom->QueryInterface(iid, ppv); pTCom->AddRef(); pTCom->Release();
	}
	class comptr {
	public:
		comptr()noexcept = default;
		comptr(TCom* pTCom)noexcept : m_pTCom(pTCom) { }
		comptr(const comptr<TCom>& rhs)noexcept : m_pTCom(rhs.get()) { safe_addref(); }
		comptr(comptr&& rhs) noexcept : m_pTCom(std::exchange(rhs.m_pTCom, nullptr)) { }
		~comptr()noexcept { safe_release(); }
		[[nodiscard]] operator TCom*()const noexcept { return get(); }
		[[nodiscard]] operator TCom**()noexcept { return get_addr(); }
		[[nodiscard]] operator IUnknown**()noexcept { return reinterpret_cast<IUnknown**>(get_addr()); }
		[[nodiscard]] operator void**()noexcept { return reinterpret_cast<void**>(get_addr()); }
		[[nodiscard]] auto operator->()const noexcept->TCom* { return get(); }
		auto operator=(const comptr<TCom>& rhs)noexcept->comptr& {
			if (this != &rhs) { rhs.safe_addref(); safe_release(); m_pTCom = rhs.get(); }
			return *this;
		}
		auto operator=(TCom* pRHS)noexcept->comptr& {
			if (get() != pRHS) { safe_release(); m_pTCom = pRHS; }
			return *this;
		}
		auto operator=(comptr&& rhs)noexcept->comptr& {
			if (this != &rhs) { safe_release();	m_pTCom = std::exchange(rhs.m_pTCom, nullptr); }
			return *this;
		}
		[[nodiscard]] bool operator==(const comptr<TCom>& rhs)const noexcept { return get() == rhs.get(); }
		[[nodiscard]] bool operator==(const TCom* pRHS)const noexcept { return get() == pRHS; }
		[[nodiscard]] explicit operator bool()const noexcept { return get() != nullptr; }
		[[nodiscard]] bool operator!()const noexcept { return get() == nullptr; }
		[[nodiscard]] auto get()const noexcept -> TCom* { return m_pTCom; }
		[[nodiscard]] auto get_addr()noexcept -> TCom** { assert(m_pTCom == nullptr); return &m_pTCom; }
		void safe_release()noexcept { if (get() != nullptr) { std::exchange(m_pTCom, nullptr)->Release(); } }
		void safe_addref()const noexcept { if (get() != nullptr) { get()->AddRef(); } }
	private:
		TCom* m_pTCom { };
	};


	struct DWFONTINFO {
		std::wstring        wstrFamilyName;
		std::wstring        wstrLocale;
		DWRITE_FONT_WEIGHT  eWeight { DWRITE_FONT_WEIGHT_NORMAL };
		DWRITE_FONT_STRETCH eStretch { DWRITE_FONT_STRETCH_NORMAL };
		DWRITE_FONT_STYLE   eStyle { DWRITE_FONT_STYLE_NORMAL };
		float               flSizeDIP { }; //Font size in Device Independent Pixels (not points).
	};

	struct DWFONTFACE {
		std::wstring wstrTypographicFamilyName;           //DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME
		std::wstring wstrWeightStretchStyleFaceName;      //DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME
		std::wstring wstrFullName;                        //DWRITE_FONT_PROPERTY_ID_FULL_NAME
		std::wstring wstrWin32FamilyName;                 //DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME
		std::wstring wstrPostScriptName;                  //DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME
		std::vector<std::wstring> vecDesignScriptLangTag; //DWRITE_FONT_PROPERTY_ID_DESIGN_SCRIPT_LANGUAGE_TAG
		std::vector<std::wstring> vecSuppScriptLangTag;   //DWRITE_FONT_PROPERTY_ID_SUPPORTED_SCRIPT_LANGUAGE_TAG
		std::vector<std::wstring> vecSemanticTag;         //DWRITE_FONT_PROPERTY_ID_SEMANTIC_TAG
		std::wstring wstrWeight;                          //DWRITE_FONT_PROPERTY_ID_WEIGHT
		std::wstring wstrStretch;                         //DWRITE_FONT_PROPERTY_ID_STRETCH
		std::wstring wstrStyle;                           //DWRITE_FONT_PROPERTY_ID_STYLE
		std::wstring wstrTypographicFaceName;             //DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FACE_NAME
	};

	struct DWFONTFAMILY {
		std::wstring            wstrFamilyName; //DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME
		std::wstring            wstrLocale;
		std::vector<DWFONTFACE> vecFontFaceInfo;
		bool                    fIsMonospaced { };
	};

	[[nodiscard]] auto D3D11CreateDevice() -> ID3D11Device* {
		UINT uDeviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	#ifdef _DEBUG
		uDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	#endif
		const D3D_FEATURE_LEVEL arrFL[] {
			D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
		};
		ID3D11Device* pD3D11Device;
		::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, uDeviceFlags, arrFL, std::size(arrFL),
			D3D11_SDK_VERSION, &pD3D11Device, nullptr, nullptr);
		assert(pD3D11Device != nullptr);
		return pD3D11Device;
	}

	[[nodiscard]] auto D3D11GetDXGIDevice(ID3D11Device* pD3D11Device) -> IDXGIDevice1* {
		IDXGIDevice1* pDXGIDevice1;
		pD3D11Device->QueryInterface(&pDXGIDevice1);
		assert(pDXGIDevice1 != nullptr);
		return pDXGIDevice1;
	}

	[[nodiscard]] auto DXGICreateSwapChainForHWND(IDXGIDevice1* pDXGIDevice, ID3D11Device* pD3D11Device, HWND hWnd) -> IDXGISwapChain1* {
		assert(::IsWindow(hWnd));
		const DXGI_SWAP_CHAIN_DESC1 scd { .Width { 0 }, .Height { 0 }, .Format { DXGI_FORMAT_B8G8R8A8_UNORM },
			.Stereo { false }, .SampleDesc { .Count { 1 }, .Quality { 0 } }, .BufferUsage { DXGI_USAGE_RENDER_TARGET_OUTPUT },
			.BufferCount { 2 }, .Scaling { DXGI_SCALING_NONE }, .SwapEffect { DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL },
			.AlphaMode { DXGI_ALPHA_MODE_UNSPECIFIED }, .Flags { 0 } };
		comptr<IDXGIAdapter> pDXGIAdapter;
		pDXGIDevice->GetAdapter(pDXGIAdapter);
		assert(pDXGIAdapter != nullptr);
		if (pDXGIAdapter == nullptr) { return { }; }

		comptr<IDXGIFactory2> pDXGIFactory2;
		pDXGIAdapter->GetParent(IID_PPV_ARGS(pDXGIFactory2.get_addr()));
		assert(pDXGIFactory2 != nullptr);
		if (pDXGIFactory2 == nullptr) { return { }; }

		IDXGISwapChain1* pDXGISwapChain1;
		pDXGIFactory2->CreateSwapChainForHwnd(pD3D11Device, hWnd, &scd, nullptr, nullptr, &pDXGISwapChain1);
		assert(pDXGISwapChain1 != nullptr);
		return pDXGISwapChain1;
	}

	[[nodiscard]] auto D2DCreateFactory(D2D1_FACTORY_TYPE eD2DFT = D2D1_FACTORY_TYPE_SINGLE_THREADED) -> ID2D1Factory1* {
		ID2D1Factory1* pD2DFactory1;
		::D2D1CreateFactory(eD2DFT, IID_PPV_ARGS(&pD2DFactory1));
		assert(pD2DFactory1 != nullptr);
		return pD2DFactory1;
	}

	[[nodiscard]] auto D2DCreateDevice(ID2D1Factory1* pD2DFactory, IDXGIDevice1* pDXGIDevice) -> ID2D1Device* {
		ID2D1Device* pD2DDevice;
		pD2DFactory->CreateDevice(pDXGIDevice, &pD2DDevice);
		assert(pD2DDevice != nullptr);
		return pD2DDevice;
	}

	[[nodiscard]] auto D2DCreateDeviceContext(ID2D1Device* pD2DDevice) -> ID2D1DeviceContext* {
		assert(pD2DDevice);
		ID2D1DeviceContext* pD2DDeviceContext;
		pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &pD2DDeviceContext);
		assert(pD2DDeviceContext != nullptr);
		return pD2DDeviceContext;
	}

	[[nodiscard]] auto D2DCreateBitmapFromDXGISurface(ID2D1DeviceContext* pD2DDC, IDXGISwapChain1* pDXGISwapChain) -> ID2D1Bitmap1* {
		assert(pD2DDC != nullptr);
		assert(pDXGISwapChain != nullptr);
		comptr<IDXGISurface> pDXGISurface;
		pDXGISwapChain->GetBuffer(0, IID_PPV_ARGS(pDXGISurface.get_addr()));
		assert(pDXGISurface != nullptr);
		if (pDXGISurface == nullptr) { return { }; }

		ID2D1Bitmap1* pD2DBitmap1;
		const auto bp = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		   D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
		pD2DDC->CreateBitmapFromDxgiSurface(pDXGISurface, bp, &pD2DBitmap1);
		return pD2DBitmap1;
	}

	[[nodiscard]] auto DWCreateFactory(DWRITE_FACTORY_TYPE eDWFT = DWRITE_FACTORY_TYPE_SHARED) -> IDWriteFactory3* {
		IDWriteFactory3* pDWriteFactory3;
		::DWriteCreateFactory(eDWFT, __uuidof(pDWriteFactory3), reinterpret_cast<IUnknown**>(&pDWriteFactory3));
		assert(pDWriteFactory3 != nullptr);
		return pDWriteFactory3;
	}

	[[nodiscard]] auto DWCreateTextFormat(IDWriteFactory3* pDWriteFactory, const DWFONTINFO& dwfi) -> IDWriteTextFormat1* {
		comptr<IDWriteTextFormat> pTextFormat;
		pDWriteFactory->CreateTextFormat(dwfi.wstrFamilyName.data(), nullptr, dwfi.eWeight, dwfi.eStyle, dwfi.eStretch,
			dwfi.flSizeDIP, dwfi.wstrLocale.data(), pTextFormat);
		assert(pTextFormat != nullptr);
		if (pTextFormat == nullptr) { return { }; }

		IDWriteTextFormat1* pTextFormat1;
		pTextFormat->QueryInterface(&pTextFormat1);
		return pTextFormat1;
	}

	[[nodiscard]] auto DWCreateTextLayout(IDWriteFactory3* pDWriteFactory, std::wstring_view wsv,
		IDWriteTextFormat1* pTextFormat, float flWidthMax, float flHeightMax) -> IDWriteTextLayout1* {
		assert(pTextFormat);
		comptr<IDWriteTextLayout> pTextLayout;
		pDWriteFactory->CreateTextLayout(wsv.data(), static_cast<UINT32>(wsv.size()), pTextFormat, flWidthMax,
			flHeightMax, pTextLayout);
		assert(pTextLayout != nullptr);
		if (pTextLayout == nullptr) { return { }; }

		IDWriteTextLayout1* pTextLayout1;
		pTextLayout->QueryInterface(&pTextLayout1);
		return pTextLayout1;
	}

	[[nodiscard]] auto DWGetSystemFonts(IDWriteFactory3* pDWriteFactory, const wchar_t* pwszLocale = L"en-US")
		-> std::vector<DWFONTFAMILY> {
		const auto lmbGetWstrLocale = [=](IDWriteLocalizedStrings* pLocStrings)->std::wstring {
			if (pLocStrings == nullptr) {
				return { };
			}

			UINT32 u32Index;
			BOOL fExist;
			if (pLocStrings->FindLocaleName(pwszLocale, &u32Index, &fExist); fExist) {
				wchar_t buff[64];
				pLocStrings->GetString(u32Index, buff, std::size(buff));
				return buff;
			}
			return { }; };
		const auto lmbGetWstrFirst = [](IDWriteLocalizedStrings* pLocStrings)->std::wstring {
			if (pLocStrings == nullptr) {
				return { };
			}

			wchar_t buff[64];
			pLocStrings->GetString(0, buff, std::size(buff));
			return buff;
			};
		const auto lmbGetWstrAll = [](IDWriteLocalizedStrings* pLocStrings)->std::vector<std::wstring> {
			if (pLocStrings == nullptr) {
				return { };
			}

			const auto sCount = pLocStrings->GetCount();
			std::vector<std::wstring> vec;
			vec.reserve(sCount);
			for (auto i = 0U; i < sCount; ++i) {
				wchar_t buff[64];
				pLocStrings->GetString(i, buff, std::size(buff));
				vec.emplace_back(buff);
			}
			return vec;
			};

		comptr<IDWriteFontSet> pSysFontSet;
		pDWriteFactory->GetSystemFontSet(pSysFontSet);
		assert(pSysFontSet);
		if (!pSysFontSet) { return { }; }

		comptr<IDWriteStringList> pStringsFamilyName;
		pSysFontSet->GetPropertyValues(DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, pwszLocale,
			pStringsFamilyName);
		assert(pStringsFamilyName);
		if (!pStringsFamilyName) { return{ }; }

		const auto iCountFontFamilies = pStringsFamilyName->GetCount(); //How many unique Font Family Names.
		std::vector<DWFONTFAMILY> vecFontInfo;
		vecFontInfo.reserve(iCountFontFamilies);
		for (auto iFontFamily = 0U; iFontFamily < iCountFontFamilies; ++iFontFamily) {
			wchar_t buffFamilyName[64];
			pStringsFamilyName->GetString(iFontFamily, buffFamilyName, std::size(buffFamilyName));
			const DWRITE_FONT_PROPERTY fp { .propertyId { DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME },
				.propertyValue { buffFamilyName } };
			comptr<IDWriteFontSet> pFamilyNameSet;
			pSysFontSet->GetMatchingFonts(&fp, 1, pFamilyNameSet);
			const auto iCountFontFaces = pFamilyNameSet->GetFontCount(); //How many fonts (Font Face) within this Family Name.
			std::vector<DWFONTFACE> vecFontFaceInfo;
			vecFontFaceInfo.reserve(iCountFontFaces);

			bool fIsMonospaced { false };
			if (iCountFontFaces > 0) {
				comptr<IDWriteFontFaceReference> pFontFaceReference;
				pFamilyNameSet->GetFontFaceReference(0, pFontFaceReference);
				if (pFontFaceReference != nullptr) {
					comptr<IDWriteFontFace3> pFontFace3;
					pFontFaceReference->CreateFontFace(pFontFace3);
					if (pFontFace3 != nullptr) {
						fIsMonospaced = pFontFace3->IsMonospacedFont();
					}
				}
			}

			for (auto iFontFace = 0U; iFontFace < iCountFontFaces; ++iFontFace) {
				BOOL f;
				comptr<IDWriteLocalizedStrings> pStrTypographicFamilyName;
				comptr<IDWriteLocalizedStrings> pStrWeightStretchStyleFaceName;
				comptr<IDWriteLocalizedStrings> pStrFullName;
				comptr<IDWriteLocalizedStrings> pStrWin32FamilyName;
				comptr<IDWriteLocalizedStrings> pStrPostScriptName;
				comptr<IDWriteLocalizedStrings> pStrDesignScriptLangTag;
				comptr<IDWriteLocalizedStrings> pStrSuppScriptLangTag;
				comptr<IDWriteLocalizedStrings> pStrSemanticTag;
				comptr<IDWriteLocalizedStrings> pStrWeight;
				comptr<IDWriteLocalizedStrings> pStrStretch;
				comptr<IDWriteLocalizedStrings> pStrStyle;
				comptr<IDWriteLocalizedStrings> pStrTypographicFaceName;
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME, &f, pStrTypographicFamilyName);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME, &f, pStrWeightStretchStyleFaceName);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_FULL_NAME, &f, pStrFullName);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME, &f, pStrWin32FamilyName);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME, &f, pStrPostScriptName);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_DESIGN_SCRIPT_LANGUAGE_TAG, &f, pStrDesignScriptLangTag);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_SUPPORTED_SCRIPT_LANGUAGE_TAG, &f, pStrSuppScriptLangTag);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_SEMANTIC_TAG, &f, pStrSemanticTag);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_WEIGHT, &f, pStrWeight);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_STRETCH, &f, pStrStretch);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_STYLE, &f, pStrStyle);
				pFamilyNameSet->GetPropertyValues(iFontFace, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FACE_NAME, &f, pStrTypographicFaceName);
				vecFontFaceInfo.emplace_back(DWFONTFACE {
					.wstrTypographicFamilyName { lmbGetWstrLocale(pStrTypographicFamilyName) },
					.wstrWeightStretchStyleFaceName { lmbGetWstrLocale(pStrWeightStretchStyleFaceName) },
					.wstrFullName { lmbGetWstrLocale(pStrFullName) },
					.wstrWin32FamilyName { lmbGetWstrLocale(pStrWin32FamilyName) },
					.wstrPostScriptName { lmbGetWstrLocale(pStrPostScriptName) },
					.vecDesignScriptLangTag { lmbGetWstrAll(pStrDesignScriptLangTag) },
					.vecSuppScriptLangTag { lmbGetWstrAll(pStrSuppScriptLangTag) },
					.vecSemanticTag { lmbGetWstrAll(pStrSemanticTag) },
					.wstrWeight { lmbGetWstrFirst(pStrWeight) },
					.wstrStretch { lmbGetWstrFirst(pStrStretch) },
					.wstrStyle { lmbGetWstrFirst(pStrStyle) },
					.wstrTypographicFaceName { lmbGetWstrLocale(pStrTypographicFaceName) } });
			}

			vecFontInfo.emplace_back(DWFONTFAMILY { .wstrFamilyName { buffFamilyName }, .wstrLocale { pwszLocale },
				.vecFontFaceInfo { std::move(vecFontFaceInfo) }, .fIsMonospaced { fIsMonospaced } });
		}

		return vecFontInfo;
	}

	[[nodiscard]] auto D3D11GetDeviceStatic() {
		static const comptr pD3D11Device = D3D11CreateDevice();
		return pD3D11Device;
	}

	[[nodiscard]] auto D3D11GetDXGIDeviceStatic() {
		static const comptr pDXGIDevice = D3D11GetDXGIDevice(D3D11GetDeviceStatic());
		return pDXGIDevice;
	}

	[[nodiscard]] auto D2DGetFactoryStatic() {
		static const comptr pD2DFactory = D2DCreateFactory();
		return pD2DFactory;
	}

	[[nodiscard]] auto DWGetFactoryStatic() {
		static const comptr pDWriteFactory = DWCreateFactory();
		return pDWriteFactory;
	}

	class CTextEffect final : public IUnknown {
	public:
		CTextEffect() = default;
		CTextEffect(ID2D1Brush* pBrushBk, ID2D1Brush* pBrushText) : m_pBrushBk(pBrushBk), m_pBrushText(pBrushText) { }
		auto AddRef() -> ULONG override { return 1UL; }
		auto Release() -> ULONG override { return 1UL; }
		auto QueryInterface([[maybe_unused]] const IID& riid, [[maybe_unused]] void** ppvObject) -> HRESULT override {
			return E_NOTIMPL;
		}
		[[nodiscard]] auto GetBkBrush()const -> ID2D1Brush* { return m_pBrushBk; };
		[[nodiscard]] auto GetTextBrush()const -> ID2D1Brush* { return m_pBrushText; };
		void SetBkBrush(ID2D1Brush* pBrushBk) { m_pBrushBk = pBrushBk; }
		void SetTextBrush(ID2D1Brush* pBrushText) { m_pBrushText = pBrushText; }
	private:
		ID2D1Brush* m_pBrushBk { };
		ID2D1Brush* m_pBrushText { };
	};

	class CDWriteTextRenderer final : public IDWriteTextRenderer {
	public:
		struct DRAWCONTEXT {
			ID2D1DeviceContext* pDeviceContext { };
			ID2D1Brush*         pBrushTextDef { }; //Default text brush.
		};
		auto AddRef() -> ULONG override { return 1UL; }
		auto Release() -> ULONG override { return 1UL; }
		auto QueryInterface(const IID& riid, void** ppvObject) -> HRESULT override {
			if (riid == __uuidof(IUnknown)) {
				*ppvObject = reinterpret_cast<IUnknown*>(this);
				return S_OK;
			}
			if (riid == __uuidof(IDWritePixelSnapping)) {
				*ppvObject = reinterpret_cast<IDWritePixelSnapping*>(this);
				return S_OK;
			}
			if (riid == __uuidof(IDWriteTextRenderer)) {
				*ppvObject = reinterpret_cast<IDWriteTextRenderer*>(this);
				return S_OK;
			}

			*ppvObject = nullptr;

			return E_NOINTERFACE;
		}
		auto DrawGlyphRun([[maybe_unused]] void* pContext, FLOAT flBaseLineX, FLOAT flBaseLineY, DWRITE_MEASURING_MODE eMMode,
			const DWRITE_GLYPH_RUN* pGR, [[maybe_unused]] const DWRITE_GLYPH_RUN_DESCRIPTION* pGRD, IUnknown* pEffect) -> HRESULT override {
			ID2D1Brush* pBrushText;
			if (const auto pTextEffect = static_cast<CTextEffect*>(pEffect); pTextEffect != nullptr) {
				const auto pBrushBk = pTextEffect->GetBkBrush();
				pBrushText = pTextEffect->GetTextBrush();

				float flTextWidth = 0;
				for (UINT32 i = 0; i < pGR->glyphCount; ++i) {
					flTextWidth += pGR->glyphAdvances[i];
				}

				DWRITE_FONT_METRICS fm;
				pGR->fontFace->GetMetrics(&fm);
				const auto flAdjust = pGR->fontEmSize / fm.designUnitsPerEm;
				const auto flAscent = fm.ascent * flAdjust;
				const auto flDescent = fm.descent * flAdjust;
				const auto rcBk = D2D1::RectF(flBaseLineX, flBaseLineY - flAscent,
					flBaseLineX + flTextWidth, flBaseLineY + flDescent);
				m_context.pDeviceContext->FillRectangle(rcBk, pBrushBk);
			}
			else {
				pBrushText = m_context.pBrushTextDef;
			}

			m_context.pDeviceContext->DrawGlyphRun(D2D1::Point2F(flBaseLineX, flBaseLineY), pGR, pBrushText, eMMode);

			return S_OK;
		}
		auto DrawInlineObject([[maybe_unused]] void* pContext, [[maybe_unused]] FLOAT flBaseLineX, [[maybe_unused]] FLOAT flBaseLineY,
			[[maybe_unused]] IDWriteInlineObject* pInlineObject, [[maybe_unused]] BOOL fIsSideways, [[maybe_unused]] BOOL fIsRightToLeft,
			[[maybe_unused]] IUnknown* pEffect) -> HRESULT override {
			return E_NOTIMPL;
		}
		auto DrawStrikethrough([[maybe_unused]] void* pContext, FLOAT flBaseLineX, FLOAT flBaseLineY,
			const DWRITE_STRIKETHROUGH* pStrikeThrough, [[maybe_unused]] IUnknown* pEffect) -> HRESULT override {
			const auto flTop = flBaseLineY + pStrikeThrough->offset;
			m_context.pDeviceContext->DrawLine(D2D1::Point2F(flBaseLineX, flTop),
				D2D1::Point2F(flBaseLineX + pStrikeThrough->width, flTop), m_context.pBrushTextDef, pStrikeThrough->thickness);
			return S_OK;
		}
		auto DrawUnderline([[maybe_unused]] void* pContext, FLOAT flBaseLineX, FLOAT flBaseLineY,
			const DWRITE_UNDERLINE* pUnderline, [[maybe_unused]] IUnknown* pEffect) -> HRESULT override {
			const auto flTop = flBaseLineY + pUnderline->offset;
			m_context.pDeviceContext->DrawLine(D2D1::Point2F(flBaseLineX, flTop),
				D2D1::Point2F(flBaseLineX + pUnderline->width, flTop), m_context.pBrushTextDef, pUnderline->thickness);
			return S_OK;
		}
		auto GetCurrentTransform([[maybe_unused]] void* pContext, DWRITE_MATRIX* pMatrix) -> HRESULT override {
			m_context.pDeviceContext->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(pMatrix));
			return S_OK;
		}
		auto GetPixelsPerDip([[maybe_unused]] void* pContext, FLOAT* pPixelsPerDip) -> HRESULT override {
			float flDPIX;
			float flDPIY;
			m_context.pDeviceContext->GetDpi(&flDPIX, &flDPIY);
			*pPixelsPerDip = flDPIX / USER_DEFAULT_SCREEN_DPI;
			return S_OK;
		}
		auto IsPixelSnappingDisabled([[maybe_unused]] void* pContext, BOOL* pfIsDisabled) -> HRESULT override {
			*pfIsDisabled = FALSE;
			return S_OK;
		}
		void SetDrawContext(const DRAWCONTEXT& context) { m_context = context; }
	private:
		DRAWCONTEXT m_context;
	};
}

export namespace GDIUT { //Windows GDI related stuff.
	auto DefWndProc(const MSG& msg) -> LRESULT {
		return ::DefWindowProcW(msg.hwnd, msg.message, msg.wParam, msg.lParam);
	}

	template<typename T>
	auto CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)->LRESULT
	{
		//Different IHexCtrl objects will share the same WndProc<ExactTypeHere> function.
		//Hence, the map is needed to differentiate these objects. 
		//The DlgProc<T> works absolutely the same way.
		static std::unordered_map<HWND, T*> uMap;

		//CREATESTRUCTW::lpCreateParams always possesses `this` pointer, passed to the CreateWindowExW
		//function as lpParam. We save it to the static uMap to have access to this->ProcessMsg() method.
		if (uMsg == WM_CREATE) {
			const auto lpCS = reinterpret_cast<LPCREATESTRUCTW>(lParam);
			if (lpCS->lpCreateParams != nullptr) {
				uMap[hWnd] = reinterpret_cast<T*>(lpCS->lpCreateParams);
			}
		}

		if (const auto it = uMap.find(hWnd); it != uMap.end()) {
			const auto ret = it->second->ProcessMsg({ .hwnd { hWnd }, .message { uMsg },
				.wParam { wParam }, .lParam { lParam } });
			if (uMsg == WM_NCDESTROY) { //Remove hWnd from the map on window destruction.
				uMap.erase(it);
			}
			return ret;
		}

		return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
	}

	template<typename T>
	auto CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)->INT_PTR {
		//DlgProc should return zero for all non-processed messages.
		//In that case messages will be processed by Windows default dialog proc.
		//Non-processed messages should not be passed neither to DefWindowProcW nor to DefDlgProcW.
		//Processed messages should return any non-zero value, depending on message type.

		static std::unordered_map<HWND, T*> uMap;

		//DialogBoxParamW and CreateDialogParamW dwInitParam arg is sent with WM_INITDIALOG as lParam.
		if (uMsg == WM_INITDIALOG) {
			if (lParam != 0) {
				uMap[hWnd] = reinterpret_cast<T*>(lParam);
			}
		}

		if (const auto it = uMap.find(hWnd); it != uMap.end()) {
			const auto ret = it->second->ProcessMsg({ .hwnd { hWnd }, .message { uMsg },
				.wParam { wParam }, .lParam { lParam } });
			if (uMsg == WM_NCDESTROY) { //Remove hWnd from the map on dialog destruction.
				uMap.erase(it);
			}
			return ret;
		}

		return 0;
	}

	class CPoint final : public POINT {
	public:
		constexpr CPoint()noexcept : POINT { } { }
		constexpr CPoint(POINT pt)noexcept : POINT { pt } { }
		constexpr CPoint(int x, int y)noexcept : POINT { .x { x }, .y { y } } { }
		constexpr ~CPoint() = default;
		constexpr operator LPPOINT()noexcept { return this; }
		constexpr operator const POINT*()const noexcept { return this; }
		[[nodiscard]] constexpr bool operator==(const CPoint& rhs)const noexcept { return x == rhs.x && y == rhs.y; }
		[[nodiscard]] constexpr CPoint operator+(POINT pt)const noexcept { return { x + pt.x, y + pt.y }; }
		[[nodiscard]] constexpr CPoint operator-(POINT pt)const noexcept { return { x - pt.x, y - pt.y }; }
		constexpr void Offset(int iX, int iY)noexcept { x += iX; y += iY; }
		constexpr void Offset(POINT pt)noexcept { Offset(pt.x, pt.y); }
	};

	class CRect final : public RECT {
	public:
		constexpr CRect()noexcept : RECT { } { }
		constexpr CRect(int iLeft, int iTop, int iRight, int iBottom)noexcept : RECT { .left { iLeft }, .top { iTop },
			.right { iRight }, .bottom { iBottom } } { }
		constexpr CRect(const RECT& rc)noexcept : RECT(rc) { }
		constexpr CRect(LPCRECT pRC)noexcept : CRect() { if (pRC != nullptr) { *this = *pRC; } }
		constexpr CRect(POINT pt, SIZE size)noexcept : RECT { .left { pt.x }, .top { pt.y }, .right { pt.x + size.cx },
			.bottom { pt.y + size.cy } } { }
		constexpr CRect(POINT topLeft, POINT botRight)noexcept : RECT { .left { topLeft.x }, .top { topLeft.y },
			.right { botRight.x }, .bottom { botRight.y } } { }
		constexpr ~CRect() = default;
		constexpr operator LPRECT()noexcept { return this; }
		constexpr operator LPCRECT()const noexcept { return this; }
		[[nodiscard]] constexpr bool operator==(const CRect& rhs)const noexcept {
			return left == rhs.left && top == rhs.top && right == rhs.right && bottom == rhs.bottom;
		}
		constexpr CRect& operator=(const RECT& rhs)noexcept {
			left = rhs.left; top = rhs.top; right = rhs.right; bottom = rhs.bottom;
			return *this;
		}
		[[nodiscard]] constexpr auto BottomRight()const noexcept -> CPoint { return { right, bottom }; }
		constexpr void DeflateRect(int x, int y)noexcept { InflateRect(-x, -y); }
		constexpr void DeflateRect(SIZE size)noexcept { InflateRect(-size.cx, -size.cy); }
		constexpr void DeflateRect(LPCRECT pRC)noexcept {
			if (pRC == nullptr) { return; }
			left += pRC->left; top += pRC->top; right -= pRC->right; bottom -= pRC->bottom;
		}
		constexpr void DeflateRect(int l, int t, int r, int b)noexcept { left += l; top += t; right -= r; bottom -= b; }
		[[nodiscard]] constexpr int Height()const noexcept { return bottom - top; }
		constexpr void InflateRect(int x, int y)noexcept { left -= x; top -= y; right += x; bottom += y; }
		[[nodiscard]] constexpr bool IsRectEmpty()const noexcept { return right <= left || bottom <= top; }
		[[nodiscard]] constexpr bool IsRectNull()const noexcept { return left == 0 && right == 0 && top == 0 && bottom == 0; }
		constexpr void OffsetRect(int x, int y)noexcept { left += x; right += x; top += y; bottom += y; }
		constexpr void OffsetRect(POINT pt)noexcept { OffsetRect(pt.x, pt.y); }
		[[nodiscard]] constexpr bool PtInRect(POINT pt)const noexcept {
			return pt.x >= left && pt.y >= top && pt.x < right && pt.y < bottom;
		}
		constexpr void SetRect(int iLeft, int iTop, int iRight, int iBottom)noexcept {
			left = iLeft; top = iTop; right = iRight; bottom = iBottom;
		}
		constexpr void SetRectEmpty()noexcept { left = top = right = bottom = 0; }
		[[nodiscard]] constexpr auto TopLeft()const noexcept -> CPoint { return { left, top }; }
		[[nodiscard]] constexpr int Width()const noexcept { return right - left; }
	};


	class CSplitter final {
	public:
		static constexpr auto SPLITTER_MSG_RESIZEEND = 0x1U;
		enum class EAnchorSide : std::uint8_t { SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM };
		CSplitter() = default;
		CSplitter(const CSplitter&) = delete;
		CSplitter(CSplitter&&) = delete;
		~CSplitter();
		CSplitter& operator=(const CSplitter&) = delete;
		CSplitter& operator=(CSplitter&&) = delete;
		void AddItem(HWND hWndItem, bool fIsResize);
		void AddItem(int iItemID, bool fIsResize);
		void Initialize(HWND hWndHost, HWND hWndAnchor, EAnchorSide eSide, std::uint32_t u32Width = 10, std::uint32_t u32CtrlID = 0);
		void Initialize(HWND hWndHost, int iAnchorID, EAnchorSide eSide, std::uint32_t u32Width = 10, std::uint32_t u32CtrlID = 0);
		void SetEdges(int iMinEdge, int iMaxEdge);
		void Uninitialize();
	private:
		[[nodiscard]] bool IsCursorWE()const;
		[[nodiscard]] bool IsCurInSplitter()const;
		[[nodiscard]] bool IsLock()const;
		[[nodiscard]] bool IsSplitting()const; //Is splitting going on atm?
		void Lock();
		void Unlock();
		void WMMouseMove(int iX, int iY);
		bool WMLButtonDown();
		void WMLButtonUp();
		bool WMSetCursor(WPARAM wParam, LPARAM lParam);
		static auto CALLBACK SubclassProcHost(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
			UINT_PTR uIDSubclass, DWORD_PTR dwRefData)->LRESULT;
		static auto CALLBACK SubclassProcChild(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
			UINT_PTR uIDSubclass, DWORD_PTR dwRefData)->LRESULT;
	private:
		inline static CSplitter* m_pSplitterCurrentlyInUse { }; //The currently active splitter.
		struct ItemData {
			HWND hWnd { };      //Item window.
			bool fIsResize { }; //Is resize or move.
		};
		std::vector<ItemData> m_vecItems; //All items to resize/move.
		HWND m_hWndHost { };   //Host window.
		HWND m_hWndAnchor { }; //Anchor window, to work as a splitter basepoint.
		std::uint32_t m_u32WidthHalf { }; //Distance from the anchor-window's edge, where a cursor turns into a splitter (<->).
		POINT m_ptCurr { }; //Current cursor coordinates under the splitter area.
		std::uint32_t m_u32CtrlID { }; //Control ID of this splitter, to send notify messages from.
		int m_iMinEdge { }; //Minimum distance from the left (or top) side to stop splitting.
		int m_iMaxEdge { 0x7FFFFFFF }; //Maximum distance from the left (or top) side to stop splitting.
		EAnchorSide m_eAnchorSide { };
		bool m_fSplitting { };     //Left mouse is down for resize atm.
	};

	CSplitter::~CSplitter() {
		Uninitialize();
	}

	void CSplitter::AddItem(HWND hWndItem, bool fIsResize) {
		assert(::IsWindow(hWndItem));
		if (!::IsWindow(hWndItem) || hWndItem == m_hWndAnchor)
			return;

		if (std::ranges::any_of(m_vecItems, [hWndItem](HWND hWnd) { return hWnd == hWndItem; }, &ItemData::hWnd)) {
			return;
		}

		::SetWindowSubclass(hWndItem, SubclassProcChild, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<UINT_PTR>(this));
		m_vecItems.emplace_back(hWndItem, fIsResize);
	}

	void CSplitter::AddItem(int iItemID, bool fIsResize) {
		AddItem(::GetDlgItem(m_hWndHost, iItemID), fIsResize);
	}

	void CSplitter::Initialize(HWND hWndHost, HWND hWndAnchor, EAnchorSide eSide, std::uint32_t u32Width, std::uint32_t u32CtrlID) {
		Uninitialize();
		assert(::IsWindow(hWndHost));
		assert(::IsWindow(hWndAnchor));
		m_hWndHost = hWndHost;
		m_hWndAnchor = hWndAnchor;
		m_eAnchorSide = eSide;
		m_u32WidthHalf = u32Width / 2;
		m_u32CtrlID = u32CtrlID;
		::SetWindowSubclass(m_hWndHost, SubclassProcHost, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<UINT_PTR>(this));
		::SetWindowSubclass(m_hWndAnchor, SubclassProcChild, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<UINT_PTR>(this));
	}

	void CSplitter::Initialize(HWND hWndHost, int iAnchorID, EAnchorSide eSide, std::uint32_t u32Width, std::uint32_t u32CtrlID) {
		Initialize(hWndHost, ::GetDlgItem(hWndHost, iAnchorID), eSide, u32Width, u32CtrlID);
	}

	void CSplitter::SetEdges(int iMinEdge, int iMaxEdge) {
		m_iMinEdge = iMinEdge;
		m_iMaxEdge = iMaxEdge;
	}

	void CSplitter::Uninitialize() {
		if (m_hWndHost == nullptr)
			return;

		if (IsSplitting()) {
			m_fSplitting = false;
			if (::GetCapture() == m_hWndHost) {
				::ReleaseCapture();
			}
		}

		if (m_pSplitterCurrentlyInUse == this) {
			Unlock();
		}

		::RemoveWindowSubclass(m_hWndHost, SubclassProcHost, 1);
		::RemoveWindowSubclass(m_hWndAnchor, SubclassProcChild, 1);

		for (const auto& item : m_vecItems) {
			::RemoveWindowSubclass(item.hWnd, SubclassProcChild, 1);
		}

		m_vecItems.clear();
		m_hWndHost = nullptr;
		m_hWndAnchor = nullptr;
	}

	//Private methods.

	bool CSplitter::IsCursorWE()const {
		using enum EAnchorSide;
		return m_eAnchorSide == SIDE_LEFT || m_eAnchorSide == SIDE_RIGHT;
	}

	bool CSplitter::IsCurInSplitter()const {
		CRect rcAnchorWnd;
		::GetWindowRect(m_hWndAnchor, &rcAnchorWnd);
		::MapWindowPoints(nullptr, m_hWndHost, reinterpret_cast<LPPOINT>(&rcAnchorWnd), 2);

		CRect rcSplitter;
		using enum EAnchorSide;
		switch (m_eAnchorSide) {
		case SIDE_LEFT:
			rcSplitter.SetRect(rcAnchorWnd.left - m_u32WidthHalf, rcAnchorWnd.top,
			rcAnchorWnd.left + m_u32WidthHalf, rcAnchorWnd.bottom);
			break;
		case SIDE_TOP:
			rcSplitter.SetRect(rcAnchorWnd.left, rcAnchorWnd.top - m_u32WidthHalf,
				rcAnchorWnd.right, rcAnchorWnd.top + m_u32WidthHalf);
			break;
		case SIDE_RIGHT:
			rcSplitter.SetRect(rcAnchorWnd.right - m_u32WidthHalf, rcAnchorWnd.top,
				rcAnchorWnd.right + m_u32WidthHalf, rcAnchorWnd.bottom);
			break;
		case SIDE_BOTTOM:
			rcSplitter.SetRect(rcAnchorWnd.left, rcAnchorWnd.bottom - m_u32WidthHalf,
				rcAnchorWnd.right, rcAnchorWnd.bottom + m_u32WidthHalf);
			break;
		default:
			break;
		}

		POINT pt;
		::GetCursorPos(&pt);
		::ScreenToClient(m_hWndHost, &pt);

		return rcSplitter.PtInRect(pt);
	}

	bool CSplitter::IsLock()const {
		//Locking mechanism is needed to avoid Set/ReleaseCapture interference 
		//between two or more splitters in the same window.
		return m_pSplitterCurrentlyInUse != nullptr && m_pSplitterCurrentlyInUse != this;
	}

	bool CSplitter::IsSplitting()const {
		return m_fSplitting;
	}

	void CSplitter::Lock() {
		m_pSplitterCurrentlyInUse = this;
	}

	void CSplitter::Unlock() {
		m_pSplitterCurrentlyInUse = nullptr;
	}

	bool CSplitter::WMLButtonDown() {
		if (IsLock() || !IsCurInSplitter()) {
			return false;
		}

		POINT pt;
		::GetCursorPos(&pt);
		::ScreenToClient(m_hWndHost, &pt);
		m_ptCurr = pt;
		m_fSplitting = true;
		Lock();
		::SetCapture(m_hWndHost);
		return true;
	}

	void CSplitter::WMLButtonUp() {
		if (IsLock() || !IsSplitting()) {
			return;
		}

		m_fSplitting = false;
		Unlock();
		if (::GetCapture() == m_hWndHost) {
			::ReleaseCapture();
		}

		if (m_u32CtrlID != 0) {
			const NMHDR hdr = { .idFrom { m_u32CtrlID }, .code { SPLITTER_MSG_RESIZEEND } };
			::SendMessageW(m_hWndHost, WM_NOTIFY, m_u32CtrlID, reinterpret_cast<WPARAM>(&hdr));
		}
	}

	void CSplitter::WMMouseMove(int iX, int iY) {
		if (IsLock() || !IsSplitting()) {
			return;
		}

		auto iOffsetX = iX - m_ptCurr.x;
		auto iOffsetY = iY - m_ptCurr.y;
		if (iOffsetX == 0 && iOffsetY == 0) {
			return;
		}

		CRect rcAnchorWnd;
		::GetWindowRect(m_hWndAnchor, &rcAnchorWnd);
		::MapWindowPoints(nullptr, m_hWndHost, reinterpret_cast<LPPOINT>(&rcAnchorWnd), 2);

		using enum EAnchorSide;
		switch (m_eAnchorSide) {
		case SIDE_LEFT: {
			const auto iNewLeft = std::clamp<int>(rcAnchorWnd.left + iOffsetX, m_iMinEdge, m_iMaxEdge);
			iOffsetX = iNewLeft - rcAnchorWnd.left;
			iOffsetY = 0;
			rcAnchorWnd.left = iNewLeft;
			break;
		}
		case SIDE_TOP: {
			const auto iNewTop = std::clamp<int>(rcAnchorWnd.top + iOffsetY, m_iMinEdge, m_iMaxEdge);
			iOffsetX = 0;
			iOffsetY = iNewTop - rcAnchorWnd.top;
			rcAnchorWnd.top = iNewTop;
			break;
		}
		case SIDE_RIGHT: {
			const auto iNewRight = std::clamp<int>(rcAnchorWnd.right + iOffsetX, m_iMinEdge, m_iMaxEdge);
			iOffsetX = iNewRight - rcAnchorWnd.right;
			iOffsetY = 0;
			rcAnchorWnd.right = iNewRight;
			break;
		}
		case SIDE_BOTTOM: {
			const auto iNewBottom = std::clamp<int>(rcAnchorWnd.bottom + iOffsetY, m_iMinEdge, m_iMaxEdge);
			iOffsetX = 0;
			iOffsetY = iNewBottom - rcAnchorWnd.bottom;
			rcAnchorWnd.bottom = iNewBottom;
			break;
		}
		default:
			return;
		}

		if (iOffsetX == 0 && iOffsetY == 0) {
			return;
		}

		auto hdwp = ::BeginDeferWindowPos(static_cast<int>(m_vecItems.size() + 1)); //+1 is the m_hWndAnchor itself.
		hdwp = ::DeferWindowPos(hdwp, m_hWndAnchor, nullptr, rcAnchorWnd.left, rcAnchorWnd.top,
			rcAnchorWnd.Width(), rcAnchorWnd.Height(), SWP_NOACTIVATE | SWP_NOZORDER);

		for (const auto& [hWnd, fIsResize] : m_vecItems) {
			CRect rcWnd;
			::GetWindowRect(hWnd, &rcWnd);
			::MapWindowPoints(nullptr, m_hWndHost, reinterpret_cast<LPPOINT>(&rcWnd), 2);

			switch (m_eAnchorSide) {
			case SIDE_LEFT:
				if (fIsResize) {
					rcWnd.right += iOffsetX;
				}
				else {
					rcWnd.OffsetRect(iOffsetX, 0);
				}
				break;
			case SIDE_TOP:
				if (fIsResize) {
					rcWnd.bottom += iOffsetY;
				}
				else {
					rcWnd.OffsetRect(0, iOffsetY);
				}
				break;
			case SIDE_RIGHT:
				if (fIsResize) {
					rcWnd.left += iOffsetX;
				}
				else {
					rcWnd.OffsetRect(iOffsetX, 0);
				}
				break;
			case SIDE_BOTTOM:
				if (fIsResize) {
					rcWnd.top += iOffsetY;
				}
				else {
					rcWnd.OffsetRect(0, iOffsetY);
				}
				break;
			default:
				break;
			}

			hdwp = ::DeferWindowPos(hdwp, hWnd, nullptr, rcWnd.left, rcWnd.top,
						rcWnd.Width(), rcWnd.Height(), SWP_NOACTIVATE | SWP_NOZORDER);
		}

		m_ptCurr.x += iOffsetX;
		m_ptCurr.y += iOffsetY;
		::EndDeferWindowPos(hdwp);
	}

	bool CSplitter::WMSetCursor([[maybe_unused]] WPARAM wParam, [[maybe_unused]] LPARAM lParam) {
		if (IsLock() || IsSplitting()) {
			return false;
		}

		if (IsCurInSplitter()) {
			static const auto hCurResizeWE =
				static_cast<HCURSOR>(::LoadImageW(nullptr, IDC_SIZEWE, IMAGE_CURSOR, 0, 0, LR_SHARED));
			static const auto hCurResizeNS =
				static_cast<HCURSOR>(::LoadImageW(nullptr, IDC_SIZENS, IMAGE_CURSOR, 0, 0, LR_SHARED));
			::SetCursor(IsCursorWE() ? hCurResizeWE : hCurResizeNS);
			return true;
		}

		return false;
	}

	auto CSplitter::SubclassProcHost(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
		[[maybe_unused]] UINT_PTR uIDSubclass, DWORD_PTR dwRefData)->LRESULT {
		switch (uMsg) {
		case WM_NCLBUTTONDOWN:
		case WM_LBUTTONDOWN:
			if (reinterpret_cast<CSplitter*>(dwRefData)->WMLButtonDown()) {
				return 0;
			}
			break;
		case WM_CAPTURECHANGED: //If Alt+Tab fires during splitting, we run WMLButtonUp().
		case WM_LBUTTONUP:
			reinterpret_cast<CSplitter*>(dwRefData)->WMLButtonUp();
			break;
		case WM_MOUSEMOVE:
			reinterpret_cast<CSplitter*>(dwRefData)->WMMouseMove(ut::GetXLPARAM(lParam), ut::GetYLPARAM(lParam));
			break;
		case WM_NCDESTROY:
			reinterpret_cast<CSplitter*>(dwRefData)->Uninitialize();
			break;
		case WM_SETCURSOR:
			if (reinterpret_cast<CSplitter*>(dwRefData)->WMSetCursor(wParam, lParam)) {
				return TRUE;
			}
			break;
		default:
			break;
		}

		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	auto CSplitter::SubclassProcChild(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
		UINT_PTR uIDSubclass, DWORD_PTR dwRefData) -> LRESULT {
		switch (uMsg) {
		case WM_NCLBUTTONDOWN:
		case WM_LBUTTONDOWN:
			if (reinterpret_cast<CSplitter*>(dwRefData)->WMLButtonDown()) {
				return 0;
			}
			break;
		case WM_NCDESTROY:
			::RemoveWindowSubclass(hWnd, SubclassProcChild, uIDSubclass);
			break;
		default:
			break;
		}

		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}


	class CDynLayout final {
	public:
		//Ratio settings, for how much to move or to resize child item when parent is resized.
		struct ItemRatio {
			[[nodiscard]] bool IsNull()const { return flXRatio == 0.F && flYRatio == 0.F; };
			float flXRatio { }; float flYRatio { };
		};
		struct MoveRatio : public ItemRatio { }; //To differentiate move from size in the AddItem.
		struct SizeRatio : public ItemRatio { };

		CDynLayout() = default;
		~CDynLayout();
		void AddItem(int iItemID, MoveRatio move, SizeRatio size);
		void AddItem(HWND hWndItem, MoveRatio move, SizeRatio size);
		void Enable(bool fTrack);
		void Initialize(HWND hWndHost); //This is the main method that should be called first.
		bool LoadFromResource(HINSTANCE hInstRes, const wchar_t* pwszResName);
		bool LoadFromResource(HINSTANCE hInstRes, UINT uResID);
		void Uninitialize();
		void UpdateItem(int iItemID, MoveRatio move, SizeRatio size);
		void UpdateItem(HWND hWndItem, MoveRatio move, SizeRatio size);

		//Static helper methods to use in the AddItem.
		[[nodiscard]] static MoveRatio MoveNone() { return { }; }
		[[nodiscard]] static MoveRatio MoveHorz(int iXRatio) {
			return { { .flXRatio { ToFlRatio(iXRatio) } } };
		}
		[[nodiscard]] static MoveRatio MoveVert(int iYRatio) {
			return { { .flYRatio { ToFlRatio(iYRatio) } } };
		}
		[[nodiscard]] static MoveRatio MoveHorzAndVert(int iXRatio, int iYRatio) {
			return { { .flXRatio { ToFlRatio(iXRatio) }, .flYRatio { ToFlRatio(iYRatio) } } };
		}
		[[nodiscard]] static SizeRatio SizeNone() { return { }; }
		[[nodiscard]] static SizeRatio SizeHorz(int iXRatio) {
			return { { .flXRatio { ToFlRatio(iXRatio) } } };
		}
		[[nodiscard]] static SizeRatio SizeVert(int iYRatio) {
			return { { .flYRatio { ToFlRatio(iYRatio) } } };
		}
		[[nodiscard]] static SizeRatio SizeHorzAndVert(int iXRatio, int iYRatio) {
			return { { .flXRatio { ToFlRatio(iXRatio) }, .flYRatio { ToFlRatio(iYRatio) } } };
		}
	private:
		void WMSize(int iWidth, int iHeight)const;
		static auto CALLBACK SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
			UINT_PTR uIDSubclass, DWORD_PTR dwRefData)->LRESULT;
	private:
		[[nodiscard]] static auto ToFlRatio(int iRatio) -> float {
			return std::clamp(iRatio, 0, 100) / 100.F;
		}
		struct ItemData {
			HWND hWnd { };   //Item window.
			RECT rcOrig { }; //Item original window rect after EnableTrack(true).
			MoveRatio move;  //How much to move the item.
			SizeRatio size;  //How much to resize the item.
		};
		HWND m_hWndHost { };   //Host window.
		RECT m_rcHostOrig { }; //Host original client area rect after EnableTrack(true).
		std::vector<ItemData> m_vecItems; //All items to resize/move.
		bool m_fTrack { };
	};

	CDynLayout::~CDynLayout() {
		Uninitialize();
	}

	void CDynLayout::AddItem(int iItemID, MoveRatio move, SizeRatio size) {
		AddItem(::GetDlgItem(m_hWndHost, iItemID), move, size);
	}

	void CDynLayout::AddItem(HWND hWndItem, MoveRatio move, SizeRatio size) {
		assert(hWndItem != nullptr);
		if (hWndItem == nullptr)
			return;

		m_vecItems.emplace_back(ItemData { .hWnd { hWndItem }, .move { move }, .size { size } });
	}

	void CDynLayout::Enable(bool fTrack) {
		m_fTrack = fTrack;
		if (m_fTrack) {
			::GetClientRect(m_hWndHost, &m_rcHostOrig);
			for (auto& [hWnd, rc, move, size] : m_vecItems) {
				::GetWindowRect(hWnd, &rc);
				::ScreenToClient(m_hWndHost, reinterpret_cast<LPPOINT>(&rc));
				::ScreenToClient(m_hWndHost, reinterpret_cast<LPPOINT>(&rc) + 1);
			}
		}
	}

	void CDynLayout::Initialize(HWND hWndHost) {
		assert(hWndHost != nullptr);
		if (m_hWndHost != nullptr) {
			Uninitialize();
		}

		m_hWndHost = hWndHost;
		::SetWindowSubclass(m_hWndHost, SubclassProc, reinterpret_cast<UINT_PTR>(this), 0);
	}

	bool CDynLayout::LoadFromResource(HINSTANCE hInstRes, const wchar_t* pwszResName) {
		assert(pwszResName != nullptr);
		if (pwszResName == nullptr)
			return false;

		assert(m_hWndHost != nullptr);
		if (m_hWndHost == nullptr)
			return false;

		const auto hDlgLayout = ::FindResourceW(hInstRes, pwszResName, L"AFX_DIALOG_LAYOUT");
		assert(hDlgLayout != nullptr);
		if (hDlgLayout == nullptr) { //No such resource found in the hInstRes.
			return false;
		}

		const auto hResData = ::LoadResource(hInstRes, hDlgLayout);
		assert(hResData != nullptr);
		if (hResData == nullptr)
			return false;

		const auto pResData = ::LockResource(hResData);
		assert(pResData != nullptr);
		if (pResData == nullptr)
			return false;

		const auto dwSizeRes = ::SizeofResource(hInstRes, hDlgLayout);
		const auto* pDataBegin = reinterpret_cast<WORD*>(pResData);
		const auto* const pDataEnd = reinterpret_cast<WORD*>(reinterpret_cast<std::byte*>(pResData) + dwSizeRes);

		assert(*pDataBegin == 0);
		if (*pDataBegin != 0) //The first WORD must be zero, it's a header (version number).
			return false;

		++pDataBegin; //Past the first WORD is the actual data.
		auto hWndChild = ::GetWindow(m_hWndHost, GW_CHILD); //First child window in the host window.
		while (pDataBegin + 4 <= pDataEnd) { //Actual AFX_DIALOG_LAYOUT data.
			if (hWndChild == nullptr)
				break;

			const auto wXMoveRatio = *pDataBegin++;
			const auto wYMoveRatio = *pDataBegin++;
			const auto wXSizeRatio = *pDataBegin++;
			const auto wYSizeRatio = *pDataBegin++;
			AddItem(hWndChild, MoveHorzAndVert(wXMoveRatio, wYMoveRatio), SizeHorzAndVert(wXSizeRatio, wYSizeRatio));
			hWndChild = ::GetWindow(hWndChild, GW_HWNDNEXT);
		}

		return true;
	}

	bool CDynLayout::LoadFromResource(HINSTANCE hInstRes, UINT uResID) {
		return LoadFromResource(hInstRes, MAKEINTRESOURCEW(uResID));
	}

	void CDynLayout::Uninitialize() {
		if (m_hWndHost == nullptr)
			return;

		::RemoveWindowSubclass(m_hWndHost, SubclassProc, reinterpret_cast<UINT_PTR>(this));
		m_vecItems.clear();
		m_hWndHost = nullptr;
	}

	void CDynLayout::UpdateItem(int iItemID, MoveRatio move, SizeRatio size) {
		UpdateItem(::GetDlgItem(m_hWndHost, iItemID), move, size);
	}

	void CDynLayout::UpdateItem(HWND hWndItem, MoveRatio move, SizeRatio size) {
		assert(hWndItem != nullptr);
		if (hWndItem == nullptr)
			return;

		auto it = std::find_if(m_vecItems.begin(), m_vecItems.end(), [=](const ItemData& id) {
			return id.hWnd == hWndItem; });
		assert(it != m_vecItems.end());
		if (it != m_vecItems.end()) {
			it->move = move;
			it->size = size;
		}
	}

	//Private methods.

	void CDynLayout::WMSize(int iWidth, int iHeight)const {
		if (!m_fTrack)
			return;

		const auto iHostWidth = m_rcHostOrig.right - m_rcHostOrig.left;
		const auto iHostHeight = m_rcHostOrig.bottom - m_rcHostOrig.top;
		const auto iDeltaX = iWidth - iHostWidth;
		const auto iDeltaY = iHeight - iHostHeight;

		const auto hDWP = ::BeginDeferWindowPos(static_cast<int>(m_vecItems.size()));
		for (const auto& [hWnd, rc, move, size] : m_vecItems) {
			const auto iNewLeft = static_cast<int>(rc.left + (iDeltaX * move.flXRatio));
			const auto iNewTop = static_cast<int>(rc.top + (iDeltaY * move.flYRatio));
			const auto iOrigWidth = rc.right - rc.left;
			const auto iOrigHeight = rc.bottom - rc.top;
			const auto iNewWidth = static_cast<int>(iOrigWidth + (iDeltaX * size.flXRatio));
			const auto iNewHeight = static_cast<int>(iOrigHeight + (iDeltaY * size.flYRatio));
			::DeferWindowPos(hDWP, hWnd, nullptr, iNewLeft, iNewTop, iNewWidth, iNewHeight, SWP_NOZORDER | SWP_NOACTIVATE);
		}
		::EndDeferWindowPos(hDWP);
	}

	auto CDynLayout::SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIDSubclass,
	 [[maybe_unused]] DWORD_PTR dwRefData)->LRESULT {
		switch (uMsg) {
		case WM_NCDESTROY:
			reinterpret_cast<CDynLayout*>(uIDSubclass)->Uninitialize();
			break;
		case WM_SIZE:
			reinterpret_cast<CDynLayout*>(uIDSubclass)->WMSize(LOWORD(lParam), HIWORD(lParam));
			break;
		default:
			break;
		}

		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}


	class CLinkCtrl final {
	public:
		CLinkCtrl() = default;
		~CLinkCtrl();
		void Initialize(HWND hWndHost, HWND hWndLink, std::wstring_view wsvURL = { },
			COLORREF clrBk = RGB(240, 240, 240), COLORREF clrText = RGB(0, 50, 250));
		void Initialize(HWND hWndHost, int iLinkID, std::wstring_view wsvURL = { },
			COLORREF clrBk = RGB(240, 240, 240), COLORREF clrText = RGB(0, 50, 250));
		void Uninitialize();
		void WMDPIChanged(); //Should be hooked into the host window's WM_DPICHANGED handler.
	private:
		void CreateFonts();
		auto WMCtlColorStatic(WPARAM wParam, LPARAM lParam) -> INT_PTR;
		void WMLButtonDown(WPARAM wParam, LPARAM lParam);
		void WMLButtonUp(WPARAM wParam, LPARAM lParam);
		void WMMouseMove(WPARAM wParam, LPARAM lParam);
		auto WMSetCursor() -> INT_PTR;
		static auto CALLBACK SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
			UINT_PTR uIDSubclass, DWORD_PTR dwRefData)->LRESULT;
	private:
		std::wstring m_wstrURL;
		HWND m_hWndHost { };
		HWND m_hWndLink { };
		HFONT m_hFontDef { }; //Main default font of the link control, should not be deleted.
		HFONT m_hFontUnderline { };
		HBRUSH m_hbrBk { };
		COLORREF m_clrBk { };
		COLORREF m_clrText { };
		bool m_fHandCursor { }; //Is cursor a hand cursor atm?
		bool m_fLBDownLink { }; //Left button was pressed on the link static control.
	};

	CLinkCtrl::~CLinkCtrl() {
		Uninitialize();
	}

	void CLinkCtrl::Initialize(HWND hWndHost, HWND hWndLink, std::wstring_view wsvURL, COLORREF clrBk, COLORREF clrText) {
		assert(hWndHost != nullptr);
		assert(hWndLink != nullptr);
		m_hWndHost = hWndHost;
		m_hWndLink = hWndLink;
		m_clrBk = clrBk;
		m_clrText = clrText;
		m_hbrBk = ::CreateSolidBrush(m_clrBk);

		if (wsvURL.empty()) {
			wchar_t buff[512];
			::GetWindowTextW(m_hWndLink, buff, 512);
			m_wstrURL = buff;
		}
		else {
			m_wstrURL = wsvURL;
		}

		CreateFonts();
		::SetWindowSubclass(m_hWndHost, SubclassProc, reinterpret_cast<UINT_PTR>(this), 0);
	}

	void CLinkCtrl::Initialize(HWND hWndHost, int iLinkID, std::wstring_view wsvURL, COLORREF clrBk, COLORREF clrText) {
		Initialize(hWndHost, ::GetDlgItem(hWndHost, iLinkID), wsvURL, clrBk, clrText);
	}

	void CLinkCtrl::Uninitialize() {
		if (m_hWndHost == nullptr)
			return;

		::RemoveWindowSubclass(m_hWndHost, SubclassProc, reinterpret_cast<UINT_PTR>(this));
		::DeleteObject(m_hFontUnderline);
		::DeleteObject(m_hbrBk);
		m_hWndHost = nullptr;
	}

	void CLinkCtrl::WMDPIChanged() {
		//This method must be placed into the host window's WM_DPICHANGED handler,
		//to properly recreate link control's underline font when DPI changes.
		//Unfortunately, it doesn't work correctly when called from the SubclassProc.
		//The underline font, for some reason, remains with the size from the previous DPI.
		//This is the behavior at least in Windows 11. Looks like a Windows bug.

		CreateFonts();
	}

	//Private methods.

	void CLinkCtrl::CreateFonts() {
		m_hFontDef = reinterpret_cast<HFONT>(::SendMessageW(m_hWndLink, WM_GETFONT, 0, 0));
		LOGFONTW lf;
		::GetObjectW(m_hFontDef, sizeof(lf), &lf);
		lf.lfUnderline = TRUE;
		::DeleteObject(m_hFontUnderline);
		m_hFontUnderline = ::CreateFontIndirectW(&lf);
	}

	auto CLinkCtrl::WMCtlColorStatic(WPARAM wParam, LPARAM lParam)->INT_PTR {
		const auto hWndFrom = reinterpret_cast<HWND>(lParam);
		if (hWndFrom != m_hWndLink)
			return FALSE; //Default handler.

		const auto hDC = reinterpret_cast<HDC>(wParam);
		::SetTextColor(hDC, m_clrText);
		::SetBkColor(hDC, m_clrBk);
		::SelectObject(hDC, m_fHandCursor ? m_hFontUnderline : m_hFontDef);
		return reinterpret_cast<INT_PTR>(m_hbrBk);
	}

	void CLinkCtrl::WMLButtonDown([[maybe_unused]] WPARAM wParam, LPARAM lParam) {
		const POINT pt { .x { ut::GetXLPARAM(lParam) }, .y { ut::GetYLPARAM(lParam) } };
		m_fLBDownLink = ::ChildWindowFromPoint(m_hWndHost, pt) == m_hWndLink;
	}

	void CLinkCtrl::WMLButtonUp([[maybe_unused]] WPARAM wParam, LPARAM lParam) {
		const POINT pt { .x { ut::GetXLPARAM(lParam) }, .y { ut::GetYLPARAM(lParam) } };
		const auto hWnd = ::ChildWindowFromPoint(m_hWndHost, pt);
		if (hWnd != m_hWndLink) {
			m_fLBDownLink = false;
			return;
		}

		if (m_fLBDownLink) {
			::ShellExecuteW(nullptr, L"open", m_wstrURL.data(), nullptr, nullptr, 0);
		}
	}

	void CLinkCtrl::WMMouseMove([[maybe_unused]] WPARAM wParam, LPARAM lParam) {
		const POINT pt { .x { ut::GetXLPARAM(lParam) }, .y { ut::GetYLPARAM(lParam) } };
		const auto hWnd = ::ChildWindowFromPoint(m_hWndHost, pt);
		if (hWnd == nullptr)
			return;

		if (m_fHandCursor != (m_hWndLink == hWnd)) {
			m_fHandCursor = m_hWndLink == hWnd;
			::InvalidateRect(m_hWndLink, nullptr, FALSE);
		}
	}

	auto CLinkCtrl::WMSetCursor()->INT_PTR {
		if (m_fHandCursor) {
			const auto hCurHand = static_cast<HCURSOR>(::LoadImageW(nullptr, IDC_HAND, IMAGE_CURSOR, 0, 0,
				LR_DEFAULTSIZE | LR_SHARED));
			::SetCursor(hCurHand);
			return TRUE;
		}

		return 0; //Default cursor.
	}

	auto CLinkCtrl::SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIDSubclass,
		[[maybe_unused]] DWORD_PTR dwRefData)->LRESULT {
		switch (uMsg) {
		case WM_CTLCOLORSTATIC:
			if (const auto hbr = reinterpret_cast<CLinkCtrl*>(uIDSubclass)->WMCtlColorStatic(wParam, lParam);
				hbr != 0) {
				return hbr;
			}
			break;
		case WM_LBUTTONDOWN:
			reinterpret_cast<CLinkCtrl*>(uIDSubclass)->WMLButtonDown(wParam, lParam);
			break;
		case WM_LBUTTONUP:
			reinterpret_cast<CLinkCtrl*>(uIDSubclass)->WMLButtonUp(wParam, lParam);
			break;
		case WM_MOUSEMOVE:
			reinterpret_cast<CLinkCtrl*>(uIDSubclass)->WMMouseMove(wParam, lParam);
			break;
		case WM_NCDESTROY:
			reinterpret_cast<CLinkCtrl*>(uIDSubclass)->Uninitialize();
			break;
		case WM_SETCURSOR:
			if (const auto ret = reinterpret_cast<CLinkCtrl*>(uIDSubclass)->WMSetCursor(); ret != FALSE) {
				return TRUE;
			}
			break;
		default:
			break;
		}

		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}


	class CDC {
	public:
		CDC() = default;
		CDC(HDC hDC) : m_hDC(hDC) { }
		~CDC() = default;
		operator HDC()const { return m_hDC; }
		void AbortDoc()const { ::AbortDoc(m_hDC); }
		int AlphaBlend(int iX, int iY, int iWidth, int iHeight, HDC hDCSrc, int iXSrc, int iYSrc, int iWidthSrc,
			int iHeightSrc, BLENDFUNCTION bf = { .SourceConstantAlpha { 255 }, .AlphaFormat { AC_SRC_ALPHA } })const {
			return ::AlphaBlend(m_hDC, iX, iY, iWidth, iHeight, hDCSrc, iXSrc, iYSrc, iWidthSrc, iHeightSrc, bf);
		}
		BOOL BitBlt(int iX, int iY, int iWidth, int iHeight, HDC hDCSource, int iXSource, int iYSource, DWORD dwROP)const {
			//When blitting from a monochrome bitmap to a color one, the black color in the monohrome bitmap 
			//becomes the destination DC’s text color, and the white color in the monohrome bitmap 
			//becomes the destination DC’s background color, when using SRCCOPY mode.
			return ::BitBlt(m_hDC, iX, iY, iWidth, iHeight, hDCSource, iXSource, iYSource, dwROP);
		}
		[[nodiscard]] auto CreateCompatibleBitmap(int iWidth, int iHeight)const -> HBITMAP {
			return ::CreateCompatibleBitmap(m_hDC, iWidth, iHeight);
		}
		[[nodiscard]] CDC CreateCompatibleDC()const { return ::CreateCompatibleDC(m_hDC); }
		void DeleteDC()const { ::DeleteDC(m_hDC); }
		bool DrawFrameControl(LPRECT pRC, UINT uType, UINT uState)const {
			return ::DrawFrameControl(m_hDC, pRC, uType, uState);
		}
		bool DrawFrameControl(int iX, int iY, int iWidth, int iHeight, UINT uType, UINT uState)const {
			RECT rc { .left { iX }, .top { iY }, .right { iX + iWidth }, .bottom { iY + iHeight } };
			return DrawFrameControl(&rc, uType, uState);
		}
		void DrawImage(HBITMAP hBmp, int iX, int iY, int iWidth, int iHeight)const {
			const auto dcMem = CreateCompatibleDC();
			dcMem.SelectObject(hBmp);
			BITMAP bm; ::GetObjectW(hBmp, sizeof(BITMAP), &bm);

			//Only 32bit bitmaps can have alpha channel.
			//If destination and source bitmaps do not have the same color format, 
			//AlphaBlend converts the source bitmap to match the destination bitmap.
			//AlphaBlend works with both, DI (DeviceIndependent) and DD (DeviceDependent), bitmaps.
			const BLENDFUNCTION bf = { .SourceConstantAlpha { 255 }, .AlphaFormat {
				static_cast<BYTE>(bm.bmBitsPixel == 32 ? AC_SRC_ALPHA : 0) } };
			AlphaBlend(iX, iY, iWidth, iHeight, dcMem, 0, 0, iWidth, iHeight, bf);
			dcMem.DeleteDC();
		}
		[[nodiscard]] HDC GetHDC()const { return m_hDC; }
		void GetTextMetricsW(LPTEXTMETRICW pTM)const { ::GetTextMetricsW(m_hDC, pTM); }
		auto SetBkColor(COLORREF clr)const -> COLORREF { return ::SetBkColor(m_hDC, clr); }
		void DrawEdge(LPRECT pRC, UINT uEdge, UINT uFlags)const { ::DrawEdge(m_hDC, pRC, uEdge, uFlags); }
		void DrawFocusRect(LPCRECT pRc)const { ::DrawFocusRect(m_hDC, pRc); }
		int DrawTextW(std::wstring_view wsv, LPRECT pRect, UINT uFormat)const {
			return DrawTextW(wsv.data(), static_cast<int>(wsv.size()), pRect, uFormat);
		}
		int DrawTextW(LPCWSTR pwszText, int iSize, LPRECT pRect, UINT uFormat)const {
			return ::DrawTextW(m_hDC, pwszText, iSize, pRect, uFormat);
		}
		int EndDoc()const { return ::EndDoc(m_hDC); }
		int EndPage()const { return ::EndPage(m_hDC); }
		void FillSolidRect(LPCRECT pRC, COLORREF clr)const {
			::SetBkColor(m_hDC, clr); ::ExtTextOutW(m_hDC, 0, 0, ETO_OPAQUE, pRC, nullptr, 0, nullptr);
		}
		[[nodiscard]] auto GetClipBox()const -> CRect { CRect rc; ::GetClipBox(m_hDC, rc); return rc; }
		bool LineTo(POINT pt)const { return LineTo(pt.x, pt.y); }
		bool LineTo(int x, int y)const { return ::LineTo(m_hDC, x, y); }
		bool MoveTo(POINT pt)const { return MoveTo(pt.x, pt.y); }
		bool MoveTo(int x, int y)const { return ::MoveToEx(m_hDC, x, y, nullptr); }
		bool Polygon(const POINT* pPT, int iCount)const { return ::Polygon(m_hDC, pPT, iCount); }
		int SetDIBits(HBITMAP hBmp, UINT uStartLine, UINT uLines, const void* pBits, const BITMAPINFO* pBMI, UINT uClrUse)const {
			return ::SetDIBits(m_hDC, hBmp, uStartLine, uLines, pBits, pBMI, uClrUse);
		}
		int SetDIBitsToDevice(int iX, int iY, DWORD dwWidth, DWORD dwHeight, int iXSrc, int iYSrc, UINT uStartLine, UINT uLines,
			const void* pBits, const BITMAPINFO* pBMI, UINT uClrUse)const {
			return ::SetDIBitsToDevice(m_hDC, iX, iY, dwWidth, dwHeight, iXSrc, iYSrc, uStartLine, uLines, pBits, pBMI, uClrUse);
		}
		int SetMapMode(int iMode)const { return ::SetMapMode(m_hDC, iMode); }
		auto SetTextColor(COLORREF clr)const -> COLORREF { return ::SetTextColor(m_hDC, clr); }
		auto SetViewportOrg(int iX, int iY)const -> POINT { POINT pt; ::SetViewportOrgEx(m_hDC, iX, iY, &pt); return pt; }
		auto SelectObject(HGDIOBJ hObj)const -> HGDIOBJ { return ::SelectObject(m_hDC, hObj); }
		int StartDocW(const DOCINFOW* pDI)const { return ::StartDocW(m_hDC, pDI); }
		int StartPage()const { return ::StartPage(m_hDC); }
		void TextOutW(int iX, int iY, LPCWSTR pwszText, int iSize)const { ::TextOutW(m_hDC, iX, iY, pwszText, iSize); }
		void TextOutW(int iX, int iY, std::wstring_view wsv)const {
			TextOutW(iX, iY, wsv.data(), static_cast<int>(wsv.size()));
		}
	protected:
		HDC m_hDC;
	};

	class CPaintDC final : public CDC {
	public:
		CPaintDC(HWND hWnd) : m_hWnd(hWnd) { assert(::IsWindow(hWnd)); m_hDC = ::BeginPaint(m_hWnd, &m_PS); }
		~CPaintDC() { ::EndPaint(m_hWnd, &m_PS); }
	private:
		PAINTSTRUCT m_PS;
		HWND m_hWnd;
	};

	class CMemDC final : public CDC {
	public:
		CMemDC(HDC hDC, RECT rc) : m_hDCOrig(hDC), m_rc(rc) {
			m_hDC = ::CreateCompatibleDC(m_hDCOrig);
			assert(m_hDC != nullptr);
			const auto iWidth = m_rc.right - m_rc.left;
			const auto iHeight = m_rc.bottom - m_rc.top;
			m_hBmp = ::CreateCompatibleBitmap(m_hDCOrig, iWidth, iHeight);
			assert(m_hBmp != nullptr);
			::SelectObject(m_hDC, m_hBmp);
		}
		~CMemDC() {
			const auto iWidth = m_rc.right - m_rc.left;
			const auto iHeight = m_rc.bottom - m_rc.top;
			::BitBlt(m_hDCOrig, m_rc.left, m_rc.top, iWidth, iHeight, m_hDC, m_rc.left, m_rc.top, SRCCOPY);
			::DeleteObject(m_hBmp);
			::DeleteDC(m_hDC);
		}
	private:
		HDC m_hDCOrig;
		HBITMAP m_hBmp;
		RECT m_rc;
	};

	class CWnd {
	public:
		CWnd() = default;
		CWnd(HWND hWnd)noexcept : m_hWnd(hWnd) { }
		CWnd(const CWnd&) = default;
		~CWnd() = default;
		CWnd& operator=(const CWnd&) = default;
		CWnd& operator=(HWND rhs) noexcept { Attach(rhs); return *this; }
		operator HWND()const { return m_hWnd; }
		[[nodiscard]] bool operator==(CWnd rhs)const { return m_hWnd == rhs.m_hWnd; }
		[[nodiscard]] bool operator==(HWND rhs)const { return m_hWnd == rhs; }
		void Attach(HWND hWnd) { m_hWnd = hWnd; } //Can attach to nullptr as well.
		void CheckRadioButton(int iIDFirst, int iIDLast, int iIDCheck)const {
			assert(IsWindow()); ::CheckRadioButton(m_hWnd, iIDFirst, iIDLast, iIDCheck);
		}
		[[nodiscard]] auto ChildWindowFromPoint(POINT pt)const -> CWnd {
			assert(IsWindow()); return ::ChildWindowFromPoint(m_hWnd, pt);
		}
		void ClientToScreen(LPPOINT pPT)const { assert(IsWindow() && pPT != nullptr); ::ClientToScreen(m_hWnd, pPT); }
		void ClientToScreen(LPRECT pRC)const { MapWindowPoints(nullptr, pRC); }
		void DestroyWindow() { assert(IsWindow()); ::DestroyWindow(m_hWnd); m_hWnd = nullptr; }
		void Detach() { m_hWnd = nullptr; }
		[[nodiscard]] auto GetClientRect()const -> CRect {
			assert(IsWindow()); CRect rc; ::GetClientRect(m_hWnd, rc); return rc;
		}
		bool EnableWindow(bool fEnable)const { assert(IsWindow()); return ::EnableWindow(m_hWnd, fEnable) != FALSE; }
		void EndDialog(INT_PTR iResult)const { assert(IsWindow()); ::EndDialog(m_hWnd, iResult); }
		[[nodiscard]] int GetCheckedRadioButton(int iIDFirst, int iIDLast)const {
			assert(IsWindow()); for (int iID = iIDFirst; iID <= iIDLast; ++iID) {
				if (::IsDlgButtonChecked(m_hWnd, iID) != 0) { return iID; }
			} return 0;
		}
		[[nodiscard]] auto GetDC()const -> HDC { assert(IsWindow()); return ::GetDC(m_hWnd); }
		[[nodiscard]] int GetDlgCtrlID()const { assert(IsWindow()); return ::GetDlgCtrlID(m_hWnd); }
		[[nodiscard]] auto GetDlgItem(int iIDCtrl)const -> CWnd { assert(IsWindow()); return ::GetDlgItem(m_hWnd, iIDCtrl); }
		[[nodiscard]] auto GetHFont()const -> HFONT {
			assert(IsWindow()); return reinterpret_cast<HFONT>(SendMsg(WM_GETFONT, 0, 0));
		}
		[[nodiscard]] auto GetHWND()const -> HWND { return m_hWnd; }
		[[nodiscard]] auto GetLogFont() const -> std::optional<LOGFONTW> {
			if (const auto hFont = GetHFont(); hFont != nullptr) {
				LOGFONTW lf { }; if (::GetObjectW(hFont, sizeof(lf), &lf) == sizeof(lf)) { return lf; }
			}
			return std::nullopt;
		}
		[[nodiscard]] auto GetParent()const -> CWnd { assert(IsWindow()); return ::GetParent(m_hWnd); }
		[[nodiscard]] auto GetScrollInfo(bool fVert, UINT uMask = SIF_ALL)const -> SCROLLINFO {
			assert(IsWindow()); SCROLLINFO si { .cbSize { sizeof(SCROLLINFO) }, .fMask { uMask } };
			::GetScrollInfo(m_hWnd, fVert, &si); return si;
		}
		[[nodiscard]] auto GetScrollPos(bool fVert)const -> int { return GetScrollInfo(fVert, SIF_POS).nPos; }
		[[nodiscard]] auto GetWindowDC()const -> HDC { assert(IsWindow()); return ::GetWindowDC(m_hWnd); }
		[[nodiscard]] auto GetWindowLongPTR(int iIndex)const {
			assert(IsWindow()); return ::GetWindowLongPtrW(m_hWnd, iIndex);
		}
		[[nodiscard]] auto GetWindowRect()const -> CRect {
			assert(IsWindow()); CRect rc; ::GetWindowRect(m_hWnd, rc); return rc;
		}
		[[nodiscard]] auto GetWindowStyles()const { return static_cast<DWORD>(GetWindowLongPTR(GWL_STYLE)); }
		[[nodiscard]] auto GetWindowStylesEx()const { return static_cast<DWORD>(GetWindowLongPTR(GWL_EXSTYLE)); }
		[[nodiscard]] auto GetWndText() const -> std::wstring {
			assert(IsWindow()); const auto iLen = GetWndTextSize(); if (iLen <= 0) return { };
			std::wstring text(static_cast<size_t>(iLen) + 1, L'\0');
			const auto iCopied = ::GetWindowTextW(m_hWnd, text.data(), iLen + 1);
			text.resize(iCopied > 0 ? static_cast<size_t>(iCopied) : 0);
			return text;
		}
		[[nodiscard]] auto GetWndTextSize()const -> int { assert(IsWindow()); return ::GetWindowTextLengthW(m_hWnd); }
		void Invalidate(bool fErase)const { assert(IsWindow()); ::InvalidateRect(m_hWnd, nullptr, fErase); }
		[[nodiscard]] bool IsDlgMessage(MSG* pMsg)const {
			assert(IsWindow() && pMsg != nullptr); return ::IsDialogMessageW(m_hWnd, pMsg);
		}
		[[nodiscard]] bool IsNull()const { return m_hWnd == nullptr; }
		[[nodiscard]] bool IsWindow()const { return ::IsWindow(m_hWnd); }
		[[nodiscard]] bool IsWindowEnabled()const { assert(IsWindow()); return ::IsWindowEnabled(m_hWnd); }
		[[nodiscard]] bool IsWindowVisible()const { assert(IsWindow()); return ::IsWindowVisible(m_hWnd); }
		[[nodiscard]] bool IsWndTextEmpty()const { return GetWndTextSize() == 0; }
		void KillTimer(UINT_PTR uID)const {
			assert(IsWindow()); ::KillTimer(m_hWnd, uID);
		}
		int MapWindowPoints(HWND hWndTo, LPRECT pRC)const {
			assert(IsWindow() && pRC != nullptr); return ::MapWindowPoints(m_hWnd, hWndTo, reinterpret_cast<LPPOINT>(pRC), 2);
		}
		bool RedrawWindow(LPCRECT pRC = nullptr, HRGN hrgn = nullptr, UINT uFlags = RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE)const {
			assert(IsWindow()); return ::RedrawWindow(m_hWnd, pRC, hrgn, uFlags) != FALSE;
		}
		int ReleaseDC(HDC hDC)const { assert(IsWindow()); return ::ReleaseDC(m_hWnd, hDC); }
		void ScreenToClient(LPPOINT pPT)const { assert(IsWindow() && pPT != nullptr); ::ScreenToClient(m_hWnd, pPT); }
		void ScreenToClient(POINT& pt)const { ScreenToClient(&pt); }
		void ScreenToClient(LPRECT pRC)const {
			assert(pRC != nullptr);	assert(IsWindow()); ::MapWindowPoints(nullptr, m_hWnd, reinterpret_cast<LPPOINT>(pRC), 2);
		}
		auto SendMsg(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0)const -> LRESULT {
			assert(IsWindow()); return ::SendMessageW(m_hWnd, uMsg, wParam, lParam);
		}
		void SetActiveWindow()const { assert(IsWindow()); ::SetActiveWindow(m_hWnd); }
		auto SetCapture()const -> CWnd { assert(IsWindow()); return ::SetCapture(m_hWnd); }
		auto SetClassLongPTR(int iIndex, LONG_PTR dwNewLong)const -> ULONG_PTR {
			assert(IsWindow()); return ::SetClassLongPtrW(m_hWnd, iIndex, dwNewLong);
		}
		void SetFocus()const { assert(IsWindow()); ::SetFocus(m_hWnd); }
		void SetForegroundWindow()const { assert(IsWindow()); ::SetForegroundWindow(m_hWnd); }
		void SetScrollInfo(bool fVert, const SCROLLINFO& si)const {
			assert(IsWindow()); ::SetScrollInfo(m_hWnd, fVert, &si, TRUE);
		}
		void SetScrollPos(bool fVert, int iPos)const {
			const SCROLLINFO si { .cbSize { sizeof(SCROLLINFO) }, .fMask { SIF_POS }, .nPos { iPos } };
			SetScrollInfo(fVert, si);
		}
		auto SetTimer(UINT_PTR uID, UINT uElapse, TIMERPROC pFN = nullptr)const -> UINT_PTR {
			assert(IsWindow()); return ::SetTimer(m_hWnd, uID, uElapse, pFN);
		}
		void SetWindowPos(HWND hWndAfter, int iX, int iY, int iWidth, int iHeight, UINT uFlags)const {
			assert(IsWindow()); ::SetWindowPos(m_hWnd, hWndAfter, iX, iY, iWidth, iHeight, uFlags);
		}
		void SetWndText(LPCWSTR pwszStr)const { assert(IsWindow()); ::SetWindowTextW(m_hWnd, pwszStr); }
		void SetWndText(const std::wstring& wstr)const { SetWndText(wstr.data()); }
		void SetRedraw(bool fRedraw)const { assert(IsWindow()); SendMsg(WM_SETREDRAW, fRedraw, 0); }
		bool ShowWindow(int iCmdShow)const { assert(IsWindow()); return ::ShowWindow(m_hWnd, iCmdShow) != FALSE; }
		[[nodiscard]] static auto GetFocus() -> CWnd { return ::GetFocus(); }
	protected:
		HWND m_hWnd { }; //Windows window handle.
	};

	class CWndBtn final : public CWnd {
	public:
		[[nodiscard]] bool IsChecked()const { assert(IsWindow()); return SendMsg(BM_GETCHECK, 0, 0); }
		void SetBitmap(HBITMAP hBmp)const {
			assert(IsWindow()); SendMsg(BM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(hBmp));
		}
		void SetCheck(bool fCheck)const { assert(IsWindow()); SendMsg(BM_SETCHECK, fCheck, 0); }
	};

	class CWndEdit final : public CWnd {
	public:
		void SetCueBanner(LPCWSTR pwszText, bool fDrawIfFocus = false)const {
			assert(IsWindow()); SendMsg(EM_SETCUEBANNER, fDrawIfFocus, reinterpret_cast<LPARAM>(pwszText));
		}
		void SetLimitText(int iSize) { assert(IsWindow()); SendMsg(EM_LIMITTEXT, iSize); }
	};

	class CWndCombo final : public CWnd {
	public:
		int AddString(const std::wstring& wstr)const { return AddString(wstr.data()); }
		int AddString(LPCWSTR pwszStr)const {
			assert(IsWindow());
			return static_cast<int>(SendMsg(CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pwszStr)));
		}
		int DeleteString(int iIndex)const {
			assert(IsWindow()); return static_cast<int>(SendMsg(CB_DELETESTRING, iIndex, 0));
		}
		[[nodiscard]] int FindStringExact(int iIndex, LPCWSTR pwszStr)const {
			assert(IsWindow());
			return static_cast<int>(SendMsg(CB_FINDSTRINGEXACT, iIndex, reinterpret_cast<LPARAM>(pwszStr)));
		}
		[[nodiscard]] auto GetComboBoxInfo()const -> COMBOBOXINFO {
			assert(IsWindow());	COMBOBOXINFO cbi { .cbSize { sizeof(COMBOBOXINFO) } };
			::GetComboBoxInfo(GetHWND(), &cbi); return cbi;
		}
		[[nodiscard]] int GetCount()const {
			assert(IsWindow()); return static_cast<int>(SendMsg(CB_GETCOUNT, 0, 0));
		}
		[[nodiscard]] int GetCurSel()const {
			assert(IsWindow()); return static_cast<int>(SendMsg(CB_GETCURSEL, 0, 0));
		}
		[[nodiscard]] auto GetItemData(int iIndex)const -> DWORD_PTR {
			assert(IsWindow()); return SendMsg(CB_GETITEMDATA, iIndex, 0);
		}
		[[nodiscard]] bool HasString(LPCWSTR pwszStr)const { return FindStringExact(-1, pwszStr) != CB_ERR; };
		[[nodiscard]] bool HasString(const std::wstring& wstr)const { return HasString(wstr.data()); };
		int InsertString(int iIndex, const std::wstring& wstr)const { return InsertString(iIndex, wstr.data()); }
		int InsertString(int iIndex, LPCWSTR pwszStr)const {
			assert(IsWindow());
			return static_cast<int>(SendMsg(CB_INSERTSTRING, iIndex, reinterpret_cast<LPARAM>(pwszStr)));
		}
		void LimitText(int iMaxChars)const { assert(IsWindow()); SendMsg(CB_LIMITTEXT, iMaxChars, 0); }
		void ResetContent()const { assert(IsWindow()); SendMsg(CB_RESETCONTENT, 0, 0); }
		void SetCueBanner(const std::wstring& wstr)const { SetCueBanner(wstr.data()); }
		void SetCueBanner(LPCWSTR pwszText)const {
			assert(IsWindow()); SendMsg(CB_SETCUEBANNER, 0, reinterpret_cast<LPARAM>(pwszText));
		}
		void SetCurSel(int iIndex)const { assert(IsWindow()); SendMsg(CB_SETCURSEL, iIndex, 0); }
		void SetItemData(int iIndex, DWORD_PTR dwData)const {
			assert(IsWindow()); SendMsg(CB_SETITEMDATA, iIndex, static_cast<LPARAM>(dwData));
		}
	};

	class CWndTree final : public CWnd {
	public:
		void DeleteAllItems()const { DeleteItem(TVI_ROOT); };
		void DeleteItem(HTREEITEM hItem)const {
			assert(IsWindow()); SendMsg(TVM_DELETEITEM, 0, reinterpret_cast<LPARAM>(hItem));
		}
		void Expand(HTREEITEM hItem, UINT uCode)const {
			assert(IsWindow()); SendMsg(TVM_EXPAND, uCode, reinterpret_cast<LPARAM>(hItem));
		}
		void GetItem(TVITEMW* pItem)const {
			assert(IsWindow()); SendMsg(TVM_GETITEMW, 0, reinterpret_cast<LPARAM>(pItem));
		}
		[[nodiscard]] auto GetItemData(HTREEITEM hItem)const -> DWORD_PTR {
			TVITEMW item { .mask { TVIF_PARAM }, .hItem { hItem } }; GetItem(&item); return item.lParam;
		}
		[[nodiscard]] auto GetNextItem(HTREEITEM hItem, UINT uCode)const -> HTREEITEM {
			assert(IsWindow()); return reinterpret_cast<HTREEITEM>
				(SendMsg(TVM_GETNEXTITEM, uCode, reinterpret_cast<LPARAM>(hItem)));
		}
		[[nodiscard]] auto GetNextSiblingItem(HTREEITEM hItem)const -> HTREEITEM { return GetNextItem(hItem, TVGN_NEXT); }
		[[nodiscard]] auto GetParentItem(HTREEITEM hItem)const -> HTREEITEM { return GetNextItem(hItem, TVGN_PARENT); }
		[[nodiscard]] auto GetRootItem()const -> HTREEITEM { return GetNextItem(nullptr, TVGN_ROOT); }
		[[nodiscard]] auto GetSelectedItem()const -> HTREEITEM { return GetNextItem(nullptr, TVGN_CARET); }
		[[nodiscard]] auto HitTest(TVHITTESTINFO* pHTI)const -> HTREEITEM {
			assert(IsWindow());
			return reinterpret_cast<HTREEITEM>(SendMsg(TVM_HITTEST, 0, reinterpret_cast<LPARAM>(pHTI)));
		}
		[[nodiscard]] auto HitTest(POINT pt, UINT* pFlags = nullptr)const -> HTREEITEM {
			TVHITTESTINFO hti { .pt { pt } }; const auto ret = HitTest(&hti);
			if (pFlags != nullptr) { *pFlags = hti.flags; }
			return ret;
		}
		auto InsertItem(LPTVINSERTSTRUCTW pTIS)const -> HTREEITEM {
			assert(IsWindow()); return reinterpret_cast<HTREEITEM>
				(SendMsg(TVM_INSERTITEM, 0, reinterpret_cast<LPARAM>(pTIS)));
		}
		void SelectItem(HTREEITEM hItem)const {
			assert(IsWindow()); SendMsg(TVM_SELECTITEM, TVGN_CARET, reinterpret_cast<LPARAM>(hItem));
		}
		bool SetItem(const TVITEMW* pItem)const {
			assert(IsWindow());
			return SendMsg(TVM_SETITEMW, 0, reinterpret_cast<LPARAM>(pItem));
		}
		bool SetItemState(HTREEITEM hItem, UINT uState, UINT uStateMask) {
			assert(IsWindow());
			const TVITEMW tvi { .mask { TVIF_STATE }, .hItem { hItem }, .state { uState }, .stateMask { uStateMask } };
			return SetItem(&tvi);
		}
	};

	class CWndProgBar final : public CWnd {
	public:
		int SetPos(int iPos)const {
			assert(IsWindow()); return static_cast<int>(SendMsg(PBM_SETPOS, iPos, 0UL));
		}
		void SetRange(int iLower, int iUpper)const {
			assert(IsWindow());
			SendMsg(PBM_SETRANGE32, static_cast<WPARAM>(iLower), static_cast<LPARAM>(iUpper));
		}
	};

	class CWndTab final : public CWnd {
	public:
		[[nodiscard]] int GetCurSel()const {
			assert(IsWindow()); return static_cast<int>(SendMsg(TCM_GETCURSEL, 0, 0L));
		}
		[[nodiscard]] auto GetItemRect(int iItem)const -> CRect {
			assert(IsWindow());
			CRect rc; SendMsg(TCM_GETITEMRECT, iItem, reinterpret_cast<LPARAM>(&rc));
			return rc;
		}
		auto InsertItem(int iItem, TCITEMW* pItem)const -> LONG {
			assert(IsWindow());
			return static_cast<LONG>(SendMsg(TCM_INSERTITEMW, iItem, reinterpret_cast<LPARAM>(pItem)));
		}
		auto InsertItem(int iItem, LPCWSTR pwszStr)const -> LONG {
			TCITEMW tci { .mask { TCIF_TEXT }, .pszText { const_cast<LPWSTR>(pwszStr) } };
			return InsertItem(iItem, &tci);
		}
		int SetCurSel(int iItem)const {
			assert(IsWindow()); return static_cast<int>(SendMsg(TCM_SETCURSEL, iItem, 0L));
		}
	};

	class CMenu {
	public:
		CMenu() = default;
		CMenu(HMENU hMenu) { Attach(hMenu); }
		~CMenu() = default;
		CMenu operator=(const CWnd&) = delete;
		CMenu operator=(HMENU) = delete;
		operator HMENU()const { return m_hMenu; }
		void AppendItem(UINT uFlags, UINT_PTR uIDItem, LPCWSTR pNameItem)const {
			assert(IsMenu()); ::AppendMenuW(m_hMenu, uFlags, uIDItem, pNameItem);
		}
		void AppendSepar()const { AppendItem(MF_SEPARATOR, 0, nullptr); }
		void AppendString(UINT_PTR uIDItem, LPCWSTR pNameItem)const { AppendItem(MF_STRING, uIDItem, pNameItem); }
		void Attach(HMENU hMenu) { m_hMenu = hMenu; }
		void CreatePopupMenu() { Attach(::CreatePopupMenu()); }
		void DestroyMenu() { assert(IsMenu()); ::DestroyMenu(m_hMenu); m_hMenu = nullptr; }
		void Detach() { m_hMenu = nullptr; }
		void EnableItem(UINT uIDItem, bool fEnable, bool fByID = true)const {
			assert(IsMenu()); ::EnableMenuItem(m_hMenu, uIDItem, (fEnable ? MF_ENABLED : MF_GRAYED) |
				(fByID ? MF_BYCOMMAND : MF_BYPOSITION));
		}
		[[nodiscard]] auto GetHMENU()const -> HMENU { return m_hMenu; }
		[[nodiscard]] auto GetItemBitmap(UINT uID, bool fByID = true)const -> HBITMAP {
			return GetItemInfo(uID, MIIM_BITMAP, fByID).hbmpItem;
		}
		[[nodiscard]] auto GetItemBitmapCheck(UINT uID, bool fByID = true)const -> HBITMAP {
			return GetItemInfo(uID, MIIM_CHECKMARKS, fByID).hbmpChecked;
		}
		[[nodiscard]] auto GetItemID(int iPos)const -> UINT {
			assert(IsMenu()); return ::GetMenuItemID(m_hMenu, iPos);
		}
		bool GetItemInfo(UINT uID, LPMENUITEMINFOW pMII, bool fByID = true)const {
			assert(IsMenu()); return ::GetMenuItemInfoW(m_hMenu, uID, !fByID, pMII) != FALSE;
		}
		[[nodiscard]] auto GetItemInfo(UINT uID, UINT uMask, bool fByID = true)const -> MENUITEMINFOW {
			MENUITEMINFOW mii { .cbSize { sizeof(MENUITEMINFOW) }, .fMask { uMask } };
			GetItemInfo(uID, &mii, fByID); return mii;
		}
		[[nodiscard]] auto GetItemState(UINT uID, bool fByID = true)const -> UINT {
			assert(IsMenu()); return ::GetMenuState(m_hMenu, uID, fByID ? MF_BYCOMMAND : MF_BYPOSITION);
		}
		[[nodiscard]] auto GetItemType(UINT uID, bool fByID = true)const -> UINT {
			return GetItemInfo(uID, MIIM_FTYPE, fByID).fType;
		}
		[[nodiscard]] auto GetItemWstr(UINT uID, bool fByID = true)const -> std::wstring {
			wchar_t buff[128]; MENUITEMINFOW mii { .cbSize { sizeof(MENUITEMINFOW) }, .fMask { MIIM_STRING },
				.dwTypeData { buff }, .cch { 128 } }; return GetItemInfo(uID, &mii, fByID) ? buff : std::wstring { };
		}
		[[nodiscard]] int GetItemsCount()const {
			assert(IsMenu()); return ::GetMenuItemCount(m_hMenu);
		}
		[[nodiscard]] auto GetSubMenu(int iPos)const -> CMenu { assert(IsMenu()); return ::GetSubMenu(m_hMenu, iPos); };
		[[nodiscard]] bool IsItemChecked(UINT uIDItem, bool fByID = true)const {
			return GetItemState(uIDItem, fByID) & MF_CHECKED;
		}
		[[nodiscard]] bool IsItemSepar(UINT uPos)const { return GetItemState(uPos, false) & MF_SEPARATOR; }
		[[nodiscard]] bool IsMenu()const { return ::IsMenu(m_hMenu); }
		bool LoadMenuW(HINSTANCE hInst, LPCWSTR pwszName) { m_hMenu = ::LoadMenuW(hInst, pwszName); return IsMenu(); }
		bool LoadMenuW(HINSTANCE hInst, UINT uMenuID) { return LoadMenuW(hInst, MAKEINTRESOURCEW(uMenuID)); }
		void SetItemBitmap(UINT uItem, HBITMAP hBmp, bool fByID = true)const {
			const MENUITEMINFOW mii { .cbSize { sizeof(MENUITEMINFOW) }, .fMask { MIIM_BITMAP }, .hbmpItem { hBmp } };
			SetItemInfo(uItem, &mii, fByID);
		}
		void SetItemBitmapCheck(UINT uItem, HBITMAP hBmp, bool fByID = true)const {
			::SetMenuItemBitmaps(m_hMenu, uItem, fByID ? MF_BYCOMMAND : MF_BYPOSITION, nullptr, hBmp);
		}
		void SetItemCheck(UINT uIDItem, bool fCheck, bool fByID = true)const {
			assert(IsMenu()); ::CheckMenuItem(m_hMenu, uIDItem, (fCheck ? MF_CHECKED : MF_UNCHECKED) |
				(fByID ? MF_BYCOMMAND : MF_BYPOSITION));
		}
		void SetItemData(UINT uItem, ULONG_PTR dwData, bool fByID = true)const {
			const MENUITEMINFOW mii { .cbSize { sizeof(MENUITEMINFOW) }, .fMask { MIIM_DATA }, .dwItemData { dwData } };
			SetItemInfo(uItem, &mii, fByID);
		}
		void SetItemInfo(UINT uItem, LPCMENUITEMINFOW pMII, bool fByID = true)const {
			assert(IsMenu()); ::SetMenuItemInfoW(m_hMenu, uItem, !fByID, pMII);
		}
		void SetItemInfo(UINT uItem, const MENUITEMINFOW& mii, bool fByID = true)const {
			SetItemInfo(uItem, &mii, fByID);
		}
		void SetItemType(UINT uItem, UINT uType, bool fByID = true)const {
			const MENUITEMINFOW mii { .cbSize { sizeof(MENUITEMINFOW) }, .fMask { MIIM_FTYPE }, .fType { uType } };
			SetItemInfo(uItem, &mii, fByID);
		}
		void SetItemWstr(UINT uItem, const std::wstring& wstr, bool fByID = true)const {
			const MENUITEMINFOW mii { .cbSize { sizeof(MENUITEMINFOW) }, .fMask { MIIM_STRING },
				.dwTypeData { const_cast<LPWSTR>(wstr.data()) } };
			SetItemInfo(uItem, &mii, fByID);
		}
		BOOL TrackPopupMenu(int iX, int iY, HWND hWndOwner, UINT uFlags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON)const {
			assert(IsMenu()); return ::TrackPopupMenuEx(m_hMenu, uFlags, iX, iY, hWndOwner, nullptr);
		}
	private:
		HMENU m_hMenu { }; //Windows menu handle.
	};

	struct MENUCOLORS {
		COLORREF clrBk { RGB(249, 249, 249) };      //Menu background color.
		COLORREF clrBkSel { RGB(240, 240, 240) };   //Selected menu item background color.
		COLORREF clrText { RGB(0, 0, 0) };          //Menu text color.
		COLORREF clrTextSel { RGB(0, 0, 0) };       //Selected menu item text color.
		COLORREF clrTextDis { RGB(109, 109, 109) }; //Disabled menu item text color.
		COLORREF clrSepar { RGB(215, 215, 215) };   //Menu separator color.
	};

	class CMenuColor final : public CMenu {
	public:
		CMenuColor(HMENU hMenu) : CMenu(hMenu) { };
		CMenuColor(HMENU hMenu, const MENUCOLORS& clrs) : CMenuColor(hMenu) { m_clrs = clrs; };
		[[nodiscard]] auto ProcessMsg(const MSG& msg) -> LRESULT;
		void SetColors(const MENUCOLORS& clrs);
		BOOL TrackPopupMenu(int iX, int iY, HWND hWndOwner, UINT uFlags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON);
	private:
		struct STRINGINFO {
			std::wstring_view wsvBeforeTab;
			std::wstring_view wsvAfterTab;
			std::uint16_t u16SizeBeforeTabPx { };
			std::uint16_t u16SizeAfterTabPx { };
		};
		[[nodiscard]] auto GetStringInfo(HDC hDC, std::wstring_view wsv)const -> STRINGINFO;
		void MakeOwnerDraw(HMENU hMenu);
		auto OnDrawItem(const MSG& msg) -> LRESULT;
		auto OnMeasureItem(const MSG& msg) -> LRESULT;
	private:
		struct ITEMINFO { //Owner draw menu item information.
			HMENU hMenu { };
			HBITMAP hBmp { };
			UINT uItemID { };
			WORD wItemPos { };
		};
		std::vector<std::unique_ptr<ITEMINFO>> m_vecItemInfo;
		MENUCOLORS m_clrs;
		HWND m_hWndOwner { };
		HFONT m_hFont { };
		HPEN m_hPen { };
		POINT m_ptIcon { }; //Icon left/top coordinates.
		std::uint16_t m_u16SizeIcon { };
		std::uint16_t m_u16SizeTab { };
		std::uint16_t m_u16SizeBeforeStr { };
		std::uint16_t m_u16SizeAfterStr { };
		std::uint16_t m_u16SizeOffsetSepar { };
		std::uint16_t m_u16SizeHeightSepar { };
		std::uint16_t m_u16SizeHeightItem { };
	};

	auto CMenuColor::ProcessMsg(const MSG& msg)->LRESULT
	{
		switch (msg.message) {
		case WM_COMMAND:
		case WM_ENTERMENULOOP:
		case WM_INITMENU:
		case WM_INITMENUPOPUP:
		case WM_MENUCHAR:
		case WM_MENUCOMMAND:
		case WM_MENUSELECT:
		case WM_UNINITMENUPOPUP:
			::SendMessageW(m_hWndOwner, msg.message, msg.wParam, msg.lParam);
			return 0;
		case WM_DRAWITEM: return OnDrawItem(msg);
		case WM_MEASUREITEM: return OnMeasureItem(msg);
		default: return DefWndProc(msg);
		}
	}

	void CMenuColor::SetColors(const MENUCOLORS& clrs)
	{
		m_clrs = clrs;
	}

	BOOL CMenuColor::TrackPopupMenu(int iX, int iY, HWND hWndOwner, UINT uFlags)
	{
		constexpr auto pwszClassName { L"MenuColorWndMsg" };
		WNDCLASSEXW wc { };
		if (::GetClassInfoExW(nullptr, pwszClassName, &wc) == FALSE) {
			wc.cbSize = sizeof(WNDCLASSEXW);
			wc.style = CS_GLOBALCLASS;
			wc.lpfnWndProc = WndProc<CMenuColor>;
			wc.lpszClassName = pwszClassName;
			if (::RegisterClassExW(&wc) == 0) {
				assert(false);
				return FALSE;
			}
		}

		const auto hWndMsg = ::CreateWindowExW(0, pwszClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, this);
		if (hWndMsg == nullptr) {
			assert(false);
			return FALSE;
		}

		m_hWndOwner = hWndOwner;
		NONCLIENTMETRICSW ncm { .cbSize { sizeof(NONCLIENTMETRICSW) } };
		::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0); //Get System Default UI Font.
		m_hFont = ::CreateFontIndirectW(&ncm.lfMenuFont);
		m_hPen = ::CreatePen(PS_SOLID, 1, m_clrs.clrSepar);

		const auto hDC = ::GetDC(hWndOwner);
		const auto flDPIScale = static_cast<float>(::GetDeviceCaps(hDC, LOGPIXELSY)) / USER_DEFAULT_SCREEN_DPI;
		TEXTMETRICW tm;
		::SelectObject(hDC, m_hFont);
		::GetTextMetricsW(hDC, &tm);
		const SIZE sizeFont { .cx { tm.tmAveCharWidth }, .cy { tm.tmHeight + tm.tmExternalLeading } };
		SIZE sizeTab { };
		::GetTextExtentPoint32W(hDC, L"        ", 8, &sizeTab); //One tab ('\t') is replaced with 8 spaces.
		::ReleaseDC(hWndOwner, hDC);
		m_ptIcon.x = static_cast<int>(4 * flDPIScale);
		m_ptIcon.y = static_cast<int>(4 * flDPIScale);
		m_u16SizeTab = static_cast<std::uint16_t>(sizeTab.cx);
		m_u16SizeIcon = static_cast<std::uint16_t>(16 * flDPIScale);
		m_u16SizeBeforeStr = static_cast<std::uint16_t>(m_ptIcon.x + m_u16SizeIcon + m_ptIcon.x + (sizeFont.cx * 2));
		m_u16SizeAfterStr = static_cast<std::uint16_t>(sizeFont.cx * 2);
		m_u16SizeOffsetSepar = static_cast<std::uint16_t>(4 * flDPIScale);
		m_u16SizeHeightSepar = static_cast<std::uint16_t>(9 * flDPIScale);
		m_u16SizeHeightItem = static_cast<std::uint16_t>(sizeFont.cy + (7 * flDPIScale));

		const auto hBrushBk = ::CreateSolidBrush(m_clrs.clrBk);
		const MENUINFO mi { .cbSize { sizeof(MENUINFO) }, .fMask { MIM_BACKGROUND | MIM_APPLYTOSUBMENUS },
			.hbrBack { hBrushBk } };
		::SetMenuInfo(GetHMENU(), &mi);
		m_vecItemInfo.clear();
		MakeOwnerDraw(GetHMENU());

		const auto ret = CMenu::TrackPopupMenu(iX, iY, hWndMsg, uFlags | TPM_RETURNCMD);
		if ((uFlags & TPM_RETURNCMD) == 0 && (uFlags & TPM_NONOTIFY) == 0) {
			::SendMessageW(hWndOwner, WM_COMMAND, MAKEWPARAM(ret, 0), 0);
		}

		::DeleteObject(hBrushBk);
		::DeleteObject(m_hFont);
		::DeleteObject(m_hPen);
		::DestroyWindow(hWndMsg);
		::UnregisterClassW(pwszClassName, nullptr);

		for (const auto& vec : m_vecItemInfo) {
			if (vec->hBmp != nullptr) {
				CMenu(vec->hMenu).SetItemBitmap(vec->wItemPos, vec->hBmp, false); //Restore original bitmap.
			}
		}

		return ret;
	}

	//Private methods.

	auto CMenuColor::GetStringInfo(HDC hDC, std::wstring_view wsv)const->STRINGINFO
	{
		STRINGINFO si;
		const auto sTabPos = wsv.find_first_of(L'\t');
		if (sTabPos != std::wstring_view::npos) {
			si.wsvBeforeTab = wsv.substr(0, sTabPos);
			si.wsvAfterTab = wsv.substr(sTabPos + 1);
		}
		else { si.wsvBeforeTab = wsv; }

		SIZE sz;
		::SelectObject(hDC, m_hFont);
		::GetTextExtentPoint32W(hDC, si.wsvBeforeTab.data(), static_cast<int>(si.wsvBeforeTab.size()), &sz);
		si.u16SizeBeforeTabPx = static_cast<std::int16_t>(sz.cx);
		::GetTextExtentPoint32W(hDC, si.wsvAfterTab.data(), static_cast<int>(si.wsvAfterTab.size()), &sz);
		si.u16SizeAfterTabPx = static_cast<std::int16_t>(sz.cx);

		return si;
	}

	void CMenuColor::MakeOwnerDraw(HMENU hMenu)
	{
		assert(hMenu != nullptr);
		CMenu menu(hMenu);
		for (auto i = 0; i < menu.GetItemsCount(); ++i) {
			MENUITEMINFOW mii { .cbSize { sizeof(MENUITEMINFOW) }, .fMask { MIIM_FTYPE | MIIM_ID | MIIM_DATA | MIIM_BITMAP } };
			menu.GetItemInfo(i, &mii, false);
			const auto& uptr = m_vecItemInfo.emplace_back(std::make_unique<ITEMINFO>(ITEMINFO {
				.hMenu { menu }, .hBmp { mii.hbmpItem }, .uItemID { mii.wID }, .wItemPos { static_cast<WORD>(i) } }));
			mii.fType |= MF_OWNERDRAW;
			//When hbmpItem is not null or HBMMENU_CALLBACK, the WM_MEASUREITEM message would not be sent for that item,
			//even if the MF_OWNERDRAW flag is set (another Windows idiocy).
			mii.hbmpItem = HBMMENU_CALLBACK;
			mii.dwItemData = reinterpret_cast<ULONG_PTR>(uptr.get());
			menu.SetItemInfo(i, &mii, false);
			if (auto menuSub(menu.GetSubMenu(i)); menuSub.IsMenu()) {
				MakeOwnerDraw(menuSub);
			}
		}
	}

	auto CMenuColor::OnDrawItem(const MSG& msg)->LRESULT
	{
		const auto pDIS = reinterpret_cast<LPDRAWITEMSTRUCT>(msg.lParam);
		const auto pII = reinterpret_cast<ITEMINFO*>(pDIS->itemData);
		const auto uPos = pII->wItemPos;
		const CMenu menu(pII->hMenu);
		const CDC dc(pDIS->hDC);
		const CRect rc(pDIS->rcItem);
		const auto fIsSelected = pDIS->itemState & ODS_SELECTED;
		const auto fIsDisabled = pDIS->itemState & ODS_DISABLED;
		const auto fIsGrayed = pDIS->itemState & ODS_GRAYED;
		const auto fIsChecked = pDIS->itemState & ODS_CHECKED;
		const auto fIsSepar = menu.IsItemSepar(uPos);

		COLORREF clrBk;
		COLORREF clrTxt;
		if (fIsDisabled || fIsGrayed) {
			clrBk = fIsSelected ? m_clrs.clrBkSel : m_clrs.clrBk;
			clrTxt = m_clrs.clrTextDis;
		}
		else if (fIsSelected) {
			clrBk = m_clrs.clrBkSel;
			clrTxt = m_clrs.clrTextSel;
		}
		else { //Default state.
			clrBk = m_clrs.clrBk;
			clrTxt = m_clrs.clrText;
		}

		dc.FillSolidRect(rc, clrBk); //Fill background.

		if (fIsSepar) { //Draw separator item.
			dc.SelectObject(m_hPen);
			dc.MoveTo(rc.left + m_u16SizeOffsetSepar, rc.top + (rc.Height() / 2));
			dc.LineTo(rc.right - m_u16SizeOffsetSepar, rc.top + (rc.Height() / 2));
		}
		else { //Draw item text.
			dc.SelectObject(m_hFont);
			dc.SetTextColor(clrTxt);
			dc.SetBkColor(clrBk);
			CRect rcText = rc;
			rcText.DeflateRect(m_u16SizeBeforeStr, 0, m_u16SizeAfterStr, 0);
			const auto wstr = menu.GetItemWstr(uPos, false);
			const auto si = GetStringInfo(dc, wstr);
			dc.DrawTextW(si.wsvBeforeTab, rcText, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
			if (si.u16SizeAfterTabPx > 0) {
				dc.DrawTextW(si.wsvAfterTab, rcText, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
			}
		}

		if (fIsChecked) { //Draw item's own "check" bitmap, if exists.
			if (const auto hBmpChecked = menu.GetItemBitmapCheck(uPos, false); hBmpChecked != nullptr) {
				dc.DrawImage(hBmpChecked, rc.left + m_ptIcon.x, rc.top + m_ptIcon.y, m_u16SizeIcon, m_u16SizeIcon);
			}
			else { //Draw default Windows "check" bitmap.
				const auto dcMem = dc.CreateCompatibleDC();
				const auto bmpMH = ::CreateBitmap(m_u16SizeIcon, m_u16SizeIcon, 1, 1, nullptr); //Monohrome bitmap.
				const auto uState = menu.GetItemState(uPos, false) & MFT_RADIOCHECK ? DFCS_MENUBULLET : DFCS_MENUCHECK;
				dcMem.SelectObject(bmpMH);
				dcMem.DrawFrameControl(0, 0, m_u16SizeIcon, m_u16SizeIcon, DFC_MENU, uState);
				dc.SetTextColor(clrTxt);
				dc.SetBkColor(clrBk);
				dc.BitBlt(rc.left + m_ptIcon.x, rc.top + m_ptIcon.y, m_u16SizeIcon, m_u16SizeIcon, dcMem, 0, 0, SRCCOPY);
				::DeleteObject(bmpMH);
				dcMem.DeleteDC();
			}
		}
		else if (pII->hBmp != nullptr) { //Draw item bitmap, if exists.
			dc.DrawImage(pII->hBmp, rc.left + m_ptIcon.x, rc.top + m_ptIcon.y, m_u16SizeIcon, m_u16SizeIcon);
		}

		return TRUE;
	}

	auto CMenuColor::OnMeasureItem(const MSG& msg)->LRESULT
	{
		const auto pMIS = reinterpret_cast<LPMEASUREITEMSTRUCT>(msg.lParam);
		const auto pII = reinterpret_cast<ITEMINFO*>(pMIS->itemData);
		const CMenu menu(pII->hMenu);
		const auto uPos = pII->wItemPos;

		const auto hDC = ::GetDC(m_hWndOwner);
		std::uint16_t u16SizeBeforeTab { 0 };
		std::uint16_t u16SizeTabAndAfterTab { 0 };

		//Check if any menu item string contains tab character.
		//The string size must be: max(SizeBeforeTab) + SizeTab + max(SizeAfterTab).
		//Since all menu rects are the same width when drawn, it's safe to set the same (max) width to all of them.
		for (auto i = 0; i < menu.GetItemsCount(); ++i) {
			const auto si = GetStringInfo(hDC, menu.GetItemWstr(i, false));
			u16SizeBeforeTab = (std::max)(u16SizeBeforeTab, si.u16SizeBeforeTabPx);
			u16SizeTabAndAfterTab = (std::max)(u16SizeTabAndAfterTab, si.u16SizeAfterTabPx);
		}
		u16SizeTabAndAfterTab += u16SizeTabAndAfterTab > 0 ? m_u16SizeTab : 0;
		::ReleaseDC(m_hWndOwner, hDC);

		pMIS->itemWidth = m_u16SizeBeforeStr + u16SizeBeforeTab + u16SizeTabAndAfterTab + m_u16SizeAfterStr;
		pMIS->itemHeight = menu.IsItemSepar(uPos) ? m_u16SizeHeightSepar : m_u16SizeHeightItem;

		return TRUE;
	}


	[[nodiscard]] auto GetDPIScaleForHWND(HWND hWnd) -> float {
		return static_cast<float>(::GetDpiForWindow(hWnd)) / USER_DEFAULT_SCREEN_DPI; //High-DPI scale factor for window.
	}

	//Get GDI font size in points from the size in pixels.
	[[nodiscard]] auto FontPointsFromPixels(long iSizePixels) -> float {
		constexpr auto flPointsInPixel = 72.F / USER_DEFAULT_SCREEN_DPI;
		return std::abs(iSizePixels) * flPointsInPixel;
	}

	//Get GDI font size in pixels from the size in points.
	[[nodiscard]] auto FontPixelsFromPoints(float flSizePoints) -> long {
		constexpr auto flPixelsInPoint = USER_DEFAULT_SCREEN_DPI / 72.F;
		return std::lround(flSizePoints * flPixelsInPoint);
	}

	//iDirection: 1:UP, -1:DOWN, -2:LEFT, 2:RIGHT.
	[[nodiscard]] auto CreateArrowBitmap(HDC hDC, DWORD dwWidth, DWORD dwHeight, int iDirection, COLORREF clrBk, COLORREF clrArrow) -> HBITMAP {
		//Make the width and height even numbers, to make it easier drawing an isosceles triangle.
		const int iWidth = dwWidth - (dwWidth % 2);
		const int iHeight = dwHeight - (dwHeight % 2);

		POINT ptArrow[3] { }; //Arrow coordinates within the bitmap.
		switch (iDirection) {
		case -2: //LEFT.
			ptArrow[0] = { .x { iWidth / 4 }, .y { iHeight / 2 } };
			ptArrow[1] = { .x { iWidth / 2 }, .y { iHeight / 4 } };
			ptArrow[2] = { .x { iWidth / 2 }, .y { iHeight - (iHeight / 4) } };
			break;
		case 1: //UP.
			ptArrow[0] = { .x { iWidth / 2 }, .y { iHeight / 4 } };
			ptArrow[1] = { .x { iWidth / 4 }, .y { iHeight / 2 } };
			ptArrow[2] = { .x { iWidth - (iWidth / 4) }, .y { iHeight / 2 } };
			break;
		case 2: //RIGHT.
			ptArrow[0] = { .x { iWidth / 2 }, .y { iHeight / 4 } };
			ptArrow[1] = { .x { iWidth / 2 }, .y { iHeight - (iHeight / 4) } };
			ptArrow[2] = { .x { iWidth - (iWidth / 4) }, .y { iHeight / 2 } };
			break;
		case -1: //DOWN.
			ptArrow[0] = { .x { iWidth / 4 }, .y { iHeight / 2 } };
			ptArrow[1] = { .x { iWidth - (iWidth / 4) }, .y { iHeight / 2 } };
			ptArrow[2] = { .x { iWidth / 2 }, .y { iHeight - (iHeight / 4) } };
			break;
		default:
			break;
		}

		const auto hdcMem = ::CreateCompatibleDC(hDC);
		const auto hBMPArrow = ::CreateCompatibleBitmap(hDC, dwWidth, dwHeight);
		::SelectObject(hdcMem, hBMPArrow);
		const RECT rcFill { .right { static_cast<int>(dwWidth) }, .bottom { static_cast<int>(dwHeight) } };
		::SetBkColor(hdcMem, clrBk); //SetBkColor+ExtTextOutW=FillSolidRect behavior.
		::ExtTextOutW(hdcMem, 0, 0, ETO_OPAQUE, &rcFill, nullptr, 0, nullptr);
		const auto hBrushArrow = ::CreateSolidBrush(clrArrow);
		::SelectObject(hdcMem, hBrushArrow);
		const auto hPenArrow = ::CreatePen(PS_SOLID, 1, clrArrow);
		::SelectObject(hdcMem, hPenArrow);
		::Polygon(hdcMem, ptArrow, 3); //Draw an arrow.
		::DeleteObject(hBrushArrow);
		::DeleteObject(hPenArrow);
		::DeleteDC(hdcMem);

		return hBMPArrow;
	}

	//Converts HICON to a 32bit ARGB HBITMAP with premultiplied alpha.
	[[nodiscard]] auto HBITMAPFromHICON(HICON hIcon) -> HBITMAP {
		assert(hIcon != nullptr);

		ICONINFO ii;
		if (::GetIconInfo(hIcon, &ii) == FALSE) {
			return { };
		}

		::DeleteObject(ii.hbmMask); //Deleting icon's BW mask bitmap.
		BITMAP bmIconClr;
		::GetObjectW(ii.hbmColor, sizeof(BITMAP), &bmIconClr);
		const auto iBmpHeight = std::abs(bmIconClr.bmHeight);
		const auto iBmpWidth = std::abs(bmIconClr.bmWidth);
		BITMAPINFO bi { .bmiHeader { .biSize { sizeof(BITMAPINFOHEADER) }, .biWidth { iBmpWidth },
			.biHeight { iBmpHeight }, .biPlanes { 1 }, .biBitCount { 32 }, .biCompression { BI_RGB } } };
		RGBQUAD* pARGB { }; //DIBitmap's data pointer.
		const auto hDCMem = ::CreateCompatibleDC(nullptr);
		const auto hBmp32 = ::CreateDIBSection(hDCMem, &bi, DIB_RGB_COLORS, reinterpret_cast<void**>(&pARGB), nullptr, 0);
		::GetDIBits(hDCMem, ii.hbmColor, 0, iBmpHeight, pARGB, &bi, DIB_RGB_COLORS);
		::DeleteDC(hDCMem);
		::DeleteObject(ii.hbmColor);

		if (hBmp32 == nullptr || pARGB == nullptr)
			return { };

		const auto dwSizePx = static_cast<DWORD>(iBmpHeight * iBmpWidth);
		if (bmIconClr.bmBitsPixel == 32) { //Premultiply alpha for all channels.
			for (auto i = 0U; i < dwSizePx; ++i) {
				pARGB[i].rgbBlue = static_cast<BYTE>(static_cast<DWORD>(pARGB[i].rgbBlue) * pARGB[i].rgbReserved / 255);
				pARGB[i].rgbGreen = static_cast<BYTE>(static_cast<DWORD>(pARGB[i].rgbGreen) * pARGB[i].rgbReserved / 255);
				pARGB[i].rgbRed = static_cast<BYTE>(static_cast<DWORD>(pARGB[i].rgbRed) * pARGB[i].rgbReserved / 255);
			}
		}
		else { //If the icon is not 32bit, merely set bitmap alpha channel to fully opaque.
			for (auto i = 0U; i < dwSizePx; ++i) {
				pARGB[i].rgbReserved = 255;
			}
		}

		return hBmp32;
	}

	//Converts HBITMAP to a HICON.
	[[nodiscard]] auto HICONFromHBITMAP(HBITMAP hBmp) -> HICON {
		assert(hBmp != nullptr);

		BITMAP bmp;
		if (::GetObjectW(hBmp, sizeof(BITMAP), &bmp) == 0) {
			return { };
		}

		const auto hDCScreen = ::GetDC(nullptr);
		const auto hbmMask = ::CreateCompatibleBitmap(hDCScreen, bmp.bmWidth, bmp.bmHeight);
		ICONINFO ii { .fIcon { TRUE }, .hbmMask { hbmMask }, .hbmColor { hBmp } };
		const auto hIcon = ::CreateIconIndirect(&ii);
		::DeleteObject(hbmMask);
		::ReleaseDC(nullptr, hDCScreen);

		return hIcon;
	}

	[[nodiscard]] auto SVGToHBITMAP(IStream* pStream, int iWidth, int iHeight, ID2D1Factory* pD2DFactory = nullptr) -> HBITMAP {
		//The "height" and "width" svg root attributes <svg ...height="30" width="30"...> must be removed from the svg file,
		//to scale image correctly with the Direct2D. Otherwise, ID2D1SvgDocument will use these attributes for scaling,
		//not its own viewport size.

		if (pD2DFactory == nullptr) {
			static const DXUT::comptr pD2DFactory1 = []() {
				ID2D1Factory1* pFactory1;
				::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&pFactory1));
				assert(pFactory1 != nullptr);
				return pFactory1;
				}();
			pD2DFactory = pD2DFactory1;
		}

		const auto d2d1RTP = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0, 0, D2D1_RENDER_TARGET_USAGE_NONE,
			D2D1_FEATURE_LEVEL_DEFAULT);
		DXUT::comptr<ID2D1DCRenderTarget> pDCRT;
		pD2DFactory->CreateDCRenderTarget(&d2d1RTP, pDCRT);
		assert(pDCRT != nullptr);
		if (pDCRT == nullptr)
			return { };

		const std::unique_ptr < std::remove_pointer_t<HDC>, decltype([](HDC hDC) { ::DeleteDC(hDC); }) > pHDC {
			::CreateCompatibleDC(nullptr) };
		assert(pHDC != nullptr);
		if (pHDC == nullptr)
			return { };

		const BITMAPINFO bi { .bmiHeader { .biSize { sizeof(BITMAPINFOHEADER) },
			.biWidth { iWidth }, .biHeight { iHeight }, .biPlanes { 1 }, .biBitCount { 32 }, .biCompression { BI_RGB } } };
		void* pBitsBmp;
		const auto hBmp = ::CreateDIBSection(pHDC.get(), &bi, DIB_RGB_COLORS, &pBitsBmp, nullptr, 0);
		assert(hBmp != nullptr);
		if (hBmp == nullptr)
			return { };

		::SelectObject(pHDC.get(), hBmp);
		const RECT rc { .right { iWidth }, .bottom { iHeight } };
		pDCRT->BindDC(pHDC.get(), &rc);

		DXUT::comptr<ID2D1DeviceContext5> pD2DDC5; //Windows 10 1703.
		pDCRT->QueryInterface(IID_PPV_ARGS(pD2DDC5.get_addr()));
		assert(pD2DDC5 != nullptr);
		if (pD2DDC5 == nullptr)
			return { };

		DXUT::comptr<ID2D1SvgDocument> pSVGDoc; //Windows 10 1703.
		pD2DDC5->CreateSvgDocument(pStream, { .width { static_cast<float>(iWidth) },
			.height { static_cast<float>(iHeight) } }, pSVGDoc);
		assert(pSVGDoc != nullptr);
		if (pSVGDoc == nullptr)
			return { };

		pDCRT->BeginDraw();
		pD2DDC5->DrawSvgDocument(pSVGDoc);
		pDCRT->EndDraw();

		return hBmp;
	}

	[[nodiscard]] auto SVGToHBITMAP(UINT uIDRes, int iWidth, int iHeight, HINSTANCE hInstRes = nullptr,
		LPCWSTR pwszTypeRes = L"SVG", ID2D1Factory* pD2DFactory = nullptr) -> HBITMAP {
		const auto hRCSVG = ::FindResourceW(hInstRes, MAKEINTRESOURCEW(uIDRes), pwszTypeRes);
		assert(hRCSVG != nullptr);
		if (hRCSVG == nullptr)
			return { };

		const auto hResData = ::LoadResource(hInstRes, hRCSVG);
		assert(hResData != nullptr);
		if (hResData == nullptr)
			return { };

		const auto pResData = static_cast<const BYTE*>(::LockResource(hResData));
		assert(pResData != nullptr);
		if (pResData == nullptr)
			return { };

		const auto dwSizeRes = ::SizeofResource(hInstRes, hRCSVG);
		const DXUT::comptr<IStream> pStream = ::SHCreateMemStream(pResData, dwSizeRes);
		assert(pStream != nullptr);
		if (pStream == nullptr)
			return { };

		return SVGToHBITMAP(pStream, iWidth, iHeight, pD2DFactory);
	}

	/*********************************************************************************************
	* @brief OPENFILEDLG struct for the OpenFileDlg function.
	* @param spnFilter Array of COMDLG_FILTERSPEC structs, specifying file extension filters.
	* @param dwFlags Combination of the _FILEOPENDIALOGOPTIONS enum flags.
	* @param hWndParent Parent window handle.
	* @param pfnOnBeforeDlg Callback, that is called before the dialog is shown.
	* @param uzDataBeforeDlg User defined data to pass to the pfnOnBeforeDlg callback.
	* @param pfnOnAfterDlg Callback, that is called after the dialog is shown.
	* @param uzDataAfterDlg User defined data to pass to the pfnOnAfterDlg callback.
	* @returns std::optional of std:vector<std::wstring>, or std::nullopt if Cancel was pressed.
	*********************************************************************************************/
	struct OPENFILEDLG {
		std::span<const COMDLG_FILTERSPEC> spnFilter; //Array of file extension filters.
		DWORD dwFlags { FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_DONTADDTORECENT };
		HWND hWndParent { }; //Parent window handle.
		void(*pfnOnBeforeDlg)(IFileOpenDialog*, uintptr_t) { }; //Callback, before dialog is shown.
		std::uintptr_t uzDataBeforeDlg { }; //User defined data to pass to the pfnOnBeforeDlg callback.
		void(*pfnOnAfterDlg)(IFileOpenDialog*, uintptr_t) { };  //Callback, after "Open" button is clicked.
		std::uintptr_t uzDataAfterDlg { }; //User defined data to pass to the pfnOnAfterDlg callback.
	};

	/*********************************************************************************************
	* @brief Open file(s) dialog.
	* @returns std::optional of std:vector<std::wstring>, or std::nullopt if Cancel was pressed.
	*********************************************************************************************/
	[[nodiscard]] auto OpenFileDlg(const OPENFILEDLG& ofd) -> std::optional<std::vector<std::wstring>> {
		IFileOpenDialog *pFOD;
		if (::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog,
			reinterpret_cast<void**>(&pFOD)) != S_OK) {
			return std::nullopt;
		}

		const std::unique_ptr < IFileOpenDialog, decltype([](IFileOpenDialog* pFOD) { pFOD->Release(); }) > upFOD(pFOD);
		DWORD dwFlagsCurr { };
		upFOD->GetOptions(&dwFlagsCurr);
		upFOD->SetOptions(dwFlagsCurr | ofd.dwFlags);
		upFOD->SetFileTypes(static_cast<UINT>(ofd.spnFilter.size()), ofd.spnFilter.data());

		if (ofd.pfnOnBeforeDlg != nullptr) {
			ofd.pfnOnBeforeDlg(upFOD.get(), ofd.uzDataBeforeDlg);
		}

		if (upFOD->Show(ofd.hWndParent) != S_OK) { //Cancel was pressed.
			return std::nullopt;
		}

		if (ofd.pfnOnAfterDlg != nullptr) {
			ofd.pfnOnAfterDlg(upFOD.get(), ofd.uzDataAfterDlg);
		}

		IShellItemArray* pShIA;
		if (upFOD->GetResults(&pShIA) != S_OK) {
			return std::nullopt;
		}

		const std::unique_ptr < IShellItemArray, decltype([](IShellItemArray* pShIA) { pShIA->Release(); }) > upShIA(pShIA);
		DWORD dwCount { };
		upShIA->GetCount(&dwCount);
		if (dwCount == 0) {
			return std::nullopt;
		}

		std::vector<std::wstring> vecRet;
		vecRet.reserve(dwCount);
		for (auto iFileIdx { 0U }; iFileIdx < dwCount; ++iFileIdx) {
			IShellItem* pShI;
			if (upShIA->GetItemAt(iFileIdx, &pShI) != S_OK) {
				continue;
			}

			const std::unique_ptr < IShellItem, decltype([](IShellItem* pShI) { pShI->Release(); }) > upShI(pShI);
			wchar_t* pwszPath { };
			if (upShI->GetDisplayName(SIGDN_FILESYSPATH, &pwszPath) != S_OK) {
				continue;
			}

			vecRet.emplace_back(pwszPath);
			::CoTaskMemFree(pwszPath);
		}

		return { std::move(vecRet) };
	}
}