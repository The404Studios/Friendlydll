#include "d3d9_hook.hpp"
#include "core/fonts.hpp"
#include "core/entity_esp.hpp"
#include "core/hud.hpp"
#include "core/stealth.hpp"
#include "core/lua_scripts.hpp"
#include "core/config_io.hpp"
#include "core/net_panel.hpp"
#include "core/sound_esp.hpp"
#include "core/misc_features.hpp"
#include "core/movement.hpp"
#include "core/prediction.hpp"
#include "core/death_replay.hpp"
#include "core/heatmap.hpp"
#include "core/killfeed.hpp"
#include "core/antiaim.hpp"
#include "core/freecam.hpp"
#include "core/fakelag.hpp"
#include "core/aim_lines.hpp"
#include "core/damage_log.hpp"
#include "core/door_memory.hpp"
#include "core/spawn_detect.hpp"
#include "core/xray.hpp"
#include "core/waypoints.hpp"
#include "core/follow_bot.hpp"
#include "core/bot_combat.hpp"
#include "core/bot_tasks.hpp"
#include "core/bot_visuals.hpp"
#include "core/rage_mode.hpp"
#include "core/printer_monitor.hpp"
#include "core/threat_radar.hpp"
#include "core/player_profiler.hpp"
#include "core/auto_disguise.hpp"
#include "core/tick_exploits.hpp"
#include "core/killstreak.hpp"
#include "core/voice_exploits.hpp"
#include "core/menu_anim.hpp"
#include "core/ui_theme.hpp"
#include "core/ui_anim.hpp"
#include "core/ui_widgets.hpp"
#include "core/ui_window.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool init = false;

TextEditor editor;

// ── Menu animation state ─────────────────────────────────────────────────────
static float  menuAlpha        = 0.0f;   // 0=fully hidden, 1=fully visible
static int    activeTab        = 0;
static float  tabUnderlineX    = 0.0f;
static bool   tabUnderlineInit = false;

// ── Enhanced animation state ────────────────────────────────────────────────
static menu_anim::ParticleSystem  headerParticles;
static menu_anim::TabTransition   tabTransition;
static float  menuSlideY       = 20.f;   // slide-in offset
static bool   lastMenuOpened   = false;
static int    lastSoundTab     = 0;
static float  sectionPulse     = 0.f;    // pulse on toggle
static float  toggleFlashTimer = 0.f;

namespace menu_sound {
    inline float lastPlay = 0.f;
    inline void Play(const char* snd) {
        if (!interfaces::engine) return;
        float now = (float)ImGui::GetTime();
        if (now - lastPlay < 0.04f) return;
        lastPlay = now;
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "play %s", snd);
        interfaces::engine->ClientCmd(cmd);
    }
    inline void MenuOpen()  { Play("buttons/button14.wav"); }
    inline void MenuClose() { Play("buttons/button15.wav"); }
    inline void TabSwitch() { Play("buttons/button24.wav"); }
    inline void ToggleOn()  { Play("buttons/button17.wav"); }
    inline void ToggleOff() { Play("buttons/button18.wav"); }
    inline void Click()     { Play("buttons/button9.wav"); }
}

static WNDPROC ogWndProc = NULL;
static HWND window = NULL;

using _Present = long(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
_Present ogPresent = nullptr;

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (d3d9hook::g_shuttingDown.load(std::memory_order_acquire))
		return CallWindowProcA(ogWndProc, hWnd, uMsg, wParam, lParam);

	static bool lastState = false;
	if (wParam == VK_INSERT)
	{
		if (uMsg == WM_KEYDOWN && lastState == false) {
			lastState = true;
			d3d9hook::menuOpened = !d3d9hook::menuOpened;
			ui_window::Toggle();
		}
		else if (uMsg == WM_KEYUP)
		{
			lastState = false;
		}
	}

	if (d3d9hook::menuOpened) {
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

		// Block mouse clicks from reaching the game (but let WM_MOUSEMOVE through)
		switch (uMsg) {
			case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
			case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL:
			case WM_XBUTTONDOWN: case WM_XBUTTONUP:
				return TRUE;
		}
		if (ImGui::GetIO().WantCaptureKeyboard)
			return TRUE;
	}

	return CallWindowProcA(ogWndProc, hWnd, uMsg, wParam, lParam);
}

