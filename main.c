#define _CRT_SECURE_NO_WARNINGS
#pragma warning(error:4820)   //error on "Padding added to struct" 
#pragma warning(disable:4702) //Dissable "unrelachable code"
#pragma warning(disable:4464) //Dissable "relative include path contains '..'"
#pragma warning(disable:4255) //Dissable "no function prototype given: converting '()' to '(void)"  
#pragma warning(disable:5045) //Dissable "Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified"  
#pragma warning(disable:4201) //Dissable "nonstandard extension used: nameless struct/union" 
#pragma warning(disable:4296) //Dissable "expression is always true" (used for example in 0 <= val && val <= max where val is unsigned. This is used in generic CHECK_BOUNDS)
#pragma warning(disable:4996) //Dissable "This function or variable may be unsafe. Consider using localtime_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details."

#define JOT_ALL_IMPL
//#define JOT_ALL_TEST
#define JOT_COUPLED
//#define RUN_TESTS
//#define RUN_JUST_TESTS

#include "lib/string.h"
#include "lib/platform.h"
#include "lib/allocator.h"
#include "lib/hash_index.h"
#include "lib/log_file.h"
#include "lib/file.h"
#include "lib/allocator_debug.h"
#include "lib/allocator_malloc.h"
#include "lib/random.h"
#include "lib/math.h"
#include "lib/guid.h"
#include "lib/profile.h"
#include "lib/stable_array.h"

#include "gl_utils/gl.h"
#include "gl_utils/gl_shader_util.h"
#include "gl_utils/gl_debug_output.h"

#include "camera.h"

#include "glfw/glfw3.h"
#include "control.h"

#undef near
#undef far

unsigned int loadTexture(const char *path, bool gammaCorrection);
void renderQuad();
void renderCube();

// settings
#define SCR_WIDTH 1400
#define SCR_HEIGHT 1000

//Controls 

#define GLFW_KEY_COUNT              (GLFW_KEY_LAST + 1)
#define GLFW_MOUSE_BUTTON_COUNT     (GLFW_MOUSE_BUTTON_LAST + 1)
#define GLFW_GAMEPAD_BUTTON_COUNT   (GLFW_GAMEPAD_BUTTON_LAST + 1)
#define GLFW_JOYSTICK_BUTTON_COUNT  (GLFW_JOYSTICK_LAST + 1)

#define CONTROL_MAX_KEYS (GLFW_KEY_COUNT + GLFW_MOUSE_BUTTON_COUNT + GLFW_GAMEPAD_BUTTON_COUNT + GLFW_JOYSTICK_BUTTON_COUNT)

#define CONTROL_CODE_MOUSE_X 1
#define CONTROL_CODE_MOUSE_Y 2
#define CONTROL_CODE_SCROLL_X 3
#define CONTROL_CODE_SCROLL_Y 4

#define CONTROL_MAX_SMOOTHS 8

#define CONTROL_SLOTS 4


u16 control_code_from_glfw_key(int glfw_key)
{
    return (u16) glfw_key;
}

u16 control_code_from_glfw_mouse_button(int mouse_button)
{
    return (u16) (mouse_button + GLFW_KEY_COUNT);
}

u16 control_code_from_glfw_gamepad_button(int mouse_button)
{
    return (u16) (mouse_button + GLFW_KEY_COUNT + GLFW_MOUSE_BUTTON_COUNT);
}

u16 control_code_from_glfw_joystick_button(int mouse_button)
{
    return (u16) (mouse_button + GLFW_KEY_COUNT + GLFW_MOUSE_BUTTON_COUNT + GLFW_GAMEPAD_BUTTON_COUNT);
}

typedef enum Control_Type {
    CONTROL_TYPE_NOT_BOUND = 0,
    CONTROL_TYPE_KEY,
    CONTROL_TYPE_SMOOTH,
    CONTROL_TYPE_TOUCHPAD,
    CONTROL_TYPE_GAMEPAD,
    CONTROL_TYPE_JOYSTICK,
} Control_Type;

