#include "controller.hpp"
#include "pad_native.hpp"
#include "video.hpp"
#include <SDL2/SDL.h>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace psgi;

void coreInput() {
    ControllerInput input;
    ControllerState state{true, 0, 0, 0};
    assert(!input.poll(state, 0));
    // Drift must not start navigation. A decisive movement emits immediately.
    for (int value : {-11999, -8000, -1, 0, 1, 8000, 11999}) {
        state.leftX = value; assert(!input.poll(state, 100));
    }
    state.leftX = 26000; assert(input.poll(state, 1000) == Button::Right);
    assert(!input.poll(state, 350999));
    assert(input.poll(state, 351000) == Button::Right);
    assert(!input.poll(state, 450999));
    assert(input.poll(state, 451000) == Button::Right);
    // Hysteresis avoids release/repress jitter and diagonal axis oscillation.
    state.leftX = 9000; assert(!input.poll(state, 452000));
    state.leftX = 7999; assert(!input.poll(state, 600000));
    state.leftX = 11999; assert(!input.poll(state, 700000));
    state.leftX = 26000; state.leftY = 20000;
    assert(input.poll(state, 710000) == Button::Right);
    state.leftX = 22000; state.leftY = 23000; assert(!input.poll(state, 720000));
    state.leftX = 10000; state.leftY = 26000; assert(input.poll(state, 730000) == Button::Down);
    state.leftY = -26000; assert(input.poll(state, 740000) == Button::Up);
    state.leftX = -32768; state.leftY = 0; assert(input.poll(state, 750000) == Button::Left);
    // D-pad overrides the stick, including hold-to-repeat and opposite presses.
    state.buttons = buttonMask(Button::Down); assert(input.poll(state, 760000) == Button::Down);
    assert(input.poll(state, 1110000) == Button::Down);
    state.buttons |= buttonMask(Button::Up); assert(!input.poll(state, 1120000));
    state = {true, 0, 0, 0}; assert(!input.poll(state, 1130000));
    for (Button button : {Button::Up, Button::Down, Button::Left, Button::Right, Button::Cross,
                          Button::Circle, Button::Square, Button::Triangle, Button::Options, Button::L1, Button::R1, Button::L2, Button::R2}) {
        state.buttons = buttonMask(button); assert(input.poll(state, 1200000) == button);
        if (button >= Button::Cross) assert(!input.poll(state, 2200000));
        state.buttons = 0; assert(!input.poll(state, 2300000));
    }
    state.buttons = buttonMask(Button::Circle) | buttonMask(Button::Cross);
    assert(input.poll(state, 2400000) == Button::Circle);
    assert(!input.poll(state, 2500000)); // Never back and confirm in one frame.
    // Closing an IME must suppress held actions and a deflected stick until release.
    state = {true, buttonMask(Button::Cross) | buttonMask(Button::Down), 0, 26000};
    input.suppress(state); assert(!input.poll(state, 3000000));
    state.leftX = -26000; state.leftY = 0; assert(!input.poll(state, 4000000));
    state.buttons = 0; assert(!input.poll(state, 5000000));
    state.leftX = state.leftY = 0; assert(!input.poll(state, 5100000));
    state.leftY = 26000; assert(input.poll(state, 5200000) == Button::Down);
    // Disconnected/failed reads reset repeat and cannot turn zero axes into input.
    state.connected = false; assert(!input.poll(state, 6200000));
    state.connected = true; assert(input.poll(state, 6300000) == Button::Down);
}

