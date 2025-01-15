#pragma once


#include "gl.h"
#include "../lib/log.h"

#define DEBUG_OUTPUT_CHANEL "opengl"

INTERNAL const char* gl_translate_error(GLenum code)
{
    switch (code)
    {
        case GL_INVALID_ENUM:                  return "INVALID_ENUM";
        case GL_INVALID_VALUE:                 return "INVALID_VALUE";
        case GL_INVALID_OPERATION:             return "INVALID_OPERATION";
        case GL_STACK_OVERFLOW:                return "STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW:               return "STACK_UNDERFLOW";
        case GL_OUT_OF_MEMORY:                 return "OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "INVALID_FRAMEBUFFER_OPERATION";
        default:                               return "UNKNOWN_ERROR";
    }
}

GLenum _gl_check_error(const char *file, int line)
{
    GLenum errorCode = 0;
    while ((errorCode = glGetError()) != GL_NO_ERROR)
    {
        const char* error = gl_translate_error(errorCode);
        LOG_ERROR(DEBUG_OUTPUT_CHANEL, "GL error %s | %s (%d)", error, file, line);
    }
    return errorCode;
}

#define gl_check_error() _gl_check_error(__FILE__, __LINE__) 

void gl_debug_output_func(GLenum source, 
                            GLenum type, 
                            unsigned int id, 
                            GLenum severity, 
                            GLsizei length, 
                            const char *message, 
                            const void *userParam)
{
    // ignore non-significant error/warning codes
    if(id == 131169 || id == 131185 || id == 131218 || id == 131204) return; 

    (void) length;
    (void) userParam;
    
    Log log = log_info(DEBUG_OUTPUT_CHANEL);
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:         
        case GL_DEBUG_SEVERITY_MEDIUM:       log = log_error(DEBUG_OUTPUT_CHANEL); break;
        case GL_DEBUG_SEVERITY_LOW:          log = log_warn(DEBUG_OUTPUT_CHANEL);  break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: log = log_info(DEBUG_OUTPUT_CHANEL);  break;
    };

    LOG(log, "GL error (%d): %s", (int) id, message);

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:             LOG(log_indented(log), "Source: API"); break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   LOG(log_indented(log), "Source: Window System"); break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: LOG(log_indented(log), "Source: Shader Compiler"); break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     LOG(log_indented(log), "Source: Third Party"); break;
        case GL_DEBUG_SOURCE_APPLICATION:     LOG(log_indented(log), "Source: Application"); break;
        case GL_DEBUG_SOURCE_OTHER:           LOG(log_indented(log), "Source: Other"); break;
    };

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:               LOG(log_indented(log), "Type: Error"); break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: LOG(log_indented(log), "Type: Deprecated Behaviour"); break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  LOG(log_indented(log), "Type: Undefined Behaviour"); break; 
        case GL_DEBUG_TYPE_PORTABILITY:         LOG(log_indented(log), "Type: Portability"); break;
        case GL_DEBUG_TYPE_PERFORMANCE:         LOG(log_indented(log), "Type: Performance"); break;
        case GL_DEBUG_TYPE_MARKER:              LOG(log_indented(log), "Type: Marker"); break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          LOG(log_indented(log), "Type: Push Group"); break;
        case GL_DEBUG_TYPE_POP_GROUP:           LOG(log_indented(log), "Type: Pop Group"); break;
        case GL_DEBUG_TYPE_OTHER:               LOG(log_indented(log), "Type: Other"); break;
    };
}


static void gl_post_call_gl_callback(void *ret, const char *name, GLADapiproc apiproc, int len_args, ...) {
    GLenum error_code;

    (void) ret;
    (void) apiproc;
    (void) len_args;

    error_code = glad_glGetError();

    if (error_code != GL_NO_ERROR) 
        log_callstack(log_error(DEBUG_OUTPUT_CHANEL), 2, "error %s in %s!", gl_translate_error(error_code), name);
}

void gl_debug_output_enable()
{
    int flags = 0; 
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        LOG_INFO(DEBUG_OUTPUT_CHANEL, "Debug info enabled");
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); 
        glDebugMessageCallback(gl_debug_output_func, NULL);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    } 
    else
        LOG_INFO(DEBUG_OUTPUT_CHANEL, "Debug info wasnt enabled! Provide appropriate window hint!");

    gladSetGLPostCallback(gl_post_call_gl_callback);
    gladInstallGLDebug();
}