typedef struct Control_Inputs {
    //for the moment we differentiate only between two types of input:
    // discrete range (usually just on/off) style "keys" (keys, mouse buttons etc) 
    // and contiguous range "smooths" (mouse position, scroll wheel, dials).

    //"keys" operate on live values: that is after each frame we discard the contents
    //of states by setting it to 0 and wait for new values to come in.
    //On the other hand "smooths" operate on kept values: smooth_states are kept between frames.

    //The reason for this distinction is quite subtle. Each approach has its pros and cons:
    // live values: There is no kept state. Having it less state in our program makes it harder for
    //              bugs to propagate.
    // kept values: For smooth inputs live values would mean something like "the mouse was *moved* by
    //              20 pixels to the right as that is what hardware usually reports. However we run into
    //              a risk of drifting values. Over a long time small errors in the representation could
    //              pile up and the value that corresponds to the gamepad stick being in the upright position
    //              would no longer match. Because of this its safer to make smooth inputs kept. 
    //              (The reason why this isnt a problem for keys is that they are fundamentally discrete so the
    //              errors dont accumulate)
    // 
    //              Big dissadvatnga of kept values is that we cannot really make sense of the slots anymore! 
    //              This is because all slots should always report some value! just because we didnt interact with

    u8 states[CONTROL_MAX_KEYS];
    u8 interacts[CONTROL_MAX_KEYS];
    
    f64 smooth_states[CONTROL_MAX_SMOOTHS];
    u8 smooth_interacts[CONTROL_MAX_SMOOTHS];
    i64 smooth_interact_generation[CONTROL_MAX_SMOOTHS];

    i64 generation;
} Control_Inputs;

typedef struct Control_Event {
    Control_Type type;
    i32 interacts;
    f64 state;
    i64 generation;
} Control_Event;

typedef struct Control {
    Control_Type type;
    struct {
        u16 key_code;
    } slots[CONTROL_SLOTS];
} Control;

typedef Control Control_Mouse;
typedef Control Control_Smooth;
typedef Control Control_Key;

Control_Event control_event_smooth_most_recent(const Control_Inputs* inputs, Control control)
{
    Control_Event event = {CONTROL_TYPE_SMOOTH};
    event.generation = -1;
    for(int i = 0; i < CONTROL_SLOTS; i++)
    {
        u16 code = control.slots[i].key_code; 
        if(code != 0 && inputs->smooth_interact_generation[code] > event.generation)
        {
            event.generation = inputs->smooth_interact_generation[code];
            event.interacts = inputs->smooth_interacts[code];
            event.state = inputs->smooth_states[code];
        }
    }

    return event;
}

bool control_was_pressed(const Control_Inputs* inputs, Control control)
{
    for(int i = 0; i < CONTROL_SLOTS; i++)
    {
        u16 code = control.slots[i].key_code; 
        if(code == 0)
            continue;

        if(inputs->interacts[code] >= 2)
            return true;
            
        if(inputs->interacts[code] == 1 && inputs->states[code] > 0)
            return true;
    }

    return false;
}

bool control_was_released(const Control_Inputs* inputs, Control control)
{
    for(int i = 0; i < CONTROL_SLOTS; i++)
    {
        u16 code = control.slots[i].key_code; 
        if(code == 0)
            continue;

        if(inputs->interacts[code] >= 2)
            return true;
            
        if(inputs->interacts[code] == 1 && inputs->states[code] == 0)
            return true;
    }

    return false;
}

bool control_is_down(const Control_Inputs* inputs, Control control)
{
    for(int i = 0; i < CONTROL_SLOTS; i++)
    {
        u16 code = control.slots[i].key_code; 
        if(code != 0 && inputs->states[code] > 0)
            return true;
    }

    return false;
}

bool control_toggle(const Control_Inputs* inputs, Control control, bool previous_state)
{
    for(int i = 0; i < CONTROL_SLOTS; i++)
    {
        u16 code = control.slots[i].key_code; 
        if(code == 0)
            continue;

        bool was_pressed = inputs->interacts[code] >= 2 || (inputs->interacts[code] == 1 && inputs->states[code] == 0);
        if(was_pressed)
            return ((u8) previous_state + inputs->interacts[code]) % 2 == 1;
    }
    
    return previous_state;
}

void control_set(Control* control, Control_Type type, u16 code, int slot)
{
    control->type = type;
    CHECK_BOUNDS(slot, CONTROL_SLOTS);
    control->slots[slot].key_code = code;
}

