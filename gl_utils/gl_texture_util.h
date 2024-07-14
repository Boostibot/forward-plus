#pragma once
#include "gl.h"
#include "gl_pixel_format.h"

typedef struct Compute_Texture {
    GLuint id;
    GL_Pixel_Format format;

    i32 width;
    i32 heigth;
} Compute_Texture;

Compute_Texture compute_texture_make_with(isize width, isize heigth, Pixel_Type format, isize channels, const void* data)
{
    GL_Pixel_Format pixel_format = gl_pixel_format_from_pixel_format(format, channels);
    ASSERT(pixel_format.unrepresentable == false);
    
    Compute_Texture tex = {0};
	glGenTextures(1, &tex.id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex.id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, pixel_format.internal_format, (GLuint) width, (GLuint) heigth, 0, pixel_format.format, pixel_format.type, data);

    tex.format = pixel_format;
    tex.width = (i32) width;
    tex.heigth = (i32) heigth;

	glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

Compute_Texture compute_texture_make(isize width, isize heigth, Pixel_Type type, isize channels)
{
    return compute_texture_make_with(width, heigth, type, channels, NULL);
}

void compute_texture_bind(Compute_Texture texture, GLenum access, isize slot)
{
	glBindImageTexture((GLuint) slot, texture.id, 0, GL_FALSE, 0, access, texture.format.internal_format);
    glBindTextureUnit((i32) slot, texture.id);
}

void compute_texture_deinit(Compute_Texture* texture)
{
    glDeleteTextures(1, &texture->id);
    memset(texture, 0, sizeof *texture);
}

void compute_texture_set_pixels(Compute_Texture* texture, Image_Builder image)
{
    compute_texture_deinit(texture);
    *texture = compute_texture_make_with(image.width, image.height, (Pixel_Type) image.pixel_format, image_builder_channel_count(image), image.pixels);
}

void compute_texture_get_pixels(Image_Builder* into, Compute_Texture texture)
{
    image_builder_init(into, into->allocator, texture.format.channels, texture.format.equivalent);
    image_builder_resize(into, (i32) texture.width, (i32) texture.heigth);

    glGetTextureImage(texture.id, 0, texture.format.format, texture.format.type, (GLsizei) image_builder_all_pixels_size(*into), into->pixels);
}

void compute_texture_set_pixels_converted(Compute_Texture* texture, Image_Builder image)
{
    if(texture->width != image.width || texture->heigth != image.height)
    {
        Pixel_Type prev_pixe_format = texture->format.equivalent;
        isize prev_channel_count = texture->format.channels;

        compute_texture_deinit(texture);
        *texture = compute_texture_make(image.width, image.height, prev_pixe_format, prev_channel_count);
    }

    GL_Pixel_Format gl_format = gl_pixel_format_from_pixel_format((Pixel_Type) image.pixel_format, image_builder_channel_count(image));
    glTextureSubImage2D(texture->id, 0, 0, 0, image.width, image.height, gl_format.format, gl_format.type, image.pixels);
}

void compute_texture_get_pixels_converted(Image_Builder* into, Compute_Texture texture)
{
    image_builder_resize(into, (i32) texture.width, (i32) texture.heigth);
    GL_Pixel_Format gl_format = gl_pixel_format_from_pixel_format((Pixel_Type) into->pixel_format, image_builder_channel_count(*into));
    
    glGetTextureImage(texture.id, 0, gl_format.format, gl_format.type, (GLsizei) image_builder_all_pixels_size(*into), into->pixels);
}