void nativeSamplesAndUi() {
    OrbisPadData pad{}; pad.connected = 1; pad.leftStick = {128, 128};
    auto state = readNativeController(pad, true);
    assert(state.connected && state.leftX == 0 && state.leftY == 0 && state.buttons == 0);
    pad.leftStick = {0, 255}; pad.buttons = ORBIS_PAD_BUTTON_CROSS | ORBIS_PAD_BUTTON_OPTIONS;
    state = readNativeController(pad, true);
    assert(state.leftX == -32768 && state.leftY == 32512);
    assert(state.buttons == (buttonMask(Button::Cross) | buttonMask(Button::Options)));
    pad.buttons=ORBIS_PAD_BUTTON_L2 | ORBIS_PAD_BUTTON_R2;
    assert(readNativeController(pad,true).buttons==(buttonMask(Button::L2)|buttonMask(Button::R2)));
    assert(!readNativeController(pad, false).connected);
    pad.connected = 0; assert(!readNativeController(pad, true).connected);

    Ui ui; ui.screen = Screen::Dashboard; ui.loading = false;
    std::vector<Entry> games;
    for (int i = 0; i < 8; ++i) {
        Entry game; game.name = "Game " + std::to_string(i); game.url = "http://fixture/" + std::to_string(i);
        for (int j = 0; j < 20; ++j) game.metadata.emplace_back("Field " + std::to_string(j), "Value");
        games.push_back(game);
    }
    ui.setLibraries(games, games);
    ControllerInput input;
    uint64_t time = 0;
    auto move = [&](int x, int y) {
        pad = {}; pad.connected = 1; pad.leftStick = {128, 128};
        input.poll(readNativeController(pad, true), time += 1000);
        pad.leftStick = {static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
        auto button = input.poll(readNativeController(pad, true), time += 1000);
        assert(button); ui.handle(*button);
    };
    move(255, 128); assert(ui.position() == 1);
    move(128, 255); assert(ui.position() == 5);
    move(128, 0); assert(ui.position() == 1);
    move(0, 128); move(0, 128); assert(ui.sidebar);
    move(128, 255); assert(ui.library == Library::Available);
    move(255, 128); assert(!ui.sidebar);
    ui.settings.layout = Layout::Summary; ui.rebuild();
    auto before = ui.position(); move(128, 255); assert(ui.position() == before + 1);
    ui.handle(Button::Options); move(128, 255); assert(ui.menuIndex == 1);
    ui.menuIndex = 0; move(128, 0); assert(ui.menuIndex == ui.options().size() - 1);
    move(128, 255); assert(ui.menuIndex == 0);
    ui.menuIndex = 6; ui.handle(Button::Cross); move(128, 255); assert(ui.settingsIndex == 1);
    ui.handle(Button::Circle); ui.handle(Button::Circle); // Settings -> Options -> Dashboard.
    ui.handle(Button::Square); assert(ui.screen == Screen::Details);
    ui.pending.info.description.clear(); ui.pending.description=std::string(1000,'x');
    move(128, 255); assert(ui.detailIndex == 1 && ui.detailScroll == 0);
    move(128, 0); assert(ui.detailIndex == 0 && ui.detailScroll == 0);
    move(128, 0); assert(ui.detailIndex == ui.detailRows().size()-1 && ui.detailScroll > 0);
    move(128, 255); assert(ui.detailIndex == 0 && ui.detailScroll == 0);
}

void sdlController() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    Video video;
    Ui ui; ui.screen = Screen::Dashboard;
    int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    assert(index >= 0 && SDL_IsGameController(index));
    SDL_Joystick* joystick = SDL_JoystickOpen(index); assert(joystick);
    assert(video.input(ui).empty());
    auto axis = [&](int x, int y) {
        assert(SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, x) == 0);
        assert(SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, y) == 0);
    };
    auto expect = [&](Button button) { auto events = video.input(ui); assert(events.size() == 1 && events[0] == button); };
    axis(26000, 0); expect(Button::Right); assert(video.input(ui).empty());
    SDL_Delay(360); expect(Button::Right);
    assert(SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, 1) == 0);
    expect(Button::Up);
    assert(SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, 1) == 0);
    expect(Button::Cross);
    video.flushInput(); assert(video.input(ui).empty());
    assert(SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, 0) == 0);
    assert(SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, 0) == 0);
    axis(0, 0); assert(video.input(ui).empty());
    assert(SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,32767)==0); expect(Button::L2);
    assert(video.input(ui).empty()); video.flushInput(); assert(video.input(ui).empty());
    assert(SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768)==0); assert(video.input(ui).empty());
    assert(SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767)==0); expect(Button::R2);
    assert(SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768)==0); assert(video.input(ui).empty());
    ui.screen = Screen::Keyboard; ui.keyboardText = "http://fixture/games";
    video.startKeyboard(ui); axis(-26000, 0); assert(video.input(ui).empty());
    SDL_Event done{}; done.type = SDL_KEYDOWN; done.key.keysym.sym = SDLK_RETURN;
    assert(SDL_PushEvent(&done) == 1); assert(video.input(ui).empty());
    assert(ui.screen == Screen::Settings && video.input(ui).empty());
    axis(0, 0); assert(video.input(ui).empty());
    ui.screen = Screen::Keyboard; ui.keyboardReturn = Screen::ArchiveAccount;
    ui.editKind = EditKind::ArchivePassword; ui.keyboardText.clear(); video.startKeyboard(ui);
    SDL_Event text{}; text.type = SDL_TEXTINPUT; std::strcpy(text.text.text, "preview-secret");
    assert(SDL_PushEvent(&text) == 1); assert(video.input(ui).empty());
    assert(ui.keyboardText == "preview-secret" && ui.keyboardDisplayText() == "**************");
    assert(SDL_PushEvent(&done) == 1); assert(video.input(ui).empty());
    assert(ui.screen == Screen::ArchiveAccount && ui.archivePassword == "preview-secret" && ui.keyboardText.empty());
    ui.screen = Screen::Keyboard; ui.keyboardText = "cancel-this"; video.startKeyboard(ui);
    done.key.keysym.sym = SDLK_ESCAPE;
    assert(SDL_PushEvent(&done) == 1); assert(video.input(ui).empty());
    assert(ui.archivePassword.empty() && ui.keyboardText.empty() && ui.screen == Screen::ArchiveAccount);
    axis(26000, 0); expect(Button::Right);
    assert(SDL_JoystickDetachVirtual(index) == 0); assert(video.input(ui).empty());
    SDL_JoystickClose(joystick);
    index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    assert(index >= 0); joystick = SDL_JoystickOpen(index); assert(joystick);
    assert(video.input(ui).empty()); axis(0, 26000); expect(Button::Down);
    assert(SDL_JoystickDetachVirtual(index) == 0); assert(video.input(ui).empty());
    SDL_JoystickClose(joystick);
}

int main() {
    coreInput(); nativeSamplesAndUi(); sdlController();
    std::puts("Analog/D-pad navigation, native samples, repeat, drift, IME suppression and SDL hotplug passed");
}
