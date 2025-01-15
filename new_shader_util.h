#pragma once

#include "../lib/file.h"
#include "../lib/hash_index.h"
#include "../lib/random.h"
#include "../lib/log_list.h"

#include "math.h"
#include "gl.h"

#define SHADER_UTIL_CHANEL "SHADER"

#define GL_PREPROCESS -1

const char* shader_stage_name(int stage)
{
    switch(stage)
    {
        case 0:                     return "not init";
        case GL_PROGRAM:            return "program";
        case GL_VERTEX_SHADER:      return "vertex";
        case GL_FRAGMENT_SHADER:    return "fragment";
        case GL_GEOMETRY_SHADER:    return "geometry";
        default:                    return "unknown";
    }
}

enum {
    SHADER_MAX_STAGES = 8,
    SHADER_ERROR_LOG_LEN = 1024,    
    SHADER_NAME_LEN = 64,
    SHADER_UNIFORM_LEN = 64,
};


typedef struct Shader_Compile_Errors {
    char logs[SHADER_ERROR_LOG_LEN];
    int  stages[SHADER_MAX_STAGES];
    int  logs_offsets[SHADER_MAX_STAGES];
    int  count;
} Shader_Compile_Errors;

GLuint shader_compile(const String* sources, const int* stages, isize stages_count, Hash_Index* repository_or_null, Shader_Compile_Errors* errors_or_null)
{
    PROFILE_START();
    ASSERT(stages_count < SHADER_MAX_STAGES - 1);
    
    int success = true;
    GLuint program = 0;
    GLuint compiled[SHADER_MAX_STAGES] = {0};
    isize error_pos = 0;
    
    for(isize i = 0; i < stages_count; i++)
    {
        String source = sources[i];
        int stage = stages[i];
        GLuint* shader = &compiled[i];

        uint64_t hash = 0;
        if(repository_or_null != NULL)
        {
            hash = xxhash64(source.data, source.size, hash);
            isize found = hash_index_find(*repository_or_null, hash);
            if(found != -1)
            {
                *shader = repository_or_null->entries[found].value;
                continue;
            }
        }

        //geometry shader doesnt have to exist
        *shader = glCreateShader(stage);
        GLint len = source.size;
        glShaderSource(*shader, 1, &source, &len);
        glCompileShader(*shader);

        //Check for errors
        glGetShaderiv(*shader, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            if(errors_or_null != NULL)
            {
                GLsizei rem_size = SHADER_ERROR_LOG_LEN - error_pos; 
                GLsizei err_size = 0;
                if(rem_size > 0)
                    glGetShaderInfoLog(shader, rem_size, &err_size, errors_or_null->logs + error_pos);
                else
                    error_pos = SHADER_ERROR_LOG_LEN;

                errors_or_null->count += 1;
                errors_or_null->stages[i] = stage;
                errors_or_null->logs_offsets[i] = error_pos;

                error_pos += err_size + 1;
            }
        }
        else
        {
            if(repository_or_null != NULL)
                hash_index_insert(repository_or_null, hash, *shader);
        }
    }
        
    if(success)
    {
        program = glCreateProgram();
        for(isize i = 0; i < stages_count; i++)
            glAttachShader(program, compiled[i]);
            
        //Link and validate program 
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if(success)
        {
            glValidateProgram(program);
            glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
        }

        //Report errors
        if(!success)
        {
            if(errors_or_null != NULL)
            {
                GLsizei rem_size = SHADER_ERROR_LOG_LEN - error_pos; 
                GLsizei err_size = 0;
                if(rem_size > 0)
                    glGetProgramInfoLog(program, rem_size, &err_size, errors_or_null->logs + error_pos);
                else
                    error_pos = SHADER_ERROR_LOG_LEN;

                errors_or_null->count += 1;
                errors_or_null->stages[stages_count] = GL_PROGRAM;
                errors_or_null->logs_offsets[stages_count] = error_pos;

                error_pos += err_size + 1;
            }

            //cleanup
            glDeleteProgram(program);
            program = 0;
        }
    }

    //Deinit shader stages if not kept in repository
    if(repository_or_null == NULL)
    {
        for(isize i = 0; i < stages_count; i++)
            glDeleteShader(compiled[i]);
    }

    PROFILE_END();

    return program;
}

