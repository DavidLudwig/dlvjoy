
#include <SDL.h>
#include <math.h>
#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

typedef enum {
    Input_None,
    Input_Finger,
    Input_Mouse
} Input_Type;

typedef struct {
    Sint64 device_id;   // either a 'touchId' or a mouse 'which'
    Sint64 sub_id;      // either a 'fingerId' or a mouse 'button' 
    Input_Type type;
} Input_Source;

static SDL_bool vjoy_is_active = SDL_FALSE; // SDL_TRUE when the vjoy is active
static SDL_FPoint vjoy_center;      // center of the vjoy
static SDL_FPoint vjoy_current;     // current position of the vjoy
static SDL_GameController * vjoy_controller = NULL; // the vjoy's SDL_GameController
static Input_Source vjoy_input_source;  // where vjoy input is actively coming from
static const float vjoy_radius = 80.f;  // max-radius of vjoy

#ifndef WINDOW_INITIAL_X
    #define WINDOW_INITIAL_X SDL_WINDOWPOS_CENTERED
#endif
#ifndef WINDOW_INITIAL_Y
    #define WINDOW_INITIAL_Y SDL_WINDOWPOS_CENTERED
#endif

static SDL_Window * window = NULL;      // app's SDL_Window
static SDL_Renderer * renderer = NULL;  // app's SDL_Renderer
static float player_cx = 0.f;       // center of player on X axis
static float player_cy = 0.f;       // center of player on Y axis
static const float player_size = 32.f;     // width and height of player, as drawn
static const int axis_dead_zone = 8000;    // lower-threshold for game controller axis values
static const float player_speed = 150.f;   // in window-units per second
static int game_w = 0;              // width of game-field; in SDL_Window size units
static int game_h = 0;              // height of game-field; in SDL_Window size units
static Uint64 prev_loop_time = 0;   // previous value of loop()'s 'now';
                                    //   as retrieved from SDL_GetPerformanceCounter()


static int round_f2i(float x) 
{ 
    return (int)((x < 0) ?
        (x - 0.5f) :
        (x + 0.5f)
    );
}

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    } else if (value > max_value) {
        return max_value;
    } else {
        return value;
    }
}

static void resize_game_field(int w, int h)
{
    SDL_RenderSetLogicalSize(renderer, w, h);
    player_cx = w / 2.f;
    player_cy = h / 2.f;
    game_w = w;
    game_h = h;
}

static SDL_bool is_from_input_source(const SDL_Event *event, const Input_Source *src)
{
    if (!src || !event) {
        return SDL_FALSE;
    }
    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (src->type == Input_Mouse &&
                src->device_id == event->button.which &&
                src->sub_id == event->button.button)
            {
                return SDL_TRUE;
            }
            break;
        case SDL_MOUSEMOTION:
            if (src->type == Input_Mouse &&
                src->device_id == event->motion.which)
            {
                return SDL_TRUE;
            }
            break;
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            if (src->type == Input_Finger &&
                src->device_id == event->tfinger.touchId &&
                src->sub_id == event->tfinger.fingerId)
            {
                return SDL_TRUE;
            }
            break;
        default:
            break;
    }
    return SDL_FALSE;
}

static SDL_FPoint to_game_coordinate(const SDL_Event *event)
{
    SDL_FPoint pt = {0.f, 0.f};

    if (!event) {
        return pt;
    }

    switch (event->type) {
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            pt.x = event->tfinger.x * (float)game_w;
            pt.y = event->tfinger.y * (float)game_h;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            pt.x = (float)event->button.x;
            pt.y = (float)event->button.y;
            break;
        case SDL_MOUSEMOTION:
            pt.x = (float)event->motion.x;
            pt.y = (float)event->motion.y;
            break;
        default:
            break;
    }

    return pt;
}

