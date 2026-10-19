#include "video.hpp"
#include <algorithm>
#include <chrono>
#include "text.hpp"
#include <cstring>
#include <stdexcept>
#include <thread>

#ifdef PSGI_HOST
#include <SDL2/SDL.h>
namespace {
SDL_GameController* openController() {
    for (int i = 0; i < SDL_NumJoysticks(); ++i) if (SDL_IsGameController(i)) {
        if (auto* controller = SDL_GameControllerOpen(i)) return controller;
    }
    return nullptr;
}
psgi::ControllerState readController(void* handle) {
    auto* controller = static_cast<SDL_GameController*>(handle);
    psgi::ControllerState state;
    if (!controller || !SDL_GameControllerGetAttached(controller)) return state;
    state.connected = true;
    state.leftX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    state.leftY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    using psgi::Button;
    const std::pair<SDL_GameControllerButton, Button> bindings[] = {
        {SDL_CONTROLLER_BUTTON_A, Button::Cross}, {SDL_CONTROLLER_BUTTON_B, Button::Circle},
        {SDL_CONTROLLER_BUTTON_X, Button::Square}, {SDL_CONTROLLER_BUTTON_Y, Button::Triangle},
        {SDL_CONTROLLER_BUTTON_START, Button::Options}, {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Button::L1},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Button::R1}, {SDL_CONTROLLER_BUTTON_DPAD_UP, Button::Up},
        {SDL_CONTROLLER_BUTTON_DPAD_DOWN, Button::Down}, {SDL_CONTROLLER_BUTTON_DPAD_LEFT, Button::Left},
        {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, Button::Right}
    };
    for (const auto& binding : bindings)
        if (SDL_GameControllerGetButton(controller, binding.first)) state.buttons |= psgi::buttonMask(binding.second);
    if (SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>20000) state.buttons |= psgi::buttonMask(Button::L2);
    if (SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>20000) state.buttons |= psgi::buttonMask(Button::R2);
    return state;
}
}
namespace psgi {
Video::Video(bool headless) {
    if (headless) return;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)) throw std::runtime_error(SDL_GetError());
    window = SDL_CreateWindow("PS4 Games Installer — Desktop Preview", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE);
    if (!window) throw std::runtime_error(SDL_GetError());
    renderer = SDL_CreateRenderer(static_cast<SDL_Window*>(window), -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) throw std::runtime_error(SDL_GetError());
    texture = SDL_CreateTexture(static_cast<SDL_Renderer*>(renderer), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Canvas::Width, Canvas::Height);
    if (!texture) throw std::runtime_error(SDL_GetError());
    SDL_RenderSetLogicalSize(static_cast<SDL_Renderer*>(renderer), Canvas::Width, Canvas::Height);
    controller = openController();
}
Video::~Video() {
    if (controller) SDL_GameControllerClose(static_cast<SDL_GameController*>(controller));
    if (texture) SDL_DestroyTexture(static_cast<SDL_Texture*>(texture));
    if (renderer) SDL_DestroyRenderer(static_cast<SDL_Renderer*>(renderer));
    if (window) SDL_DestroyWindow(static_cast<SDL_Window*>(window));
    SDL_Quit();
}
void Video::present(const Canvas& c) {
    if (!window) return;
    SDL_UpdateTexture(static_cast<SDL_Texture*>(texture), nullptr, c.pixels.data(), Canvas::Width * 4);
    SDL_RenderClear(static_cast<SDL_Renderer*>(renderer));
    SDL_RenderCopy(static_cast<SDL_Renderer*>(renderer), static_cast<SDL_Texture*>(texture), nullptr, nullptr);
    SDL_RenderPresent(static_cast<SDL_Renderer*>(renderer));
}
void Video::startKeyboard(Ui&) { keyboard = true; SDL_StartTextInput(); }
void Video::pollKeyboard(Ui&) {}
void Video::flushInput() {
    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
    controllerInput.suppress(readController(controller));
}
std::vector<Button> Video::input(Ui& ui) {
    std::vector<Button> buttons;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_CONTROLLERDEVICEADDED && !controller) controller = openController();
        if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller &&
            event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(static_cast<SDL_GameController*>(controller)))) {
            SDL_GameControllerClose(static_cast<SDL_GameController*>(controller));
            controller = nullptr; controllerInput.suppress({});
        }
        if (event.type == SDL_QUIT) { if (keyboard) { ui.finishEdit({}); keyboard = false; SDL_StopTextInput(); } else buttons.push_back(Button::Circle); }
        if (keyboard) {
            if (event.type == SDL_CONTROLLERBUTTONDOWN && (event.cbutton.button == SDL_CONTROLLER_BUTTON_A || event.cbutton.button == SDL_CONTROLLER_BUTTON_B)) {
                keyboard = false; SDL_StopTextInput();
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) ui.finishEdit(ui.keyboardText); else ui.finishEdit({});
                flushInput(); continue;
            }
            if (event.type == SDL_TEXTINPUT) {
                size_t maximum = keyboardMaximum(ui.editKind);
                if (ui.keyboardText.size() + std::strlen(event.text.text) <= maximum) ui.keyboardText += event.text.text;
            }
            if (event.type != SDL_KEYDOWN) continue;
            auto key = event.key.keysym.sym;
            if (key == SDLK_ESCAPE || key == SDLK_RETURN) {
                keyboard = false; SDL_StopTextInput();
                if (key == SDLK_RETURN) ui.finishEdit(ui.keyboardText); else ui.finishEdit({});
                flushInput();
            } else if (key == SDLK_BACKSPACE && !ui.keyboardText.empty()) {
                size_t last = ui.keyboardText.size() - 1;
                while (last && (static_cast<unsigned char>(ui.keyboardText[last]) & 0xc0) == 0x80) --last;
                ui.keyboardText.erase(last);
            } else if (key == SDLK_v && (event.key.keysym.mod & KMOD_CTRL)) {
                char* text = SDL_GetClipboardText();
                if (text && ui.keyboardText.size() + std::strlen(text) <= keyboardMaximum(ui.editKind)) ui.keyboardText += text;
                SDL_free(text);
            }
            continue;
        }
        if (event.type == SDL_KEYDOWN) {
            bool repeated = event.key.repeat != 0;
            switch (event.key.keysym.sym) {
                case SDLK_UP: buttons.push_back(Button::Up); break;
                case SDLK_DOWN: buttons.push_back(Button::Down); break;
                case SDLK_LEFT: buttons.push_back(Button::Left); break;
                case SDLK_RIGHT: buttons.push_back(Button::Right); break;
                case SDLK_RETURN: case SDLK_x: if (!repeated) buttons.push_back(Button::Cross); break;
                case SDLK_ESCAPE: case SDLK_o: if (!repeated) buttons.push_back(Button::Circle); break;
                case SDLK_s: if (!repeated) buttons.push_back(Button::Square); break;
                case SDLK_t: if (!repeated) buttons.push_back(Button::Triangle); break;
                case SDLK_TAB: if (!repeated) buttons.push_back(Button::Options); break;
                case SDLK_PAGEUP: if (!repeated) buttons.push_back(Button::L2); break;
                case SDLK_PAGEDOWN: if (!repeated) buttons.push_back(Button::R2); break;
                case SDLK_q: if (!repeated) buttons.push_back(Button::L1); break;
                case SDLK_e: if (!repeated) buttons.push_back(Button::R1); break;
            }
        }
    }
    if (!controller) controller = openController();
    if (!keyboard) if (auto button = controllerInput.poll(readController(controller), SDL_GetTicks64() * 1000))
        buttons.push_back(*button);
    return buttons;
}
}
#else
#include "pad_native.hpp"
#include <orbis/UserService.h>
#include <orbis/Sysmodule.h>
#include <orbis/CommonDialog.h>
#include <orbis/ImeDialog.h>
#include <orbis/libkernel.h>