void control_system_change_key(Control_Inputs* inputs, u16 key_code, u8 state)
{
    ASSERT(key_code < CONTROL_MAX_KEYS);
    inputs->interacts[key_code] += 1;
    inputs->states[key_code] = state;
}

void control_system_change_smooth(Control_Inputs* inputs, u16 key_code, f64 state)
{
    ASSERT(key_code < CONTROL_MAX_SMOOTHS);
    if(inputs->smooth_states[key_code] != state)
    {
        inputs->smooth_interact_generation[key_code] = inputs->generation;
        inputs->smooth_interacts[key_code] += 1;
        inputs->smooth_states[key_code] = state;
    }
}

void control_system_new_frame(Control_Inputs* inputs)
{
    memset(inputs->interacts, 0, sizeof inputs->interacts);
    //memset(inputs->states, 0, sizeof inputs->states);
    memset(inputs->smooth_interacts, 0, sizeof inputs->smooth_interacts);
    inputs->generation += 1;
}

typedef struct App_Controls {
    Control_Key move_forward;
    Control_Key move_backward;
    Control_Key move_left;
    Control_Key move_right;
    Control_Key move_up;
    Control_Key move_down;
    Control_Key move_sprint;

    Control_Key interact;

    Control_Key mouse_mode;

    Control_Smooth camera_zoom_smooth;
    Control_Key camera_zoom_in;
    Control_Key camera_zoom_out;
    Control_Key camera_lock_on_origin;
    Control_Key camera_roll_right;
    Control_Key camera_roll_left;

    Control_Smooth mouse_x;
    Control_Smooth mouse_y;
    Control_Smooth look_x;
    Control_Smooth look_y;

    Control_Key debug_log_perf_counters;
    Control_Key debug_reload_shaders;

    //u32 _padding;
} App_Controls;

typedef struct App_Settings {
    f32 base_fov;
    f32 movement_speed;
    f32 movement_sprint_mult;

    f32 screen_gamma;
    f32 screen_exposure;
    f32 paralax_heigh_scale;

    f32 mouse_sensitivity;
    f32 mouse_wheel_sensitivity;
    f32 mouse_sensitivity_scale_with_fov_amount; //[0, 1]

    f32 camera_roll_speed;
    f32 zoom_key_speed;
    f32 zoom_adjust_time;
    i32 MSAA_samples;
} App_Settings;

void set_default_settings(App_Settings* settings)
{
    settings->zoom_key_speed = 1;
    settings->camera_roll_speed = 1;

    settings->base_fov = PI/2;
    settings->movement_speed = 2.5f;
    settings->movement_sprint_mult = 5;
    settings->screen_gamma = 2.2f;
    settings->screen_exposure = 1;
    settings->mouse_sensitivity = 0.002f;
    settings->mouse_wheel_sensitivity = 2; // uwu
    settings->zoom_adjust_time = 0.1f;
    settings->mouse_sensitivity_scale_with_fov_amount = 1.0f;
    settings->MSAA_samples = 4;
}

typedef struct App_Kept_State {
    Control_Inputs inputs;
    Camera camera;

    Vec3 active_object_pos;
    Vec3 player_pos;

    f32 fov;
    f32 zoom_target_fov;
    f32 zoom_change_per_sec;

    u32 _padding2;
    f64 zoom_target_time;
    
    f64 frame_start_time;
    f64 delta_time;

    i32 window_screen_width;
    i32 window_screen_height;
    i32 framebuffer_width;
    i32 framebuffer_height;

    isize frame_index;

    bool is_in_mouse_mode;
    bool should_close;
    bool _padding[6];
} App_Kept_State;

typedef struct App_State {
    App_Settings settings;
    App_Controls controls;
    GLFWwindow* window;
    
    f64 scroll_x;
    f64 scroll_y;

    bool input_initialized;
    bool _padding[7];
    
    App_Kept_State curr;
    App_Kept_State prev;
} App_State;

