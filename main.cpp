#include <SDL.h>
#include <cmath>
#include <algorithm>
// #include <dirent.h>
#include <mutex>
#include <limits>

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static SDL_Window * window = nullptr;
static SDL_Renderer * renderer = nullptr;
static SDL_Surface * font_surface = nullptr;
static SDL_Texture * font_texture = nullptr;
static SDL_FPoint player_pos = {0.f, 0.f};
const int axis_dead_zone = 4000;
const float speed_logical_units_per_s = 150.f;


/*

TODO:
- support multiple windows
- move internal static variables to top of file
- move internal const, static-able variables to top of file
- port to C
--- move variable declarations to top of functions
--- don't use auto
--- remove C++ std lib use
--- use C-style casts
- detach SDL_Render's logical-size from API?
- always re-center player on logical-size change (and get rid
  of std::call_once use)

*/

struct DL_Joy {
    enum Joy_State {
        Inactive,
        Active
    };

    enum Input_Type {
        None,
        Finger,
        Mouse
    };

    struct Input_Source {
        Input_Type type;
        uint32_t device_id; // either a 'touchId' or a mouse 'which'
        uint32_t sub_id;    // either a 'fingerId' or a mouse 'button' 
    };

    Joy_State state;
    Input_Source input;
    SDL_FPoint center;
    SDL_FPoint current;
    float radius;
    // context (screen size?  display number?) -- for now, use renderer logical size
    int sdl_joystick_index;
    // SDL_Joystick *sdl_joystick;
    SDL_GameController *sdl_controller;

    bool is_active(const SDL_Event & ev) const;
    SDL_FPoint to_logical_space(const SDL_Event &ev) const;
    const char *state_name() const;
    SDL_Joystick *joystick();
};

static DL_Joy djoy;

SDL_Joystick * DL_Joy::joystick()
{
    if (sdl_controller) {
        return SDL_GameControllerGetJoystick(sdl_controller);
    }
    return nullptr;
}

bool DL_Joy::is_active(const SDL_Event & ev) const
{
    if (state != Active) {
        return false;
    }
    switch (ev.type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (input.device_id != ev.button.which ||
                input.sub_id != ev.button.button)
            {
                return false;
            }
            break;
        case SDL_MOUSEMOTION:
            if (input.device_id != ev.motion.which) {
                return false;
            }
            break;
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            if (input.device_id != ev.tfinger.touchId ||
                input.sub_id != ev.tfinger.fingerId)
            {
                return false;
            }
            break;
        default:
            return false;
    }
    return true;
}

SDL_FPoint DL_Joy::to_logical_space(const SDL_Event &ev) const
{
    SDL_FPoint pt = {0.f, 0.f};

    int logical_w = 0;
    int logical_h = 0;
    SDL_RenderGetLogicalSize(renderer, &logical_w, &logical_h);

    switch (ev.type) {
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            pt.x = ev.tfinger.x * static_cast<float>(logical_w);
            pt.y = ev.tfinger.y * static_cast<float>(logical_h);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            pt.x = static_cast<float>(ev.button.x);
            pt.y = static_cast<float>(ev.button.y);
            break;
        case SDL_MOUSEMOTION:
            pt.x = static_cast<float>(ev.motion.x);
            pt.y = static_cast<float>(ev.motion.y);
            break;
        default:
            break;
    }

    return pt;
}

const char * DL_Joy::state_name() const
{
    switch (state) {
        case Inactive: return "Inactive";
        case Active: return "Active";
    }
    return "";
}


SDL_Point DrawText(int x, int y, int scale, int wrap_at, const char *text, ...)
{
    va_list ap;
    va_start(ap, text);

    char formatted[1024 * 4] = {0};
    SDL_vsnprintf(formatted, sizeof(formatted), text, ap);
    const char * current = formatted;

    const int char_w = 5;
    const int char_h = 12;
    const int total_w = 100;
    const int total_h = 60;
    const int num_per_row = (total_w / char_w);

    SDL_Rect src_rect = {0, 0, char_w, char_h};
    SDL_Rect dst_rect = {x, y, char_w * scale, char_h * scale};
    int chars_in_row = 0;
    while (*current != '\0') {
        if (*current >= ' ' && *current <= '~') {
            const int ch_index = *current - ' ';
            const int row = ch_index / (num_per_row);
            const int col = ch_index - (row * num_per_row);
            src_rect.x = col * char_w;
            src_rect.y = row * char_h;
            SDL_RenderCopy(renderer, font_texture, &src_rect, &dst_rect);
            dst_rect.x += char_w * scale;
            chars_in_row++;
        } else if (*current == '\n') {
            dst_rect.x = x;
            dst_rect.y += (char_h * scale);
            chars_in_row = 0;
        } else {
            dst_rect.x += char_w * scale;
            chars_in_row++;
        }

        if (wrap_at > 0 && chars_in_row >= wrap_at) {
            dst_rect.x = x;
            dst_rect.y += (char_h * scale);
            chars_in_row = 0;
        }

        ++current;
    }

    va_end(ap);

    return SDL_Point { dst_rect.x, dst_rect.y };
}