namespace psgi {
namespace {
std::vector<char16_t> inputBuffer;
std::u16string inputTitle;
}
Video::Video(bool) {
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_PAD);
    OrbisUserServiceInitializeParams params{};
    params.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&params);
    if (sceUserServiceGetInitialUser(&userId) < 0 || scePadInit() < 0) throw std::runtime_error("Controller initialization failed");
    pad = scePadOpen(userId, 0, 0, nullptr);
    if (pad < 0) throw std::runtime_error("Controller could not be opened");
    video = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, nullptr);
    if (video < 0) throw std::runtime_error("Video output could not be opened");
    constexpr size_t alignment = 2 * 1024 * 1024, bytes = Canvas::Width * Canvas::Height * 4;
    allocation = (bytes * 2 + alignment - 1) & ~(alignment - 1);
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), allocation, alignment, 3, &offset) < 0 ||
        sceKernelMapDirectMemory(&memory, allocation, 0x33, 0, offset, alignment) < 0) throw std::runtime_error("Video memory allocation failed");
    std::memset(memory, 0, allocation);
    buffers[0] = memory; buffers[1] = static_cast<char*>(memory) + bytes;
    OrbisVideoOutBufferAttribute attr{};
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, Canvas::Width, Canvas::Height, Canvas::Width);
    if (sceVideoOutRegisterBuffers(video, 0, buffers, 2, &attr) < 0) throw std::runtime_error("Display registration failed");
    sceVideoOutSetFlipRate(video, 1); // 30 FPS, matching the CPU renderer's frame budget.
}
Video::~Video() {
    if (keyboard) { sceImeDialogAbort(); sceImeDialogTerm(); }
    std::fill(inputBuffer.begin(), inputBuffer.end(), 0);
    if (pad >= 0) scePadClose(pad);
    if (video >= 0) sceVideoOutClose(video);
    if (memory) sceKernelMunmap(memory, allocation);
    if (allocation) sceKernelReleaseDirectMemory(offset, allocation);
}
void Video::present(const Canvas& canvas) {
    std::memcpy(buffers[active], canvas.pixels.data(), canvas.pixels.size() * 4);
    if (sceVideoOutSubmitFlip(video, active, ORBIS_VIDEO_OUT_FLIP_VSYNC, frame) < 0) throw std::runtime_error("Video flip failed");
    uint64_t deadline = sceKernelGetProcessTime() + 3000000;
    OrbisVideoOutFlipStatus status{};
    do {
        if (sceVideoOutGetFlipStatus(video, &status) < 0) throw std::runtime_error("Video status failed");
        if (status.flipArg == frame) break;
        if (sceKernelGetProcessTime() > deadline) throw std::runtime_error("Video flip timed out");
        sceKernelUsleep(1000);
    } while (true);
    active ^= 1; ++frame;
}
void Video::flushInput() {
    OrbisPadData data{};
    const bool valid = scePadReadState(pad, &data) >= 0;
    controllerInput.suppress(readNativeController(data, valid));
}
std::vector<Button> Video::input(Ui&) {
    if (keyboard) return {};
    OrbisPadData data{};
    const bool valid = scePadReadState(pad, &data) >= 0;
    auto button = controllerInput.poll(readNativeController(data, valid), sceKernelGetProcessTime());
    return button ? std::vector<Button>{*button} : std::vector<Button>{};
}
void Video::startKeyboard(Ui& ui) {
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);
    sceCommonDialogInitialize();
    size_t maximum = keyboardMaximum(ui.editKind);
    auto initial = toUtf16(ui.keyboardText);
    if (initial.size() > maximum) throw std::runtime_error("Text is too long for the keyboard");
    inputBuffer.assign(maximum + 1, 0);
    std::copy(initial.begin(), initial.end(), inputBuffer.begin());
    inputTitle = toUtf16(keyboardTitle(ui.editKind));
    OrbisImeDialogSetting params{};
    params.userId = userId;
    params.type = ui.editKind == EditKind::SourceUrl ? ORBIS_TYPE_TYPE_URL : ORBIS_TYPE_DEFAULT;
    if (ui.editKind == EditKind::ArchiveEmail) params.type = ORBIS_TYPE_MAIL;
    if (ui.editKind == EditKind::ArchivePassword) {
        // SceIme option bits are omitted from the pinned OpenOrbis header.
        constexpr uint32_t password = 4, noAutoCapitalization = 2, noLearning = 32,
                           disableResume = 256, disableAutoSpace = 512;
        params.option = password | noAutoCapitalization | noLearning | disableResume | disableAutoSpace;
    }
    params.maxTextLength = maximum;
    // SceIme uses UTF-16; the OpenOrbis wchar_t* declaration has a 32-bit host type.
    params.inputTextBuffer = reinterpret_cast<wchar_t*>(inputBuffer.data());
    params.title = reinterpret_cast<const wchar_t*>(inputTitle.c_str());
    params.enterLabel = ORBIS_BUTTON_LABEL_DEFAULT;
    int result = sceImeDialogInit(&params, nullptr);
    if (result < 0) {
        std::fill(inputBuffer.begin(), inputBuffer.end(), 0);
        throw std::runtime_error("The PS4 on-screen keyboard could not be opened");
    }
    keyboard = true;
}
void Video::pollKeyboard(Ui& ui) {
    if (!keyboard || sceImeDialogGetStatus() != ORBIS_DIALOG_STATUS_STOPPED) return;
    OrbisDialogResult result{};
    int rc = sceImeDialogGetResult(&result);
    sceImeDialogTerm(); keyboard = false;
    if (rc == 0 && result.endstatus == ORBIS_DIALOG_OK) {
        size_t size = 0;
        while (size + 1 < inputBuffer.size() && inputBuffer[size]) ++size;
        ui.finishEdit(fromUtf16(inputBuffer.data(), size));
    } else ui.finishEdit({});
    std::fill(inputBuffer.begin(), inputBuffer.end(), 0);
    flushInput();
}
}
#endif