void glfw_key_func(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_mouse_button_func(GLFWwindow* window, int button, int action, int mods);
void glfw_scroll_func(GLFWwindow* window, double xoffset, double yoffset);
void glfw_mouse_func(GLFWwindow* window, double mouse_x, double mouse_y);

void window_poll_input(App_State* app);
void process_input(App_State* app);
void set_default_controls(App_Controls* controls);
void set_default_settings(App_Settings* settings);

int main()
{
    //Jot things
    platform_init();

    Malloc_Allocator static_allocator = {0};
    malloc_allocator_init(&static_allocator, "static allocator");
    allocator_set_static(&static_allocator.allocator);
    
    Malloc_Allocator malloc_allocator = {0};
    malloc_allocator_init(&malloc_allocator, "fallback allocator");

    profile_init(&malloc_allocator.allocator);

    File_Logger file_logger = {0};
    file_logger_init_use(&file_logger, &malloc_allocator.allocator, "logs");

    Debug_Allocator debug_alloc = {0};
    debug_allocator_init_use(&debug_alloc, &malloc_allocator.allocator, DEBUG_ALLOCATOR_CAPTURE_CALLSTACK);
    
    static App_State app = {0};
    set_default_controls(&app.controls);
    set_default_settings(&app.settings);
    camera_set_look_dir(&app.curr.camera, vec3(0, 0, 1));

    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);  
    glfwWindowHint(GLFW_SAMPLES, app.settings.MSAA_samples);
    app.window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Undefered", NULL, NULL);
    if (app.window == NULL)
    {
        LOG_FATAL("APP", "Failed to create GLFW window");
        return -1;
    }
    glfwSetWindowUserPointer(app.window, &app);
    glfwMakeContextCurrent(app.window);

    //Glad and debug output
    gladSetGLOnDemandLoader((GLADloadfunc) glfwGetProcAddress);
    gl_debug_output_enable();

    // build and compile shaders
    Render_Shader shaderGeometryPass = {0};
    Render_Shader shaderLightingPass = {0};
    Render_Shader shaderLightBox = {0};
    TEST(render_shader_init_from_disk_split(&shaderGeometryPass, STRING("new_shaders/g_buffer.vert"), STRING("new_shaders/g_buffer.frag"), STRING()));
    TEST(render_shader_init_from_disk_split(&shaderLightingPass, STRING("new_shaders/deferred_shading.vert"), STRING("new_shaders/deferred_shading.frag"), STRING()));
    TEST(render_shader_init_from_disk_split(&shaderLightBox,     STRING("new_shaders/deferred_light_box.vert"), STRING("new_shaders/deferred_light_box.frag"), STRING()));

    // load models
    // -----------
    typedef Array(Vec3) Vec3_Array;
    Vec3_Array objectPositions = {0};
    array_push(&objectPositions, vec3(-3.0,  -0.5, -3.0));
    array_push(&objectPositions, vec3( 0.0,  -0.5, -3.0));
    array_push(&objectPositions, vec3( 3.0,  -0.5, -3.0));
    array_push(&objectPositions, vec3(-3.0,  -0.5,  0.0));
    array_push(&objectPositions, vec3( 0.0,  -0.5,  0.0));
    array_push(&objectPositions, vec3( 3.0,  -0.5,  0.0));
    array_push(&objectPositions, vec3(-3.0,  -0.5,  3.0));
    array_push(&objectPositions, vec3( 0.0,  -0.5,  3.0));
    array_push(&objectPositions, vec3( 3.0,  -0.5,  3.0));

    // configure g-buffer framebuffer
    // ------------------------------
    unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    unsigned int gPosition, gNormal, gAlbedoSpec;
    // position color buffer
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);
    // normal color buffer
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);
    // color + specular color buffer
    glGenTextures(1, &gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);
    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // finally check if framebuffer is complete
    TEST(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // lighting info
    // -------------
    const unsigned int NR_LIGHTS = 32;
    Vec3_Array lightPositions = {0};
    Vec3_Array lightColors = {0};
    srand(13);
    for (unsigned int i = 0; i < NR_LIGHTS; i++)
    {
        // calculate slightly random offsets
        float x = (rand()%100 / 100.0f)*6.0f;
        float y = (rand()%100 / 100.0f)*6.0f;
        float z = (rand()%100 / 100.0f)*6.0f;
        array_push(&lightPositions, vec3(x, y, z));
        // also calculate random color
        float r = (rand()%100 / 200.0f) + 0.5f; // between 0.5 and 1.0
        float g = (rand()%100 / 200.0f) + 0.5f; // between 0.5 and 1.0
        float b = (rand()%100 / 200.0f) + 0.5f; // between 0.5 and 1.0
        array_push(&lightColors, vec3(r, g, b));
    }
    
    glEnable(GL_DEPTH_TEST);
    
    render_shader_use(&shaderLightingPass);
    render_shader_set_i32(&shaderLightingPass, "gPosition", 0);
    render_shader_set_i32(&shaderLightingPass, "gNormal", 1);
    render_shader_set_i32(&shaderLightingPass, "gAlbedoSpec", 2);

    Mat4 view = {0};
    // render loop
    // -----------
    while (!glfwWindowShouldClose(app.window))
    {
        app.curr.frame_start_time = glfwGetTime();
        app.curr.delta_time = app.curr.frame_start_time - app.prev.frame_start_time; 

        control_system_new_frame(&app.curr.inputs);
        window_poll_input(&app);
        process_input(&app);

        camera_set_perspective(&app.curr.camera, app.curr.fov, (f32)app.curr.framebuffer_width/(f32)app.curr.framebuffer_height, 0.01f, 1000.0f);
        if(app.curr.framebuffer_width != app.prev.framebuffer_width || app.curr.framebuffer_height != app.prev.framebuffer_height)
            glViewport(0, 0, app.curr.framebuffer_width, app.curr.framebuffer_height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Mat4 projection = camera_get_projection_matrix(app.curr.camera);
        view = camera_get_view_matrix(app.curr.camera);
        
        // 1. geometry pass: render scene's geometry/color data into gbuffer
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            render_shader_use(&shaderGeometryPass);
            render_shader_set_mat4(&shaderGeometryPass, "projection", projection);
            render_shader_set_mat4(&shaderGeometryPass, "view", view);
            for (unsigned int i = 0; i < objectPositions.size; i++)
            {
                Mat4 model = mat4_identity();
                model = mat4_scale_affine(model, vec3_of(0.5f));
                model = mat4_translate(model, objectPositions.data[i]);
                render_shader_set_mat4(&shaderGeometryPass, "model", model);
                renderCube();
            }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. lighting pass: calculate lighting by iterating over a screen filled quad pixel-by-pixel using the gbuffer's content.
        render_shader_use(&shaderLightingPass);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
        // send light relevant uniforms
        for (int i = 0; i < lightPositions.size; i++)
        {
            render_shader_set_vec3(&shaderLightingPass, format_ephemeral("lights[%i].Position", i).data, lightPositions.data[i]);
            render_shader_set_vec3(&shaderLightingPass, format_ephemeral("lights[%i].Color", i).data, lightColors.data[i]);
            // update attenuation parameters and calculate radius
            const float linear = 0.7f;
            const float quadratic = 1.8f;
            render_shader_set_f32(&shaderLightingPass, format_ephemeral("lights[%i].Linear", i).data, linear);
            render_shader_set_f32(&shaderLightingPass, format_ephemeral("lights[%i].Quadratic", i).data, quadratic);
        }
        render_shader_set_vec3(&shaderLightingPass, "viewPos", app.curr.camera.pos);
        // finally render quad
        renderQuad();

        // 2.5. copy content of geometry's depth buffer to default framebuffer's depth buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
        // blit to default framebuffer. Note that this may or may not work as the internal formats of both the FBO and default framebuffer have to match.
        // the internal formats are implementation defined. This works on all of my systems, but if it doesn't on yours you'll likely have to write to the 		
        // depth buffer in another shader stage (or somehow see to match the default framebuffer's internal format with the FBO's internal format).
        glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 3. render lights on top of scene
        render_shader_use(&shaderLightBox);
        render_shader_set_mat4(&shaderLightBox, "projection", projection);
        render_shader_set_mat4(&shaderLightBox, "view", view);
        for (unsigned int i = 0; i < lightPositions.size; i++)
        {
            Mat4 model = mat4_identity();
            model = mat4_scale_affine(model, vec3_of(0.125f));
            model = mat4_translate(model, lightPositions.data[i]);
            
            render_shader_set_mat4(&shaderLightBox, "model", model);
            render_shader_set_vec3(&shaderLightBox, "lightColor", lightColors.data[i]);
            renderCube();
        }

        glfwSwapBuffers(app.window);
        app.curr.frame_index += 1;
        app.prev = app.curr;
    }

    glfwTerminate();
    return 0;
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}


// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}


void set_default_controls(App_Controls* controls)
{
    control_set(&controls->move_forward,    CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_W), 0);
    control_set(&controls->move_backward,   CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_S), 0);
    control_set(&controls->move_left,       CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_A), 0);
    control_set(&controls->move_right,      CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_D), 0);
    control_set(&controls->move_up,         CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_SPACE), 0);
    control_set(&controls->move_down,       CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_LEFT_SHIFT), 0);

    control_set(&controls->move_forward,    CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_UP), 1);
    control_set(&controls->move_backward,   CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_DOWN), 1);
    control_set(&controls->move_left,       CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_LEFT), 1);
    control_set(&controls->move_right,      CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_RIGHT), 1);
    control_set(&controls->move_sprint,     CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_LEFT_ALT), 1);

    control_set(&controls->mouse_mode,      CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_ESCAPE), 0);
    control_set(&controls->interact,        CONTROL_TYPE_KEY, control_code_from_glfw_mouse_button(GLFW_MOUSE_BUTTON_LEFT), 0);
    control_set(&controls->interact,        CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_E), 1);

    control_set(&controls->camera_lock_on_origin,  CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_L), 0);
    control_set(&controls->camera_roll_right,      CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_M), 0);
    control_set(&controls->camera_roll_left,       CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_N), 0);
    control_set(&controls->camera_zoom_in,         CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_O), 0);
    control_set(&controls->camera_zoom_out,        CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_P), 0);
    control_set(&controls->camera_zoom_smooth,     CONTROL_TYPE_SMOOTH, CONTROL_CODE_SCROLL_Y, 0);

    control_set(&controls->mouse_x,         CONTROL_TYPE_SMOOTH, CONTROL_CODE_MOUSE_X, 0);
    control_set(&controls->mouse_y,         CONTROL_TYPE_SMOOTH, CONTROL_CODE_MOUSE_Y, 0);
    control_set(&controls->look_x,          CONTROL_TYPE_SMOOTH, CONTROL_CODE_MOUSE_X, 0);
    control_set(&controls->look_y,          CONTROL_TYPE_SMOOTH, CONTROL_CODE_MOUSE_Y, 0);
    
    control_set(&controls->debug_log_perf_counters, CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_F1), 0);
    control_set(&controls->debug_reload_shaders,    CONTROL_TYPE_KEY, control_code_from_glfw_key(GLFW_KEY_F2), 0);
}