void shader_repository_deinit(Hash_Index* shader_repository)
{
    for(isize i = 0; i < shader_repository->entries_count; i++)
    {
        Hash_Index_Entry entry = shader_repository->entries[i];
        if(hash_index_is_entry_used(entry))
            glDeleteShader((GLuint) entry.value);
    }

    hash_index_deinit(shader_repository);
}


#include "../lib/parse.h"
#include "../lib/path.h"

String_Builder shader_source_add_line_numbers(String data, Allocator* allocator)
{
    String_Builder composed = builder_make(allocator, data.size*12/10 + 10);
    for(Line_Iterator it = {0}; line_iterator_get_line(&it, data);)
        vformat_append_into(&composed, "%4i %*.s\n", (int) it.line_number, (int) it.line.size, it.line.data);

    return composed;
}

typedef struct Shader_File_Cache {
    Allocator* allocator;
    Hash_Index path_hash;
    String_Builder_Array file_contents;
    String_Builder_Array file_paths;
} Shader_File_Cache;

enum {
    SHADER_PREPROCESS_MAX_DEPTH = 128,
};

isize shader_file_cache_add_file(Shader_File_Cache* cache, Path path, String file_content, bool* was_inserted)
{
    Arena_Frame scratch = scratch_arena_acquire();
    Path norm_path = path.info.is_normalized ? path : path_normalize(&scratch.allocator, path, 0).path;
    u64 file_path_hash = xxhash64(norm_path.data, norm_path.size, 0);

    isize found = hash_index_find_or_insert(&cache->path_hash, file_path_hash, HASH_INDEX_VALUE_MAX);
    Hash_Index_Entry* entry = &cache->path_hash.entries[found];
    if(entry->value == HASH_INDEX_VALUE_MAX)
    {
        *was_inserted = true;
        entry->value = cache->file_contents.size;
        array_push(&cache->file_contents, builder_from_string(cache->allocator, norm_path.string));
        array_push(&cache->file_paths, builder_from_string(cache->allocator, file_content));
    }
    else
        *was_inserted = false;

    arena_frame_release(&scratch);

    return entry->value;
}