static void loop()
{
    // Determine time-difference from last frame
    const Uint64 now = SDL_GetPerformanceCounter();
    const float dt = (prev_loop_time > 0) ?
        (float)(now - prev_loop_time) / SDL_GetPerformanceFrequency() :
        0.f;

    // Other vars
    float player_dx = 0.f;
    float player_dy = 0.f;
    int num_joysticks = 0;
    SDL_FRect player_rect = {0};
    SDL_Event event;
    int i;
    int joy_index;

    // We're done with 'prev_loop_time' for now; go ahead
    // and set it up for the next call to loop.
    prev_loop_time = now;

    // Process SDL events
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                exit(0);    // exit() to let the OS clean up resources
                break;
            
            case SDL_WINDOWEVENT: {
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED: {
                        // SDL_Window size == game-field size
                        resize_game_field(event.window.data1, event.window.data2);
                        break;
                    }
                }
                break;
            }

            // Open and close game-controllers as they're
            // connected and disconnected
            case SDL_CONTROLLERDEVICEADDED: {
                SDL_JoystickID joy_id = SDL_JoystickGetDeviceInstanceID(event.cdevice.which);
                if (!SDL_GameControllerFromInstanceID(joy_id)) {
                    SDL_GameControllerOpen(event.cdevice.which);
                }
                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED: {
                SDL_GameController *game_ctrl = SDL_GameControllerFromInstanceID(event.cdevice.which);
                if (game_ctrl) {
                    SDL_GameControllerClose(game_ctrl);
                    game_ctrl = NULL;
                }
                break;
            }

            // Process input events from touch-screens and mice
            case SDL_FINGERDOWN:
            case SDL_MOUSEBUTTONDOWN: {
                const SDL_FPoint input_coord = to_game_coordinate(&event);
                if (!vjoy_is_active) {
                    vjoy_center.x = vjoy_current.x = input_coord.x;
                    vjoy_center.y = vjoy_current.y = input_coord.y;
                    switch (event.type) {
                        case SDL_FINGERDOWN:
                            vjoy_input_source.type = Input_Finger;
                            vjoy_input_source.device_id = event.tfinger.touchId;
                            vjoy_input_source.sub_id = event.tfinger.fingerId;
                            break;
                        case SDL_MOUSEBUTTONDOWN:
                            vjoy_input_source.type = Input_Mouse;
                            vjoy_input_source.device_id = event.button.which;
                            vjoy_input_source.sub_id = event.button.button;
                            break;
                    }

                    // Mark vjoy as active
                    vjoy_is_active = SDL_TRUE;

                    // Make sure vjoy position is at zero, just-in-case
                    SDL_JoystickSetVirtualAxis(
                        SDL_GameControllerGetJoystick(vjoy_controller),
                        SDL_CONTROLLER_AXIS_LEFTX,
                        0
                    );
                    SDL_JoystickSetVirtualAxis(
                        SDL_GameControllerGetJoystick(vjoy_controller),
                        SDL_CONTROLLER_AXIS_LEFTY,
                        0
                    );
                }

                break;
            }

            case SDL_FINGERMOTION:
            case SDL_MOUSEMOTION: {
                float dx;
                float dy;
                float dlength;

                if (vjoy_is_active &&
                    is_from_input_source(&event, &vjoy_input_source))
                {
                    vjoy_current = to_game_coordinate(&event);
                    dx = vjoy_current.x - vjoy_center.x;
                    dy = vjoy_current.y - vjoy_center.y;

                    // Move the v-joy's center if it is outside of its radius
                    dlength = SDL_sqrtf((dx * dx) + (dy * dy));
                    if (dlength > vjoy_radius) {
                        vjoy_center.x = vjoy_current.x - (dx * (vjoy_radius / dlength));
                        vjoy_center.y = vjoy_current.y - (dy * (vjoy_radius / dlength));
                        dx = vjoy_current.x - vjoy_center.x;
                        dy = vjoy_current.y - vjoy_center.y;
                    }

                    // Update vjoy state
                    SDL_JoystickSetVirtualAxis(
                        SDL_GameControllerGetJoystick(vjoy_controller),
                        SDL_CONTROLLER_AXIS_LEFTX,
                        (Sint16)((dx / vjoy_radius) * SDL_JOYSTICK_AXIS_MAX)
                    );
                    SDL_JoystickSetVirtualAxis(
                        SDL_GameControllerGetJoystick(vjoy_controller),
                        SDL_CONTROLLER_AXIS_LEFTY,
                        (Sint16)((dy / vjoy_radius) * SDL_JOYSTICK_AXIS_MAX)
                    );
                }
                break;
            }

            case SDL_FINGERUP:
            case SDL_MOUSEBUTTONUP: {
                if (vjoy_is_active &&
                    is_from_input_source(&event, &vjoy_input_source))
                {
                    // Reset vjoy position to zero
                    SDL_JoystickSetVirtualAxis(
                        SDL_GameControllerGetJoystick(vjoy_controller),
                        SDL_CONTROLLER_AXIS_LEFTX,
                        0
                    );
                    SDL_JoystickSetVirtualAxis(
                        SDL_GameControllerGetJoystick(vjoy_controller),
                        SDL_CONTROLLER_AXIS_LEFTY,
                        0
                    );

                    // Mark vjoy as inactive
                    vjoy_is_active = SDL_FALSE;
                }
                break;
            }

            default:
                break;
        }
    }

    // Render a black background
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    // Draw vjoy debug-state
    if (vjoy_is_active) {
        SDL_Rect temp_rect = {0, 0, 5, 5};
        SDL_FPoint circle[32];
        const float rad_step = (2.f * (float)M_PI) / (SDL_arraysize(circle) - 1);

        // Draw vjoy center position
        temp_rect.x = round_f2i(vjoy_center.x) - ((temp_rect.w-1)/2);
        temp_rect.y = round_f2i(vjoy_center.y) - ((temp_rect.h-1)/2);
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &temp_rect);

        // Draw vjoy current position
        temp_rect.x = round_f2i(vjoy_current.x) - ((temp_rect.w-1)/2);
        temp_rect.y = round_f2i(vjoy_current.y) - ((temp_rect.h-1)/2);
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &temp_rect);

        // Draw line from vjoy center position, to vjoy current position
        SDL_SetRenderDrawColor(renderer, 0xaa, 0xaa, 0xaa, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawLine(renderer,
            round_f2i(vjoy_center.x),
            round_f2i(vjoy_center.y),
            round_f2i(vjoy_current.x),
            round_f2i(vjoy_current.y)
        );

        // Draw vjoy radius
        for (i = 0; i < (int)SDL_arraysize(circle); ++i) {
            circle[i].x = vjoy_center.x + (SDL_cosf(rad_step * i) * vjoy_radius);
            circle[i].y = vjoy_center.y + (SDL_sinf(rad_step * i) * vjoy_radius);
        }
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, SDL_ALPHA_OPAQUE);
        SDL_RenderDrawLinesF(renderer, circle, SDL_arraysize(circle));
    }

    // Move player, using inputs from each game-controller
    num_joysticks = SDL_NumJoysticks();
    for (joy_index = 0; joy_index < num_joysticks; ++joy_index) {
        Sint16 input_value = 0;
        const SDL_JoystickID joy_id = SDL_JoystickGetDeviceInstanceID(joy_index);
        SDL_GameController *controller = SDL_GameControllerFromInstanceID(joy_id);
        if (!controller) {
            // The joystick isn't supported by SDL's Game Controller API,
            // and thus has unidentifiable axes.  Skip it.
            continue;
        }

        input_value = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
        if (SDL_abs(input_value) > axis_dead_zone) {
            player_dx += ((float)input_value / SDL_JOYSTICK_AXIS_MAX) *
                         dt * player_speed;
        }

        input_value = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
        if (SDL_abs(input_value) > axis_dead_zone) {
            player_dy += ((float)input_value / SDL_JOYSTICK_AXIS_MAX) *
                         dt * player_speed;
        }
    }
    player_cx = clampf(player_cx + player_dx, 0.f, game_w);
    player_cy = clampf(player_cy + player_dy, 0.f, game_h);

    // Draw the player
    player_rect.x = player_cx - (player_size / 2.f);
    player_rect.y = player_cy - (player_size / 2.f);
    player_rect.w = player_size;
    player_rect.h = player_size;
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRectF(renderer, &player_rect);

    // Present rendered frame
    SDL_RenderPresent(renderer);
}

int main()
{
    int vjoy_index = -1;    // SDL-index of virtual joystick/game-controller
    int window_w = 0;
    int window_h = 0;

    // Initialize SDL
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(
        "dlvjoy",
        WINDOW_INITIAL_X, WINDOW_INITIAL_Y,
        320, 480,
        SDL_WINDOW_RESIZABLE
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

    // Create a virtual joystick/game-controller
    vjoy_index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                           SDL_CONTROLLER_AXIS_MAX,
                                           SDL_CONTROLLER_BUTTON_MAX,
                                           0);
    if (vjoy_index < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_JoystickAttachVirtual failed: %s", SDL_GetError());
        return 1;
    }

    vjoy_controller = SDL_GameControllerOpen(vjoy_index);
    if (!vjoy_controller) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GameControllerOpen failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("Num joysticks at init: %d\n", SDL_NumJoysticks());

    // Resize the game field to that of the window.
    // Please note that on some platforms, the specified window
    // size might not be equal to the requested size.
    SDL_GetWindowSize(window, &window_w, &window_h);
    resize_game_field(window_w, window_h);

    // Loop indefinitely
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (1) {
        loop();
    }
#endif
}