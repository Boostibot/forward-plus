#pragma once
#include "gl.h"
#include "../lib/image.h"

typedef struct GL_Pixel_Format {
    GLuint channel_type;     //GL_FLOAT, GL_UNSIGNED_BYTE, ...
    GLuint access_format;    //GL_RGBA, GL_RED, ...
    GLuint internal_format;  //GL_RGBA16I, GL_R8, GL_RGBA16F...
} GL_Pixel_Format;



//If fails sets all members to 0
GL_Pixel_Format gl_pixel_format_from_pixel_type(Pixel_Type pixel_format, isize channels)
{
    GL_Pixel_Format error_format = {0};
    
    GL_Pixel_Format out = {0};
    
    switch(channels)
    {
        case 1: out.access_format = GL_RED; break;
        case 2: out.access_format =  GL_RG; break;
        case 3: out.access_format =  GL_RGB; break;
        case 4: out.access_format =  GL_RGBA; break;
        
        default: return error_format;
    }

    #define PIXEL_TYPE_CASE(PIXEL_TYPE, GL_CHANNEL_TYPE, POSTFIX) \
        case PIXEL_TYPE: { \
            out.channel_type = GL_CHANNEL_TYPE; \
            switch(channels) \
            { \
                case 1: out.internal_format = GL_R ## POSTFIX; break; \
                case 2: out.internal_format = GL_RG ## POSTFIX; break; \
                case 3: out.internal_format = GL_RGB ## POSTFIX; break; \
                case 4: out.internal_format = GL_RGBA ## POSTFIX; break; \
            } \
        } break \


    switch(pixel_format)
    {
        PIXEL_TYPE_CASE(PIXEL_TYPE_U8, GL_UNSIGNED_BYTE, 8);
        PIXEL_TYPE_CASE(PIXEL_TYPE_U16, GL_UNSIGNED_SHORT, 16);
        PIXEL_TYPE_CASE(PIXEL_TYPE_U32, GL_UNSIGNED_INT, 32UI);
        case PIXEL_TYPE_U24: return error_format;
        case PIXEL_TYPE_U64: return error_format;

        PIXEL_TYPE_CASE(PIXEL_TYPE_I8, GL_BYTE, 8I);
        PIXEL_TYPE_CASE(PIXEL_TYPE_I16, GL_SHORT, 16I);
        PIXEL_TYPE_CASE(PIXEL_TYPE_I32, GL_INT, 32I);
        case PIXEL_TYPE_I24: return error_format;
        case PIXEL_TYPE_I64: return error_format;
        
        PIXEL_TYPE_CASE(PIXEL_TYPE_F16, GL_HALF_FLOAT, 16F);
        PIXEL_TYPE_CASE(PIXEL_TYPE_F32, GL_FLOAT, 32F);
        case PIXEL_TYPE_F8: return error_format;
        case PIXEL_TYPE_F64: return error_format;
        
        case PIXEL_TYPE_INVALID: 
        case PIXEL_TYPE_NONE: 
        default: return error_format;
    }

    #undef PIXEL_TYPE_CASE
    return out;
}

GL_Pixel_Format gl_pixel_format_from_pixel_type_size(Pixel_Type pixel_format, isize pixel_size)
{
    return gl_pixel_format_from_pixel_type(pixel_format, pixel_size / pixel_type_size(pixel_format));
}

//returns PIXEL_TYPE_INVALID and sets channels to 0 if couldnt match
Pixel_Type pixel_type_from_gl_internal_format(GLuint internal_format, i32* channels)
{
    ASSERT(channels != 0);
    #define GL_TYPE_CASE1(P1, PIXEL_TYPE) \
        case GL_R ## P1: \
            *channels = 1; return PIXEL_TYPE; \
        case GL_RG ## P1: \
            *channels = 2; return PIXEL_TYPE; \
        case GL_RGB ## P1: \
            *channels = 3; return PIXEL_TYPE; \
        case GL_RGBA ## P1: \
            *channels = 4; return PIXEL_TYPE; \

    #define GL_TYPE_CASE2(P1, P2, PIXEL_TYPE) \
        case GL_R ## P1: \
        case GL_R ## P2: \
            *channels = 1; return PIXEL_TYPE; \
        case GL_RG ## P1: \
        case GL_RG ## P2: \
            *channels = 2; return PIXEL_TYPE; \
        case GL_RGB ## P1: \
        case GL_RGB ## P2: \
            *channels = 3; return PIXEL_TYPE; \
        case GL_RGBA ## P1: \
        case GL_RGBA ## P2: \
            *channels = 4; return PIXEL_TYPE; \

    switch(internal_format)
    {
        GL_TYPE_CASE2(8, 8UI, PIXEL_TYPE_U8);
        GL_TYPE_CASE1(8I, PIXEL_TYPE_U8);
        
        GL_TYPE_CASE2(16, 16UI, PIXEL_TYPE_U16);
        GL_TYPE_CASE1(16I, PIXEL_TYPE_U16);

        GL_TYPE_CASE1(32UI, PIXEL_TYPE_U32);
        GL_TYPE_CASE1(32I, PIXEL_TYPE_I32);

        GL_TYPE_CASE1(16F, PIXEL_TYPE_F16);
        GL_TYPE_CASE1(32F, PIXEL_TYPE_F32);

        default: 
            *channels = 0;
            return PIXEL_TYPE_INVALID;
        break;
    }

    #undef GL_TYPE_CASE1
    #undef GL_TYPE_CASE2
}

//returns PIXEL_TYPE_INVALID on failiure
Pixel_Type pixel_type_from_gl_access_format(GL_Pixel_Format gl_format)
{
    switch(gl_format.channel_type)
    {
        case GL_UNSIGNED_BYTE: return PIXEL_TYPE_U8;
        case GL_UNSIGNED_SHORT: return PIXEL_TYPE_U16;
        case GL_UNSIGNED_INT: return PIXEL_TYPE_U32;

        case GL_BYTE: return PIXEL_TYPE_I8;
        case GL_SHORT: return PIXEL_TYPE_I16;
        case GL_INT: return PIXEL_TYPE_I32;

        case GL_HALF_FLOAT: return PIXEL_TYPE_F16;
        case GL_FLOAT: return PIXEL_TYPE_F32;

        default: return PIXEL_TYPE_INVALID;
    }
}

//Outputs 0 to channels if couldnt match
Pixel_Type pixel_type_from_gl_pixel_format(GL_Pixel_Format gl_format, i32* channels)
{
    return pixel_type_from_gl_internal_format(gl_format.internal_format, channels);
}