String_Builder _shader_preprocess(String source, String prepend, Path source_directory, String top_file, i32 already_included[SHADER_PREPROCESS_MAX_DEPTH], isize depth, Shader_File_Cache* file_cache, bool allow_filesystem_access, bool* state, String_Builder* errors_or_null, Allocator* allocator)
{
    String_Builder preprocessed = builder_make(allocator, 0);
    Arena_Frame scratch = scratch_arena_acquire();
    
    *state = true;
    isize version_line = -1;
    for(Line_Iterator it = {0}; line_iterator_get_line(&it, source);)
    {
        if(version_line == -1)
        {
            if(string_find_first(it.line, STRING("#version "), 0) != -1)
                version_line = it.line_number;     
        }

        if(it.line_number == version_line + 1)
            builder_append_line(&preprocessed, prepend);

        bool push_line =true;
        if(file_cache)
        {
            isize preprocess_i = string_find_first_char(it.line, '#', 0);
            if(preprocess_i != -1)
            {
                bool nested_state = true;
                isize include_i = preprocess_i;
                match_whitespace(it.line, &include_i);
                if(match_sequence(it.line, &include_i, STRING("include")))
                {
                    push_line = false;
                    match_whitespace(it.line, &include_i);
                    isize before_quote_include = include_i;
                    isize after_quote_include = before_quote_include;
                    if(match_char(it.line, &before_quote_include, '"') == false || match_char_custom(it.line, &before_quote_include, '"', MATCH_INVERTED) == false)
                    {
                        if(errors_or_null)
                            format_append_into(errors_or_null, "'%.%s':%lli: malformed include statement: %*.s\n", 
                                it.line_number, (int) top_file.size, top_file.data, (int) it.line.size, it.line.data);
                        nested_state = false;
                    }
                    else if(depth + 1 >= SHADER_PREPROCESS_MAX_DEPTH)
                    {
                        if(errors_or_null)
                            format_append_into(errors_or_null, "'%.%s':%lli: include recursion %lli too deep! Cyclical include?\n", 
                                it.line_number, (int) top_file.size, top_file.data, depth);

                        nested_state = false;
                        return preprocessed;
                    }
                    else
                    {
                        String include_path_str = string_range(it.line, before_quote_include + 1, after_quote_include - 1);
                        Path include_path = path_parse(include_path_str);
                        Path_Builder combined_include_path = path_concat(&scratch.allocator, source_directory, include_path);
                        
                        u64 file_path_hash = xxhash64(combined_include_path.data, combined_include_path.size, 0);
                        
                        String_Builder read = builder_make(&scratch.allocator, 0);
                        isize found = hash_index_find(file_cache->path_hash, file_path_hash);
                        if(found != -1)
                        {
                            //Check for cyclical include
                            for(isize i = 0; i < depth; i++)
                            {
                                if(file_path_hash == already_included[i])
                                {
                                    nested_state = false;
                                    if(errors_or_null)
                                        format_append_into(errors_or_null, "'%.%s':%lli: Cyclical include through file '%.s'\n", 
                                            it.line_number, (int) top_file.size, top_file.data, (int) combined_include_path.size, combined_include_path.data);
                                }
                            }

                            Hash_Index_Entry* entry = &file_cache->path_hash.entries[found];
                            ASSERT(string_is_equal(file_cache->file_paths.data[entry->value].string, combined_include_path.string));
                            read = file_cache->file_contents.data[entry->value];
                        }
                        else if(allow_filesystem_access == false)
                        {
                            if(errors_or_null)
                                format_append_into(errors_or_null, "'%.%s':%lli: failed to read file '%.s' because it isnt in cache and file system access is disabled.\n", 
                                    it.line_number, (int) top_file.size, top_file.data, (int) combined_include_path.size, combined_include_path.data);
                        }
                        else
                        {
                            //Read the entire file into read
                            Platform_File_Info info = {0};
                            Platform_File file = {0};
                            Platform_Error error = platform_file_info(combined_include_path.string, &info);
                            
                            if(error == 0)
                                error = platform_file_open(&file, combined_include_path.string, PLATFORM_FILE_MODE_READ);
                            if(error == 0)
                            {
                                isize read_bytes = 0;
                                read = builder_make(&scratch.allocator, info.size);
                                error = platform_file_read(&file, read.data, info.size, &read_bytes);
                            }
                            platform_file_close(&file);

                            if(error != 0)
                            {
                                nested_state = false;
                                if(errors_or_null)
                                    format_append_into(errors_or_null, "'%.%s':%lli: failed to read file '%.s': %s\n", 
                                        it.line_number, (int) top_file.size, top_file.data, (int) combined_include_path.size, combined_include_path.data, platform_translate_error(error));
                            }

                            //We insert even when invalid. Because I am lazy.
                            isize index = file_cache->file_contents.size;
                            array_push(&file_cache->file_contents, builder_from_string(file_cache->allocator, read.string));
                            array_push(&file_cache->file_paths, builder_from_string(file_cache->allocator, combined_include_path.string));
                            found = hash_index_insert(&file_cache->path_hash, file_path_hash, index);
                        }

                        if(nested_state)
                        {
                            already_included[depth] = file_path_hash;
                            Path nested_directory = path_parse(path_get_directory(combined_include_path.path));
                            String_Builder nested_preprocessed = _shader_preprocess(read.string, STRING(), nested_directory, top_file, already_included, depth + 1, file_cache, &nested_state, errors_or_null, &scratch.allocator);
                            builder_append_line(&preprocessed, nested_preprocessed.string);
                        }
                    }
                }

                *state = *state && nested_state; 
            }
        }

        if(push_line && *state)
            builder_append_line(&preprocessed, it.line);
    }

    arena_frame_release(&scratch);

    //reset the return string if error
    if(*state)
        builder_init(&preprocessed, allocator);

    return preprocessed;
}

