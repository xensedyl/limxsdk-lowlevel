#include "mobile_platform_common.hpp"
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <SDL2/SDL.h>

namespace tron2 {

const std::set<std::string> CHASSIS_MODES{
    "ackerman", "parallel", "park", "spinning", "emergency_stop"};

Json read_chassis_state(Client& client) {
    auto response = client.request("request_chassis_state");
    auto values = numbers(response.at("data").at("data"), 3, "data.data");
    return {{"timestamp", response.value("timestamp", Json())},
            {"linear_velocity", values[0]}, {"angular_velocity", values[1]},
            {"steering_angle", values[2]}};
}

Json read_lifter_state(Client& client) {
    auto response = client.request("request_lifter_state");
    auto data = response.at("data");
    return {{"timestamp", response.value("timestamp", Json())},
            {"q", numbers(data.at("q"), 1, "q")[0]},
            {"v", numbers(data.at("v"), 1, "v")[0]}};
}

Json read_lifter_position(Client& client) {
    auto response = client.request("request_get_lifter_position");
    auto data = response.at("data");
    std::cout << "response_get_lifter_position 完整响应体：" << std::endl;
    print_json(response);
    for (auto key : {"position", "q", "q_per_mm", "timestamp"}) number(data.at(key), key);
    data.erase("result");
    data["response_timestamp"] = response.value("timestamp", Json());
    return data; // Keep floating point timestamps and optional calibration fields.
}

void poll_state(Client& client, double interval, const std::function<Json(Client&)>& read) {
    if (interval > 0) std::cout << "持续打印，按 Ctrl+C 退出" << std::endl;
    do {
        print_json(read(client));
        if (interval == 0) break;
        sleep_interruptible(interval);
    } while (!interrupted);
}

int run_keyboard(Client& client, const Options& options) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(std::string("SDL 初始化失败：") + SDL_GetError());
    SDL_Window* window = SDL_CreateWindow("TRON2 Keyboard Control", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          700, 300, SDL_WINDOW_SHOWN);
    if (!window) { SDL_Quit(); throw std::runtime_error(std::string("创建控制窗口失败：") + SDL_GetError()); }
    const double speed = options.real("speed", 0.5), turn = options.real("turn-speed", 0.5);
    const double rate = options.real("rate", 20);
    const double response_timeout = options.real("response-timeout", 0.5);
    require(speed > 0 && speed <= 1 && turn > 0 && turn <= 1 && rate >= 1 && rate <= 100, "底盘参数超出范围");
    KeyboardControl control;
    Command previous = {{999, 999, 999}};
    auto next_send = Clock::now();
    std::deque<Ticket> pending;
    auto stop_chassis = [&] {
        const auto ticket = client.send("request_chassis_move", {{"x", 0}, {"y", 0}, {"yaw", 0}});
        client.wait(ticket, response_timeout, true);
        std::cout << "零速度指令已获成功响应（不是实际停稳反馈）" << std::endl;
    };
    try {
        stop_chassis();
        std::cout << "W/S 前后，A/D 转向，Q/E 发送 y；松开/失焦/空格停止，Esc 退出。\n"
                     "Q/E 是否生效取决于固件和模式；已知该实机 ackerman 模式不能通过 y 横移。" << std::endl;
        bool running = true;
        bool focused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
        while (running && !interrupted) {
            SDL_Event event;
            bool stop = false;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = false;
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) stop = true;
                else if (event.type == SDL_WINDOWEVENT &&
                         (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST || event.window.event == SDL_WINDOWEVENT_MINIMIZED)) {
                    focused = false;
                    control.update(Keys{}, false, speed, turn);
                }
                else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) focused = true;
            }
            if (!running || interrupted) break;
            const auto* keys = SDL_GetKeyboardState(nullptr);
            Keys state;
            state.forward = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
            state.backward = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
            state.left = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
            state.right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
            state.side_left = keys[SDL_SCANCODE_Q];
            state.side_right = keys[SDL_SCANCODE_E];
            state.stop = stop || keys[SDL_SCANCODE_SPACE];
            auto command = control.update(state, focused, speed, turn);
            auto now = Clock::now();
            client.check();
            for (auto it = pending.begin(); it != pending.end();) {
                if (auto response = client.poll(*it)) {
                    if (options.has("debug")) std::cout << "[响应] " << response->dump() << std::endl;
                    it = pending.erase(it);
                }
                else {
                    if (std::chrono::duration<double>(now - it->sent_at).count() > response_timeout)
                        throw std::runtime_error("底盘运动响应超时，guid=" + it->guid);
                    ++it;
                }
            }
            if (command != previous || now >= next_send) {
                pending.push_back(client.send("request_chassis_move", {{"x", command[0]}, {"y", command[1]}, {"yaw", command[2]}}));
                if (command != previous)
                    std::cout << "[目标] x=" << command[0] << " y=" << command[1] << " yaw=" << command[2] << std::endl;
                previous = command;
                next_send = now + std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>(1 / rate));
            }
            std::ostringstream caption;
            caption << "TRON2 | W/S A/D Q/E | Target x=" << std::fixed << std::setprecision(2)
                    << command[0] << " y=" << command[1] << " yaw=" << command[2]
                    << (control.armed() ? " | READY" : " | RELEASE KEYS") << " | Space:STOP Esc:EXIT";
            SDL_SetWindowTitle(window, caption.str().c_str());
            if (auto* surface = SDL_GetWindowSurface(window)) {
                SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 25, control.armed() ? 65 : 25, 40));
                SDL_UpdateWindowSurface(window);
            }
            SDL_Delay(5);
        }
        stop_chassis();
    } catch (...) {
        try { stop_chassis(); }
        catch (const std::exception& error) { std::cerr << "停止指令未获确认：" << error.what() << std::endl; }
        SDL_DestroyWindow(window); SDL_Quit(); throw;
    }
    SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}

} // namespace tron2
