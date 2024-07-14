#ifndef JOT_GL
#define JOT_GL

#include "../extrenal/include/glad2/gl.h"

#endif


#if (defined(JOT_ALL_IMPL) || defined(JOT_GL_IMPL)) && !defined(JOT_GL_HAS_IMPL) && !defined(GLAD_GL_IMPLEMENTATION)
#define JOT_GL_HAS_IMPL

#define GLAD_GL_IMPLEMENTATION
#include "../extrenal/include/glad2/gl.h"

#endif