typedef struct Log_Stream {
    void (*log)(Log_Stream* self, const char* name, const char* fmt, ...);
    void *context;
} Log_Stream;

void log_call(Log_Stream* stream, const char* name, const char* fmt, ...);

#define LOG(stream, name, ...) (sizeof printf("" ##__VA_ARGS__), log_call(stream, (name), "" ##__VA_ARGS__))

typedef struct Specific_Error_Log {
    Log_Stream stream;
    void (*log)(int a, int b, int c, void* context);
    void* context;
} Specific_Error_Log;

void error_log(void* log_stream, const char* name, const char* fmt, ...);

void log_add_error(String some_context, int a, int b, Specific_Error_Log* log)
{
    //yes

    LOG(log, ">Hello", "world");

    //LOG_INFO(X) -> log_call(log_info_stream(), __FILE__, __func__, __LINE__, X, Y);
    error_log(log, ">>>:file.txt|func|123:hello", "%s", "hi!");

    //yes
    error_log(log, "hello", "%s", "hi!");
    if(log_has_specific(log))
        log->log(1, 2, 3, log->context);
}

void test(Specific_Error_Log* specific)
{
    if(specific)
        specific->specific_log(1, 2, 3, specific->specific_context);

    if(specific == NULL)
    {   

    }
}

Specific_Error_Log* error_log_from_string_builder(String_Builder* builder);

void caller()
{
    String_Builder errors = {0};
    test(NULL);
    test(&errors);
    test(error_log_from_string_builder(&errors));

}

String_Builder shader_preprocess(String source, String prepend, String source_file, Shader_File_Cache* file_cache, bool* state, String_Builder* errors_or_null, Allocator* allocator)
{
    Arena_Frame scratch = scratch_arena_acquire();

    i32 already_included[SHADER_PREPROCESS_MAX_DEPTH] = {0};
    Path_Builder norm_path = path_normalize(&scratch, path_parse(source_file), 0);
    already_included[0] = xxhash64(norm_path.data, norm_path.size, 0);

    Path source_directory = path_parse(path_get_directory(norm_path.path));
    String_Builder out = _shader_preprocess(source, prepend, source_directory, source_file, already_included, 1, file_cache, state, errors_or_null, allocator);

    arena_frame_release(&scratch);
}

String_Builder shader_preprocess_from_disk(String source, String prepend, String source_file, Shader_File_Cache* file_cache, bool* state, String_Builder* errors_or_null, Allocator* allocator)
{
    
}

typedef struct {
    i32 max_block_invocations;
    i32 max_block_count[3];
    i32 max_block_size[3];
} Compute_Shader_Limits;

Compute_Shader_Limits compute_shader_query_limits()
{
    static Compute_Shader_Limits querried = {-1};
    if(querried.max_block_invocations < 0)
    {
        STATIC_ASSERT(sizeof(GLint) == sizeof(i32));
	    for (GLuint i = 0; i < 3; i++) 
        {
		    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &querried.max_block_count[i]);
		    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &querried.max_block_size[i]);

            querried.max_block_size[i] = MAX(querried.max_block_size[i], 1);
            querried.max_block_count[i] = MAX(querried.max_block_count[i], 1);
	    }	
	    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &querried.max_block_invocations);

        querried.max_block_invocations = MAX(querried.max_block_invocations, 1);
    }

    return querried;
}

typedef struct Shader_Uniform {
    GLuint uniform;
    char name[SHADER_UNIFORM_LEN];
} Shader_Uniform;

typedef Array(Shader_Uniform) Shader_Uniform_Array;