void window_poll_input(App_State* app)
{
    PROFILE_START();
    GLFWwindow* window = app->window;
    
    if(app->input_initialized == false)
    {
        glfwSetCursorPosCallback(window, glfw_mouse_func);
        glfwSetScrollCallback(window, glfw_scroll_func);
        glfwSetKeyCallback(window, glfw_key_func);
        glfwSetMouseButtonCallback(window, glfw_mouse_button_func);
        app->input_initialized = true;
    }

    glfwPollEvents();
    glfwGetWindowSize(window, &app->curr.window_screen_width, &app->curr.window_screen_height);
    glfwGetFramebufferSize(window, &app->curr.framebuffer_width, &app->curr.framebuffer_height);
    
    bool mouse_enabled = glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;
    if(mouse_enabled)
    {
        f64 new_mouse_x = 0;
        f64 new_mouse_y = 0;
        glfwGetCursorPos(window, &new_mouse_x, &new_mouse_y);
        control_system_change_smooth(&app->curr.inputs, CONTROL_CODE_MOUSE_X, new_mouse_x);
        control_system_change_smooth(&app->curr.inputs, CONTROL_CODE_MOUSE_Y, new_mouse_y);
    }

    if(mouse_enabled != app->curr.is_in_mouse_mode)
    {
        if(app->curr.is_in_mouse_mode == false)
        {
            if(glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); 
        }
        else
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); 
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }
    }
    
    PROFILE_END();
}