void loop()
{
    const Uint64 now = SDL_GetPerformanceCounter();
    static Uint64 last = now;
    const float dt = (float)(now - last) / SDL_GetPerformanceFrequency();

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                std::exit(0);
                return;
            
            // case SDL_JOYDEVICEADDED: {
            //     SDL_Log("Joystick added, id: %d", ev.jdevice.which);
            //     break;
            // }

            case SDL_CONTROLLERDEVICEADDED: {
                SDL_JoystickID joy_id = SDL_JoystickGetDeviceInstanceID(ev.cdevice.which);
                SDL_GameController *game_ctrl = SDL_GameControllerFromInstanceID(joy_id);
                SDL_Log("Game-controller added, index:%d id:%d initial-ptr:%p",
                    ev.cdevice.which, joy_id, game_ctrl);
                if (!game_ctrl) {
                    game_ctrl = SDL_GameControllerOpen(ev.cdevice.which);
                    if (game_ctrl) {
                        SDL_Log("Game-controller opened, index:%d id:%d ptr:%p",
                            ev.cdevice.which,
                            SDL_JoystickGetDeviceInstanceID(ev.cdevice.which),
                            game_ctrl);
                    } else {
                        SDL_Log("Game-controller failed to opened, index:%d error:\"%s\"",
                            ev.cdevice.which,
                            SDL_GetError());
                    }
                }
                break;
            }

            case SDL_CONTROLLERDEVICEREMOVED: {
                SDL_GameController *game_ctrl = SDL_GameControllerFromInstanceID(ev.cdevice.which);
                SDL_Log("Game-controller removed, id:%d ptr:%p", ev.cdevice.which, game_ctrl);
                if (game_ctrl) {
                    SDL_GameControllerClose(game_ctrl);
                    game_ctrl = nullptr;
                }
                break;
            }

            case SDL_FINGERDOWN:
            case SDL_MOUSEBUTTONDOWN: {
                const SDL_FPoint point_in_logical_space = \
                    djoy.to_logical_space(ev);

                int logical_w = 0;
                int logical_h = 0;
                SDL_RenderGetLogicalSize(renderer, &logical_w, &logical_h);

                const bool is_in_logical_space = ! ( \
                    point_in_logical_space.x < 0. ||
                    point_in_logical_space.x >= static_cast<float>(logical_w) ||
                    point_in_logical_space.y < 0. ||
                    point_in_logical_space.y >= static_cast<float>(logical_h)
                );
                if (is_in_logical_space) {
                    if (djoy.state == DL_Joy::Inactive) {
                        djoy.center = djoy.current = {
                            point_in_logical_space.x,
                            point_in_logical_space.y
                        };

                        switch (ev.type) {
                            case SDL_FINGERDOWN:
                                djoy.input.type = DL_Joy::Finger;
                                djoy.input.device_id = ev.tfinger.touchId;
                                djoy.input.sub_id = ev.tfinger.fingerId;
                                break;
                            case SDL_MOUSEBUTTONDOWN:
                                djoy.input.type = DL_Joy::Mouse;
                                djoy.input.device_id = ev.button.which;
                                djoy.input.sub_id = ev.button.button;
                                break;
                            default:
                                SDL_assert(false && "unexpected SDL_Event type");
                                break;
                        }

                        djoy.state = DL_Joy::Active;
                        
                        // TODO: get joy_index from djoy, making sure that its value is -1 when disabled
                        // int joy_id = SDL_GetJoystick
                        // SDL_Joystick * joy = nullptr;
                        // joy = SDL_GameControllerGetJoystick(djoy.controller);
                        // if (joy) {
                        SDL_JoystickSetVirtualAxis(
                            djoy.joystick(),
                            SDL_CONTROLLER_AXIS_LEFTX,
                            0
                        );
                        SDL_JoystickSetVirtualAxis(
                            djoy.joystick(),
                            SDL_CONTROLLER_AXIS_LEFTY,
                            0
                        );
                        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                            "JOY START: {%f,%f} %p %p",
                            point_in_logical_space.x,
                            point_in_logical_space.y,
                            djoy.joystick(),
                            djoy.sdl_controller
                        );
                    }
                }

                break;
            }

            case SDL_FINGERMOTION:
            case SDL_MOUSEMOTION: {
                if (djoy.is_active(ev)) {
                    djoy.current = djoy.to_logical_space(ev);
                    float dx = djoy.current.x - djoy.center.x;
                    float dy = djoy.current.y - djoy.center.y;
                    const float dlen = std::sqrt((dx * dx) + (dy * dy));
                    if (dlen > djoy.radius) {
                        djoy.center = SDL_FPoint {
                            djoy.current.x - (dx * (djoy.radius / dlen)),
                            djoy.current.y - (dy * (djoy.radius / dlen)),
                        };
                        dx = djoy.current.x - djoy.center.x;
                        dy = djoy.current.y - djoy.center.y;
                    }

                    // djoy.center = (dlen <= djoy.radius) ?
                    //     djoy.center :

                    const Sint16 axis_x = \
                        (dx / djoy.radius) *
                        std::numeric_limits<Sint16>::max();
                    const Sint16 axis_y = \
                        (dy / djoy.radius) *
                        std::numeric_limits<Sint16>::max();

                    SDL_JoystickSetVirtualAxis(
                        djoy.joystick(),
                        SDL_CONTROLLER_AXIS_LEFTX,
                        axis_x
                    );
                    SDL_JoystickSetVirtualAxis(
                        djoy.joystick(),
                        SDL_CONTROLLER_AXIS_LEFTY,
                        axis_y
                    );

                    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                        "JOY MOVE: {%.2f,%.2f} %.2f ax:{%d,%d}\n", // -> {%f,%f} %f",
                        djoy.current.x,
                        djoy.current.y,
                        dlen,
                        (int)axis_x, (int)axis_y
                    );
                }
                break;
            }

            case SDL_FINGERUP:
            case SDL_MOUSEBUTTONUP: {
                if (djoy.is_active(ev)) {
                    // const float backup_radius = djoy.radius;
                    // memset(&djoy, 0, sizeof(djoy));
                    // djoy.radius = backup_radius;
                    SDL_JoystickSetVirtualAxis(
                        djoy.joystick(),
                        SDL_CONTROLLER_AXIS_LEFTX,
                        0
                    );
                    SDL_JoystickSetVirtualAxis(
                        djoy.joystick(),
                        SDL_CONTROLLER_AXIS_LEFTY,
                        0
                    );

                    djoy.state = DL_Joy::Inactive;
                    // djoy.input = {0};
                    // djoy.center = {0};
                    // djoy.current = {0};
                    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                        "JOY STOP"
                    );
                }
                break;
            }

            default:
                break;
        }
    }

    // Clear
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
    // SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xff, SDL_ALPHA_OPAQUE);
    // SDL_SetRenderDrawColor(renderer, 0x7f, 0x7f, 0x7f, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    // Draw joystick debug-state
    if (djoy.state == DL_Joy::Active) {
        SDL_Rect tmpRect = {0, 0, 5, 5};

        tmpRect.x = static_cast<int>(std::lround(djoy.center.x)) - ((tmpRect.w-1)/2);
        tmpRect.y = static_cast<int>(std::lround(djoy.center.y)) - ((tmpRect.h-1)/2);
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &tmpRect);

        tmpRect.x = static_cast<int>(std::lround(djoy.current.x)) - ((tmpRect.w-1)/2);
        tmpRect.y = static_cast<int>(std::lround(djoy.current.y)) - ((tmpRect.h-1)/2);
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &tmpRect);

        SDL_FPoint circ[32];
        const float pi = std::acos(-1.f);
        const float rad_step = (2.f * pi) / (SDL_arraysize(circ) - 1);
        for (int i = 0; i < SDL_arraysize(circ); ++i) {
            circ[i].x = djoy.center.x + (std::cos(rad_step * i) * djoy.radius);
            circ[i].y = djoy.center.y + (std::sin(rad_step * i) * djoy.radius);
        }
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawLinesF(renderer, circ, SDL_arraysize(circ));

        SDL_SetRenderDrawColor(renderer, 0xaa, 0xaa, 0xaa, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawLine(renderer,
            static_cast<int>(std::lround(djoy.center.x)),
            static_cast<int>(std::lround(djoy.center.y)),
            static_cast<int>(std::lround(djoy.current.x)),
            static_cast<int>(std::lround(djoy.current.y))
        );
    }

    // Update + Draw player
    int logical_w = 0;
    int logical_h = 0;
    SDL_RenderGetLogicalSize(renderer, &logical_w, &logical_h);
    static std::once_flag player_init_flag;
    std::call_once(player_init_flag, [&]() {
        if (logical_w > 0) {
            player_pos.x = logical_w / 2.f;
        }
        if (logical_h > 0) {
            player_pos.y = logical_h / 2.f;
        }
    });

    SDL_FPoint player_delta = {0.f, 0.f};

    const int num_joysticks = SDL_NumJoysticks();
    int num_controllers = 0;
    for (int i = 0; i < num_joysticks; ++i) {
        if (SDL_IsGameController(i)) {
            ++num_controllers;
        }
    }

    // Draw debug text
    SDL_Point text_pos = {20, 40};
    const int text_scale = 2;
    const int text_wrap_at = 35;
    text_pos = DrawText(text_pos.x, text_pos.y, text_scale, text_wrap_at,
        "state: %s\n"
        "num joysticks: %d\n"
        "num controllers: %d\n"
        "dt: %f seconds\n"
        "dead zone: %d\n"
        "player speed: %.02f v-pix/sec\n"
        ,
        djoy.state_name(),
        num_joysticks, num_controllers,
        dt,
        axis_dead_zone,
        speed_logical_units_per_s
    );

    for (int joy_index = 0; joy_index < num_joysticks; ++joy_index) {
        Sint16 controller_x = 0;
        Sint16 controller_y = 0;
        const SDL_JoystickID joy_id = SDL_JoystickGetDeviceInstanceID(joy_index);
        SDL_GameController *ctrl = SDL_GameControllerFromInstanceID(joy_id);
        if (!ctrl) {
            continue;
        }
        controller_x = SDL_GameControllerGetAxis(
            ctrl,
            SDL_CONTROLLER_AXIS_LEFTX
        );
        controller_y = SDL_GameControllerGetAxis(
            ctrl,
            SDL_CONTROLLER_AXIS_LEFTY
        );
        const float controller_normal_x = \
            static_cast<float>(controller_x) /
            SDL_JOYSTICK_AXIS_MAX;
        const float controller_normal_y = \
            static_cast<float>(controller_y) /
            SDL_JOYSTICK_AXIS_MAX;
        float ddx = 0.f;
        float ddy = 0.f;
        if (std::abs(controller_x) > axis_dead_zone) {
            ddx += controller_normal_x * dt * speed_logical_units_per_s;
        }
        if (std::abs(controller_y) > axis_dead_zone) {
            ddy += controller_normal_y * dt * speed_logical_units_per_s;
        }

        text_pos = DrawText(text_pos.x, text_pos.y, text_scale, text_wrap_at,
            "ctrl-er #%d (%s):\n"
            "... raw: %d, %d\n"
            "... normal: %.02f , %.02f\n"
            "... p-move: %.02f , %.02f\n"
            ,
            joy_index,
            (SDL_JoystickIsVirtual(joy_index) ? "virtual" : "hardware"),
            controller_x, controller_y,
            controller_normal_x, controller_normal_y,
            ddx, ddy
        );

        // text_pos = DrawText(text_pos.x, text_pos.y, text_scale, text_wrap_at,
        //     ": %.02f , %.02f\n\n",

        player_delta.x += ddx;
        player_delta.y += ddy;
    }

    player_pos.x = std::clamp(
        player_pos.x + player_delta.x,
        0.f,
        static_cast<float>(logical_w)
    );
    player_pos.y = std::clamp(
        player_pos.y + player_delta.y,
        0.f,
        static_cast<float>(logical_h)
    );

    text_pos = DrawText(text_pos.x, text_pos.y, text_scale, text_wrap_at,
        "player move: %.02f , %.02f\n\n",
        player_delta.x, player_delta.y
    );

    const float player_dimension = 32.;
    SDL_FRect player_rect = {
        player_pos.x - (player_dimension / 2.f),
        player_pos.y - (player_dimension / 2.f),
        player_dimension,
        player_dimension
    };
    SDL_BlendMode old_blend_mode;
    SDL_GetRenderDrawBlendMode(renderer, &old_blend_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0x7f);
    SDL_RenderFillRectF(renderer, &player_rect);
    SDL_SetRenderDrawBlendMode(renderer, old_blend_mode);

    // Present rendered frame
    SDL_RenderPresent(renderer);

    last = now;
}

