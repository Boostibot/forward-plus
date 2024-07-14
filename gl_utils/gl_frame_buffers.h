#pragma once

#include "gl.h"
#include "../lib/string.h"

typedef struct Render_Screen_Frame_Buffers
{
    GLuint frame_buff;
    GLuint screen_color_buff;
    GLuint render_buff;

    i32 width;
    i32 height;

    String_Builder name;
} Render_Screen_Frame_Buffers;

typedef struct Render_Screen_Frame_Buffers_MSAA
{
    GLuint frame_buff;
    GLuint map_color_multisampled_buff;
    GLuint render_buff;
    GLuint intermediate_frame_buff;
    GLuint screen_color_buff;

    i32 width;
    i32 height;
    
    //used so that this becomes visible to debug_allocator
    // and thus we prevent leaking
    String_Builder name;
} Render_Screen_Frame_Buffers_MSAA;

void render_screen_frame_buffers_deinit(Render_Screen_Frame_Buffers* buffer)
{
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteFramebuffers(1, &buffer->frame_buff);
    glDeleteBuffers(1, &buffer->screen_color_buff);
    glDeleteRenderbuffers(1, &buffer->render_buff);

    array_deinit(&buffer->name); 

    memset(buffer, 0, sizeof *buffer);
}

void render_screen_frame_buffers_init(Render_Screen_Frame_Buffers* buffer, i32 width, i32 height)
{
    render_screen_frame_buffers_deinit(buffer);

    LOG_INFO("RENDER", "render_screen_frame_buffers_init %-4d x %-4d", width, height);
    
    buffer->width = width;
    buffer->height = height;
    buffer->name = builder_from_cstring(NULL, "Render_Screen_Frame_Buffers");

    //@NOTE: 
    //The lack of the following line caused me 2 hours of debugging why my application crashed due to NULL ptr 
    //deref in glDrawArrays. I still dont know why this occurs but just for safety its better to leave this here.
    glBindVertexArray(0);

    glGenFramebuffers(1, &buffer->frame_buff);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff);    

    // generate map
    glGenTextures(1, &buffer->screen_color_buff);
    glBindTexture(GL_TEXTURE_2D, buffer->screen_color_buff);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // attach it to currently bound render_screen_frame_buffers object
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer->screen_color_buff, 0);

    glGenRenderbuffers(1, &buffer->render_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, buffer->render_buff); 
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);  
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buffer->render_buff);

    TEST(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "frame buffer creation failed!");

    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); 
}

void render_screen_frame_buffers_render_begin(Render_Screen_Frame_Buffers* buffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff); 
        
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); 
    glFrontFace(GL_CW); 
}

void render_screen_frame_buffers_render_end(Render_Screen_Frame_Buffers* buffer)
{
    (void) buffer;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_screen_frame_buffers_post_process_begin(Render_Screen_Frame_Buffers* buffer)
{
    (void) buffer;
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // back to default
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void render_screen_frame_buffers_post_process_end(Render_Screen_Frame_Buffers* buffer)
{
    (void) buffer;
    //nothing
}


void render_screen_frame_buffers_msaa_deinit(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteFramebuffers(1, &buffer->frame_buff);
    glDeleteBuffers(1, &buffer->map_color_multisampled_buff);
    glDeleteRenderbuffers(1, &buffer->render_buff);
    glDeleteFramebuffers(1, &buffer->intermediate_frame_buff);
    glDeleteBuffers(1, &buffer->screen_color_buff);

    array_deinit(&buffer->name);

    memset(buffer, 0, sizeof *buffer);
}

bool render_screen_frame_buffers_msaa_init(Render_Screen_Frame_Buffers_MSAA* buffer, i32 width, i32 height, i32 sample_count)
{
    render_screen_frame_buffers_msaa_deinit(buffer);
    LOG_INFO("RENDER", "render_screen_frame_buffers_msaa_init %-4d x %-4d samples: %d", width, height, sample_count);

    glBindVertexArray(0);

    buffer->width = width;
    buffer->height = height;
    buffer->name = builder_from_cstring(NULL, "Render_Screen_Frame_Buffers_MSAA");

    bool state = true;
    glGenFramebuffers(1, &buffer->frame_buff);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff);

    // create a multisampled color attachment map
    glGenTextures(1, &buffer->map_color_multisampled_buff);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, buffer->map_color_multisampled_buff);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, sample_count, GL_RGB32F, width, height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, buffer->map_color_multisampled_buff, 0);

    // create a (also multisampled) renderbuffer object for depth and stencil attachments
    glGenRenderbuffers(1, &buffer->render_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, buffer->render_buff);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, sample_count, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buffer->render_buff);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR("RENDER", "frame buffer creation failed!");
        ASSERT(false);
        state = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // configure second post-processing framebuffer
    glGenFramebuffers(1, &buffer->intermediate_frame_buff);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->intermediate_frame_buff);

    // create a color attachment map
    glGenTextures(1, &buffer->screen_color_buff);
    glBindTexture(GL_TEXTURE_2D, buffer->screen_color_buff);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer->screen_color_buff, 0);	// we only need a color buffer

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR("RENDER", "frame buffer creation failed!");
        ASSERT(false);
        state = false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
}

void render_screen_frame_buffers_msaa_render_begin(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff); 
        
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); 
    glFrontFace(GL_CW); 
}

void render_screen_frame_buffers_msaa_render_end(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    (void) buffer;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_screen_frame_buffers_msaa_post_process_begin(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    // 2. now blit multisampled buffer(s) to normal colorbuffer of intermediate FBO. Image_Builder is stored in screenTexture
    glBindFramebuffer(GL_READ_FRAMEBUFFER, buffer->frame_buff);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, buffer->intermediate_frame_buff);
    glBlitFramebuffer(0, 0, buffer->width, buffer->height, 0, 0, buffer->width, buffer->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // back to default
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void render_screen_frame_buffers_msaa_post_process_end(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    (void) buffer;
}