long __stdcall detouredPresent(IDirect3DDevice9* pDevice, const RECT* rect1, const RECT* rect2, HWND hwnd, const RGNDATA* rgndata) {
	if (d3d9hook::g_shuttingDown.load(std::memory_order_acquire))
		return ogPresent(pDevice, rect1, rect2, hwnd, rgndata);

	if (!init) {
		spdlog::default_logger()->info("Successfully hooked Direct3D device: {}", (void*)pDevice);

		d3d9hook::renderer = pDevice;

		ImGui::CreateContext();
		spdlog::default_logger()->info("Created ImGui context!");

		ui_theme::ApplyTheme();
		ui_window::LoadState("friendlydll_window.cfg");

		ImGuiStyle& style = ImGui::GetStyle();

		// ── Sizing & rounding ────────────────────────────────────────────────
		style.Alpha                  = 1.0f;
		style.DisabledAlpha          = 0.55f;
		style.WindowPadding          = ImVec2(10.0f, 10.0f);
		style.WindowRounding         = 10.0f;
		style.WindowBorderSize       = 1.0f;
		style.WindowMinSize          = ImVec2(32.0f, 32.0f);
		style.WindowTitleAlign       = ImVec2(0.5f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_None;
		style.ChildRounding          = 6.0f;
		style.ChildBorderSize        = 1.0f;
		style.PopupRounding          = 6.0f;
		style.PopupBorderSize        = 1.0f;
		style.FramePadding           = ImVec2(6.0f, 4.0f);
		style.FrameRounding          = 4.0f;
		style.FrameBorderSize        = 1.0f;
		style.ItemSpacing            = ImVec2(8.0f, 5.0f);
		style.ItemInnerSpacing       = ImVec2(4.0f, 4.0f);
		style.CellPadding            = ImVec2(4.0f, 2.0f);
		style.IndentSpacing          = 20.0f;
		style.ColumnsMinSpacing      = 6.0f;
		style.ScrollbarRounding      = 18.0f;
		style.GrabMinSize            = 10.0f;
		style.GrabRounding           = 4.0f;
		style.TabRounding            = 4.0f;
		style.TabBorderSize          = 0.0f;
		style.ColorButtonPosition    = ImGuiDir_Right;
		style.ButtonTextAlign        = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign    = ImVec2(0.0f, 0.0f);

		// ── Color palette ────────────────────────────────────────────────────
		// Window bg:   #0D0D0F  (0.051, 0.051, 0.059)
		// Child bg:    #141418  (0.078, 0.078, 0.094)
		// Border:      #1E1E24  (0.118, 0.118, 0.141)
		// Frame bg:    #141418
		// Frame hover: #1A1A22  (0.102, 0.102, 0.133)
		// Frame active:#222230  (0.133, 0.133, 0.188)
		// Accent:      #00B4D8  (0.000, 0.706, 0.847)
		// Accent dark: #0090B0
		style.Colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled]         = ImVec4(0.38f, 0.38f, 0.42f, 1.00f);
		style.Colors[ImGuiCol_WindowBg]             = ImVec4(0.051f, 0.051f, 0.059f, 1.00f);
		style.Colors[ImGuiCol_ChildBg]              = ImVec4(0.078f, 0.078f, 0.094f, 1.00f);
		style.Colors[ImGuiCol_PopupBg]              = ImVec4(0.078f, 0.078f, 0.094f, 1.00f);
		style.Colors[ImGuiCol_Border]               = ImVec4(0.118f, 0.118f, 0.141f, 1.00f);
		style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg]              = ImVec4(0.078f, 0.078f, 0.094f, 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.102f, 0.102f, 0.133f, 1.00f);
		style.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.133f, 0.133f, 0.188f, 1.00f);
		style.Colors[ImGuiCol_TitleBg]              = ImVec4(0.051f, 0.051f, 0.059f, 1.00f);
		style.Colors[ImGuiCol_TitleBgActive]        = ImVec4(0.051f, 0.051f, 0.059f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.051f, 0.051f, 0.059f, 1.00f);
		style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.078f, 0.078f, 0.094f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.051f, 0.051f, 0.059f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.118f, 0.118f, 0.141f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.160f, 0.160f, 0.196f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.000f, 0.706f, 0.847f, 1.00f);
		style.Colors[ImGuiCol_CheckMark]            = ImVec4(0.000f, 0.706f, 0.847f, 1.00f);
		style.Colors[ImGuiCol_SliderGrab]           = ImVec4(0.000f, 0.706f, 0.847f, 1.00f);
		style.Colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.000f, 0.565f, 0.690f, 1.00f);
		style.Colors[ImGuiCol_Button]               = ImVec4(0.078f, 0.078f, 0.094f, 1.00f);
		style.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.102f, 0.102f, 0.133f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.000f, 0.706f, 0.847f, 0.80f);
		style.Colors[ImGuiCol_Header]               = ImVec4(0.102f, 0.102f, 0.133f, 1.00f);
		style.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.133f, 0.133f, 0.188f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.000f, 0.706f, 0.847f, 0.40f);
		style.Colors[ImGuiCol_Separator]            = ImVec4(0.118f, 0.118f, 0.141f, 1.00f);
		style.Colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.160f, 0.160f, 0.196f, 1.00f);
		style.Colors[ImGuiCol_SeparatorActive]      = ImVec4(0.000f, 0.706f, 0.847f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip]           = ImVec4(0.118f, 0.118f, 0.141f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.000f, 0.706f, 0.847f, 0.60f);
		style.Colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.000f, 0.706f, 0.847f, 1.00f);
		style.Colors[ImGuiCol_Tab]                  = ImVec4(0.078f, 0.078f, 0.094f, 1.00f);
		style.Colors[ImGuiCol_TabHovered]           = ImVec4(0.102f, 0.102f, 0.133f, 1.00f);
		style.Colors[ImGuiCol_TabActive]            = ImVec4(0.000f, 0.706f, 0.847f, 1.00f);
		style.Colors[ImGuiCol_TabUnfocused]         = ImVec4(0.078f, 0.078f, 0.094f, 1.00f);
		style.Colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.102f, 0.102f, 0.133f, 1.00f);
		style.Colors[ImGuiCol_PlotLines]            = ImVec4(0.000f, 0.706f, 0.847f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered]     = ImVec4(0.000f, 0.565f, 0.690f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram]        = ImVec4(0.000f, 0.706f, 0.847f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.000f, 0.565f, 0.690f, 1.00f);
		style.Colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.078f, 0.078f, 0.094f, 1.00f);
		style.Colors[ImGuiCol_TableBorderStrong]    = ImVec4(0.118f, 0.118f, 0.141f, 1.00f);
		style.Colors[ImGuiCol_TableBorderLight]     = ImVec4(0.102f, 0.102f, 0.118f, 1.00f);
		style.Colors[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
		style.Colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.000f, 0.706f, 0.847f, 0.35f);
		style.Colors[ImGuiCol_DragDropTarget]       = ImVec4(0.000f, 0.706f, 0.847f, 0.90f);
		style.Colors[ImGuiCol_NavHighlight]         = ImVec4(0.000f, 0.706f, 0.847f, 1.00f);
		style.Colors[ImGuiCol_NavWindowingHighlight]= ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		style.Colors[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
		style.Colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.45f);

		ImGuiIO& io = ImGui::GetIO();
		io.DeltaTime = 10.0f / 60.0f;
		io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
		//defaultFont = io.Fonts->Fonts[0];

		// clear old fonts
		io.Fonts->Clear();

		// load your font as the only font = becomes default
		d3d9hook::defaultFont = io.Fonts->AddFontFromMemoryTTF(dataRoboto, sizeof(dataRoboto), 16.0f);
		d3d9hook::editorFont = io.Fonts->AddFontFromMemoryTTF(dataRobotoMono, sizeof(dataRobotoMono), 14.0f);

		ImGui_ImplWin32_Init(window);
		ImGui_ImplDX9_Init(pDevice);

		spdlog::default_logger()->info("Initialized ImGui Win32 and Direct3D implementations!");

		init = true;
	}

	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);
	pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, false);

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ui_anim::Tick();

	int viewIdx = config::g_viewReadIdx.load(std::memory_order_acquire);
	int width  = config::g_screenW[viewIdx];
	int height = config::g_screenH[viewIdx];
	if (width <= 0 || height <= 0) { width = 1920; height = 1080; }

	// ── Fade + slide animation ──────────────────────────────────────────────────
	{
		const float dt       = ImGui::GetIO().DeltaTime;
		const float lerpRate = 10.0f * dt;
		const float target   = d3d9hook::menuOpened ? 1.0f : 0.0f;
		menuAlpha += (target - menuAlpha) * lerpRate;
		if (menuAlpha < 0.005f) menuAlpha = 0.0f;
		if (menuAlpha > 0.995f) menuAlpha = 1.0f;

		// Slide-in offset (slides up from below)
		float slideTarget = d3d9hook::menuOpened ? 0.f : 20.f;
		menuSlideY += (slideTarget - menuSlideY) * lerpRate;

		// Sound triggers on open/close
		if (d3d9hook::menuOpened && !lastMenuOpened)
			menu_sound::MenuOpen();
		else if (!d3d9hook::menuOpened && lastMenuOpened)
			menu_sound::MenuClose();
		lastMenuOpened = d3d9hook::menuOpened;

		// Tab transition update
		tabTransition.Update(dt);
	}

	// ── Watermark — always rendered, visible only when menu is closed ─────────
	if (menuAlpha < 0.99f) {
		ImDrawList* wm_dl   = ImGui::GetBackgroundDrawList();
		const float wmAlpha = (1.0f - menuAlpha) * 0.55f;
		const float wmFontSz = d3d9hook::editorFont->LegacySize;
		const char* wmText  = "friendlydll";
		ImVec2 wmSz = d3d9hook::editorFont->CalcTextSizeA(wmFontSz, FLT_MAX, 0.f, wmText);
		float wmX = static_cast<float>(width)  - wmSz.x - 14.f;
		float wmY = static_cast<float>(height) - wmSz.y - 14.f;

		// Background pill
		float pillPad = 6.f;
		wm_dl->AddRectFilled(
			ImVec2(wmX - pillPad, wmY - pillPad * 0.5f),
			ImVec2(wmX + wmSz.x + pillPad, wmY + wmSz.y + pillPad * 0.5f),
			IM_COL32(8, 8, 12, (int)(wmAlpha * 180.f)), 8.f);
		wm_dl->AddRect(
			ImVec2(wmX - pillPad, wmY - pillPad * 0.5f),
			ImVec2(wmX + wmSz.x + pillPad, wmY + wmSz.y + pillPad * 0.5f),
			IM_COL32(0, 180, 216, (int)(wmAlpha * 60.f)), 8.f);

		// Animated color watermark text
		float wmTime = (float)ImGui::GetTime();
		float curWmX = wmX;
		for (int wi = 0; wmText[wi]; ++wi) {
			char wch[2] = { wmText[wi], 0 };
			float hue = 190.f + sinf(wmTime * 1.5f + wi * 0.5f) * 30.f;
			ImU32 wcol = menu_anim::HsvToRgb(hue, 0.7f, 1.f, wmAlpha);
			// shadow
			wm_dl->AddText(d3d9hook::editorFont, wmFontSz,
				ImVec2(curWmX + 1.f, wmY + 1.f),
				IM_COL32(0, 0, 0, (int)(wmAlpha * 200.f)), wch);
			wm_dl->AddText(d3d9hook::editorFont, wmFontSz,
				ImVec2(curWmX, wmY), wcol, wch);
			curWmX += d3d9hook::editorFont->CalcTextSizeA(wmFontSz, FLT_MAX, 0.f, wch).x;
		}
	}

	// ── Main menu window ─────────────────────────────────────────────────────
	if (ui_window::g_state.isOpen || ui_window::g_state.openAlpha > 0.01f) {
		// Window constants — width/height are now dynamic (from ui_window)
		constexpr float kHeaderH     = 52.f;   // gradient bar (3px) + title area
		constexpr float kGradBarH    = 3.f;
		constexpr float kTabBarH     = 34.f;
		constexpr int   kTabCount    = 10;

		// Accent colors
		const ImU32 kAccentCyan   = IM_COL32(  0, 180, 216, 255);
		const ImU32 kAccentPurple = IM_COL32(123,  47, 190, 255);
		const ImU32 kAccentSolid  = IM_COL32(  0, 180, 216, 255);
		const ImU32 kTabHoverBg   = IM_COL32( 26,  26,  34, 255);
		const ImU32 kTabActiveBg  = IM_COL32( 16,  16,  22, 255);
		const ImU32 kSeparator    = IM_COL32( 30,  30,  36, 255);

		const char* kTabNames[] = { "Combat", "Movement", "ESP", "HUD", "Stealth", "Players", "Network", "Exploits", "World", "Config" };

		if (!ui_window::BeginWindow()) goto skip_menu;

		{
		ImDrawList* dl   = ImGui::GetWindowDrawList();
		ImVec2      wPos = ImGui::GetWindowPos();
		ImVec2      wSize= ImGui::GetWindowSize();

		// Tab width is now dynamic based on actual window width
		const float kTabW = wSize.x / (float)kTabCount;

		// ── 1. Animated gradient accent bar (top 3px) ───────────────────────
		menu_anim::DrawAnimatedGradientBar(dl, wPos.x, wPos.y, wSize.x, kGradBarH);

		// Subtle outer glow on the window
		menu_anim::DrawGlowRect(dl, wPos, ImVec2(wPos.x + wSize.x, wPos.y + wSize.y),
			kAccentCyan, 10.f, 3.f);

		// ── 2. Title "FRIENDLYDLL" — animated color wave ────────────────────
		{
			const char* title    = "FRIENDLYDLL";
			float titleFontSz    = d3d9hook::defaultFont->LegacySize;
			ImVec2 titleSz       = d3d9hook::defaultFont->CalcTextSizeA(
				titleFontSz, FLT_MAX, 0.f, title);
			float titleX = wPos.x + (wSize.x - titleSz.x) * 0.5f;
			float titleY = wPos.y + kGradBarH + (kHeaderH - kGradBarH - titleSz.y) * 0.5f;

			// Breathing glow behind title
			float glowPulse = (sinf((float)ImGui::GetTime() * 2.f) + 1.f) * 0.5f;
			ImU32 glowCol = IM_COL32(0, 180, 216, (int)(20.f + glowPulse * 30.f));
			for (int gx = -3; gx <= 3; ++gx)
				for (int gy = -3; gy <= 3; ++gy)
					if (gx != 0 || gy != 0)
						dl->AddText(d3d9hook::defaultFont, titleFontSz,
							ImVec2(titleX + gx, titleY + gy), glowCol, title);

			// Per-character color wave
			menu_anim::DrawTextWave(dl, d3d9hook::defaultFont, titleFontSz,
				titleX, titleY, title, 2.5f);

			// Particle system in header
			headerParticles.Update(ImGui::GetIO().DeltaTime,
				wPos.x, wPos.y, wSize.x, kHeaderH);
			headerParticles.Draw(dl);
		}

		// ── 3. Separator below header ────────────────────────────────────────
		dl->AddLine(
			ImVec2(wPos.x,           wPos.y + kHeaderH),
			ImVec2(wPos.x + wSize.x, wPos.y + kHeaderH),
			kSeparator, 1.0f);

		// ── 4. Custom tab bar ────────────────────────────────────────────────
		float tabBarY     = wPos.y + kHeaderH;
		float tabBarBaseY = tabBarY;
		float tabBarEndY  = tabBarY + kTabBarH;

		// Animate underline X position toward the active tab
		float targetUlX = wPos.x + activeTab * kTabW;
		if (!tabUnderlineInit) {
			tabUnderlineX    = targetUlX;
			tabUnderlineInit = true;
		} else {
			const float dt = ImGui::GetIO().DeltaTime;
			tabUnderlineX  += (targetUlX - tabUnderlineX) * (12.0f * dt);
		}

		// Per-tab hover animation state
		static float tabHoverAnim[10] = {};

		for (int t = 0; t < kTabCount; ++t) {
			float tabX0 = wPos.x + t * kTabW;
			float tabX1 = tabX0 + kTabW;

			ImGui::SetCursorScreenPos(ImVec2(tabX0, tabBarBaseY));
			ImGui::PushID(t);
			bool hovered = false;
			bool clicked = ImGui::InvisibleButton("##tab", ImVec2(kTabW, kTabBarH));
			hovered = ImGui::IsItemHovered();
			ImGui::PopID();

			// Smooth hover animation
			float hoverTarget = hovered ? 1.f : 0.f;
			tabHoverAnim[t] += (hoverTarget - tabHoverAnim[t]) * (12.f * ImGui::GetIO().DeltaTime);

			if (clicked && t != activeTab) {
				activeTab = t;
				tabTransition.Trigger();
				ui_anim::g_tabAnim.SwitchTo(t);
				menu_sound::TabSwitch();
			}

			// Background: active gets a subtle gradient, hover gets animated brightness
			if (t == activeTab) {
				dl->AddRectFilledMultiColor(
					ImVec2(tabX0, tabBarBaseY), ImVec2(tabX1, tabBarEndY),
					IM_COL32(0, 180, 216, 15), IM_COL32(0, 180, 216, 15),
					IM_COL32(0, 180, 216, 35), IM_COL32(0, 180, 216, 35));
			} else if (tabHoverAnim[t] > 0.01f) {
				int hAlpha = (int)(tabHoverAnim[t] * 30.f);
				dl->AddRectFilled(ImVec2(tabX0, tabBarBaseY), ImVec2(tabX1, tabBarEndY),
					IM_COL32(255, 255, 255, hAlpha));
			}

			// Tab label — centered with animated color
			float lblFontSz  = d3d9hook::defaultFont->LegacySize;
			ImVec2 lblSz     = d3d9hook::defaultFont->CalcTextSizeA(
				lblFontSz, FLT_MAX, 0.f, kTabNames[t]);
			float lblX = tabX0 + (kTabW - lblSz.x) * 0.5f;
			float lblY = tabBarBaseY + (kTabBarH - lblSz.y) * 0.5f;

			ImU32 lblCol;
			if (t == activeTab) {
				float pulse = (sinf((float)ImGui::GetTime() * 2.f) + 1.f) * 0.5f;
				lblCol = menu_anim::LerpColor(IM_COL32(0, 180, 216, 255), IM_COL32(140, 200, 255, 255), pulse * 0.3f);
			} else {
				lblCol = menu_anim::LerpColor(IM_COL32(110, 115, 130, 255), IM_COL32(200, 205, 220, 255), tabHoverAnim[t]);
			}

			// Shadow
			dl->AddText(d3d9hook::defaultFont, lblFontSz, ImVec2(lblX + 1.f, lblY + 1.f),
				IM_COL32(0, 0, 0, 80), kTabNames[t]);
			dl->AddText(d3d9hook::defaultFont, lblFontSz, ImVec2(lblX, lblY), lblCol, kTabNames[t]);
		}

		// Animated underline bar with glow
		float ulGlow = (sinf((float)ImGui::GetTime() * 3.f) + 1.f) * 0.5f;
		ImU32 ulCol = menu_anim::LerpColor(kAccentSolid, IM_COL32(100, 220, 255, 255), ulGlow * 0.4f);
		dl->AddRectFilled(
			ImVec2(tabUnderlineX,          tabBarEndY - 2.f),
			ImVec2(tabUnderlineX + kTabW,  tabBarEndY),
			ulCol);
		// Underline glow
		dl->AddRectFilled(
			ImVec2(tabUnderlineX + 2.f,          tabBarEndY - 4.f),
			ImVec2(tabUnderlineX + kTabW - 2.f,  tabBarEndY + 1.f),
			IM_COL32(0, 180, 216, 30));

		// Separator below tab bar
		dl->AddLine(
			ImVec2(wPos.x,           tabBarEndY),
			ImVec2(wPos.x + wSize.x, tabBarEndY),
			kSeparator, 1.0f);

		// ── 5. Scanning line effect across content ──────────────────────────
		menu_anim::DrawScanLine(dl, wPos.x, wPos.y + kHeaderH + kTabBarH,
			wSize.x, wSize.y - kHeaderH - kTabBarH, 0.3f);

		// ── 6. Content area — push cursor below custom header + tab bar ─────
		float contentTopY = kHeaderH + kTabBarH + 6.f;

		// Tab transition: offset content during switch
		float tabOffY = tabTransition.OffsetY();
		ImGui::SetCursorPos(ImVec2(10.f, contentTopY + tabOffY));

		// Fade content alpha during tab transition
		float contentAlpha = tabTransition.Alpha();
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * contentAlpha);

		ImGui::BeginChild("##content_area", ImVec2(0.f, -28.f), false, ImGuiWindowFlags_NoScrollbar);

		// Checkbox wrapper that plays toggle sounds
		auto SndCheckbox = [](const char* label, bool* v) -> bool {
			bool prev = *v;
			bool changed = ImGui::Checkbox(label, v);
			if (changed) {
				if (*v) menu_sound::ToggleOn();
				else    menu_sound::ToggleOff();
			}
			return changed;
		};

		// Button wrapper with click sound
		auto SndButton = [](const char* label, ImVec2 sz = ImVec2(0,0)) -> bool {
			bool clicked = ImGui::Button(label, sz);
			if (clicked) menu_sound::Click();
			return clicked;
		};

		auto SndSmallButton = [](const char* label) -> bool {
			bool clicked = ImGui::SmallButton(label);
			if (clicked) menu_sound::Click();
			return clicked;
		};

		// ── Tab content ──────────────────────────────────────────────────────
		switch (activeTab)
		{
		// ── Combat ────────────────────────────────────────────────────────────
		case 0:
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Aimbot");
			ImGui::PopStyleColor();

			SndCheckbox("Aimbot", &config::aimbot);
			ImGui::SameLine();
			SndCheckbox("Silent Aim", &config::silent);
			SndCheckbox("Autoshoot", &config::autoshoot);
			SndCheckbox("Resolver", &config::resolver);
			SndCheckbox("Recoil Compensation", &config::rcs);
			SndCheckbox("Long Range Melee", &config::longRangeMelee);
			SndCheckbox("Priority Mode", &config::aim_priority);
			if (config::aim_priority) {
				ImGui::SliderFloat("FOV Weight", &config::aim_w_fov, 0.f, 2.f, "%.1f");
				ImGui::SliderFloat("Distance Weight", &config::aim_w_distance, 0.f, 2.f, "%.1f");
				ImGui::SliderFloat("Low HP Weight", &config::aim_w_health, 0.f, 2.f, "%.1f");
				ImGui::SliderFloat("Threat Weight", &config::aim_w_threat, 0.f, 2.f, "%.1f");
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Triggerbot");
			ImGui::PopStyleColor();

			SndCheckbox("Triggerbot", &config::triggerbot);
			{
				const char* tbKeyNames[] = { "Use Aim Key", "Mouse 4", "Mouse 5", "Left Alt", "Left Shift" };
				const int tbKeyCodes[]   = { 0, 0x05, 0x06, 0xA4, 0xA0 };
				int curTbKey = 0;
				for (int k = 0; k < 5; ++k)
					if (config::triggerbot_key == tbKeyCodes[k]) { curTbKey = k; break; }
				if (ImGui::BeginCombo("Trigger Key", tbKeyNames[curTbKey])) {
					for (int k = 0; k < 5; ++k)
						if (ImGui::Selectable(tbKeyNames[k], curTbKey == k))
							config::triggerbot_key = tbKeyCodes[k];
					ImGui::EndCombo();
				}
			}

			ImGui::SliderInt("Trigger Delay (ms)", &config::triggerbot_delay_ms, 0, 500);
			SndCheckbox("Head Only", &config::triggerbot_head_only);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Backtrack");
			ImGui::PopStyleColor();

			SndCheckbox("Backtrack", &config::backtrack);
			ImGui::SliderInt("Ticks", &config::backtrack_ticks, 1, 12);
			SndCheckbox("Visualize Ticks", &config::backtrack_visualize);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Targeting");
			ImGui::PopStyleColor();

			{
				const char* boneNames[] = { "Head", "Neck", "Spine1", "Spine2", "LeftHand", "RightHand", "Pelvis" };
				const int boneCodes[]   = { Bones::bone_head, Bones::bone_neck, Bones::bone_spine_1, Bones::bone_spine_2, Bones::bone_hand_l, Bones::bone_hand_r, Bones::bone_pelvis };
				int curBone = 0;
				for (int b = 0; b < 7; ++b)
					if (config::bone == boneCodes[b]) { curBone = b; break; }
				if (ImGui::BeginCombo("Bone", boneNames[curBone])) {
					for (int b = 0; b < 7; ++b)
						if (ImGui::Selectable(boneNames[b], curBone == b))
							config::bone = boneCodes[b];
					ImGui::EndCombo();
				}
			}

			ImGui::SliderFloat("Aimbot FOV", &config::aimbot_fov, 1.f, 180.f);

			{
				const char* keyNames[] = { "Always On", "Mouse 4", "Mouse 5", "Left Alt", "Left Shift", "C", "V", "X" };
				const int keyCodes[]   = { 0, 0x05, 0x06, 0xA4, 0xA0, 'C', 'V', 'X' };
				int curKey = 0;
				for (int k = 0; k < 8; ++k)
					if (config::aimkey == keyCodes[k]) { curKey = k; break; }
				if (ImGui::BeginCombo("Aim Key", keyNames[curKey])) {
					for (int k = 0; k < 8; ++k)
						if (ImGui::Selectable(keyNames[k], curKey == k))
							config::aimkey = keyCodes[k];
					ImGui::EndCombo();
				}
			}

			ImGui::SliderFloat("Smoothing", &config::aim_smooth, 0.f, 20.f, "%.1f");
			SndCheckbox("Target Lock", &config::aim_target_lock);
			ImGui::SliderInt("Lock Ticks", &config::aim_lock_ticks, 1, 30);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("View");
			ImGui::PopStyleColor();

			ImGui::SliderFloat("View FOV", &config::fov, 25.f, 160.f);
			SndCheckbox("View Punch Removal", &config::viewpunch_remove);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.4f, 1.f));
			ImGui::SeparatorText("Anti-Aim");
			ImGui::PopStyleColor();

			SndCheckbox("Anti-Aim", &antiaim::enabled);
			if (antiaim::enabled) {
				const char* yawModes[] = { "Jitter", "Spin", "Backward", "Random", "Desync", "Micro" };
				ImGui::Combo("Yaw Mode", &antiaim::yaw_mode, yawModes, 6);
				if (antiaim::yaw_mode == 0 || antiaim::yaw_mode == 5)
					ImGui::SliderFloat("Jitter Range", &antiaim::jitter_range, 30.f, 180.f, "%.0f");
				if (antiaim::yaw_mode == 1)
					ImGui::SliderFloat("Spin Speed", &antiaim::spin_speed, 1.f, 60.f, "%.0f");
				if (antiaim::yaw_mode == 4)
					ImGui::SliderFloat("Desync Offset", &antiaim::desync_offset, 20.f, 120.f, "%.0f");
				const char* pitchModes[] = { "None", "Down", "Up", "Jitter", "Zero" };
				ImGui::Combo("Pitch Mode", &antiaim::pitch_mode, pitchModes, 5);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.4f, 1.f));
			ImGui::SeparatorText("Fake Lag");
			ImGui::PopStyleColor();

			SndCheckbox("Fake Lag", &fakelag::enabled);
			if (fakelag::enabled) {
				const char* flModes[] = { "Static", "Adaptive", "On-Peek", "Random" };
				ImGui::Combo("FL Mode", &fakelag::mode, flModes, 4);
				if (fakelag::mode == 0)
					ImGui::SliderInt("Choke Ticks", &fakelag::choke_ticks, 1, 14);
				ImGui::SliderInt("Max Choke", &fakelag::max_choke, 2, 14);
				SndCheckbox("FL Visualize", &fakelag::visualize);
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Choking: %d", fakelag::g_chokedCount);
			}
			break;
		}

		// ── Movement ──────────────────────────────────────────────────────────
		case 1:
		{
			SndCheckbox("Bunny-Hop", &config::bunnyhop);
			ImGui::Separator();
			ImGui::Spacing();
			SndCheckbox("Auto Strafe", &config::autostrafe);

			const char* legit_strafe       = "Legit Strafing";
			const char* silent_strafe      = "Silent Strafing";
			const char* directional_strafe = "Directional Strafing";

			static int current_strafing_mode = 0;
			if (config::autostrafe_legit)
				current_strafing_mode = 0;
			else if (config::autostrafe_silent)
				current_strafing_mode = 1;
			else if (config::autostrafe_directional)
				current_strafing_mode = 2;

			const char* preview = "";
			switch (current_strafing_mode) {
			case 0: preview = legit_strafe;       break;
			case 1: preview = silent_strafe;      break;
			case 2: preview = directional_strafe; break;
			}

			if (ImGui::BeginCombo("Strafing Type", preview)) {
				if (ImGui::Selectable(legit_strafe, current_strafing_mode == 0)) {
					current_strafing_mode = 0;
					config::autostrafe_legit = true;
					config::autostrafe_silent = false;
					config::autostrafe_directional = false;
				}
				if (ImGui::Selectable(silent_strafe, current_strafing_mode == 1)) {
					current_strafing_mode = 1;
					config::autostrafe_legit = false;
					config::autostrafe_silent = true;
					config::autostrafe_directional = false;
				}
				if (ImGui::Selectable(directional_strafe, current_strafing_mode == 2)) {
					current_strafing_mode = 2;
					config::autostrafe_legit = false;
					config::autostrafe_silent = false;
					config::autostrafe_directional = true;
				}
				ImGui::EndCombo();
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Edge Jump");
			ImGui::PopStyleColor();

			SndCheckbox("Edge Jump", &movement::edge_jump);
			SndCheckbox("Edge Jump Duck", &movement::edge_jump_duck);
			SndCheckbox("Fast Stop", &movement::fast_stop);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Follow Bot");
			ImGui::PopStyleColor();

			SndCheckbox("Follow Bot", &follow_bot::enabled);
			if (follow_bot::enabled) {
				// ---- Mode selector ----
				ImGui::Text("Mode:");
				ImGui::SameLine();
				const char* modeNames[] = {"Idle","Follow","Guard","Patrol","Farm","Flee"};
				int modeIdx = (int)bot_tasks::currentMode;
				ImGui::PushItemWidth(120.f);
				if (ImGui::Combo("##botmode", &modeIdx, modeNames, 6)) {
					bot_tasks::SetMode((bot_tasks::BotMode)modeIdx);
				}
				ImGui::PopItemWidth();
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1.f), "(%s)", bot_tasks::GetModeName());

				// ---- Follow settings ----
				if (bot_tasks::currentMode == bot_tasks::BotMode::Follow || bot_tasks::currentMode == bot_tasks::BotMode::Idle) {
					ImGui::SliderFloat("Follow Distance", &follow_bot::follow_distance, 50.f, 500.f, "%.0f units");
					ImGui::SliderFloat("Stop Distance", &follow_bot::stop_distance, 30.f, 300.f, "%.0f units");
					ImGui::SliderFloat("Sprint Distance", &follow_bot::sprint_distance, 200.f, 1500.f, "%.0f units");
					ImGui::SliderFloat("Max Speed", &follow_bot::max_speed, 100.f, 1000.f, "%.0f");
					SndCheckbox("Auto Jump", &follow_bot::auto_jump);
					ImGui::SameLine();
					SndCheckbox("Auto Door", &follow_bot::auto_door);
					SndCheckbox("Silent Move", &follow_bot::silent_move);
					ImGui::SameLine();
					SndCheckbox("Mimic Mode", &follow_bot::mimic_mode);
				}

				// ---- Guard settings ----
				if (bot_tasks::currentMode == bot_tasks::BotMode::Guard) {
					ImGui::SliderFloat("Guard Radius", &bot_tasks::guardRadius, 100.f, 1000.f, "%.0f");
					ImGui::SliderFloat("Guard Speed", &bot_tasks::guardSpeed, 100.f, 500.f, "%.0f");
					if (SndSmallButton("Set Guard Here")) {
						C_BasePlayer* lp = interfaces::engine ? (C_BasePlayer*)interfaces::entityList->GetClientEntity(interfaces::engine->GetLocalPlayer()) : nullptr;
						if (lp) bot_tasks::SetGuardPos(lp->GetAbsOrigin());
					}
				}

				// ---- Patrol settings ----
				if (bot_tasks::currentMode == bot_tasks::BotMode::Patrol) {
					ImGui::Text("Waypoints: %d/16", bot_tasks::patrolCount);
					ImGui::SameLine();
					if (SndSmallButton("Add Here")) {
						C_BasePlayer* lp = interfaces::engine ? (C_BasePlayer*)interfaces::entityList->GetClientEntity(interfaces::engine->GetLocalPlayer()) : nullptr;
						if (lp) bot_tasks::AddPatrolPoint(lp->GetAbsOrigin());
					}
					ImGui::SameLine();
					if (SndSmallButton("Clear All")) bot_tasks::ClearPatrol();
					SndCheckbox("Loop", &bot_tasks::patrolLoop);
					ImGui::SameLine();
					SndCheckbox("Ping-Pong", &bot_tasks::patrolPingPong);
				}

				// ---- Farm settings ----
				if (bot_tasks::currentMode == bot_tasks::BotMode::Farm) {
					ImGui::SliderFloat("Farm Radius", &bot_tasks::farmRadius, 100.f, 1500.f, "%.0f");
					SndCheckbox("Money", &bot_tasks::farmMoney);
					ImGui::SameLine();
					SndCheckbox("Items", &bot_tasks::farmItems);
					ImGui::SameLine();
					SndCheckbox("Printers", &bot_tasks::farmPrinters);
					ImGui::Text("Targets found: %d", bot_tasks::farmTargetCount);
				}

				// ---- Combat AI ----
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.3f, 1.f));
				ImGui::SeparatorText("Combat AI");
				ImGui::PopStyleColor();
				SndCheckbox("Combat AI", &bot_combat::enabled);
				if (bot_combat::enabled) {
					SndCheckbox("Auto Shoot", &bot_combat::auto_shoot);
					ImGui::SameLine();
					SndCheckbox("Auto Retreat", &bot_combat::auto_retreat);
					ImGui::SliderFloat("Engage Distance", &bot_combat::g_engageCfg.engage_distance, 200.f, 2000.f, "%.0f");
					ImGui::SliderFloat("Retreat HP", &bot_combat::g_engageCfg.disengage_health, 5.f, 80.f, "%.0f");
					SndCheckbox("Fight Back Only", &bot_combat::g_engageCfg.fight_back_only);
					ImGui::SameLine();
					SndCheckbox("Protect Target", &bot_combat::g_engageCfg.protect_target);
					SndCheckbox("Headshots Only", &bot_combat::g_engageCfg.headshot_only);
					if (bot_combat::g_threatCount > 0)
						ImGui::TextColored(ImVec4(1.f,0.3f,0.3f,1.f), "Threats: %d", bot_combat::g_threatCount);
					if (bot_combat::g_health.underFire)
						ImGui::TextColored(ImVec4(1.f,0.f,0.f,1.f), "!! UNDER FIRE !!");
				}

				// ---- Visuals ----
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.9f, 1.f, 1.f));
				ImGui::SeparatorText("Bot Visuals");
				ImGui::PopStyleColor();
				SndCheckbox("Status HUD", &bot_visuals::drawHUD);
				ImGui::SameLine();
				SndCheckbox("Path Lines", &bot_visuals::drawPath);
				SndCheckbox("Threats", &bot_visuals::drawThreats);
				ImGui::SameLine();
				SndCheckbox("Minimap", &bot_visuals::drawMinimap);
				SndCheckbox("Draw Path (simple)", &follow_bot::draw_path);

				// ---- Nav debug ----
				ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1.f), "Nav nodes: %d/%d", bot_nav::g_nodeCount, bot_nav::MAX_NODES);

				// ---- Target selector ----
				ImGui::Spacing();
				ImGui::Text("Target:");
				ImGui::SameLine();
				if (follow_bot::targetIdx >= 0 && follow_bot::targetIdx < 128) {
					auto& tb = config::BoneRead()[follow_bot::targetIdx];
					if (tb.valid) {
						const char* tname = tb.rpName[0] ? tb.rpName : (tb.name[0] ? tb.name : "???");
						ImGui::TextColored(ImVec4(0.f, 0.85f, 1.f, 1.f), "%s", tname);
						ImGui::SameLine();
						if (SndSmallButton("Clear")) { follow_bot::Reset(); bot_nav::Reset(); bot_combat::Reset(); }
					} else {
						ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Invalid");
						follow_bot::Reset();
					}
				} else {
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "None (select below)");
				}

				auto& allBones = config::BoneRead();
				int localIdx = interfaces::engine ? interfaces::engine->GetLocalPlayer() : -1;
				ImGui::BeginChild("##follow_players", ImVec2(0, 150), true);
				for (int i = 1; i < 128; i++) {
					if (i == localIdx) continue;
					if (!allBones[i].valid || allBones[i].dormant) continue;
					const char* pname = allBones[i].rpName[0] ? allBones[i].rpName : (allBones[i].name[0] ? allBones[i].name : nullptr);
					if (!pname) continue;

					bool selected = (follow_bot::targetIdx == i);
					char label[96];
					snprintf(label, sizeof(label), "%s [%.0fm]", pname, allBones[i].distance / 52.49f);
					if (ImGui::Selectable(label, selected)) {
						follow_bot::targetIdx = i;
						follow_bot::g_crumbHead = 0;
						follow_bot::g_crumbCount = 0;
						bot_tasks::SetMode(bot_tasks::BotMode::Follow);
					}
				}
				ImGui::EndChild();
			}
			break;
		}

		// ── ESP ───────────────────────────────────────────────────────────────
		case 2:
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Players");
			ImGui::PopStyleColor();

			SndCheckbox("Snapline", &config::snapline);
			ImGui::SameLine();
			ImGui::ColorEdit3("##snapcol", config::snapline_color, ImGuiColorEditFlags_NoInputs);

			SndCheckbox("Square ESP", &config::squareesp);
			ImGui::SameLine();
			ImGui::ColorEdit3("##squarecol", config::squareesp_color, ImGuiColorEditFlags_NoInputs);

			SndCheckbox("Skeleton ESP", &config::boneskeleton);
			ImGui::SameLine();
			ImGui::ColorEdit3("##skelcol", config::skeleton_color, ImGuiColorEditFlags_NoInputs);

			SndCheckbox("Chams", &config::chams);
			if (config::chams) {
				ImGui::SameLine();
				ImGui::Text("Vis:");
				ImGui::SameLine();
				ImGui::ColorEdit3("##chamsvis", config::chams_visible_color, ImGuiColorEditFlags_NoInputs);
				ImGui::SameLine();
				ImGui::Text("Hidden:");
				ImGui::SameLine();
				ImGui::ColorEdit3("##chamshid", config::chams_hidden_color, ImGuiColorEditFlags_NoInputs);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Entities");
			ImGui::PopStyleColor();

			SndCheckbox("Entity ESP", &config::entity_esp);
			if (config::entity_esp) {
				ImGui::Indent();
				SndCheckbox("Printers", &config::entity_esp_printers);
				ImGui::SameLine();
				ImGui::ColorEdit3("##printcol", config::entity_esp_color_printer, ImGuiColorEditFlags_NoInputs);

				SndCheckbox("Shipments", &config::entity_esp_shipments);
				ImGui::SameLine();
				ImGui::ColorEdit3("##shipcol", config::entity_esp_color_shipment, ImGuiColorEditFlags_NoInputs);

				SndCheckbox("Drugs", &config::entity_esp_drugs);
				ImGui::SameLine();
				ImGui::ColorEdit3("##drugcol", config::entity_esp_color_drug, ImGuiColorEditFlags_NoInputs);

				SndCheckbox("Doors", &config::entity_esp_doors);
				ImGui::SameLine();
				ImGui::ColorEdit3("##doorcol", config::entity_esp_color_door, ImGuiColorEditFlags_NoInputs);

				SndCheckbox("Weapons", &config::entity_esp_weapons);
				ImGui::SameLine();
				ImGui::ColorEdit3("##weapcol", config::entity_esp_color_weapon, ImGuiColorEditFlags_NoInputs);

				SndCheckbox("Money", &config::entity_esp_money);
				ImGui::SameLine();
				ImGui::ColorEdit3("##moneycol", config::entity_esp_color_money, ImGuiColorEditFlags_NoInputs);

				SndCheckbox("Vehicles", &config::entity_esp_vehicles);
				ImGui::SameLine();
				ImGui::ColorEdit3("##vehcol", config::entity_esp_color_vehicle, ImGuiColorEditFlags_NoInputs);

				SndCheckbox("Entity Health Bars", &config::entity_esp_health_bars);
				ImGui::Unindent();
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Intel");
			ImGui::PopStyleColor();

			SndCheckbox("Admin Alert", &config::admin_alert);
			SndCheckbox("Spectator Alert", &config::spectator_alert);
			SndCheckbox("Money Tracker", &config::money_tracker);
			SndCheckbox("ESP Intel Badges", &config::esp_intel_badges);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Range");
			ImGui::PopStyleColor();

			ImGui::SliderFloat("Min Distance", &config::esp_min_dist, 0.f, 5000.f, "%.0f");
			ImGui::SliderFloat("Max Distance", &config::esp_max_dist, 0.f, 99999.f, "%.0f");
			SndCheckbox("ESP Debug Info", &config::esp_debug);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Full Map Radar");
			ImGui::PopStyleColor();

			SndCheckbox("Show Dormant Players", &config::show_dormant);
			if (SndCheckbox("Full Radar (Lua)", &config::full_radar)) {
				if (!config::full_radar)
					luascripts::RunLuaScript(luascripts::LUA_FULLRADAR_STOP);
			}
			if (config::full_radar) {
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.f, 0.85f, 0.f, 1.f), "ACTIVE");
			}
			SndCheckbox("Force PVS Refresh", &config::force_pvs);
			if (config::force_pvs) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(100.f);
				ImGui::SliderFloat("##pvs_int", &config::force_pvs_interval, 2.f, 60.f, "%.0fs");
				SndCheckbox("Aggressive (2s when dormant nearby)", &config::pvs_aggressive);
			}
			if (SndSmallButton("cl_fullupdate")) {
				if (interfaces::engine) interfaces::engine->ClientCmd("cl_fullupdate");
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Force server to resend all entities");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Prediction");
			ImGui::PopStyleColor();

			SndCheckbox("Movement Prediction", &prediction::enabled);
			if (prediction::enabled) {
				ImGui::SliderFloat("Predict Time", &prediction::predict_time, 0.1f, 2.0f, "%.1fs");
				ImGui::SliderFloat("Ghost Alpha", &prediction::ghost_alpha, 20.f, 200.f, "%.0f");
			}
			SndCheckbox("Voice Indicators", &killfeed::voice_indicators);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Aim Direction");
			ImGui::PopStyleColor();

			SndCheckbox("Aim Lines", &aim_lines::enabled);
			if (aim_lines::enabled) {
				ImGui::SliderFloat("Line Length", &aim_lines::line_length, 50.f, 500.f, "%.0f");
				ImGui::ColorEdit3("Aim Color", aim_lines::aim_color, ImGuiColorEditFlags_NoInputs);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Detection");
			ImGui::PopStyleColor();

			SndCheckbox("Spawn Protection Detect", &spawn_detect::enabled);
			SndCheckbox("Damage Indicators", &damage_log::enabled);
			if (damage_log::enabled) {
				SndCheckbox("Show Numbers", &damage_log::show_indicators);
				ImGui::SliderFloat("Duration##dmg", &damage_log::indicator_duration, 0.5f, 5.f, "%.1fs");
				if (SndSmallButton("Install Damage Tracker")) {
					luascripts::RunLuaScript(luascripts::LUA_DAMAGE_TRACKER_SETUP);
				}
				if (damage_log::g_totalDealt > 0 || damage_log::g_totalReceived > 0) {
					ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Dealt: %d", damage_log::g_totalDealt);
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "Recv: %d", damage_log::g_totalReceived);
					ImGui::SameLine();
					if (SndSmallButton("Reset##dmg")) damage_log::Reset();
				}
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Door Memory");
			ImGui::PopStyleColor();

			SndCheckbox("Door Code Memory", &door_memory::enabled);
			if (door_memory::enabled) {
				SndCheckbox("Show on ESP", &door_memory::show_on_esp);
				ImGui::Text("Codes stored: %d", (int)door_memory::g_posCodes.size());
				if (SndSmallButton("Save Codes")) door_memory::SaveCodes();
				ImGui::SameLine();
				if (SndSmallButton("Load Codes")) door_memory::LoadCodes();
				ImGui::SameLine();
				if (SndSmallButton("Clear Codes")) door_memory::g_posCodes.clear();
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Printer Monitor");
			ImGui::PopStyleColor();

			SndCheckbox("Printer Monitor", &printer_monitor::enabled);
			if (printer_monitor::enabled) {
				SndCheckbox("Show Panel", &printer_monitor::show_panel);
				ImGui::Text("Tracking: %d printers", (int)printer_monitor::g_printers.size());
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.f, 1.f));
			ImGui::SeparatorText("Threat Assessment");
			ImGui::PopStyleColor();

			SndCheckbox("Threat Radar", &threat_radar::enabled);
			if (threat_radar::enabled) {
				SndCheckbox("Show Threat Level", &threat_radar::show_level);
				SndCheckbox("Color ESP by Threat", &threat_radar::color_esp);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Player Profiler");
			ImGui::PopStyleColor();

			SndCheckbox("Player Profiler", &player_profiler::enabled);
			if (player_profiler::enabled) {
				SndCheckbox("Show Paths", &player_profiler::show_paths);
				SndCheckbox("Profiler Panel", &player_profiler::show_panel);
			}
			break;
		}

		// ── HUD ───────────────────────────────────────────────────────────────
		case 3:
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Radar");
			ImGui::PopStyleColor();

			SndCheckbox("Minimap", &config::minimap);
			ImGui::SliderFloat("Zoom", &config::minimap_zoom, 500.f, 5000.f, "%.0f");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Crosshair");
			ImGui::PopStyleColor();

			SndCheckbox("FOV Circle", &config::fov_circle);
			ImGui::SameLine();
			ImGui::ColorEdit3("##fovcirccol", config::fov_circle_color, ImGuiColorEditFlags_NoInputs);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Effects");
			ImGui::PopStyleColor();

			SndCheckbox("Fullbright", &config::fullbright);
			SndCheckbox("Sound ESP", &config::sound_esp);

			SndCheckbox("X-Ray Vision", &xray::enabled);
			if (xray::enabled) {
				const char* xrayModes[] = { "Wireframe", "Transparent", "Textured" };
				ImGui::Combo("X-Ray Mode", &xray::mode, xrayModes, 3);
				ImGui::SliderFloat("Wall Alpha", &xray::wall_alpha, 0.05f, 0.8f, "%.2f");
			}

			if (SndCheckbox("Night Vision", &config::nightVision)) {
				if (config::nightVision)
					luascripts::RunLuaScript(luascripts::LUA_NIGHT_VISION);
				else
					luascripts::RunLuaScript(luascripts::LUA_NIGHT_VISION_STOP);
			}

			if (SndCheckbox("Hitmarker", &config::hitmarker)) {
				if (config::hitmarker)
					luascripts::RunLuaScript(luascripts::LUA_HITMARKER_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_HITMARKER_STOP);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Info Panels");
			ImGui::PopStyleColor();

			SndCheckbox("Spectator List", &hud::spectator_list);
			SndCheckbox("Velocity Graph", &hud::velocity_graph);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Thirdperson");
			ImGui::PopStyleColor();

			SndCheckbox("Thirdperson", &misc_features::thirdperson);
			ImGui::SliderFloat("TP Distance", &misc_features::thirdperson_dist, 50.f, 500.f, "%.0f");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Freecam");
			ImGui::PopStyleColor();

			SndCheckbox("Freecam (F3 Toggle)", &freecam::enabled);
			if (freecam::enabled) {
				ImGui::SliderFloat("Cam Speed", &freecam::speed, 100.f, 3000.f, "%.0f");
				if (freecam::g_active) {
					ImGui::TextColored(ImVec4(0.f, 1.f, 0.5f, 1.f), "ACTIVE - Press F3 to exit");
				} else {
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Press F3 to activate");
				}
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Crosshair");
			ImGui::PopStyleColor();

			SndCheckbox("Custom Crosshair", &misc_features::custom_crosshair);
			if (misc_features::custom_crosshair) {
				const char* styles[] = { "Cross", "Dot", "Circle", "Cross+Dot" };
				ImGui::Combo("Style", &misc_features::crosshair_style, styles, 4);
				ImGui::SliderFloat("Size", &misc_features::crosshair_size, 1.f, 20.f, "%.1f");
				ImGui::SliderFloat("Thickness", &misc_features::crosshair_thickness, 0.5f, 5.f, "%.1f");
				ImGui::ColorEdit3("Color##xhair", misc_features::crosshair_color, ImGuiColorEditFlags_NoInputs);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Replay & Analysis");
			ImGui::PopStyleColor();

			SndCheckbox("Death Replay", &death_replay::enabled);
			if (death_replay::enabled)
				ImGui::SliderFloat("Replay Time", &death_replay::replay_seconds, 1.f, 6.f, "%.1fs");

			SndCheckbox("Heat Map", &heatmap::enabled);
			if (heatmap::enabled) {
				ImGui::SliderFloat("Grid Size", &heatmap::grid_size, 50.f, 500.f, "%.0f");
				ImGui::SliderFloat("Opacity##hm", &heatmap::opacity, 0.05f, 0.8f, "%.2f");
				if (SndSmallButton("Clear Heatmap")) heatmap::Clear();
			}

			SndCheckbox("Kill Feed Analyzer", &killfeed::analyzer_enabled);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Waypoints");
			ImGui::PopStyleColor();

			SndCheckbox("Waypoints", &waypoints::enabled);
			if (waypoints::enabled) {
				static char wpLabel[32] = "Marker";
				ImGui::InputText("Label##wp", wpLabel, 32);
				const char* wpColors[] = { "Red", "Green", "Blue", "Yellow", "Purple", "Cyan" };
				ImGui::Combo("Color##wp", &waypoints::g_selectedColor, wpColors, 6);
				static std::string s_wpResult;
				if (SndSmallButton("Place at Crosshair")) {
					auto cmd = std::string("local r=\"\" pcall(function() local tr=LocalPlayer():GetEyeTrace() ")
						+ "r=string.format(\"%.1f\\t%.1f\\t%.1f\",tr.HitPos.x,tr.HitPos.y,tr.HitPos.z) end) return r";
					luascripts::QueryLuaScript(cmd.c_str(), &s_wpResult);
				}
				if (!s_wpResult.empty()) {
					float x = 0, y = 0, z = 0;
					if (sscanf_s(s_wpResult.c_str(), "%f\t%f\t%f", &x, &y, &z) == 3) {
						waypoints::Add({x, y, z}, wpLabel);
					}
					s_wpResult.clear();
				}
				ImGui::SameLine();
				if (SndSmallButton("Clear All##wp")) waypoints::Clear();
				ImGui::Text("Active: %d waypoints", (int)waypoints::g_waypoints.size());

				if (!waypoints::g_waypoints.empty()) {
					ImGui::BeginChild("##wplist", ImVec2(0.f, 80.f), true);
					for (int wi = 0; wi < static_cast<int>(waypoints::g_waypoints.size()); ++wi) {
						auto label = std::format("[{}] {}", wi, waypoints::g_waypoints[wi].label);
						ImGui::Text("%s", label.c_str());
						ImGui::SameLine();
						auto delLabel = std::format("X##{}", wi);
						if (SndSmallButton(delLabel.c_str())) {
							waypoints::Remove(wi);
							break;
						}
					}
					ImGui::EndChild();
				}
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.2f, 0.2f, 1.f));
			ImGui::SeparatorText("Rage Mode");
			ImGui::PopStyleColor();

			if (rage_mode::active) {
				ImGui::TextColored(ImVec4(1.f, 0.2f, 0.2f, 1.f), "RAGE MODE ACTIVE (F5 to deactivate)");
			} else {
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Press F5 to activate");
			}
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Max aimbot + anti-aim + fakelag + triggerbot");
			break;
		}

		// ── Stealth ───────────────────────────────────────────────────────────
		case 4:
		{
			// ── Panic ─────────────────────────────────────────────────────
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Panic");
			ImGui::PopStyleColor();

			{
				const char* panicKeyNames[] = { "End", "Home", "Delete", "F6", "F7", "F8" };
				const int panicKeyCodes[]   = { VK_END, VK_HOME, VK_DELETE, VK_F6, VK_F7, VK_F8 };
				int curPanic = 0;
				for (int k = 0; k < 6; ++k)
					if (config::panic_key == panicKeyCodes[k]) { curPanic = k; break; }
				if (ImGui::BeginCombo("Panic Key", panicKeyNames[curPanic])) {
					for (int k = 0; k < 6; ++k)
						if (ImGui::Selectable(panicKeyNames[k], curPanic == k))
							config::panic_key = panicKeyCodes[k];
					ImGui::EndCombo();
				}
			}
			ImGui::Text("Panic State: %s", config::g_panic.load(std::memory_order_relaxed) ? "ACTIVE" : "Off");

			// ── Auto-Disable ──────────────────────────────────────────────
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Auto-Disable");
			ImGui::PopStyleColor();

			SndCheckbox("Spectator Auto-Disable (full panic)", &config::spectator_auto_disable);
			SndCheckbox("Spectator Cloak (selective, no panic)", &config::spectator_cloak);
			if (config::spectator_cloak) {
				static std::string s_cloakState;
				if (SndSmallButton("Cloak Status")) luascripts::QueryLuaScript(luascripts::LUA_SPECTATOR_CLOAK_QUERY, &s_cloakState);
				if (!s_cloakState.empty()) {
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.f, 0.85f, 0.2f, 1.f), "%s", s_cloakState.c_str());
				}
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Hides ESP/aimbot snap when watched (keeps movement)");
			}

			SndCheckbox("Auto-Disguise", &auto_disguise::enabled);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), auto_disguise::g_disguised ? "DISGUISED" : "Normal");

			// ── Anti-Detection ────────────────────────────────────────────
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Anti-Detection");
			ImGui::PopStyleColor();

			SndCheckbox("Screenshot Cleaner (basic)", &config::screenshot_cleaner);
			SndCheckbox("Anti-Screenshot (expanded)", &config::anti_screenshot);
			if (config::anti_screenshot)
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Hooks render.Capture, surface.GetTextureID, screenshot cmds");

			SndCheckbox("Anti-AntiCheat (hook stripper)", &config::anti_anticheat);
			SndCheckbox("AC Bypass (gAC/CAC/StackAC)", &config::ac_bypass);
			if (config::ac_bypass)
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Hooks debug.getinfo, file.Find, concommand.GetTable; blocks AC nets");

			// ── Admin Bypass ──────────────────────────────────────────────
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Admin Bypass");
			ImGui::PopStyleColor();

			SndCheckbox("Admin Bypass (ULX/SAM/FAdmin/SG)", &config::admin_bypass);
			if (config::admin_bypass)
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Hides from player.GetAll, blocks spectate nets, intercepts teleport");

			// ── Anti-Kick ─────────────────────────────────────────────────
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Anti-Kick");
			ImGui::PopStyleColor();

			SndCheckbox("Anti-Kick (vote-no + intercept)", &config::anti_kick);
			if (config::anti_kick) {
				static std::string s_kickLog;
				if (SndSmallButton("Kick Log")) luascripts::QueryLuaScript(luascripts::LUA_ANTI_KICK_READ, &s_kickLog);
				if (!s_kickLog.empty()) {
					ImGui::BeginChild("##kicklog", ImVec2(0.f, 60.f), true);
					ImGui::TextUnformatted(s_kickLog.c_str());
					ImGui::EndChild();
				}
			}

			// ── Name Steal ────────────────────────────────────────────────
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Identity");
			ImGui::PopStyleColor();

			SndCheckbox("Name Steal Cycle (30s)", &config::name_steal_cycle);
			ImGui::SameLine();
			if (SndSmallButton("Steal Now")) {
				static std::string s_stealResult;
				luascripts::QueryLuaScript(luascripts::LUA_NAME_STEAL_NOW, &s_stealResult);
			}
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Cycles through other players' names to confuse admins");

			// ── Fake Death ────────────────────────────────────────────────
			SndCheckbox("Fake Death (periodic broadcast)", &config::fake_death);
			ImGui::SameLine();
			if (SndSmallButton("Fire Now")) luascripts::RunLuaScript(luascripts::LUA_FAKE_DEATH);
			if (config::fake_death)
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Sends death events/sounds while staying alive");

			// ── Recording ────────────────────────────────────────────────
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Recording");
			ImGui::PopStyleColor();

			SndCheckbox("Recording Mode", &config::recording_mode);
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Hides all visuals; aim/bhop still run in background");

			// ── Anti-Cheat Bypass (manual tools) ─────────────────────────
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.3f, 0.3f, 1.f));
			ImGui::SeparatorText("AC Tools (manual)");
			ImGui::PopStyleColor();

			static std::string s_acResult;
			if (SndSmallButton("Scan AC Hooks")) luascripts::QueryLuaScript(luascripts::LUA_HOOK_HIJACK_SCAN, &s_acResult);
			ImGui::SameLine();
			if (SndSmallButton("Remove AC Hooks")) luascripts::QueryLuaScript(luascripts::LUA_HOOK_HIJACK_REMOVE_AC, &s_acResult);
			ImGui::SameLine();
			if (SndSmallButton("Block RunString")) luascripts::RunLuaScript(luascripts::LUA_RUNSTRING_BLOCK_SETUP);

			if (SndSmallButton("Debug Spy")) luascripts::RunLuaScript(luascripts::LUA_DEBUG_SPY_SETUP);
			ImGui::SameLine();
			if (SndSmallButton("Debug Log")) luascripts::QueryLuaScript(luascripts::LUA_DEBUG_SPY_READ, &s_acResult);
			ImGui::SameLine();
			if (SndSmallButton("Blocked RS")) luascripts::QueryLuaScript(luascripts::LUA_RUNSTRING_BLOCK_READ, &s_acResult);

			if (!s_acResult.empty()) {
				ImGui::BeginChild("##acresult", ImVec2(0.f, 80.f), true, ImGuiWindowFlags_HorizontalScrollbar);
				ImGui::TextUnformatted(s_acResult.c_str());
				ImGui::EndChild();
			}
			break;
		}

		// ── Players ───────────────────────────────────────────────────────────
		case 5:
		{
			SndCheckbox("Whitelist Mode", &config::whitelistMode);
			ImGui::SameLine();
			if (SndSmallButton("Clear All")) config::playerListChecked.clear();
			ImGui::SameLine();
			if (SndSmallButton("Select All")) {
				auto& rc = config::BoneRead();
				for (int pi = 1; pi < 128; ++pi)
					if (rc[pi].valid) config::playerListChecked.insert(pi);
			}
			ImGui::Separator();
			if (config::whitelistMode)
				ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Checked = targeted/shown");
			else
				ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Checked = ignored/hidden");
			ImGui::Separator();

			ImGui::BeginChild("##playerlist", ImVec2(0, 0), true);
			auto& rc = config::BoneRead();
			for (int pi = 1; pi < 128; ++pi) {
				if (!rc[pi].valid) continue;
				bool checked = config::playerListChecked.count(pi) > 0;
				const char* displayName = rc[pi].rpName[0] ? rc[pi].rpName : rc[pi].name;
				auto label = std::format("[{}] {}", pi, displayName);
				if (SndCheckbox(label.c_str(), &checked)) {
					if (checked) config::playerListChecked.insert(pi);
					else config::playerListChecked.erase(pi);
				}
				if (rc[pi].job[0]) {
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.f, 1.f), "(%s)", rc[pi].job);
				}
				if (rc[pi].gang[0]) {
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.f, 0.67f, 0.2f, 1.f), "[%s]", rc[pi].gang);
				}
			}
			ImGui::EndChild();
			break;
		}

		// ── Net/Lua ───────────────────────────────────────────────────────────
		case 6:
		{
			static std::string s_scanResult;

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Network Sniffer");
			ImGui::PopStyleColor();

			SndCheckbox("Net Sniffer", &config::net_sniffer);
			if (net_panel::g_deepHookInstalled) {
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.f, 0.6f, 0.3f, 1.f), "(Deep Hook Active)");
			}

			// Traffic statistics
			if (config::net_sniffer && !net_panel::g_netLog.empty()) {
				int inCount = 0, outCount = 0;
				int inBytes = 0, outBytes = 0;
				for (const auto& e : net_panel::g_netLog) {
					if (e.name[0] == '>' && e.name[1] == '>') { outCount++; outBytes += e.length; }
					else { inCount++; inBytes += e.length; }
				}
				ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.f, 1.f), "IN: %d msgs (%d B)  OUT: %d msgs (%d B)  Total: %d",
					inCount, inBytes, outCount, outBytes, (int)net_panel::g_netLog.size());

				// Frequency analysis — top 5 most frequent messages
				if (net_panel::g_stats.totalMessages > 0) {
					ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "Msg/sec: %.1f | Peak: %.1f",
						net_panel::g_stats.msgPerSecond, net_panel::g_stats.peakMsgPerSecond);
				}
			}

			// Filter controls
			ImGui::InputText("Filter##net", net_panel::g_filterBuf, sizeof(net_panel::g_filterBuf));
			ImGui::SameLine();
			SndCheckbox("Whitelist", &net_panel::g_filterWhitelist);
			ImGui::SameLine();
			if (SndSmallButton("Clear Log")) net_panel::g_netLog.clear();

			ImGui::BeginChild("##netlog", ImVec2(0.f, 140.f), true, ImGuiWindowFlags_HorizontalScrollbar);
			if (net_panel::g_netLog.empty()) {
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "No net messages captured...");
			} else {
				for (const auto& e : net_panel::g_netLog) {
					if (net_panel::g_filterBuf[0] != '\0') {
						bool matches = strstr(e.name, net_panel::g_filterBuf) != nullptr;
						if (net_panel::g_filterWhitelist && !matches) continue;
						if (!net_panel::g_filterWhitelist && matches) continue;
					}
					ImVec4 col(0.7f, 0.8f, 0.9f, 1.f);
					if (e.name[0] == '>' && e.name[1] == '>') col = ImVec4(1.f, 0.63f, 0.31f, 1.f);
					else if (e.name[0] == '<' && e.name[1] == '<') col = ImVec4(0.31f, 0.78f, 1.f, 1.f);
					if (e.payload[0])
						ImGui::TextColored(col, "[%.1f] %s (%d B) | %s", e.time, e.name, e.length, e.payload);
					else
						ImGui::TextColored(col, "[%.1f] %s (%d B)", e.time, e.name, e.length);
				}
				if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
					ImGui::SetScrollHereY(1.0f);
			}
			ImGui::EndChild();

			// Payload inspector for selected message
			if (net_panel::g_selectedMsg >= 0 && net_panel::g_selectedMsg < (int)net_panel::g_netLog.size()) {
				const auto& sel = net_panel::g_netLog[net_panel::g_selectedMsg];
				ImGui::TextColored(ImVec4(1.f, 0.9f, 0.3f, 1.f), "Inspecting: %s", sel.name);
				if (sel.payload[0])
					ImGui::TextWrapped("Payload: %s", sel.payload);
			}

			// Frequency table
			if (ImGui::TreeNode("Message Frequency##netfreq")) {
				for (const auto& kv : net_panel::g_stats.msgFrequency) {
					ImGui::Text("%-30s %d", kv.first.c_str(), kv.second);
				}
				ImGui::TreePop();
			}

			// Export/logging
			SndCheckbox("Log to File", &net_panel::g_fileLogging);
			ImGui::SameLine();
			if (SndSmallButton("Export Log")) net_panel::ExportLog();

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Chat Spy");
			ImGui::PopStyleColor();

			SndCheckbox("Chat Spy", &net_panel::chat_spy_enabled);
			if (net_panel::chat_spy_enabled) {
				ImGui::BeginChild("##chatlog", ImVec2(0.f, 100.f), true, ImGuiWindowFlags_HorizontalScrollbar);
				if (net_panel::g_chatLog.empty()) {
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "No chat messages...");
				} else {
					for (const auto& c : net_panel::g_chatLog) {
						ImVec4 col = c.teamChat ? ImVec4(0.4f, 1.f, 0.4f, 1.f) :
						             c.dead ? ImVec4(0.8f, 0.4f, 0.4f, 1.f) :
						             ImVec4(0.9f, 0.9f, 0.9f, 1.f);
						const char* prefix = c.dead ? "*DEAD* " : (c.teamChat ? "(TEAM) " : "");
						ImGui::TextColored(col, "%s%s: %s", prefix, c.player, c.message);
					}
					if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
						ImGui::SetScrollHereY(1.0f);
				}
				ImGui::EndChild();
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Packet Manipulation");
			ImGui::PopStyleColor();

			if (SndSmallButton("Replay Capture")) luascripts::RunLuaScript(luascripts::LUA_NET_REPLAY_CAPTURE);
			ImGui::SameLine();
			if (SndSmallButton("Replay Send")) luascripts::QueryLuaScript(luascripts::LUA_NET_REPLAY_SEND, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Modified Replay")) luascripts::QueryLuaScript(luascripts::LUA_NET_MODIFIED_REPLAY, &s_scanResult);

			// Net message forge section
			static int s_forgeType = 0;
			static char s_forgeMsgName[64] = "";
			static char s_forgePayload[256] = "";
			ImGui::Combo("Forge Type", &s_forgeType, "String\0Int\0Float\0Bool\0Entity\0");
			ImGui::InputText("Message Name##forge", s_forgeMsgName, sizeof(s_forgeMsgName));
			ImGui::InputText("Payload##forge", s_forgePayload, sizeof(s_forgePayload));
			if (SndSmallButton("Send Forged Packet")) {
				net_panel::SendForgedPacket(s_forgeMsgName, s_forgeType, s_forgePayload);
			}
			ImGui::SameLine();
			if (SndSmallButton("Flood Last")) luascripts::QueryLuaScript(luascripts::LUA_NET_FLOOD_LAST, &s_scanResult);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Network Intelligence");
			ImGui::PopStyleColor();

			if (SndSmallButton("Channel Info")) luascripts::QueryLuaScript(luascripts::LUA_NET_CHANNEL_INFO, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Rate Limit Test")) luascripts::QueryLuaScript(luascripts::LUA_NET_RATELIMIT_TEST, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Full String Dump")) luascripts::QueryLuaScript(luascripts::LUA_NET_STRING_DUMP, &s_scanResult);

			if (SndSmallButton("NW Variable Spy")) luascripts::QueryLuaScript(luascripts::LUA_NW_VARIABLE_SPY, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("HTTP Spy Setup")) luascripts::RunLuaScript(luascripts::LUA_HTTP_SPY_SETUP);
			ImGui::SameLine();
			if (SndSmallButton("HTTP Spy Read")) luascripts::QueryLuaScript(luascripts::LUA_HTTP_SPY_READ, &s_scanResult);

			if (SndSmallButton("ConCmd Intercept")) luascripts::RunLuaScript(luascripts::LUA_CONCOMMAND_INTERCEPT_SETUP);
			ImGui::SameLine();
			if (SndSmallButton("ConCmd Log")) luascripts::QueryLuaScript(luascripts::LUA_CONCOMMAND_INTERCEPT_READ, &s_scanResult);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Lua Exploits");
			ImGui::PopStyleColor();

			if (SndSmallButton("Spoof ConVars")) luascripts::RunLuaScript(luascripts::LUA_CONVAR_SPOOF);
			ImGui::SameLine();
			if (SndSmallButton("Remove Smoke")) luascripts::RunLuaScript(luascripts::LUA_NO_SMOKE);
			ImGui::SameLine();
			if (SndSmallButton("Restore Smoke")) luascripts::RunLuaScript(luascripts::LUA_NO_SMOKE_RESTORE);
			ImGui::SameLine();
			if (SndSmallButton("Detect RunString")) luascripts::RunLuaScript(luascripts::LUA_RUNSTRING_DETECT);

			if (SndSmallButton("Deep Net Hook")) {
				luascripts::RunLuaScript(luascripts::LUA_NET_DEEP_HOOK);
				net_panel::g_deepHookInstalled = true;
			}
			ImGui::SameLine();
			if (SndSmallButton("Police Scanner")) luascripts::QueryLuaScript(luascripts::LUA_DARKRP_POLICE_SCANNER, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Drop Money")) luascripts::RunLuaScript(luascripts::LUA_DARKRP_DROP_MONEY);

			if (SndSmallButton("Speed Timers")) luascripts::QueryLuaScript(luascripts::LUA_TIMER_SPEED, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Sniff ConCmds")) luascripts::RunLuaScript(luascripts::LUA_CONCOMMAND_SNIFF_SETUP);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Scanners");
			ImGui::PopStyleColor();

			if (SndSmallButton("Globals")) luascripts::QueryLuaScript(luascripts::LUA_GLOBAL_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Hooks")) luascripts::QueryLuaScript(luascripts::LUA_HOOK_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Doors")) luascripts::QueryLuaScript(luascripts::LUA_DOOR_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Printers")) luascripts::QueryLuaScript(luascripts::LUA_PRINTER_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Inventory")) luascripts::QueryLuaScript(luascripts::LUA_INVENTORY_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("RunStrings")) luascripts::QueryLuaScript(luascripts::LUA_RUNSTRING_READ, &s_scanResult);

			if (SndSmallButton("Net Strings")) luascripts::QueryLuaScript(luascripts::LUA_NET_STRING_TABLE, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("DarkRP Entities")) luascripts::QueryLuaScript(luascripts::LUA_DARKRP_ENTITY_ABUSE, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Backdoor Scan")) luascripts::QueryLuaScript(luascripts::LUA_BACKDOOR_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Nearby Doors")) luascripts::QueryLuaScript(luascripts::LUA_DARKRP_STEAL_DOORS, &s_scanResult);

			if (SndSmallButton("Panel Spy")) luascripts::QueryLuaScript(luascripts::LUA_PANEL_SPY, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Timers")) luascripts::QueryLuaScript(luascripts::LUA_TIMER_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("ConVars")) luascripts::QueryLuaScript(luascripts::LUA_CONVAR_DUMP, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("ConCommands")) luascripts::QueryLuaScript(luascripts::LUA_CONCOMMAND_SNIFF_READ, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Base Value")) luascripts::QueryLuaScript(luascripts::LUA_ENTITY_VALUE_CALC, &s_scanResult);

			if (SndSmallButton("Save Session")) luascripts::QueryLuaScript(luascripts::LUA_FILE_SAVE, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Load Session")) luascripts::QueryLuaScript(luascripts::LUA_FILE_LOAD, &s_scanResult);

			if (!s_scanResult.empty()) {
				ImGui::BeginChild("##netscanresult", ImVec2(0.f, 100.f), true, ImGuiWindowFlags_HorizontalScrollbar);
				ImGui::TextUnformatted(s_scanResult.c_str());
				ImGui::EndChild();
				ImGui::SameLine();
				if (SndSmallButton("Clear##netscan")) s_scanResult.clear();
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Lua Loader");
			ImGui::PopStyleColor();

			static bool editor_initialized = false;
			if (!editor_initialized) {
				editor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
				editor_initialized = true;
			}

			if (SndSmallButton("Execute")) {
				lualoader::QueueScript(editor.GetText());
			}
			ImGui::SameLine();
			if (SndSmallButton("Anti Screen Grab")) {
				lualoader::QueueScript(R"(render.Capture = function() return end)");
			}
			ImGui::SameLine();
			ImGui::Text(std::format("Lua State -> {}", (void*)config::luastate).c_str());

			if (lualoader::g_hasResult.load(std::memory_order_acquire)) {
				if (!lualoader::g_lastError.empty())
					ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Error: %s", lualoader::g_lastError.c_str());
				else if (!lualoader::g_lastOutput.empty())
					ImGui::TextColored(ImVec4(0.3f, 1.f, 0.5f, 1.f), "Result: %s", lualoader::g_lastOutput.c_str());
			}

			editor.Render("codeeditor");
			break;
		}

		// ── Exploits ──────────────────────────────────────────────────────────
		case 7:
		{
			static std::string s_scanResult;

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.5f, 0.f, 1.f));
			ImGui::SeparatorText("Raids & Theft");
			ImGui::PopStyleColor();

			if (SndCheckbox("Keypad Cracker", &config::keypad_cracker)) {
				if (config::keypad_cracker)
					luascripts::RunLuaScript(luascripts::LUA_KEYPAD_CRACKER_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_KEYPAD_CRACKER_STOP);
			}
			ImGui::SameLine();
			if (SndSmallButton("Read Codes")) luascripts::QueryLuaScript(luascripts::LUA_KEYPAD_CRACKER_READ, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Keypad Log")) luascripts::QueryLuaScript(luascripts::LUA_KEYPAD_SPY_ENHANCED_READ, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Auto-Pickup")) luascripts::RunLuaScript(luascripts::LUA_AUTO_PICKUP);

			if (SndSmallButton("Collect Printers")) luascripts::RunLuaScript(luascripts::LUA_AUTO_COLLECT_PRINTERS);
			ImGui::SameLine();
			if (SndSmallButton("Prop Counter")) luascripts::QueryLuaScript(luascripts::LUA_PROP_COUNTER, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Crosshair Info")) luascripts::QueryLuaScript(luascripts::LUA_CROSSHAIR_INFO, &s_scanResult);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.3f, 0.3f, 1.f));
			ImGui::SeparatorText("Intelligence");
			ImGui::PopStyleColor();

			if (SndSmallButton("Admin Spy")) luascripts::RunLuaScript(luascripts::LUA_ADMIN_SPY_SETUP);
			ImGui::SameLine();
			if (SndSmallButton("Admin Log")) luascripts::QueryLuaScript(luascripts::LUA_ADMIN_SPY_READ, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Steal Name")) luascripts::QueryLuaScript(luascripts::LUA_NAME_STEAL, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Model List")) luascripts::QueryLuaScript(luascripts::LUA_MODEL_LIST, &s_scanResult);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.5f, 0.f, 1.f));
			ImGui::SeparatorText("DarkRP Actions");
			ImGui::PopStyleColor();

			if (SndSmallButton("Auto-Warrant")) luascripts::QueryLuaScript(luascripts::LUA_AUTO_WARRANT, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Auto-Arrest")) luascripts::QueryLuaScript(luascripts::LUA_AUTO_ARREST, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Money Spam")) luascripts::RunLuaScript(luascripts::LUA_MONEY_EXPLOIT);

			if (SndCheckbox("Chat Auto-Advert", &config::advertRunning)) {
				if (config::advertRunning)
					luascripts::RunLuaScript(luascripts::LUA_CHAT_ADVERT);
				else
					luascripts::RunLuaScript(luascripts::LUA_CHAT_ADVERT_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Set msgs via Lua");

			if (SndCheckbox("Anti-AFK", &config::antiAfk)) {
				if (config::antiAfk)
					luascripts::RunLuaScript(luascripts::LUA_ANTI_AFK);
				else
					luascripts::RunLuaScript(luascripts::LUA_ANTI_AFK_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Prevents AFK kick");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.8f, 1.f, 1.f));
			ImGui::SeparatorText("Recon");
			ImGui::PopStyleColor();

			if (SndSmallButton("Server Info")) luascripts::QueryLuaScript(luascripts::LUA_SERVER_INFO, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Prop Surf Scan")) luascripts::QueryLuaScript(luascripts::LUA_PROP_SURF_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Voice Unmute All")) luascripts::RunLuaScript(luascripts::LUA_VOICE_UNMUTE_ALL);

			if (SndSmallButton("Teleport Spy")) luascripts::RunLuaScript(luascripts::LUA_TELEPORT_DETECT_SETUP);
			ImGui::SameLine();
			if (SndSmallButton("Teleport Log")) luascripts::QueryLuaScript(luascripts::LUA_TELEPORT_DETECT_READ, &s_scanResult);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.3f, 0.3f, 1.f));
			ImGui::SeparatorText("Network Exploits");
			ImGui::PopStyleColor();

			if (SndCheckbox("Lag Compensation Abuse", &config::lagExploit)) {
				if (config::lagExploit)
					luascripts::RunLuaScript(luascripts::LUA_LAGCOMP_EXPLOIT);
				else
					luascripts::RunLuaScript(luascripts::LUA_LAGCOMP_RESET);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Extends backtrack window");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.5f, 1.f));
			ImGui::SeparatorText("Advanced Exploits");
			ImGui::PopStyleColor();

			if (SndSmallButton("Net Forge: Money")) luascripts::QueryLuaScript(luascripts::LUA_NET_FORGE_MONEY_REQUEST, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Job List")) luascripts::QueryLuaScript(luascripts::LUA_NET_FORGE_JOB_ABUSE, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Buy Net Scan")) luascripts::QueryLuaScript(luascripts::LUA_NET_FORGE_ENTITY_DUPE, &s_scanResult);

			if (SndSmallButton("Impersonate")) luascripts::QueryLuaScript(luascripts::LUA_IMPERSONATE, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Economy Scan")) luascripts::QueryLuaScript(luascripts::LUA_ECONOMY_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Bait Money")) luascripts::RunLuaScript(luascripts::LUA_BAIT_MONEY);
			ImGui::SameLine();
			if (SndSmallButton("Chat Bypass")) luascripts::RunLuaScript(luascripts::LUA_CHAT_BYPASS);

			if (SndCheckbox("Silent Walk", &config::silentWalk)) {
				if (config::silentWalk)
					luascripts::RunLuaScript(luascripts::LUA_SILENT_WALK);
				else
					luascripts::RunLuaScript(luascripts::LUA_SILENT_WALK_STOP);
			}
			ImGui::SameLine();

			if (SndCheckbox("Slide Walk", &config::slideWalk)) {
				if (config::slideWalk)
					luascripts::RunLuaScript(luascripts::LUA_SLIDE_WALK);
				else
					luascripts::RunLuaScript(luascripts::LUA_SLIDE_WALK_STOP);
			}

			if (SndCheckbox("Auto-Lockpick", &config::lockpickAuto)) {
				if (config::lockpickAuto)
					luascripts::RunLuaScript(luascripts::LUA_AUTO_LOCKPICK_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_AUTO_LOCKPICK_STOP);
			}
			ImGui::SameLine();

			if (SndCheckbox("Prop Kill Alert", &config::propAlert)) {
				if (config::propAlert)
					luascripts::RunLuaScript(luascripts::LUA_PROPKILL_DETECTOR_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_PROPKILL_DETECTOR_STOP);
			}

			if (SndSmallButton("Lua Persistence")) luascripts::RunLuaScript(luascripts::LUA_PERSISTENCE_SETUP);
			ImGui::SameLine();
			if (SndSmallButton("Entity HP Monitor")) luascripts::RunLuaScript(luascripts::LUA_ENTITY_HEALTH_MONITOR);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 1.f, 1.f));
			ImGui::SeparatorText("Raiding & Social");
			ImGui::PopStyleColor();

			if (SndSmallButton("Fading Door Scan")) luascripts::QueryLuaScript(luascripts::LUA_FADING_DOOR_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Force Doors")) luascripts::QueryLuaScript(luascripts::LUA_FADING_DOOR_FORCE, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Auto-Mug")) luascripts::QueryLuaScript(luascripts::LUA_AUTO_MUG, &s_scanResult);

			if (SndSmallButton("Door Exploit")) luascripts::QueryLuaScript(luascripts::LUA_DOOR_EXPLOIT, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Door Buster (Legacy)")) luascripts::QueryLuaScript(luascripts::LUA_DOOR_BUSTER, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Lock Scan")) luascripts::QueryLuaScript(luascripts::LUA_DOOR_SCAN_ENHANCED, &s_scanResult);

			if (SndSmallButton("Hitman Accept")) luascripts::QueryLuaScript(luascripts::LUA_HITMAN_ABUSE, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("ATM Scan")) luascripts::QueryLuaScript(luascripts::LUA_ATM_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Container Scan")) luascripts::QueryLuaScript(luascripts::LUA_CONTAINER_SCAN, &s_scanResult);

			if (SndSmallButton("Fake Disconnect")) luascripts::RunLuaScript(luascripts::LUA_FAKE_DISCONNECT);
			ImGui::SameLine();
			if (SndSmallButton("Fake Admin Msg")) luascripts::RunLuaScript(luascripts::LUA_FAKE_ADMIN_MSG);
			ImGui::SameLine();
			if (SndSmallButton("Weapon Stats")) luascripts::QueryLuaScript(luascripts::LUA_WEAPON_STATS, &s_scanResult);

			if (SndCheckbox("Vote Bot", &config::voteBot)) {
				if (config::voteBot)
					luascripts::RunLuaScript(luascripts::LUA_VOTE_BOT_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_VOTE_BOT_STOP);
			}
			ImGui::SameLine();

			if (SndCheckbox("Prop Flight (LAlt)", &config::propFly)) {
				if (config::propFly)
					luascripts::RunLuaScript(luascripts::LUA_PROP_FLIGHT_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_PROP_FLIGHT_STOP);
			}

			if (SndCheckbox("Kill Sound", &config::killSound)) {
				if (config::killSound)
					luascripts::RunLuaScript(luascripts::LUA_KILL_SOUND_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_KILL_SOUND_STOP);
			}
			ImGui::SameLine();
			if (SndSmallButton("Spec Richest")) luascripts::QueryLuaScript(luascripts::LUA_SPEC_TARGET, &s_scanResult);

			if (SndSmallButton("Map Teleports")) luascripts::QueryLuaScript(luascripts::LUA_TELEPORT_MAP_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Wire Exploit")) luascripts::QueryLuaScript(luascripts::LUA_WIRE_EXPLOIT, &s_scanResult);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
			ImGui::SeparatorText("INSANE EXPLOITS");
			ImGui::PopStyleColor();

			// Lag Switch
			SndCheckbox("Lag Switch (Mouse4)", &tick_exploits::lagswitch_enabled);
			if (tick_exploits::lagswitch_enabled) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(80.f);
				ImGui::SliderInt("Burst##ls", &tick_exploits::lagswitch_duration, 4, 20);
			}

			// Doubletap
			SndCheckbox("Doubletap (Mouse5)", &tick_exploits::doubletap_enabled);
			if (tick_exploits::doubletap_enabled) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(80.f);
				ImGui::SliderInt("Shift##dt", &tick_exploits::dt_shift_ticks, 6, 24);
				ImGui::SameLine();
				if (SndSmallButton("DT Info")) luascripts::QueryLuaScript(luascripts::LUA_DOUBLETAP_INFO, &s_scanResult);
			}

			// Speedhack
			SndCheckbox("Speedhack", &tick_exploits::speedhack_enabled);
			if (tick_exploits::speedhack_enabled) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(100.f);
				ImGui::SliderFloat("Factor##spd", &tick_exploits::speedhack_factor, 1.5f, 5.f, "%.1fx");
			}

			// Entity Magnet
			if (SndCheckbox("Entity Magnet", &config::entityMagnet)) {
				if (config::entityMagnet)
					luascripts::RunLuaScript(luascripts::LUA_ENTITY_MAGNET_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_ENTITY_MAGNET_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Pull printers/money");

			// Ghost Mode
			if (SndCheckbox("Ghost Mode", &config::ghostMode)) {
				if (config::ghostMode)
					luascripts::RunLuaScript(luascripts::LUA_GHOST_MODE_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_GHOST_MODE_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Invisible + no footsteps");

			// Infinite Ammo
			if (SndCheckbox("Infinite Ammo", &config::infiniteAmmo)) {
				if (config::infiniteAmmo)
					luascripts::RunLuaScript(luascripts::LUA_INFINITE_AMMO_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_INFINITE_AMMO_STOP);
			}
			ImGui::SameLine();

			// No Recoil + No Spread
			if (SndCheckbox("No Recoil/Spread", &config::noRecoilLua)) {
				if (config::noRecoilLua)
					luascripts::RunLuaScript(luascripts::LUA_NO_RECOIL_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_NO_RECOIL_STOP);
			}

			// Anti-Crash Shield
			if (SndCheckbox("Anti-Crash Shield", &config::antiCrash)) {
				if (config::antiCrash)
					luascripts::RunLuaScript(luascripts::LUA_ANTICRASH_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_ANTICRASH_STOP);
			}
			ImGui::SameLine();
			if (SndSmallButton("Crash Log")) luascripts::QueryLuaScript(luascripts::LUA_ANTICRASH_READ, &s_scanResult);

			// Prop Kill
			if (SndCheckbox("Prop Kill", &config::propKill)) {
				if (config::propKill)
					luascripts::RunLuaScript(luascripts::LUA_PROP_KILL_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_PROP_KILL_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Hurl props at nearest enemy");

			// Server Crash
			if (SndCheckbox("Server Crash", &config::serverCrash)) {
				if (config::serverCrash)
					luascripts::RunLuaScript(luascripts::LUA_SERVER_CRASH);
				else
					luascripts::RunLuaScript(luascripts::LUA_SERVER_CRASH_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.f, 0.2f, 0.2f, 1.f), "Entity/net/particle spam");

			// Teleport Exploit
			if (SndSmallButton("Teleport Exploit")) luascripts::QueryLuaScript(luascripts::LUA_TELEPORT_EXPLOIT, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Vehicle + prediction warp");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 0.6f, 1.f));
			ImGui::SeparatorText("COMBAT EXPLOITS");
			ImGui::PopStyleColor();

			SndCheckbox("Anti-Aim", &config::anti_aim);
			if (config::anti_aim) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(120.f);
				const char* aaModes[] = { "Jitter", "Spin", "Backwards", "Down" };
				ImGui::Combo("##aa_mode", &config::anti_aim_mode, aaModes, 4);
			}

			SndCheckbox("Speed Hack", &config::speed_hack);
			if (config::speed_hack) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(120.f);
				ImGui::SliderFloat("##speed_mul", &config::speed_multiplier, 1.5f, 10.f, "%.1fx");
			}

			if (SndCheckbox("Rapid Fire", &config::rapid_fire)) {
				if (config::rapid_fire)
					luascripts::RunLuaScript(luascripts::LUA_RAPID_FIRE_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_RAPID_FIRE_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Zero fire cooldown + auto-reload");

			if (SndCheckbox("No Fall Damage", &config::no_fall_damage)) {
				if (config::no_fall_damage)
					luascripts::RunLuaScript(luascripts::LUA_NO_FALL_DAMAGE_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_NO_FALL_DAMAGE_STOP);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.6f, 1.f));
			ImGui::SeparatorText("SOCIAL EXPLOITS");
			ImGui::PopStyleColor();

			if (SndSmallButton("Steal Entities")) luascripts::QueryLuaScript(luascripts::LUA_ENTITY_STEAL, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "CPPI ownership claim (500u radius)");

			if (SndCheckbox("Chat Spam", &config::chat_spam)) {
				if (config::chat_spam) {
					char buf[256];
					snprintf(buf, sizeof(buf), luascripts::LUA_CHAT_SPAM_SET,
						config::chat_spam_msg, std::to_string(config::chat_spam_delay).c_str());
					luascripts::RunLuaScript(luascripts::LUA_CHAT_SPAM_SETUP);
					lualoader::Execute(std::string(buf));
				} else {
					luascripts::RunLuaScript(luascripts::LUA_CHAT_SPAM_STOP);
				}
			}
			if (config::chat_spam) {
				ImGui::SetNextItemWidth(200.f);
				if (ImGui::InputText("##spam_msg", config::chat_spam_msg, sizeof(config::chat_spam_msg))) {
					char buf[256];
					snprintf(buf, sizeof(buf), luascripts::LUA_CHAT_SPAM_SET,
						config::chat_spam_msg, std::to_string(config::chat_spam_delay).c_str());
					lualoader::Execute(std::string(buf));
				}
				ImGui::SameLine();
				ImGui::SetNextItemWidth(80.f);
				if (ImGui::SliderFloat("##spam_delay", &config::chat_spam_delay, 0.5f, 10.f, "%.1fs")) {
					char buf[256];
					snprintf(buf, sizeof(buf), luascripts::LUA_CHAT_SPAM_SET,
						config::chat_spam_msg, std::to_string(config::chat_spam_delay).c_str());
					lualoader::Execute(std::string(buf));
				}
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.1f, 0.1f, 1.f));
			ImGui::SeparatorText("TARGETED ATTACKS");
			ImGui::PopStyleColor();

			ImGui::Text("Crasher Target:");
			ImGui::SameLine();
			{
				auto& allBones = config::BoneRead();
				int localIdx = interfaces::engine ? interfaces::engine->GetLocalPlayer() : -1;
				ImGui::SetNextItemWidth(200.f);
				if (ImGui::BeginCombo("##crash_target", config::crasher_target >= 0 ?
					(allBones[config::crasher_target].valid ? allBones[config::crasher_target].name : "Invalid") : "Select...")) {
					for (int i = 1; i < 128; i++) {
						if (i == localIdx) continue;
						if (!allBones[i].valid) continue;
						const char* pn = allBones[i].rpName[0] ? allBones[i].rpName : (allBones[i].name[0] ? allBones[i].name : nullptr);
						if (!pn) continue;
						bool sel = (config::crasher_target == i);
						if (ImGui::Selectable(pn, sel))
							config::crasher_target = i;
					}
					ImGui::EndCombo();
				}
			}
			if (config::crasher_target >= 0) {
				ImGui::SameLine();
				if (SndSmallButton("CRASH")) {
					char buf[128];
					snprintf(buf, sizeof(buf), luascripts::LUA_PLAYER_CRASHER, config::crasher_target);
					lualoader::Execute(std::string(buf));
				}
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 1.f, 1.f));
			ImGui::SeparatorText("ORIGINAL EXPLOITS v2");
			ImGui::PopStyleColor();

			// Server Lua Dumper
			if (SndCheckbox("Server Lua Dumper", &config::server_dump)) {
				if (config::server_dump)
					luascripts::RunLuaScript(luascripts::LUA_SERVER_DUMP_SETUP);
			}
			ImGui::SameLine();
			if (SndSmallButton("Dump Status")) luascripts::QueryLuaScript(luascripts::LUA_SERVER_DUMP_READ, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Saves to garrysmod/data/fdll_dump/");

			// Spectator Camera Mirror
			if (SndCheckbox("Spectator Mirror (PIP)", &config::spec_mirror)) {
				if (config::spec_mirror)
					luascripts::RunLuaScript(luascripts::LUA_SPEC_MIRROR_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_SPEC_MIRROR_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "See spectator's POV live");

			// Movement Predictor
			if (SndCheckbox("Movement Predictor", &config::movement_predict)) {
				if (config::movement_predict)
					luascripts::RunLuaScript(luascripts::LUA_MOVEMENT_PREDICT_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_MOVEMENT_PREDICT_STOP);
			}
			ImGui::SameLine();
			if (SndSmallButton("Predict Info")) luascripts::QueryLuaScript(luascripts::LUA_MOVEMENT_PREDICT_READ, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "3D spheres at +5/10/30s");

			// Chat Intelligence
			if (SndCheckbox("Chat Intelligence", &config::chat_intel)) {
				if (config::chat_intel)
					luascripts::RunLuaScript(luascripts::LUA_CHAT_INTEL_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_CHAT_INTEL_STOP);
			}
			ImGui::SameLine();
			if (SndSmallButton("Intel Report")) luascripts::QueryLuaScript(luascripts::LUA_CHAT_INTEL_READ, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Parses money/raid/admin chat");

			// Server Vuln Scanner
			if (SndSmallButton("Vuln Scan Server")) luascripts::QueryLuaScript(luascripts::LUA_VULN_SCAN, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Net receivers, cmds, globals, debug/io access");

			// Aimbot Humanizer
			if (SndCheckbox("Aimbot Humanizer", &config::aim_humanizer)) {
				if (config::aim_humanizer)
					luascripts::RunLuaScript(luascripts::LUA_AIM_HUMANIZER_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_AIM_HUMANIZER_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Bezier curves + micro-jitter (legit aim)");

			// Macro System
			if (SndCheckbox("Macro System", &config::macro_system)) {
				if (config::macro_system)
					luascripts::RunLuaScript(luascripts::LUA_MACRO_SYSTEM_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_MACRO_STOP);
			}
			if (config::macro_system) {
				ImGui::SetNextItemWidth(150.f);
				const char* macros[] = {"raid_prep", "escape", "annoy", "lockdown", "sprint_jump", "distraction", "money_beg"};
				ImGui::Combo("##macro_sel", &config::macro_selected, macros, 7);
				ImGui::SameLine();
				if (SndSmallButton("Execute##macro")) {
					const char* macroNames[] = {"raid_prep", "escape", "annoy", "lockdown", "sprint_jump", "distraction", "money_beg"};
					char buf[256];
					snprintf(buf, sizeof(buf), luascripts::LUA_MACRO_EXECUTE,
						macroNames[config::macro_selected], macroNames[config::macro_selected], macroNames[config::macro_selected]);
					luascripts::QueryLuaScript(buf, &s_scanResult);
				}
				ImGui::SameLine();
				if (SndSmallButton("List All##macro")) luascripts::QueryLuaScript(luascripts::LUA_MACRO_LIST, &s_scanResult);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.85f, 0.f, 1.f));
			ImGui::SeparatorText("SERVER RECON (Custom RP)");
			ImGui::PopStyleColor();

			if (SndSmallButton("Full Recon Dump")) luascripts::QueryLuaScript(luascripts::LUA_RECON_DUMP, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Net msgs, jobs, ents, hooks, addons");

			if (SndSmallButton("Job Exploits")) luascripts::QueryLuaScript(luascripts::LUA_JOB_EXPLOIT_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Entity Classes")) luascripts::QueryLuaScript(luascripts::LUA_ENTITY_CLASS_DUMP, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("NW Var Scan")) luascripts::QueryLuaScript(luascripts::LUA_NW_EXPLOIT_SCAN, &s_scanResult);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
			ImGui::SeparatorText("DUPLICATION EXPLOITS");
			ImGui::PopStyleColor();

			if (SndSmallButton("Dupe Scan")) luascripts::QueryLuaScript(luascripts::LUA_DUPE_SCAN, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Find all dupe vectors on this server");

			// Net Burst Dupe (the big one)
			if (SndCheckbox("Net Capture (Dupe)", &config::dupe_net_capture)) {
				if (config::dupe_net_capture)
					luascripts::RunLuaScript(luascripts::LUA_DUPE_NET_BURST_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_DUPE_NET_BURST_STOP);
			}
			if (config::dupe_net_capture) {
				ImGui::SameLine();
				if (SndSmallButton("Status##dupe")) luascripts::QueryLuaScript(luascripts::LUA_DUPE_NET_BURST_STATUS, &s_scanResult);
				ImGui::SetNextItemWidth(80.f);
				ImGui::SliderInt("Burst Count", &config::dupe_burst_count, 2, 50);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(100.f);
				ImGui::SliderFloat("Delay##burst", &config::dupe_burst_delay, 0.0f, 0.5f, "%.3fs");
				ImGui::SameLine();
				if (SndSmallButton("FIRE BURST")) {
					char buf[256];
					snprintf(buf, sizeof(buf), luascripts::LUA_DUPE_NET_BURST_FIRE,
						config::dupe_burst_count, config::dupe_burst_delay);
					luascripts::QueryLuaScript(buf, &s_scanResult);
				}
			}
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.f), "Buy an item -> FIRE BURST to replay x%d", config::dupe_burst_count);

			// Buy Command Burst
			if (SndCheckbox("Buy Cmd Capture", &config::dupe_buy_capture)) {
				if (config::dupe_buy_capture)
					luascripts::RunLuaScript(luascripts::LUA_DUPE_BUY_BURST_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_DUPE_BUY_BURST_STOP);
			}
			if (config::dupe_buy_capture) {
				ImGui::SameLine();
				if (SndSmallButton("Fire Buy Burst")) {
					char buf[128];
					snprintf(buf, sizeof(buf), luascripts::LUA_DUPE_BUY_BURST_FIRE, config::dupe_burst_count);
					luascripts::QueryLuaScript(buf, &s_scanResult);
				}
			}

			// Quick dupe buttons
			if (SndSmallButton("Pocket Dupe")) luascripts::QueryLuaScript(luascripts::LUA_DUPE_POCKET, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Shipment Extract")) luascripts::QueryLuaScript(luascripts::LUA_DUPE_SHIPMENT, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Printer Multiply")) luascripts::QueryLuaScript(luascripts::LUA_DUPE_PRINTER, &s_scanResult);

			if (SndSmallButton("Duplicator Force")) luascripts::QueryLuaScript(luascripts::LUA_DUPE_DUPLICATOR, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Entity Snapshot")) luascripts::QueryLuaScript(luascripts::LUA_DUPE_SNAPSHOT, &s_scanResult);

			// Auto dupe loop
			if (SndCheckbox("Auto-Dupe Loop", &config::dupe_auto_loop)) {
				if (config::dupe_auto_loop) {
					char buf[128];
					snprintf(buf, sizeof(buf), luascripts::LUA_DUPE_AUTO_SETUP, config::dupe_auto_interval);
					luascripts::RunLuaScript(buf);
				} else {
					luascripts::RunLuaScript(luascripts::LUA_DUPE_AUTO_STOP);
				}
			}
			if (config::dupe_auto_loop) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(120.f);
				if (ImGui::SliderFloat("Interval##autodupe", &config::dupe_auto_interval, 0.1f, 5.0f, "%.1fs")) {
					luascripts::RunLuaScript(luascripts::LUA_DUPE_AUTO_STOP);
					char buf[128];
					snprintf(buf, sizeof(buf), luascripts::LUA_DUPE_AUTO_SETUP, config::dupe_auto_interval);
					luascripts::RunLuaScript(buf);
				}
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 0.5f, 1.f));
			ImGui::SeparatorText("RENDER EXPLOITS");
			ImGui::PopStyleColor();

			if (SndCheckbox("HDR Brightness Stack", &config::hdr_stack)) {
				if (config::hdr_stack)
					luascripts::RunLuaScript(luascripts::LUA_HDR_STACK_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_HDR_STACK_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Compound brightness boost");

			if (SndCheckbox("Material Glow (Halos)", &config::mat_glow)) {
				if (config::mat_glow)
					luascripts::RunLuaScript(luascripts::LUA_MAT_GLOW_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_MAT_GLOW_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Orange glow outline on players");

			if (SndCheckbox("Render Override (IgnoreZ)", &config::render_override)) {
				if (config::render_override)
					luascripts::RunLuaScript(luascripts::LUA_RENDER_OVERRIDE_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_RENDER_OVERRIDE_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Green silhouettes through walls");

			if (SndCheckbox("Physics Prediction", &config::phys_predict)) {
				if (config::phys_predict)
					luascripts::RunLuaScript(luascripts::LUA_PHYS_PREDICT_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_PHYS_PREDICT_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "3D spheres showing future positions");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.f, 1.f, 1.f));
			ImGui::SeparatorText("STEALTH ENGINE");
			ImGui::PopStyleColor();

			if (SndCheckbox("Error Sanitizer", &config::error_sanitizer)) {
				if (config::error_sanitizer)
					luascripts::RunLuaScript(luascripts::LUA_ERROR_SANITIZER_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_ERROR_SANITIZER_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Hide exploit errors from console");

			if (SndCheckbox("CRC/File Spoof", &config::crc_spoof)) {
				if (config::crc_spoof)
					luascripts::RunLuaScript(luascripts::LUA_CRC_SPOOF_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_CRC_SPOOF_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Spoof CRC + hide cheat files");

			if (SndCheckbox("Stack Trace Spoof", &config::stack_spoof)) {
				if (config::stack_spoof)
					luascripts::RunLuaScript(luascripts::LUA_STACK_SPOOF_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_STACK_SPOOF_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Hide RunString from debug.traceback");

			if (SndCheckbox("Hook Table Sanitizer", &config::hook_sanitizer)) {
				if (config::hook_sanitizer)
					luascripts::RunLuaScript(luascripts::LUA_HOOK_SANITIZER_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_HOOK_SANITIZER_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Cleans _fdll_ hooks from scans");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.6f, 0.f, 1.f));
			ImGui::SeparatorText("ECONOMY EXPLOITS");
			ImGui::PopStyleColor();

			if (SndSmallButton("Tip/Donate Exploit")) luascripts::QueryLuaScript(luascripts::LUA_TIP_EXPLOIT, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Trade Sniper")) luascripts::QueryLuaScript(luascripts::LUA_TRADE_SNIPER, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Hit/Bounty Dupe")) luascripts::QueryLuaScript(luascripts::LUA_HIT_DUPE, &s_scanResult);

			if (SndSmallButton("Salary Arbitrage")) luascripts::QueryLuaScript(luascripts::LUA_SALARY_ARBITRAGE, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("ConVar Trigger")) luascripts::QueryLuaScript(luascripts::LUA_CONVAR_TRIGGER, &s_scanResult);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.8f, 1.f, 1.f));
			ImGui::SeparatorText("MOVEMENT & UTILITY");
			ImGui::PopStyleColor();

			if (SndSmallButton("Constraint Catapult")) luascripts::QueryLuaScript(luascripts::LUA_CONSTRAINT_CATAPULT, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Launch via nearest prop");

			if (SndCheckbox("Vehicle Speed Boost", &config::vehicle_boost)) {
				if (config::vehicle_boost)
					luascripts::RunLuaScript(luascripts::LUA_VEHICLE_BOOST_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_VEHICLE_BOOST_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "3x vehicle speed");

			if (SndCheckbox("Animation Override", &config::anim_override)) {
				if (config::anim_override)
					luascripts::RunLuaScript(luascripts::LUA_ANIM_OVERRIDE_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_ANIM_OVERRIDE_STOP);
			}
			if (config::anim_override) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(120.f);
				const char* animModes[] = {"Off", "T-Pose", "Zombie", "Dance", "Play Dead"};
				if (ImGui::Combo("##anim_mode", &config::anim_mode, animModes, 5)) {
					char buf[64];
					snprintf(buf, sizeof(buf), "_fdll_anim_mode = %d", config::anim_mode);
					lualoader::Execute(std::string(buf));
				}
			}

			if (SndCheckbox("Sound Mask", &config::sound_mask)) {
				if (config::sound_mask)
					luascripts::RunLuaScript(luascripts::LUA_SOUND_MASK_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_SOUND_MASK_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Mute own footsteps, amplify enemies");

			if (SndCheckbox("Death Cam (Freecam)", &config::death_cam)) {
				if (config::death_cam)
					luascripts::RunLuaScript(luascripts::LUA_DEATHCAM_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_DEATHCAM_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "WASD freecam while dead");

			if (SndSmallButton("Fake Admin Panel")) luascripts::QueryLuaScript(luascripts::LUA_FAKE_ADMIN_PANEL, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Spawn fake ULX menu");

			if (SndSmallButton("Prop Shield")) luascripts::QueryLuaScript(luascripts::LUA_PROP_SHIELD, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Clear Shields")) luascripts::RunLuaScript(luascripts::LUA_PROP_SHIELD_CLEAR);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Invisible clientside barriers");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.f, 1.f));
			ImGui::SeparatorText("Intelligence & Exploitation");
			ImGui::PopStyleColor();

			if (SndSmallButton("ConCmd Fuzzer")) luascripts::QueryLuaScript(luascripts::LUA_CONCOMMAND_FUZZER, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Raid Intel")) luascripts::QueryLuaScript(luascripts::LUA_RAID_INTEL, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Economy Destroy")) luascripts::QueryLuaScript(luascripts::LUA_ECONOMY_DESTROY, &s_scanResult);

			if (SndSmallButton("Spawn Exploiter")) luascripts::QueryLuaScript(luascripts::LUA_SPAWN_EXPLOITER, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Auto-Pocket All")) luascripts::QueryLuaScript(luascripts::LUA_AUTO_POCKET, &s_scanResult);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.9f, 1.f, 1.f));
			ImGui::SeparatorText("DarkRP Intelligence");
			ImGui::PopStyleColor();

			if (SndCheckbox("Warrant Shield", &config::warrantShield)) {
				if (config::warrantShield)
					luascripts::RunLuaScript(luascripts::LUA_WARRANT_SHIELD_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_WARRANT_SHIELD_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Auto-hide on warrant");

			if (SndCheckbox("Anti-Arrest", &config::antiArrestEnabled)) {
				if (config::antiArrestEnabled)
					luascripts::RunLuaScript(luascripts::LUA_ANTI_ARREST_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_ANTI_ARREST_STOP);
			}
			ImGui::SameLine();

			if (SndCheckbox("Base Alarm", &config::baseAlarmEnabled)) {
				if (config::baseAlarmEnabled)
					luascripts::RunLuaScript(luascripts::LUA_BASE_ALARM_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_BASE_ALARM_STOP);
			}

			if (SndCheckbox("Auto-Bounty", &config::autoBountyEnabled)) {
				if (config::autoBountyEnabled)
					luascripts::RunLuaScript(luascripts::LUA_AUTO_BOUNTY_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_AUTO_BOUNTY_STOP);
			}
			ImGui::SameLine();

			if (SndCheckbox("Door Auto-Close", &config::doorAutoClose)) {
				if (config::doorAutoClose)
					luascripts::RunLuaScript(luascripts::LUA_DOOR_AUTOCLOSE_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_DOOR_AUTOCLOSE_STOP);
			}

			if (SndCheckbox("Proximity Alert", &config::proximityAlertEnabled)) {
				if (config::proximityAlertEnabled)
					luascripts::RunLuaScript(luascripts::LUA_PROXIMITY_ALERT_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_PROXIMITY_ALERT_STOP);
			}
			ImGui::SameLine();

			if (SndCheckbox("Loot Vacuum", &config::lootVacuumEnabled)) {
				if (config::lootVacuumEnabled)
					luascripts::RunLuaScript(luascripts::LUA_LOOT_VACUUM_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_LOOT_VACUUM_STOP);
			}

			if (SndSmallButton("Police Scanner")) luascripts::QueryLuaScript(luascripts::LUA_DARKRP_POLICE_SCANNER, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Economy Analyzer")) luascripts::QueryLuaScript(luascripts::LUA_ECONOMY_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Hitman Intel")) luascripts::QueryLuaScript(luascripts::LUA_HITMAN_ABUSE, &s_scanResult);

			if (SndSmallButton("Identity Clone")) luascripts::QueryLuaScript(luascripts::LUA_IMPERSONATE, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Server Lua Dump")) luascripts::QueryLuaScript(luascripts::LUA_BACKDOOR_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Shipment Scan")) luascripts::QueryLuaScript(luascripts::LUA_DARKRP_ENTITY_ABUSE, &s_scanResult);

			if (SndSmallButton("Base Scanner")) luascripts::QueryLuaScript(luascripts::LUA_DOOR_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Auto-Demote")) luascripts::QueryLuaScript(luascripts::LUA_AUTO_WARRANT, &s_scanResult);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.f, 0.3f, 1.f));
			ImGui::SeparatorText("Voice Intelligence");
			ImGui::PopStyleColor();

			SndCheckbox("Voice Exploits##ve_master", &voice_exploits::enabled);
			if (voice_exploits::enabled) {
				SndCheckbox("Intercept Channels", &voice_exploits::intercept_channels);
				ImGui::SameLine();
				SndCheckbox("Force Unmute", &voice_exploits::force_unmute);

				if (SndCheckbox("Volume Boost", &voice_exploits::volume_boost))
					voice_exploits::SyncSettings();
				if (voice_exploits::volume_boost) {
					ImGui::SameLine();
					ImGui::SetNextItemWidth(100.f);
					if (ImGui::SliderFloat("##boost_lvl", &voice_exploits::boost_level, 1.0f, 5.0f, "%.1fx"))
						voice_exploits::SyncSettings();
				}

				SndCheckbox("Activity ESP", &voice_exploits::activity_esp);
				ImGui::SameLine();
				SndCheckbox("Direction Arrows", &voice_exploits::direction_arrows);

				SndCheckbox("Raid Alert", &voice_exploits::raid_alert);
				ImGui::SameLine();
				SndCheckbox("Voice Radar", &voice_exploits::voice_radar);

				SndCheckbox("Social Mapper", &voice_exploits::social_mapper);
				ImGui::SameLine();
				SndCheckbox("Pattern Profiler", &voice_exploits::pattern_profiler);

				SndCheckbox("File Logger", &voice_exploits::file_logger);
				if (voice_exploits::file_logger) {
					ImGui::SameLine();
					if (SndSmallButton("Save Log Now"))
						voice_exploits::SaveLog();
				}

				if (SndCheckbox("Freecam Proximity##vc_fcp", &voice_exploits::freecam_proximity))
					voice_exploits::SyncSettings();
				if (voice_exploits::freecam_proximity) {
					ImGui::SameLine();
					ImGui::SetNextItemWidth(120.f);
					if (ImGui::SliderFloat("##fc_radius", &voice_exploits::freecam_radius, 5.0f, 2000.0f, "%.0fm"))
						voice_exploits::SyncSettings();
				}
			}

			// Puppet Mode
			if (SndCheckbox("Record Puppet", &config::puppetRecording)) {
				if (config::puppetRecording)
					luascripts::RunLuaScript(luascripts::LUA_PUPPET_RECORD_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_PUPPET_RECORD_STOP);
			}
			ImGui::SameLine();
			if (SndSmallButton("Replay Puppet")) luascripts::QueryLuaScript(luascripts::LUA_PUPPET_REPLAY, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Aim at player first");

			if (!s_scanResult.empty()) {
				ImGui::BeginChild("##exploitscanresult", ImVec2(0.f, 130.f), true, ImGuiWindowFlags_HorizontalScrollbar);
				ImGui::TextUnformatted(s_scanResult.c_str());
				ImGui::EndChild();
				ImGui::SameLine();
				if (SndSmallButton("Clear##exploitscan")) s_scanResult.clear();
			}
			break;
		}

		// ── World ─────────────────────────────────────────────────────────────
		case 8:
		{
			static std::string s_scanResult;

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.f, 0.6f, 1.f));
			ImGui::SeparatorText("Player Manipulation");
			ImGui::PopStyleColor();

			// Model Changer
			static char s_modelPath[128] = "models/player/kleiner.mdl";
			ImGui::InputText("Model##changer", s_modelPath, sizeof(s_modelPath));
			ImGui::SameLine();
			if (SndSmallButton("Set Model")) {
				std::string script = std::string("pcall(function() LocalPlayer():SetModel(\"") + s_modelPath + "\") end)";
				lualoader::QueueScript(script);
			}
			ImGui::SameLine();
			if (SndSmallButton("Reset Model")) {
				lualoader::QueueScript("pcall(function() RunConsoleCommand(\"cl_playermodel\", \"kleiner\") end)");
			}

			// Name Stealer
			if (SndCheckbox("Name Stealer", &config::nameSteal)) {
				if (config::nameSteal)
					luascripts::RunLuaScript(luascripts::LUA_NAME_STEALER_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_NAME_STEALER_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Cycle others' names");

			// Anti-Kick
			if (SndCheckbox("Anti-Kick Shield", &config::antiKick)) {
				if (config::antiKick)
					luascripts::RunLuaScript(luascripts::LUA_ANTIKICK_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_ANTIKICK_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Auto vote-no on kicks");

			// Fake Death
			if (SndCheckbox("Fake Death", &config::fakeDeath)) {
				if (config::fakeDeath)
					luascripts::RunLuaScript(luascripts::LUA_FAKE_DEATH_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_FAKE_DEATH_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Play ragdoll, stay alive");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.8f, 0.2f, 1.f));
			ImGui::SeparatorText("World Manipulation");
			ImGui::PopStyleColor();

			// Material Wallhack
			if (SndCheckbox("Material Wallhack", &config::matWallhack)) {
				if (config::matWallhack)
					luascripts::RunLuaScript(luascripts::LUA_MAT_WALLHACK_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_MAT_WALLHACK_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "See through walls");

			// Gravity Manipulator
			if (SndCheckbox("Low Gravity", &config::lowGrav)) {
				if (config::lowGrav)
					luascripts::RunLuaScript(luascripts::LUA_LOW_GRAVITY_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_LOW_GRAVITY_STOP);
			}

			// Prop Launcher
			if (SndSmallButton("Prop Launcher")) luascripts::RunLuaScript(luascripts::LUA_PROP_LAUNCHER);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Punt prop at crosshair target");

			// Sound Spammer
			if (SndCheckbox("Sound Spammer", &config::soundSpam)) {
				if (config::soundSpam)
					luascripts::RunLuaScript(luascripts::LUA_SOUND_SPAM_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_SOUND_SPAM_STOP);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 1.f, 1.f));
			ImGui::SeparatorText("Automation");
			ImGui::PopStyleColor();

			// Auto-Buy
			if (SndCheckbox("Auto-Buy on Spawn", &config::autoBuy)) {
				if (config::autoBuy)
					luascripts::RunLuaScript(luascripts::LUA_AUTOBUY_SETUP);
				else
					luascripts::RunLuaScript(luascripts::LUA_AUTOBUY_STOP);
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Buy weapons + armor");

			// Crosshair Info
			SndCheckbox("Crosshair Target Info", &config::crosshair_info);

			// Kill Streak
			SndCheckbox("Kill Streak Tracker", &config::killstreak_enabled);

			// Auto-Arrest Chain
			if (SndSmallButton("Auto-Arrest Chain")) luascripts::QueryLuaScript(luascripts::LUA_AUTO_ARREST_CHAIN, &s_scanResult);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Warrant+Arrest+Jail combo");

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.3f, 0.3f, 1.f));
			ImGui::SeparatorText("Map Exploitation");
			ImGui::PopStyleColor();

			if (SndSmallButton("Map OOB Scanner")) luascripts::QueryLuaScript(luascripts::LUA_MAP_OOB_SCAN, &s_scanResult);
			ImGui::SameLine();
			if (SndSmallButton("Skybox Holes")) luascripts::QueryLuaScript(luascripts::LUA_SKYBOX_SCAN, &s_scanResult);

			if (!s_scanResult.empty()) {
				ImGui::BeginChild("##worldscan", ImVec2(0.f, 130.f), true, ImGuiWindowFlags_HorizontalScrollbar);
				ImGui::TextUnformatted(s_scanResult.c_str());
				ImGui::EndChild();
				ImGui::SameLine();
				if (SndSmallButton("Clear##worldscan")) s_scanResult.clear();
			}
			break;
		}

		// ── Config ────────────────────────────────────────────────────────────
		case 9:
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.706f, 0.847f, 1.f));
			ImGui::SeparatorText("Config");
			ImGui::PopStyleColor();

			if (SndButton("Save Config", ImVec2(130, 0))) config_io::Save();
			ImGui::SameLine();
			if (SndButton("Load Config", ImVec2(130, 0))) config_io::Load();
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "friendlydll.cfg");
			ImGui::Spacing();

			SndCheckbox("Chat on Death", &config::chatondeath);

			SndCheckbox("sv_allowcslua", &config::allowcslua);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "(detected, not recommended)");

			SndCheckbox("sv_cheats", &config::allowcheats);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "(detected, not recommended)");

			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.3f, 0.3f, 1.f));
			ImGui::SeparatorText("Eject");
			ImGui::PopStyleColor();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.08f, 0.08f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.15f, 0.15f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 0.2f, 0.2f, 1.f));
			if (ImGui::Button("Eject DLL", ImVec2(280, 32))) {
				menu_sound::Click();
				spdlog::info("[eject] Eject button pressed!");
				config::g_requestEject.store(true, std::memory_order_release);
			}
			ImGui::PopStyleColor(3);
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Safely unhooks & unloads from memory");
			break;
		}

		default: break;
		} // switch (activeTab)

		ImGui::EndChild(); // ##content_area
		ImGui::PopStyleVar(); // Alpha for tab transition

		// ── Status bar at bottom ────────────────────────────────────────────
		{
			float statusY = wPos.y + wSize.y - 24.f;
			dl->AddLine(ImVec2(wPos.x, statusY), ImVec2(wPos.x + wSize.x, statusY),
				IM_COL32(30, 30, 36, 255), 1.f);
			dl->AddRectFilled(ImVec2(wPos.x, statusY), ImVec2(wPos.x + wSize.x, wPos.y + wSize.y),
				IM_COL32(10, 10, 14, 255));

			float sfz = d3d9hook::editorFont ? d3d9hook::editorFont->LegacySize * 0.85f : 12.f;

			// Left: active features count
			int activeCount = 0;
			if (config::aimbot) activeCount++;
			if (config::bunnyhop) activeCount++;
			if (config::snapline || config::squareesp || config::boneskeleton) activeCount++;
			if (config::chams) activeCount++;
			if (config::triggerbot) activeCount++;
			if (config::backtrack) activeCount++;
			if (config::entity_esp) activeCount++;
			if (antiaim::enabled) activeCount++;
			if (fakelag::enabled) activeCount++;
			if (tick_exploits::lagswitch_enabled) activeCount++;
			if (tick_exploits::doubletap_enabled) activeCount++;
			if (tick_exploits::speedhack_enabled) activeCount++;

			char statusLeft[64];
			snprintf(statusLeft, sizeof(statusLeft), "%d features active", activeCount);

			ImU32 statusCol = activeCount > 0
				? IM_COL32(0, 180, 216, 180)
				: IM_COL32(80, 85, 100, 180);
			dl->AddText(d3d9hook::editorFont, sfz,
				ImVec2(wPos.x + 12.f, statusY + 5.f), statusCol, statusLeft);

			// Center: animated dots
			float dotPhase = (float)ImGui::GetTime();
			float dotCenterX = wPos.x + wSize.x * 0.5f;
			for (int d = 0; d < 3; ++d) {
				float dp = sinf(dotPhase * 3.f + d * 0.8f) * 0.5f + 0.5f;
				int da = (int)(dp * 160.f + 40.f);
				dl->AddCircleFilled(
					ImVec2(dotCenterX - 10.f + d * 10.f, statusY + 12.f),
					2.f, IM_COL32(0, 180, 216, da), 8);
			}

			// Right: velocity
			char statusRight[32];
			snprintf(statusRight, sizeof(statusRight), "%.0f u/s", config::currentvelocity);
			ImVec2 rvSz = d3d9hook::editorFont->CalcTextSizeA(sfz, FLT_MAX, 0.f, statusRight);
			dl->AddText(d3d9hook::editorFont, sfz,
				ImVec2(wPos.x + wSize.x - rvSz.x - 12.f, statusY + 5.f),
				IM_COL32(80, 85, 100, 180), statusRight);
		}

		ui_window::EndWindow();
		}
	}
	skip_menu:

	if (config::g_inGame.load(std::memory_order_relaxed) && interfaces::engine && interfaces::engine->IsInGame()) {
		ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
		static const std::pair<Bones, Bones> boneConnections[] = {
			{ bone_head,      bone_neck      },
			{ bone_neck,      bone_spine_1   },
			{ bone_spine_1,   bone_spine_3   },
			{ bone_spine_3,   bone_spine_2   },
			{ bone_spine_2,   bone_pelvis    },
			{ bone_spine_1,   bone_arm_top_l },
			{ bone_arm_top_l, bone_arm_bot_l },
			{ bone_arm_bot_l, bone_hand_l    },
			{ bone_spine_1,   bone_arm_top_r },
			{ bone_arm_top_r, bone_arm_bot_r },
			{ bone_arm_bot_r, bone_hand_r    },
			{ bone_pelvis,    bone_leg_top_l },
			{ bone_leg_top_l, bone_leg_bot_l },
			{ bone_leg_bot_l, bone_ANKLE_l   },
			{ bone_pelvis,    bone_leg_top_r },
			{ bone_leg_top_r, bone_leg_bot_r },
			{ bone_leg_bot_r, bone_ANKLE_r   },
		};

		int dbg_bones = 0;

		// Grab a snapshot of the front buffer — render thread reads ONLY cached data
		auto& readCache = config::BoneRead();
		config::dbg_cache_valid = 0;
		for (int ci = 0; ci < 128; ++ci)
			if (readCache[ci].valid) ++config::dbg_cache_valid;

		{ debug::Breadcrumb _bc(debug::Phase::EspLoop);

		auto* dl = ImGui::GetBackgroundDrawList();
		const float fontSize = (d3d9hook::editorFont->LegacySize > 1.f) ? d3d9hook::editorFont->LegacySize : 14.f;
		const float smallFont = fontSize * 0.85f;
		const float pad = 5.f;
		const float lineH = fontSize + 2.f;
		const float smallLineH = smallFont + 2.f;

		const ImU32 kPanelBg    = IM_COL32(12, 12, 18, 100);
		const ImU32 kPanelBrd   = IM_COL32(40, 40, 55, 80);
		const ImU32 kShadow     = IM_COL32(0, 0, 0, 200);
		const ImU32 kAccent     = IM_COL32(0, 180, 216, 255);
		const ImU32 kNameCol    = IM_COL32(235, 240, 250, 255);
		const ImU32 kJobCol     = IM_COL32(100, 200, 255, 220);
		const ImU32 kGangCol    = IM_COL32(255, 180, 60, 200);
		const ImU32 kWeaponCol  = IM_COL32(255, 220, 80, 220);
		const ImU32 kInvCol     = IM_COL32(150, 155, 165, 180);
		const ImU32 kDistCol    = IM_COL32(100, 110, 130, 180);

		auto textShadow = [&](ImFont* f, float sz, float x, float y, ImU32 col, const char* t) {
			dl->AddText(f, sz, ImVec2(x + 1.f, y + 1.f), kShadow, t);
			dl->AddText(f, sz, ImVec2(x, y), col, t);
		};

		auto centeredText = [&](ImFont* f, float sz, float cx, float y, ImU32 col, const char* t) {
			ImVec2 ts = f->CalcTextSizeA(sz, FLT_MAX, 0.f, t);
			textShadow(f, sz, cx - ts.x * 0.5f, y, col, t);
		};

		auto boneOk = [](const Vector& a, const Vector& b, float maxDist) -> bool {
			float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
			return (dx * dx + dy * dy + dz * dz) < (maxDist * maxDist);
		};

		for (int i = 0; i < 128; ++i)
		{
			if (!readCache[i].valid) continue;
			if (!config::snapline && !config::squareesp && !config::boneskeleton) continue;

			const auto& rec = readCache[i];

			if (rec.distance < config::esp_min_dist || rec.distance > config::esp_max_dist) continue;
			if (!config::IsTargetAllowed(i)) continue;
			if (rec.dormant && !config::show_dormant) continue;

			// Dormant or invisible player: draw ghost ESP using origin only
			if (rec.noBones || rec.dormant) {
				if (!config::squareesp) continue;
				Vector ghostHead = rec.absOrigin; ghostHead.z += 72.f;
				float ghx, ghy, gbx, gby;
				if (!config::WorldToScreen(ghostHead, ghx, ghy)) continue;
				if (!config::WorldToScreen(rec.absOrigin, gbx, gby)) continue;
				float gh = fabsf(gby - ghy);
				if (gh < 4.f) continue;
				float gw = gh * 0.35f;

				float pulse = sinf(static_cast<float>(ImGui::GetTime()) * 4.f) * 0.3f + 0.7f;
				bool isDormant = rec.dormant;
				int galpha = isDormant ? static_cast<int>(pulse * 140.f) : static_cast<int>(pulse * 200.f);
				ImU32 ghostCol = isDormant ? IM_COL32(255, 160, 0, galpha) : IM_COL32(255, 50, 50, galpha);
				ImU32 tagCol = isDormant ? IM_COL32(255, 180, 40, galpha) : IM_COL32(255, 80, 80, galpha);

				if (isDormant) {
					float dashLen = 8.f;
					auto drawDashedRect = [&](float x1, float y1, float x2, float y2) {
						ImVec2 corners[4] = {{x1,y1},{x2,y1},{x2,y2},{x1,y2}};
						for (int e = 0; e < 4; ++e) {
							ImVec2 a = corners[e], b = corners[(e+1)%4];
							float dx = b.x-a.x, dy = b.y-a.y;
							float len = sqrtf(dx*dx+dy*dy);
							if (len < 1.f) continue;
							float ux = dx/len, uy = dy/len;
							float d = 0.f;
							bool on = true;
							while (d < len) {
								float seg = (std::min)(dashLen, len-d);
								if (on) dl->AddLine(ImVec2(a.x+ux*d, a.y+uy*d), ImVec2(a.x+ux*(d+seg), a.y+uy*(d+seg)), ghostCol, 2.f);
								d += seg;
								on = !on;
							}
						}
					};
					drawDashedRect(ghx - gw, ghy, ghx + gw, gby);
				} else {
					dl->AddRect(ImVec2(ghx - gw, ghy), ImVec2(ghx + gw, gby), ghostCol, 0.f, 0, 2.f);
				}

				const char* gname = rec.rpName[0] ? rec.rpName : (rec.name[0] ? rec.name : "???");
				ImVec2 gns = d3d9hook::editorFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, gname);
				dl->AddText(d3d9hook::editorFont, fontSize,
					ImVec2(ghx - gns.x * 0.5f + 1.f, ghy - fontSize - 18.f + 1.f), kShadow, gname);
				dl->AddText(d3d9hook::editorFont, fontSize,
					ImVec2(ghx - gns.x * 0.5f, ghy - fontSize - 18.f), ghostCol, gname);

				const char* ghostTag = isDormant ? "DORMANT" : "INVISIBLE";
				ImVec2 gts = d3d9hook::editorFont->CalcTextSizeA(smallFont, FLT_MAX, 0.f, ghostTag);
				dl->AddText(d3d9hook::editorFont, smallFont,
					ImVec2(ghx - gts.x * 0.5f, ghy - 16.f), tagCol, ghostTag);

				if (rec.job[0]) {
					ImVec2 gjs = d3d9hook::editorFont->CalcTextSizeA(smallFont, FLT_MAX, 0.f, rec.job);
					dl->AddText(d3d9hook::editorFont, smallFont,
						ImVec2(ghx - gjs.x * 0.5f, ghy - fontSize - 18.f - smallFont - 2.f),
						IM_COL32(100, 200, 255, static_cast<int>(pulse * 180.f)), rec.job);
				}

				char gdist[16];
				snprintf(gdist, sizeof(gdist), "%.0fm", rec.distance / 52.49f);
				ImVec2 gds = d3d9hook::editorFont->CalcTextSizeA(smallFont * 0.85f, FLT_MAX, 0.f, gdist);
				dl->AddText(d3d9hook::editorFont, smallFont * 0.85f,
					ImVec2(ghx - gds.x * 0.5f, gby + 2.f),
					IM_COL32(100, 110, 130, 180), gdist);

				if (isDormant) {
					char hpBuf[16]; snprintf(hpBuf, sizeof(hpBuf), "%dhp", rec.health);
					ImVec2 hps = d3d9hook::editorFont->CalcTextSizeA(smallFont * 0.85f, FLT_MAX, 0.f, hpBuf);
					dl->AddText(d3d9hook::editorFont, smallFont * 0.85f,
						ImVec2(ghx - hps.x * 0.5f, gby + 2.f + smallFont),
						IM_COL32(180, 220, 100, 160), hpBuf);
				}

				++dbg_bones;
				continue;
			}

			const Matrix3x4* bones = rec.bones;
			Vector headWorld = bones[Bones::bone_head].GetOrigin();
			{
				float dx = headWorld.x - rec.absOrigin.x;
				float dy = headWorld.y - rec.absOrigin.y;
				float dz = headWorld.z - rec.absOrigin.z;
				if (dx * dx + dy * dy + dz * dz > 80.f * 80.f) continue;
			}

			debug::lastEntity.store(i, std::memory_order_relaxed);

			float headX, headY;
			if (!config::WorldToScreen(headWorld, headX, headY))
				continue;

			float bottomX, bottomY;
			if (!config::WorldToScreen(rec.absOrigin, bottomX, bottomY))
				continue;

			++dbg_bones;

			const float h = fabsf(bottomY - headY);
			if (h < 4.f) continue;
			const float w = h * 0.35f;

			const float left  = headX - w;
			const float right = headX + w;
			const float cx = (left + right) * 0.5f;
			const float healthFraction = std::clamp(static_cast<float>(rec.health) * 0.01f, 0.f, 1.f);

			if (config::snapline) {
				ImU32 snapCol = ImColor(config::snapline_color[0], config::snapline_color[1], config::snapline_color[2]);
				dl->AddLine(ImVec2(width * 0.5f, (float)height), ImVec2(bottomX, bottomY), snapCol, 1.2f);
			}

			if (config::squareesp) {
				// ── Name plate (single consolidated panel above box) ──
				const char* displayName = rec.rpName[0] ? rec.rpName : (rec.name[0] ? rec.name : "???");
				ImVec2 nameSz = d3d9hook::editorFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, displayName);

				char moneyPlate[48] = {};
				if (config::money_tracker && rec.money > 0) {
					snprintf(moneyPlate, sizeof(moneyPlate), "$%d", rec.money);
				}

				float plateW = nameSz.x + pad * 4.f;
				auto widenPlate = [&](const char* txt) {
					if (!txt || !txt[0]) return;
					ImVec2 sz = d3d9hook::editorFont->CalcTextSizeA(smallFont, FLT_MAX, 0.f, txt);
					if (sz.x + pad * 4.f > plateW) plateW = sz.x + pad * 4.f;
				};
				widenPlate(rec.job);
				widenPlate(rec.gang);

				int plateLines = 1;
				if (rec.job[0]) plateLines++;
				if (rec.gang[0]) plateLines++;
				if (moneyPlate[0]) plateLines++;
				float plateH = lineH + (plateLines - 1) * smallLineH + pad * 2.f;
				float plateX = cx - plateW * 0.5f;
				float plateY = headY - plateH - 6.f;

				dl->AddRectFilled(ImVec2(plateX, plateY), ImVec2(plateX + plateW, plateY + plateH), kPanelBg, 4.f);
				dl->AddRect(ImVec2(plateX, plateY), ImVec2(plateX + plateW, plateY + plateH), kPanelBrd, 4.f);

				float textY = plateY + pad;
				centeredText(d3d9hook::editorFont, fontSize, cx, textY, kNameCol, displayName);
				textY += lineH;

				if (rec.job[0]) {
					centeredText(d3d9hook::editorFont, smallFont, cx, textY, kJobCol, rec.job);
					textY += smallLineH;
				}
				if (rec.gang[0]) {
					centeredText(d3d9hook::editorFont, smallFont, cx, textY, kGangCol, rec.gang);
					textY += smallLineH;
				}
				if (moneyPlate[0]) {
					centeredText(d3d9hook::editorFont, smallFont, cx, textY, IM_COL32(0, 220, 80, 255), moneyPlate);
				}

				// accent line under name plate
				dl->AddRectFilled(ImVec2(plateX + 2.f, plateY + plateH - 1.f),
				                  ImVec2(plateX + plateW - 2.f, plateY + plateH + 1.f),
				                  IM_COL32(0, 180, 216, 80), 1.f);

				// ── Box ESP ──
				// outer glow
				dl->AddRect(ImVec2(left - 1.f, headY - 1.f), ImVec2(right + 1.f, bottomY + 1.f),
				            IM_COL32(0, 0, 0, 120), 0.f, 0, 1.f);
				// main box
				ImU32 boxCol = ImColor(config::squareesp_color[0], config::squareesp_color[1], config::squareesp_color[2]);
				dl->AddRect(ImVec2(left, headY), ImVec2(right, bottomY), boxCol, 0.f, 0, 1.5f);
				// subtle fill
				dl->AddRectFilled(ImVec2(left, headY), ImVec2(right, bottomY), IM_COL32(0, 0, 0, 30));

				// ── Health bar (left side, rounded, gradient) ──
				float barW = 3.f;
				float barGap = 4.f;
				float barLeft = left - barW - barGap;
				float barH = bottomY - headY;
				dl->AddRectFilled(ImVec2(barLeft - 1.f, headY - 1.f), ImVec2(barLeft + barW + 1.f, bottomY + 1.f),
				                  IM_COL32(0, 0, 0, 120), 2.f);
				float fillH = barH * healthFraction;
				ImU32 hpColTop = IM_COL32(
					(int)((1.f - healthFraction) * 255),
					(int)(healthFraction * 200 + 55), 0, 255);
				ImU32 hpColBot = IM_COL32(
					(int)((1.f - healthFraction) * 200),
					(int)(healthFraction * 255), 0, 255);
				dl->AddRectFilledMultiColor(
					ImVec2(barLeft, bottomY - fillH), ImVec2(barLeft + barW, bottomY),
					hpColTop, hpColTop, hpColBot, hpColBot);

				// ── Below-box info panel ──
				const char* infoTexts[6]; ImU32 infoCols[6]; int infoCount = 0;
				char hpBuf[16]; snprintf(hpBuf, sizeof(hpBuf), "%dhp", rec.health);
				char distBuf[16]; snprintf(distBuf, sizeof(distBuf), "%.0fm", rec.distance * 0.01905f);
				char wlBuf[48]; wlBuf[0] = '\0';

				infoTexts[infoCount] = hpBuf; infoCols[infoCount] = hpColBot; infoCount++;

				if (rec.weapon[0]) {
					infoTexts[infoCount] = rec.weapon; infoCols[infoCount] = kWeaponCol; infoCount++;
				}
				if (rec.weaponList[0]) {
					strncpy_s(wlBuf, rec.weaponList, 44);
					if (strlen(rec.weaponList) > 44) { wlBuf[41] = '.'; wlBuf[42] = '.'; wlBuf[43] = '.'; wlBuf[44] = '\0'; }
					infoTexts[infoCount] = wlBuf; infoCols[infoCount] = kInvCol; infoCount++;
				}
				infoTexts[infoCount] = distBuf; infoCols[infoCount] = kDistCol; infoCount++;

				{
					float infoPad = 4.f;
					float infoH = infoPad * 2.f;
					float maxInfoW = 0.f;
					for (int j = 0; j < infoCount; ++j) {
						ImVec2 sz = d3d9hook::editorFont->CalcTextSizeA(smallFont, FLT_MAX, 0.f, infoTexts[j]);
						if (sz.x > maxInfoW) maxInfoW = sz.x;
						infoH += smallLineH + 1.f;
					}
					float infoW = maxInfoW + infoPad * 4.f;
					float infoX = cx - infoW * 0.5f;
					float infoY = bottomY + 4.f;

					dl->AddRectFilled(ImVec2(infoX, infoY), ImVec2(infoX + infoW, infoY + infoH), kPanelBg, 3.f);
					dl->AddRect(ImVec2(infoX, infoY), ImVec2(infoX + infoW, infoY + infoH), kPanelBrd, 3.f);

					float iy = infoY + infoPad;
					for (int j = 0; j < infoCount; ++j) {
						centeredText(d3d9hook::editorFont, smallFont, cx, iy, infoCols[j], infoTexts[j]);
						iy += smallLineH + 1.f;
					}

					if (config::esp_intel_badges) {
						hud::DrawPlayerIntel(dl, d3d9hook::editorFont, smallFont, cx, infoY + infoH + 2.f, rec);
					}
				}
			}

			if (config::boneskeleton) {
				const ImU32 skelCol = ImColor(config::skeleton_color[0], config::skeleton_color[1], config::skeleton_color[2]);
				const ImU32 skelGlow = IM_COL32(0, 0, 0, 100);
				int badBones = 0;
				for (const auto& [a, b] : boneConnections) {
					Vector boneA = bones[a].GetOrigin();
					Vector boneB = bones[b].GetOrigin();
					if (!boneOk(boneA, rec.absOrigin, 50.f)) { ++badBones; continue; }
					if (!boneOk(boneB, rec.absOrigin, 50.f)) { ++badBones; continue; }
					if (!boneOk(boneA, boneB, 30.f)) { ++badBones; continue; }
					float scrAx, scrAy, scrBx, scrBy;
					if (!config::WorldToScreen(boneA, scrAx, scrAy)) continue;
					if (!config::WorldToScreen(boneB, scrBx, scrBy)) continue;
					dl->AddLine(ImVec2(scrAx, scrAy), ImVec2(scrBx, scrBy), skelGlow, 3.f);
					dl->AddLine(ImVec2(scrAx, scrAy), ImVec2(scrBx, scrBy), skelCol, 1.5f);
				}
				if (badBones > 8) {
					// Most bones are bad — skip skeleton entirely, custom model
				}
			}
		}

		// Backtrack tick visualization
		if (config::backtrack_visualize && config::backtrack) {
			float curtime = interfaces::globalVars ? interfaces::globalVars->curtime : 0.f;
			for (int i = 1; i < 128; ++i) {
				if (!readCache[i].valid) continue;
				int ticksToScan = (config::backtrack_ticks < config::BT_MAX_TICKS) ? config::backtrack_ticks : config::BT_MAX_TICKS;
				float prevSx = 0.f, prevSy = 0.f;
				bool hasPrev = false;
				for (int t = 0; t < ticksToScan; ++t) {
					const auto& tick = config::g_btBuf[i][t];
					if (!tick.valid) continue;
					float age = (float)(curtime - tick.simtime);
					if (age < 0.f || age > 0.25f) continue;
					int boneIdx = config::bone;
					if (boneIdx < 0 || boneIdx >= 128) continue;
					Vector bonePos = tick.bones[boneIdx].GetOrigin();
					float sx, sy;
					if (config::WorldToScreen(bonePos, sx, sy)) {
						float freshness = 1.f - (age / 0.2f);
						if (freshness < 0.f) freshness = 0.f;
						int r = (int)((1.f - freshness) * 255.f);
						int g = (int)(freshness * 255.f);
						int alpha = (int)((0.3f + freshness * 0.7f) * 200.f);
						float radius = 2.f + freshness * 3.f;
						dl->AddCircleFilled(ImVec2(sx, sy), radius, IM_COL32(r, g, 0, alpha));
						if (hasPrev) {
							dl->AddLine(ImVec2(prevSx, prevSy), ImVec2(sx, sy),
								IM_COL32(r, g, 0, alpha / 2), 1.f);
						}
						prevSx = sx; prevSy = sy; hasPrev = true;
					}
				}
			}
		}

		} // breadcrumb

		if (stealth::ShouldDrawVisuals()) {
			auto* dl2 = ImGui::GetBackgroundDrawList();
			const float fs2 = (d3d9hook::editorFont->LegacySize > 1.f) ? d3d9hook::editorFont->LegacySize : 14.f;
			DrawEntityESP(dl2, d3d9hook::editorFont, fs2, width, height,
			              interfaces::globalVars ? interfaces::globalVars->curtime : 0.f);

			Vector localPos{};
			float localYaw = 0.f;
			{
				int ri = config::g_viewReadIdx.load(std::memory_order_acquire);
				const auto& vm = config::g_viewMatrix[ri];
				localYaw = atan2f(vm[0][1], vm[0][0]) * 57.2957795f;
			}
			if (localPlayer) localPos = localPlayer->GetAbsOrigin();
			hud::DrawRadar(dl2, d3d9hook::editorFont, fs2, width, height, localPos, localYaw);
			hud::DrawFOVCircle(dl2, width, height);
			hud::DrawSpectatorAlert(dl2, d3d9hook::editorFont, fs2, width, height);

			int localIdx = localPlayer ? interfaces::engine->GetLocalPlayer() : -1;
			if (hud::spectator_list)
				hud::DrawSpectatorList(dl2, d3d9hook::editorFont, fs2, width, height, localIdx);
			if (hud::velocity_graph)
				hud::DrawVelocityGraph(dl2, d3d9hook::editorFont, fs2, width, height);

			sound_esp::Draw(dl2, d3d9hook::editorFont, fs2, width, height);
			misc_features::DrawCrosshair(dl2, width, height);
			net_panel::DrawNetPanel(dl2, d3d9hook::editorFont, fs2, width, height);
			net_panel::DrawChatPanel(dl2, d3d9hook::editorFont, fs2, width, height);
			prediction::Draw(dl2, d3d9hook::editorFont, fs2, width, height);
			heatmap::Draw(dl2, d3d9hook::editorFont, fs2, width, height);
			killfeed::DrawAnalyzer(dl2, d3d9hook::editorFont, fs2, width, height);
			killfeed::DrawVoiceIndicators(dl2, d3d9hook::editorFont, fs2);
			death_replay::Draw(dl2, d3d9hook::editorFont, fs2, width, height, interfaces::globalVars ? interfaces::globalVars->curtime : 0.f);
			freecam::DrawIndicator(dl2, d3d9hook::editorFont, fs2, width, height);
			fakelag::DrawIndicator(dl2, d3d9hook::editorFont, fs2, width, height);
			aim_lines::Draw(dl2, d3d9hook::editorFont, fs2, width, height);
			damage_log::Draw(dl2, d3d9hook::editorFont, fs2, width, height, interfaces::globalVars ? interfaces::globalVars->curtime : 0.f);
			spawn_detect::Draw(dl2, d3d9hook::editorFont, fs2, width, height);
			door_memory::DrawOnESP(dl2, d3d9hook::editorFont, fs2, width, height);
			waypoints::Draw(dl2, d3d9hook::editorFont, fs2, width, height);
			follow_bot::Draw(dl2, d3d9hook::editorFont, fs2);
			bot_visuals::Draw(dl2, d3d9hook::editorFont, fs2, width, height);
			rage_mode::DrawIndicator(dl2, d3d9hook::editorFont, fs2, width, height);
			tick_exploits::DrawIndicators(dl2, d3d9hook::editorFont, fs2, width, height);
			killstreak::Draw(dl2, d3d9hook::editorFont, fs2, width, height,
			                 interfaces::globalVars ? interfaces::globalVars->curtime : 0.f);

			if (config::crosshair_info) {
				int ri = config::g_crosshairInfoIdx.load(std::memory_order_acquire);
				const char* ciData = config::g_crosshairInfoBuf[ri];
				if (ciData[0] != '\0') {
					char ciClass[64]{}, ciName[64]{}, ciHP[16]{}, ciExtra[64]{};
					int parsed = sscanf_s(ciData, "%63[^\t]\t%63[^\t]\t%15[^\t]\t%63[^\t]",
						ciClass, 64, ciName, 64, ciHP, 16, ciExtra, 64);
					if (parsed >= 1) {
						float cx = static_cast<float>(width) * 0.5f;
						float cy = static_cast<float>(height) * 0.5f + 30.f;
						char ciBuf[192];
						if (parsed >= 3)
							snprintf(ciBuf, sizeof(ciBuf), "%s | %s | HP: %s", ciClass, ciName, ciHP);
						else
							snprintf(ciBuf, sizeof(ciBuf), "%s", ciClass);
						ImVec2 sz = d3d9hook::editorFont->CalcTextSizeA(fs2, FLT_MAX, 0.f, ciBuf);
						float tx = cx - sz.x * 0.5f;
						dl2->AddRectFilled(ImVec2(tx - 6.f, cy - 2.f),
							ImVec2(tx + sz.x + 6.f, cy + sz.y + 2.f),
							IM_COL32(0, 0, 0, 120), 3.f);
						dl2->AddText(d3d9hook::editorFont, fs2, ImVec2(tx, cy),
							IM_COL32(0, 220, 255, 230), ciBuf);
						if (parsed >= 4 && ciExtra[0] != '\0') {
							ImVec2 esz = d3d9hook::editorFont->CalcTextSizeA(fs2 * 0.85f, FLT_MAX, 0.f, ciExtra);
							float ex = cx - esz.x * 0.5f;
							dl2->AddText(d3d9hook::editorFont, fs2 * 0.85f,
								ImVec2(ex, cy + sz.y + 3.f),
								IM_COL32(180, 180, 180, 200), ciExtra);
						}
					}
				}
			}
			printer_monitor::DrawPanel(dl2, d3d9hook::editorFont, fs2, width, height,
			                           interfaces::globalVars ? interfaces::globalVars->curtime : 0.f);
			threat_radar::Draw(dl2, d3d9hook::editorFont, fs2, width, height);
			player_profiler::Draw(dl2, d3d9hook::editorFont, fs2, width, height);
			voice_exploits::Draw(dl2, d3d9hook::editorFont, fs2, width, height);
		}

		// minimal debug info (bottom-left, very subtle)
		if (config::esp_debug) {
			const float oy = static_cast<float>(height) - 100.f;
			auto* dbgDl = ImGui::GetBackgroundDrawList();
			auto dbgStr = std::format("cache:{} drawn:{} vel:{:.0f}\ntotal:{} null:{} !player:{} dead:{} dormant:{} bonefail:{} ok:{}",
				config::dbg_cache_valid, dbg_bones, config::currentvelocity,
				config::dbg_bc_total, config::dbg_bc_null, config::dbg_bc_notplayer,
				config::dbg_bc_dead, config::dbg_bc_dormant, config::dbg_bc_bonefail, config::dbg_bc_ok);
			dbgDl->AddText(d3d9hook::editorFont, 12.f, ImVec2(10.f, oy),
				IM_COL32(200, 220, 255, 200), dbgStr.c_str());
		}

		ImGui::End();
	}

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

	return ogPresent(pDevice, rect1, rect2, hwnd, rgndata);
}

void d3d9hook::Init() noexcept {
	if (kiero::init(kiero::RenderType::D3D9) == kiero::Status::Success) {
		window = FindWindowA("Valve001", 0);
		if (!window) {
			spdlog::default_logger()->error("Failed to find game window!");
			return;
		}
		ogWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(window, GWLP_WNDPROC, (LONG_PTR)WndProc));

		auto result = kiero::bind(17, reinterpret_cast<void**>(&ogPresent), &detouredPresent);
		if (result != kiero::Status::Success) {
			spdlog::default_logger()->error("Failed to hook Direct3D device: {}", (int)result);
			return;
		}
	}
	return;
}

void d3d9hook::Shutdown() noexcept {
	g_shuttingDown.store(true, std::memory_order_release);
	Sleep(200);
	kiero::unbind(17);
	Sleep(100);
	if (ogWndProc && window)
		SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ogWndProc));
	ui_window::SaveState("friendlydll_window.cfg");
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	kiero::shutdown();
}