f32 math_fmod(f32 val, f32 range)
{
    f32 remainder = fmodf(val, range);
    remainder += remainder < 0 ? range : 0; 
    return remainder;
}

f32 angle_dist(f32 a, f32 b)
{
    f32 dist1 = math_fmod(a - b, TAU);
    f32 dist2 = math_fmod(b - a, TAU);
    f32 min_dist = fminf(dist1, dist2);
    return min_dist;
}


f32 point_line_dist(Vec3 line_offset, Vec3 line_dir, Vec3 point)
{
    Vec3 point_dir = vec3_sub(point, line_offset);
    Vec3 base = vec3_scale(line_dir, vec3_dot(line_dir, point_dir) / vec3_dot(line_dir, line_dir));
    return vec3_dist(base, point_dir);
}

void process_input(App_State* app)
{
    PROFILE_START();

    Control_Inputs* inputs = &app->curr.inputs;
    App_Settings* settings = &app->settings;
    App_Controls* controls = &app->controls;

    //interacts
    {
        if(control_was_pressed(inputs, controls->interact))
            LOG_DEBUG("CONTROL", "interact pressed!");
    
        app->curr.is_in_mouse_mode = control_toggle(inputs, controls->mouse_mode, app->curr.is_in_mouse_mode);
        app->curr.camera.is_locked_on = control_toggle(inputs, controls->camera_lock_on_origin, app->curr.camera.is_locked_on);
    }

    //Movement
    {
        f32 move_speed = settings->movement_speed;
        if(control_is_down(inputs, controls->move_sprint))
            move_speed *= settings->movement_sprint_mult;

        Vec3 direction_forward = camera_get_look_dir(app->curr.camera);
        Vec3 direction_up = camera_get_up_dir(app->curr.camera);
        Vec3 direction_right = vec3_cross(direction_forward, direction_up);
            
        Vec3 move_dir = {0};
        move_dir = vec3_add(move_dir, vec3_scale(direction_forward, (f32)control_is_down(inputs, controls->move_forward)));
        move_dir = vec3_add(move_dir, vec3_scale(direction_up,      (f32)control_is_down(inputs, controls->move_up)));
        move_dir = vec3_add(move_dir, vec3_scale(direction_right,   (f32)control_is_down(inputs, controls->move_right)));

        move_dir = vec3_add(move_dir, vec3_scale(direction_forward, -1*(f32)control_is_down(inputs, controls->move_backward)));
        move_dir = vec3_add(move_dir, vec3_scale(direction_up,      -1*(f32)control_is_down(inputs, controls->move_down)));
        move_dir = vec3_add(move_dir, vec3_scale(direction_right,   -1*(f32)control_is_down(inputs, controls->move_left)));

        if(vec3_len(move_dir) != 0.0f)
        {
            Vec3 move_ammount = vec3_scale(vec3_norm(move_dir), move_speed * (f32) app->curr.delta_time);
            Vec3 look_at = camera_get_look_at(app->curr.camera);
            Vec3 new_pos = vec3_add(app->curr.camera.pos, move_ammount);
            f32 dist = point_line_dist(look_at, direction_up, new_pos);

            f32 safe_dist = 0.3f;
            if(app->curr.camera.is_locked_on == false || dist > safe_dist)
                camera_set_position(&app->curr.camera, new_pos);
        }
    }

    //Camera rotation
    {
        Control_Event mouse_x = control_event_smooth_most_recent(inputs, controls->look_x);
        Control_Event mouse_y = control_event_smooth_most_recent(inputs, controls->look_y);

        if((mouse_x.interacts || mouse_y.interacts) && app->prev.is_in_mouse_mode == false && app->curr.camera.is_locked_on == false)
        {
            Control_Event mouse_x_prev = control_event_smooth_most_recent(&app->prev.inputs, app->controls.look_x);
            Control_Event mouse_y_prev = control_event_smooth_most_recent(&app->prev.inputs, app->controls.look_y);

            f64 xoffset = (mouse_x.state - mouse_x_prev.state);
            f64 yoffset = (mouse_y_prev.state - mouse_y.state);

            f32 fov_sens_modifier = sinf(app->curr.zoom_target_fov);
            f32 total_sensitivity = settings->mouse_sensitivity * lerpf(1, fov_sens_modifier, settings->mouse_sensitivity_scale_with_fov_amount);
            f32 yaw = (f32) xoffset*total_sensitivity;
            f32 pitch = (f32) yoffset*total_sensitivity;

            camera_set_angles_relative_to_up_dir(&app->curr.camera, yaw, pitch, app->curr.camera.yaw, app->curr.camera.pitch);
        }

        int roll_delta = control_is_down(inputs, controls->camera_roll_right) - control_is_down(inputs, controls->camera_roll_left);
        if(roll_delta != 0)
        {
            f32 roll = app->curr.camera.roll + roll_delta * settings->camera_roll_speed * (f32) app->curr.delta_time;
            camera_set_roll(&app->curr.camera, roll);
        }
    }
        
    //smooth zoom
    {
        //Works by setting a target fov, time to hit it and speed and then
        //interpolates smoothly until the value is hit
        if(app->curr.frame_index == 0)
        {
            app->curr.fov = settings->base_fov;
            app->curr.zoom_target_fov = settings->base_fov;
        }
           
        Control_Event scroll = control_event_smooth_most_recent(inputs, controls->camera_zoom_smooth);
        f64 zoom_mouse_delta = 0;
        if(scroll.interacts > 0)
            zoom_mouse_delta = scroll.state - control_event_smooth_most_recent(&app->prev.inputs, controls->camera_zoom_smooth).state;

        f64 zoom_key_delta = (f64) control_is_down(inputs, controls->camera_zoom_in) - (f64) control_is_down(inputs, controls->camera_zoom_out);
        zoom_mouse_delta *= settings->mouse_wheel_sensitivity*app->curr.delta_time;
        zoom_key_delta *= settings->zoom_key_speed*app->curr.delta_time;

        f64 zoom_delta = zoom_mouse_delta + zoom_key_delta;

        if(zoom_delta != 0)
        {   
            f32 fov_sens_modifier = sinf(app->curr.zoom_target_fov);
            f32 fov_delta = (f32) zoom_delta * settings->mouse_wheel_sensitivity * fov_sens_modifier;
                
            app->curr.zoom_target_fov = CLAMP(app->curr.zoom_target_fov + fov_delta, 0, TAU/2);
            app->curr.zoom_target_time = app->curr.frame_start_time + settings->zoom_adjust_time;
            app->curr.zoom_change_per_sec = (app->curr.zoom_target_fov - app->curr.fov) / settings->zoom_adjust_time;

        }
            
        if(app->curr.frame_start_time < app->curr.zoom_target_time)
        {
            f32 fov_before = app->curr.fov;
            app->curr.fov += app->curr.zoom_change_per_sec * (f32) app->curr.delta_time;

            //if is already past the target snap to target
            if(fov_before < app->curr.zoom_target_fov && app->curr.fov > app->curr.zoom_target_fov)
                app->curr.fov = app->curr.zoom_target_fov;
            if(fov_before > app->curr.zoom_target_fov && app->curr.fov < app->curr.zoom_target_fov)
                app->curr.fov = app->curr.zoom_target_fov;
        }
        else
            app->curr.fov = app->curr.zoom_target_fov;
            
        //LOG_INFO("CONTROL", "Fov %f", app->curr.fov);
    }
    

    PROFILE_END();
}

