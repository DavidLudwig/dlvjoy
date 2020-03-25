#include <SDL.h>
#include <cmath>
#include <algorithm>

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"

static SDL_Window * window = nullptr;
static SDL_Renderer * renderer = nullptr;
static SDL_Surface * font_surface = nullptr;
static SDL_Texture * font_texture = nullptr;

/*

TODO:
- support multiple windows
- refactor out anonymous, parameter-less,
  referencing functions

*/


// "logical space" - the SDL_Renderer's logical space
//
SDL_FPoint finger_to_logical_space(float finger_x, float finger_y, bool *is_inside_logical_space)
{
    int logical_w = 0;
    int logical_h = 0;
    SDL_RenderGetLogicalSize(renderer, &logical_w, &logical_h);

    // Finger coordinates are normalized in the 0..1 range
    const SDL_FPoint finger_point_in_logical_space = {
        finger_x * static_cast<float>(logical_w),
        finger_y * static_cast<float>(logical_h)
    };

    if (is_inside_logical_space) {
        *is_inside_logical_space = ! \
            (finger_point_in_logical_space.x < 0. ||
            finger_point_in_logical_space.x >= static_cast<float>(logical_w) ||
            finger_point_in_logical_space.y < 0. ||
            finger_point_in_logical_space.y >= static_cast<float>(logical_h));
    }

    return finger_point_in_logical_space;
}


static const float Default_Radius = 40.f;

struct DL_Joy {
    SDL_FingerID fingerId;
    enum {
        Inactive,
        Active
    } state;
    SDL_FPoint center;
    SDL_FPoint current;
    float radius;
    // context (screen size?  display number?) -- for now, use renderer logical size
};

DL_Joy djoy;


void DrawTextScaled(int x, int y, int scale, const char *text)
{
    // static bool first = true;
    const int char_w = 5;
    const int char_h = 12;
    const int total_w = 100;
    const int total_h = 60;
    // int total_w = 0;
    // int total_h = 0;
    // SDL_QueryTexture(font_texture, nullptr, nullptr, &total_w, &total_h);

    const int num_per_row = (total_w / char_w);

    SDL_Rect src_rect = {0, 0, char_w, char_h};
    SDL_Rect dst_rect = {x, y, char_w * scale, char_h * scale};
    while (*text != '\0') {
        if (*text >= ' ' && *text <= '~') {
            const int ch_index = *text - ' ';
            const int row = ch_index / (num_per_row);
            const int col = ch_index - (row * num_per_row);
            src_rect.x = col * char_w;
            src_rect.y = row * char_h;
            // if (first) {
            //     SDL_Log("%s ch_index=%02d num-pr=%d row=%d col=%d src={%d,%d,%d,%d} dst={%d,%d,%d,%d}",
            //         __FUNCTION__, ch_index, num_per_row, row, col,
            //         src_rect.x, src_rect.y, src_rect.w, src_rect.h,
            //         dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h
            //     );
            // }
            SDL_RenderCopy(renderer, font_texture, &src_rect, &dst_rect);
        }
        dst_rect.x += char_w * scale;
        ++text;
    }

    // first = false;
}

int DefaultTextScale = 2;
void DrawText(int x, int y, const char *text)
{
    DrawTextScaled(x, y, DefaultTextScale, text);
}

int main(int, char **)
{
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

    if (SDL_CreateWindowAndRenderer(320, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP, &window, &renderer) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return 1;
    }

    int vjoy_index = \
        SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                  SDL_CONTROLLER_AXIS_MAX,
                                  SDL_CONTROLLER_BUTTON_MAX,
                                  0);
    if (vjoy_index < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_JoystickAttachVirtual failed: %s", SDL_GetError());
        return 1;
    }
    SDL_Joystick *joystick = SDL_JoystickOpen(vjoy_index);
    if (!joystick) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_JoystickOpen failed: %s", SDL_GetError());
        return 1;
    }
    // SDL_Log("joystick: %p", joystick);
    // SDL_JoystickGUID joystick_guid = SDL_JoystickGetGUID(joystick);
    // char joystick_guid_str[33] = {0};
    // SDL_JoystickGetGUIDString(joystick_guid, joystick_guid_str, sizeof(joystick_guid_str));
    // SDL_Log("joystick guid: %s\n", joystick_guid_str);

    SDL_GameController *controller = SDL_GameControllerOpen(vjoy_index);
    if (!controller) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GameControllerOpen failed: %s", SDL_GetError());
        return 1;
    }
    // SDL_Log("controller: %p", controller);

    djoy.radius = Default_Radius;

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

    while (true) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    return 0;
                case SDL_FINGERDOWN: {
                    bool finger_in_logical_space = false;
                    const SDL_FPoint finger_point_in_logical_space = \
                        finger_to_logical_space(ev.tfinger.x, ev.tfinger.y, &finger_in_logical_space);

                    SDL_LogVerbose(SDL_LOG_CATEGORY_INPUT,
                        "%s, finger in logical: {%f,%f} %s fgr_id=%llu", __FUNCTION__,
                        finger_point_in_logical_space.x, finger_point_in_logical_space.y,
                        (finger_in_logical_space ? "IN" : "OUT"),
                        ev.tfinger.fingerId
                    );

                    if (finger_in_logical_space) {
                        [&]() { // start_joystick
                            if (djoy.state != DL_Joy::Inactive) {
                                return;
                            }
                            djoy.center = djoy.current = {
                                finger_point_in_logical_space.x,
                                finger_point_in_logical_space.y
                            };
                            djoy.fingerId = ev.tfinger.fingerId;
                            djoy.state = DL_Joy::Active;
                            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                                "JOY START: {%f,%f}",
                                finger_point_in_logical_space.x,
                                finger_point_in_logical_space.y
                            );
                        }();
                    }

                    break;
                }
                case SDL_FINGERMOTION: {
                    if (djoy.fingerId == ev.tfinger.fingerId) {
                        if (djoy.state == DL_Joy::Active) {
                            bool is_finger_inside = false;
                            const SDL_FPoint finger_in_logical_space = \
                                finger_to_logical_space(ev.tfinger.x, ev.tfinger.y, &is_finger_inside);

                            djoy.current = finger_in_logical_space;

                            const float dx = djoy.current.x - djoy.center.x;
                            const float dy = djoy.current.y - djoy.center.y;
                            const float dlen = std::sqrt((dx * dx) + (dy * dy));
                            djoy.center = (dlen <= djoy.radius) ?
                                djoy.center :
                                SDL_FPoint {
                                    djoy.current.x - (dx * (djoy.radius / dlen)),
                                    djoy.current.y - (dy * (djoy.radius / dlen)),
                                };

                            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                                "JOY MOVE: {%.2f,%.2f} %.2f", // -> {%f,%f} %f",
                                djoy.current.x,
                                djoy.current.y,
                                dlen
                            );

                        }
                    }
                    break;
                }
                case SDL_FINGERUP: {
                    if (djoy.fingerId == ev.tfinger.fingerId) {
                        if (djoy.state == DL_Joy::Active) {
                            // stop_joystick
                            const float backup_radius = djoy.radius;
                            memset(&djoy, 0, sizeof(djoy));
                            djoy.radius = backup_radius;
                            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                                "JOY STOP"
                            );
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }

        // Clear
        // SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        // SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xff, SDL_ALPHA_OPAQUE);
        SDL_SetRenderDrawColor(renderer, 0x7f, 0x7f, 0x7f, SDL_ALPHA_OPAQUE);
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

        // DrawText(20, 100, "A B~C");

        // Present rendered frame
        SDL_RenderPresent(renderer);
    }
}