typedef struct Shader {
    char name[SHADER_NAME_LEN];
    int stages[SHADER_MAX_STAGES];
    GLuint shader;
    i32 block_size_x;
    i32 block_size_y;
    i32 block_size_z;
    
    Allocator* allocator;
    Hash_Index uniform_hash;
    Shader_Uniform_Array uniforms;
} Shader;

void shader_deinit(Shader* shader)
{
    glDeleteProgram(shader->shader);
    hash_index_deinit(&shader->uniform_hash);
    array_deinit(&shader->uniforms);
    memset(shader, 0, sizeof *shader);
}

typedef struct Shader_Source {

    String pieces[8];
    String path;
    String name;
};

bool shader_init_split(Shader* shader, const String* sources, const String* names, const int* stages, isize stages_count, Shader_Compile_Errors* errors_or_null)
{
    shader_deinit(shader);
    
    String name_start = string_safe_head(name, SHADER_NAME_LEN - 1);
    memcpy(shader->name, name_start.data, name_start.size);

    Arena_Frame scratch = scratch_arena_acquire();

    String prepend_sources[SHADER_MAX_STAGES] = {0};
    if(prepends_or_null == NULL)
    {
        for(isize i = 0; i < stages_count; i++)
            prepend_sources[i] = sources[i];
    }
    else
    {
        for(isize i = 0; i < stages_count; i++)
            prepend_sources[i] = shader_source_prepend(sources[i], prepends_or_null[i], &scratch.allocator).string;
    }       

    Shader_Compile_Errors errors_backup = {0};
    Shader_Compile_Errors* errors = errors_or_null ? errors_or_null : &errors_backup;
    shader->shader = shader_compile(prepend_sources, stages, stages_count, repository_or_null, errors);

    if(errors_or_null == NULL && errors->count > 0)
    {
        
        for(isize i = 0; i < )
    }

    arena_frame_release(&scratch);
}

bool compute_shader_init_from_disk(Shader* shader, String path, isize work_group_x, isize work_group_y, isize work_group_z)
{
    Compute_Shader_Limits limits = compute_shader_query_limits();
    work_group_x = MIN(work_group_x, limits.max_block_size[0]);
    work_group_y = MIN(work_group_y, limits.max_block_size[1]);
    work_group_z = MIN(work_group_z, limits.max_block_size[2]);

    String name = path_get_filename_without_extension(path_parse(path));
    String prepend = format_ephemeral(
        "\n #define CUSTOM_DEFINES"
        "\n #define BLOCK_SIZE_X %lli"
        "\n #define BLOCK_SIZE_Y %lli"
        "\n #define BLOCK_SIZE_Z %lli"
        "\n",
        work_group_x, work_group_y, work_group_z
        );
    
    bool state = true;
    Arena_Frame arena = scratch_arena_acquire();
    {
        Log_List log_list = {0};
        log_list_init_capture(&log_list, &arena.allocator);
            String_Builder source = builder_make(&arena.allocator, 0);
            state = state && file_read_entire(path, &source);

            String_Builder prepended_source = shader_source_prepend(source.string, prepend, &arena.allocator);
            String_Builder error_string = builder_make(&arena.allocator, 0);

            //LOG_DEBUG(SHADER_UTIL_CHANEL, "Compute shader source:\n%s", prepended_source.data);
            state = state && compute_shader_init(shader, prepended_source.data, name, &error_string);

        log_capture_end(&log_list);

        if(state == false)
        {
            LOG_ERROR_CHILD(SHADER_UTIL_CHANEL, "compile error", NULL, "compute_shader_init_from_disk() failed: ");
                LOG_INFO(">" SHADER_UTIL_CHANEL, "path: '%s'", cstring_ephemeral(path));
                LOG_ERROR(">" SHADER_UTIL_CHANEL, "error: %s", error_string.data);
        }
        else
        {
            shader->work_group_size_x = (i32) work_group_x;
            shader->work_group_size_y = (i32) work_group_y;
            shader->work_group_size_z = (i32) work_group_z;
        }
    }
    arena_frame_release(&arena);

    return state;
}