void glfw_key_func(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void) mods;
    (void) scancode;
    u8 value = 0;
    if(action == GLFW_PRESS)
        value = 1;
    else if(action != GLFW_RELEASE)
        return;

    App_State* app = (App_State*) glfwGetWindowUserPointer(window);
    control_system_change_key(&app->curr.inputs, control_code_from_glfw_key(key), value);
}

void glfw_mouse_button_func(GLFWwindow* window, int button, int action, int mods)
{
    (void) mods;
    u8 value = 0;
    if(action == GLFW_PRESS)
        value = 1;
    else if(action != GLFW_RELEASE)
        return;

    App_State* app = (App_State*) glfwGetWindowUserPointer(window);
    control_system_change_key(&app->curr.inputs, control_code_from_glfw_mouse_button(button), value);
}

void glfw_scroll_func(GLFWwindow* window, double xoffset, double yoffset)
{   
    (void) xoffset;
    App_State* app = (App_State*) glfwGetWindowUserPointer(window);
    app->scroll_x += xoffset;
    app->scroll_y += yoffset;
    control_system_change_smooth(&app->curr.inputs, CONTROL_CODE_SCROLL_Y, app->scroll_y);
}

void glfw_mouse_func(GLFWwindow* window, double mouse_x, double mouse_y)
{
    App_State* app = (App_State*) glfwGetWindowUserPointer(window);

    control_system_change_smooth(&app->curr.inputs, CONTROL_CODE_MOUSE_X, mouse_x);
    control_system_change_smooth(&app->curr.inputs, CONTROL_CODE_MOUSE_Y, mouse_y);
}