int main(int, char **)
{
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    // const Uint32 sdl_flags = \
    //     SDL_INIT_VIDEO |
    //     SDL_INIT_JOYSTICK | 
    //     SDL_INIT_GAMECONTROLLER |
    //     SDL_INIT_EVENTS;
    const Uint32 sdl_flags = SDL_INIT_EVERYTHING;
    if (SDL_Init(sdl_flags) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(
        // title
        "dlvjoy",

        // x, y
        // SDL_WINDOWPOS_CENTERED_DISPLAY(1),
        // SDL_WINDOWPOS_CENTERED_DISPLAY(1),
        1920,
        40,

        // w, h
        320, 480,

        // flags
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }

    // 'djoy' initialization
    djoy.radius = 80.;
    djoy.sdl_joystick_index = \
        SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                  SDL_CONTROLLER_AXIS_MAX,
                                  SDL_CONTROLLER_BUTTON_MAX,
                                  0);
    if (djoy.sdl_joystick_index < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_JoystickAttachVirtual failed: %s", SDL_GetError());
        return 1;
    }
    // djoy.sdl_joystick_index = 0;

    // djoy.sdl_joystick = SDL_JoystickOpen(djoy.sdl_joystick_index);
    // if (!djoy.sdl_joystick) {
    //     SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_JoystickOpen failed: %s", SDL_GetError());
    //     return 1;
    // }
    // SDL_Log("djoy.sdl_joystick: %p", djoy.sdl_joystick);
    // SDL_JoystickGUID joystick_guid = SDL_JoystickGetGUID(djoy.sdl_joystick);
    // char joystick_guid_str[33] = {0};
    // SDL_JoystickGetGUIDString(joystick_guid, joystick_guid_str, sizeof(joystick_guid_str));
    // SDL_Log("joystick guid: %s\n", joystick_guid_str);

    djoy.sdl_controller = SDL_GameControllerOpen(djoy.sdl_joystick_index);
    if (!djoy.sdl_controller) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GameControllerOpen failed: %s", SDL_GetError());
        return 1;
    }
    SDL_Log("djoy.sdl_controller: %p", djoy.sdl_controller);


    // Since we're using renderer's logical-size to determine the
    // joystick's logical-space size, and because we want the
    // logical-space to encompass the entire window (whose size
    // may be set by the OS, via SDL), we'll apply the window-size
    // to the renderer's logical-size, here.
    int current_window_width = 0;
    int current_window_height = 0;
    SDL_GetWindowSize(window, &current_window_width, &current_window_height);
    SDL_RenderSetLogicalSize(renderer, current_window_width, current_window_height);
    // const int logical_width = 100;
    // const int logical_height = 320;
    // SDL_RenderSetLogicalSize(renderer, logical_w, logical_h);

    font_surface = STBIMG_Load("gnsh-bitmapfont-colour8.png");
    // SDL_Log("font_surface=%p ; size=%dx%d",
    //     font_surface,
    //     (font_surface ? font_surface->w : 0),
    //     (font_surface ? font_surface->h : 0)
    // );
    font_texture = SDL_CreateTextureFromSurface(renderer, font_surface);
    int tex_w = 0;
    int tex_h = 0;
    SDL_QueryTexture(font_texture, nullptr, nullptr, &tex_w, &tex_h);
    // SDL_Log("font_texture=%p ; size=%dx%d",
    //     font_texture,
    //     tex_w,
    //     tex_h
    // );

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (true) {
        loop();
    }
#endif
}