bool shader_init_from_disk_split(Shader* shader, String vertex_path, String fragment_path, String geometry_path)
{
    PROFILE_START();
    bool state = true;
    Arena_Frame arena = scratch_arena_acquire();
    {
        Log_List log_list = {0};
        log_list_init_capture(&log_list, &arena.allocator);

        String_Builder vertex_source = builder_make(&arena.allocator, 0);
        String_Builder fragment_source = builder_make(&arena.allocator, 0);
        String_Builder geometry_source = builder_make(&arena.allocator, 0);
        
        String name = path_get_filename_without_extension(path_parse(fragment_path));
        bool vertex_state = file_read_entire(vertex_path, &vertex_source);
        bool fragment_state = true;
        bool geometry_state = true;

        if(fragment_path.size > 0)
            file_read_entire(fragment_path, &fragment_source);

        if(geometry_path.size > 0)
            geometry_state = file_read_entire(geometry_path, &geometry_source);

        state = vertex_state && fragment_state && geometry_state;
        state = state &&shader_init(shader,
            vertex_source.data,
            fragment_source.data,
            geometry_source.data,
            name);
            
        log_capture_end(&log_list);

        if(state == false)
        {
            LOG_ERROR_CHILD(SHADER_UTIL_CHANEL, "compile error", NULL, "shader_init_from_disk() failed!");
                LOG_INFO(">" SHADER_UTIL_CHANEL, "vertex:   '%s'", cstring_ephemeral(vertex_path));
                LOG_INFO(">" SHADER_UTIL_CHANEL, "fragment: '%s'", cstring_ephemeral(fragment_path));
                LOG_INFO(">" SHADER_UTIL_CHANEL, "geometry: '%s'", cstring_ephemeral(geometry_path));
                LOG_ERROR_CHILD(">" SHADER_UTIL_CHANEL, "", log_list.first, "errors:");
        }
    }
    arena_frame_release(&arena);
    PROFILE_END();
    return state;
}

bool shader_init_from_disk(Shader* shader, String path)
{
    LOG_INFO(SHADER_UTIL_CHANEL, "loading: '%s'", cstring_ephemeral(path));

    PROFILE_START();
    bool state = true;
    Arena_Frame arena = scratch_arena_acquire();
    {
        Log_List log_list = {0};
        log_list_init_capture(&log_list, &arena.allocator);

        String_Builder source_text = builder_make(&arena.allocator, 0);

        String name = path_get_filename_without_extension(path_parse(path));
        state = state && file_read_entire(path, &source_text);
        String source = source_text.string;
        
        String_Builder vertex_source = shader_source_prepend(source, STRING("#define VERT"), &arena.allocator);
        String_Builder fragment_source = shader_source_prepend(source, STRING("#define FRAG"), &arena.allocator);
        String_Builder geometry_source = builder_make(&arena.allocator, 0);

        if(string_find_first(source, STRING("#ifdef GEOM"), 0) != -1)
            geometry_source = shader_source_prepend(source, STRING("#define GEOM"), &arena.allocator);

        state = state && shader_init(shader,
            vertex_source.data,
            fragment_source.data,
            geometry_source.data,
            name);

        log_capture_end(&log_list);
        if(state == false)
        {
            LOG_ERROR_CHILD(SHADER_UTIL_CHANEL, "compile error", NULL, "shader_init_from_disk() failed: ");
                LOG_INFO(">" SHADER_UTIL_CHANEL, "path: '%s'", cstring_ephemeral(path));
                LOG_ERROR_CHILD(">" SHADER_UTIL_CHANEL, "", log_list.first, "errors:");
        }
        
    }
    arena_frame_release(&arena);
    PROFILE_END();
    return state;
}

static GLuint current_used_shader = 0;
void shader_use(const Shader* shader)
{
    ASSERT(shader->shader != 0);
    if(shader->shader != current_used_shader)
    {
        glUseProgram(shader->shader);
        current_used_shader = shader->shader;
    }
}

