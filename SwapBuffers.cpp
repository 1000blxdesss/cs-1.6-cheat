#include <pch.h>
#include <base.h>
// смещения
#define ViewMatrix          0xEC9780
#define EntityList          0x12043CC
#define LocalTeam           0x100DF4
#define entityOrigin        0x184
#define stateValue          0x17C
#define entityModel         0x12C
#define entitySize          0x250
// Константы для ESP-боксов
#define ESP_BOX_BOTTOM 30.0f
#define ESP_BOX_TOP    25.0f
static float state_list[64] = { 0 }; // Для хранения состояний сущностей
typedef float view_matrix_t[4][4];
static bool espEnabled = false;
static bool showDead = false;
struct ImVec3 {
    float x, y, z;
    ImVec3(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) : x(_x), y(_y), z(_z) {}
};
void SetCustomStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 12.0f;
    style.WindowRounding = 15.0f;
    style.GrabRounding = 10.0f;
    style.ChildRounding = 12.0f;
    style.ItemSpacing = ImVec2(10, 10);
    style.FramePadding = ImVec2(12, 6);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.70f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.85f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}
void clear_entity_list() {
    memset(state_list, 0, sizeof(state_list));
}
// координат мира -> экранные (паста с Zodiac)
bool w2s(const ImVec2& screen, const ImVec3& position, ImVec2* out, const view_matrix_t matrix) {
    float x = matrix[0][0] * position.x + matrix[1][0] * position.y + matrix[2][0] * position.z + matrix[3][0];
    float y = matrix[0][1] * position.x + matrix[1][1] * position.y + matrix[2][1] * position.z + matrix[3][1];
    float w = matrix[0][3] * position.x + matrix[1][3] * position.y + matrix[2][3] * position.z + matrix[3][3];

    if (w < 0.01f)
        return false;
    float inv_w = 1.0f / w;
    x *= inv_w;
    y *= inv_w;

    out->x = (x + 1.0f) * 0.5f * screen.x;
    out->y = (1.0f - y) * 0.5f * screen.y;
    return true;
}

void DrawEntitiesEsp(void* base, const view_matrix_t viewMatrix, const ImVec2& screenSize, bool showDead) {
    uintptr_t baseAddr = reinterpret_cast<uintptr_t>(base);
    if (!baseAddr)
        return;

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();

    for (int i = 0; i < 64; i++) {
        
        uintptr_t entityBase = baseAddr + EntityList + (i * entitySize);

      
       
        
        ImVec3* originPtr = (ImVec3*)(entityBase + entityOrigin);
        ImVec3 origin = *originPtr;
        if (origin.x == 0.0f && origin.y == 0.0f && origin.z == 0.0f)
            continue;

        ImVec3 origin_bottom = origin;
        ImVec3 origin_top = origin;
        origin_bottom.z -= ESP_BOX_BOTTOM;
        origin_top.z += ESP_BOX_TOP;

        // Преобразуем в экранные координаты
        ImVec2 screen_bottom = { 0.0f, 0.0f };
        ImVec2 screen_top = { 0.0f, 0.0f };
        if (!w2s(screenSize, origin_bottom, &screen_bottom, viewMatrix) ||
            !w2s(screenSize, origin_top, &screen_top, viewMatrix))
            continue;

        // Вычисляем размеры бокса
        float height = screen_bottom.y - screen_top.y;
        if (height < 0)
            continue;
        float width = height / 2.4f;
        float boxX = screen_top.x - width / 2;
        float boxY = screen_top.y;
        ImU32 boxColor;
        boxColor = IM_COL32(0, 255, 0, 255);
        draw_list->AddRect(ImVec2(boxX, boxY), ImVec2(boxX + width, boxY + height),
            boxColor, 0.0f, 0, 2.0f);
        char* entityName = (char*)(entityBase + 0x100);
        if (entityName && entityName[0] != '\0')
            draw_list->AddText(ImVec2(boxX, boxY - 15), IM_COL32(255, 255, 255, 255), entityName);
    }
}




static bool ContextCreated = false;
static HGLRC g_Context = (HGLRC)NULL;

BOOL __stdcall Base::Hooks::SwapBuffers(_In_ HDC hdc)
{
	Data::hWindow = WindowFromDC(hdc);
	HGLRC oContext = wglGetCurrentContext();

	if (!Data::oWndProc)
		Data::oWndProc = (WndProc_t)SetWindowLongPtr(Data::hWindow, WNDPROC_INDEX, (LONG_PTR)Hooks::WndProc);

	if (!ContextCreated)
	{
		g_Context = wglCreateContext(hdc);
		wglMakeCurrent(hdc, g_Context);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		GLint m_viewport[4];
		glGetIntegerv(GL_VIEWPORT, m_viewport);

		glOrtho(0, m_viewport[2], m_viewport[3], 0, 1, -1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glClearColor(0, 0, 0, 0);
		ContextCreated = true;
	}

	wglMakeCurrent(hdc, g_Context);

    if (!Data::InitImGui && ContextCreated)
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplWin32_Init(Data::hWindow);
        ImGui_ImplOpenGL2_Init();
        Data::InitImGui = true;
    }

	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
    // типо размер экрана
    GLint m_viewport[4] = { 0 };
    glGetIntegerv(GL_VIEWPORT, m_viewport);
    ImVec2 screen_size = ImVec2(static_cast<float>(m_viewport[2]), static_cast<float>(m_viewport[3]));

    // читаем матрицу вида
    view_matrix_t viewMatrix = { 0 };
    uintptr_t baseAddr = reinterpret_cast<uintptr_t>(Data::m_hw->Module.base);
    if (baseAddr) {
        float(*viewMatrixPtr)[4] = (float(*)[4])(baseAddr + ViewMatrix);
        memcpy(viewMatrix, viewMatrixPtr, sizeof(view_matrix_t));
    }
	if (Data::ShowMenu)
	{
        SetCustomStyle();
        ImGui::Begin("ESP Menu");
        ImGui::Text("Counter-Strike 1.6 ESP");
        ImGui::Checkbox("Enable ESP", &espEnabled);
        
		if (ImGui::Button("Detach"))
		{
			ImGui::End();
			ImGui::Render();
			ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
			wglMakeCurrent(hdc, oContext);
			wglDeleteContext(g_Context);
			Base::Detach();
			return Data::oSwapBuffers(hdc);
		}
        ImGui::Text("hw.dll base: %p", Data::m_hw->Module.base);
		ImGui::End();
	} 
    
    if (espEnabled) 
    {
        clear_entity_list();
        DrawEntitiesEsp(Data::m_hw->Module.base, viewMatrix, screen_size, true);
    }
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

	wglMakeCurrent(hdc, oContext);
	return Data::oSwapBuffers(hdc);
}