void shader_unuse(const Shader* shader)
{
    ASSERT(shader->shader != 0);
    if(current_used_shader != 0)
    {
        glUseProgram(0);
        current_used_shader = 0;
    }
}

void compute_shader_dispatch(Shader* compute_shader, isize size_x, isize size_y, isize size_z)
{
    GLuint num_groups_x = (GLuint) MAX(DIV_CEIL(size_x, compute_shader->work_group_size_x), 1);
    GLuint num_groups_y = (GLuint) MAX(DIV_CEIL(size_y, compute_shader->work_group_size_y), 1);
    GLuint num_groups_z = (GLuint) MAX(DIV_CEIL(size_z, compute_shader->work_group_size_z), 1);

    shader_use(compute_shader);
	glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}

GLint shader_get_uniform_location(Shader* shader, const char* uniform)
{
    PROFILE_START();
    GLint location = 0;
    String uniform_str = string_of(uniform);
    u64 hash = xxhash64(uniform_str.data, uniform_str.size, 0);
    isize found = hash_index_find(shader->uniform_hash, hash);

    if(found == -1)
    {
        shader_use(shader);
        location = glGetUniformLocation(shader->shader, uniform);
        if(location == -1)
            LOG_ERROR("RENDER", "failed to find uniform %-25s shader: %s", uniform, shader->name.data);

        array_push(&shader->uniforms, builder_from_cstring(shader->allocator, uniform));
        found = hash_index_insert(&shader->uniform_hash, hash, (u64) location);
    }
    else
    {
        location = (GLint) shader->uniform_hash.entries[found].value;
    }

    #ifdef DO_ASSERTS
    f64 random = random_f64();
    if(random <= shader->check_probabiity)
    {
        //LOG_DEBUG("RENDER", "Checking uniform %-25s for collisions shader: %s", uniform, shader->name.data);
        for(isize i = 0; i < shader->uniforms.size; i++)
        {
            String_Builder* curr_uniform = &shader->uniforms.data[i];
            u64 curr_hash = xxhash64(curr_uniform->data, curr_uniform->size, 0);
            if(curr_hash == hash && string_is_equal(curr_uniform->string, uniform_str) == false)
                LOG_DEBUG("RENDER", "uniform %s hash coliding with uniform %s in shader %s", uniform, curr_uniform->data, shader->name.data);
        }
    }
    #endif
    
    PROFILE_END();

    return location;
}


bool shader_set_i32(Shader* shader, const char* name, i32 val)
{
    shader_use(shader);
    GLint location = shader_get_uniform_location(shader, name);
    if(location == -1) 
        return false;

    glUniform1i(location, (GLint) val);
    return true;
}
    
bool shader_set_f32(Shader* shader, const char* name, f32 val)
{
    shader_use(shader);
    GLint location = shader_get_uniform_location(shader, name);
    if(location == -1)
        return false;

    glUniform1f(location, (GLfloat) val);
    return true;
}

bool shader_set_vec3(Shader* shader, const char* name, Vec3 val)
{
    shader_use(shader);
    GLint location = shader_get_uniform_location(shader, name);
    if(location == -1)
        return false;

    glUniform3fv(location, 1, AS_FLOATS(val));
    return true;
}

bool shader_set_mat4(Shader* shader, const char* name, Mat4 val)
{
    shader_use(shader);
    GLint location = shader_get_uniform_location(shader, name);
    if(location == -1)
        return false;

    glUniformMatrix4fv(location, 1, GL_FALSE, AS_FLOATS(val));
    return true;
}
    
bool shader_set_mat3(Shader* shader, const char* name, Mat3 val)
{
    shader_use(shader);
    GLint location = shader_get_uniform_location(shader, name);
    if(location == -1)
        return false;

    glUniformMatrix3fv(location, 1, GL_FALSE, AS_FLOATS(val));
    return true;
}
