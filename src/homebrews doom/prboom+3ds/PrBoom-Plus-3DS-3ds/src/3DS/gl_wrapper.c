#include "gl_wrapper.h"
#include "gl_swizzle.h"

#include "../z_zone.h" // For malloc/free

#include <citro3d.h>
#include "vshader_shbin.h"

#define ANG_PI 3.14159265358979323846
#define DEG2RAD(x) (x * ANG_PI / 180.0)

#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef enum _gl_c3d_dirty_flags {
    DIRTY_FLAGS_CULL        = 0x0001,
    DIRTY_FLAGS_BLEND       = 0x0002,
    DIRTY_FLAGS_DEPTH       = 0x0004,
    DIRTY_FLAGS_ALPHA       = 0x0008,
    DIRTY_FLAGS_VIEWPORT    = 0x0010,
    DIRTY_FLAGS_SCISSOR     = 0x0020,
    DIRTY_FLAGS_TEV         = 0x0040,
    DIRTY_FLAGS_FOG         = 0x0080,
} gl_c3d_dirty_flags;

typedef struct _gl_c3d_tex {
    C3D_Tex c3d_tex;

    struct _gl_c3d_tex *prev;
    struct _gl_c3d_tex *next;
} gl_c3d_tex;

static int dirty_flags;

static float clear_color_r;
static float clear_color_g;
static float clear_color_b;
static float clear_color_a;

static float clear_depth;

static int cull_enable;
static GLenum cull_mode;

static int blend_enable;
static GLenum blend_sfactor;
static GLenum blend_dfactor;

static int depth_enable;
static GLenum depth_func;
static GLboolean depth_mask;

static int alpha_enable;
static GLenum alpha_func;
static float alpha_ref;

static int scissor_enable;
static int scissor_x;
static int scissor_y;
static int scissor_width;
static int scissor_height;

static int viewport_x;
static int viewport_y;
static int viewport_width;
static int viewport_height;

static GLenum tev_combine_func_rgb;
static GLenum tev_source0_rgb;
static GLenum tev_source1_rgb;

static GLenum tev_combine_func_alpha;
static GLenum tev_source0_alpha;
static GLenum tev_source1_alpha;

static int fog_enable;
static GLfloat fog_color[4];
static GLfloat fog_density;

static GLfloat cur_color[4];
static GLfloat cur_texcoord[4];

// Linked list of active GL texture objects
static gl_c3d_tex *gl_c3d_tex_base = NULL;
static gl_c3d_tex *gl_c3d_tex_head = NULL;

// Currently bound GL texture
static gl_c3d_tex *cur_texture = NULL;

int gl_is_inited = 0;

static C3D_RenderTarget *hw_screen_l = NULL;
static C3D_RenderTarget *hw_screen_r = NULL;
float hw_stereo_offset;

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uloc_mv_mtx, uloc_p_mtx, uloc_t_mtx;

static C3D_FogLut fog_Lut;

static C3D_MtxStack mtx_modelview, mtx_projection, mtx_texture;
static C3D_MtxStack *cur_mtxstack;

static inline void _set_default_render_states() {
    clear_color_r = 0.0f;
    clear_color_g = 0.0f;
    clear_color_b = 0.0f;
    clear_color_a = 0.0f;

    clear_depth = -1.0f;

    cull_enable = 0;
    cull_mode = GL_BACK;

    blend_enable = 0;
    blend_sfactor = GL_ONE;
    blend_dfactor = GL_ZERO;

    depth_enable = 0;
    depth_func = GL_LESS;
    depth_mask = GL_TRUE;

    alpha_enable = 0;
    alpha_func = GL_ALWAYS;
    alpha_ref = 0.0f;

    scissor_enable = 0;
    scissor_x = 0;
    scissor_y = 0;
    scissor_width = 240;
    scissor_height = 400;

    viewport_x = 0;
    viewport_y = 0;
    viewport_width = 240;
    viewport_height = 400;

    tev_combine_func_rgb = GL_MODULATE;
    tev_source0_rgb = GL_TEXTURE;
    tev_source1_rgb = GL_PRIMARY_COLOR;

    tev_combine_func_alpha = GL_MODULATE;
    tev_source0_alpha = GL_TEXTURE;
    tev_source1_alpha = GL_PRIMARY_COLOR;

    fog_enable = 0;

    fog_color[0] = 0.0f;
    fog_color[1] = 0.0f;
    fog_color[2] = 0.0f;
    fog_color[3] = 0.0f;

    fog_density = 1.0f;

    cur_color[0] = 1.0f;
    cur_color[1] = 1.0f;
    cur_color[2] = 1.0f;
    cur_color[3] = 1.0f;

    cur_texcoord[0] = 0.0f;
    cur_texcoord[1] = 0.0f;
    cur_texcoord[2] = 0.0f;
    cur_texcoord[3] = 1.0f;
}

void gl_wrapper_init() {
    // TODO: Keep an eye on this. So far, we're using immediate drawing,
    // which gobbles up a LOT of the command buffer. This should be big enough
    // for the vanilla game to handle, but maps like nuts.wad will shit the bed...
    C3D_Init(0x200000);

    hw_screen_l = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH16);
    hw_screen_r = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH16);
    C3D_RenderTargetSetOutput(hw_screen_l, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
    C3D_RenderTargetSetOutput(hw_screen_r, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

    // Load the vertex shader, create a shader program and bind it
    vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
    C3D_BindProgram(&program);

    // Configure attributes for use with the vertex shader
    // Attribute format and element count are ignored in immediate mode
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 4); // v0=color
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 4); // v1=texcoord0
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 4); // v2=position

    // Init matrix stacks
    MtxStack_Init(&mtx_modelview);
    uloc_mv_mtx = shaderInstanceGetUniformLocation(program.vertexShader, "mv_mtx");
    MtxStack_Bind(&mtx_modelview, GPU_VERTEX_SHADER, uloc_mv_mtx, 4);
    Mtx_Identity(MtxStack_Cur(&mtx_modelview));
    MtxStack_Update(&mtx_modelview);

    MtxStack_Init(&mtx_projection);
    uloc_p_mtx = shaderInstanceGetUniformLocation(program.vertexShader, "p_mtx");
    MtxStack_Bind(&mtx_projection, GPU_VERTEX_SHADER, uloc_p_mtx, 4);
    Mtx_Identity(MtxStack_Cur(&mtx_projection));
    MtxStack_Update(&mtx_projection);

    MtxStack_Init(&mtx_texture);
    uloc_t_mtx = shaderInstanceGetUniformLocation(program.vertexShader, "t_mtx");
    MtxStack_Bind(&mtx_texture, GPU_VERTEX_SHADER, uloc_t_mtx, 4);
    Mtx_Identity(MtxStack_Cur(&mtx_texture));
    MtxStack_Update(&mtx_texture);

    // Default matrix mode
    cur_mtxstack = &mtx_modelview;

    // Set default render state parameters
    _set_default_render_states();
    dirty_flags = 0xffffffff;

    // Start first frame
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    gl_is_inited = 1;
}

static inline void _release_all_gl_textures() {
    gl_c3d_tex *gl_delete_tex = gl_c3d_tex_base;

    while(gl_delete_tex) {
        if(gl_delete_tex->c3d_tex.data)
            linearFree(gl_delete_tex->c3d_tex.data);

        gl_delete_tex = gl_delete_tex->next;
    }

    gl_c3d_tex_base = NULL;
    gl_c3d_tex_head = NULL;

    cur_texture = NULL;
}

void gl_wrapper_cleanup() {
    // Release allocated textures
    _release_all_gl_textures();

    // Release shader objects
    shaderProgramFree(&program);
    DVLB_Free(vshader_dvlb);

    // Release framebuffer objects
    C3D_RenderTargetDelete(hw_screen_l);
    C3D_RenderTargetDelete(hw_screen_r);
    hw_screen_l = NULL;
    hw_screen_r = NULL;

    C3D_Fini();

    gl_is_inited = 0;
}

int gl_wrapper_is_initialized() {
    return gl_is_inited;
}

void gl_wrapper_perspective(float fovy, float aspect, float znear) {
    Mtx_PerspStereoTilt(MtxStack_Cur(cur_mtxstack), fovy, aspect, 1000.0f, znear, hw_stereo_offset, 0.15f, false);
}

void gl_wrapper_select_screen(gfx3dSide_t side) {
    C3D_RenderTarget *rt = (side == GFX_LEFT) ? hw_screen_l : hw_screen_r;
    C3D_FrameDrawOn(rt);
}

void gl_wrapper_swap_buffers() {
    // End frame
    C3D_FrameEnd(0);

    // Start next frame
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
}


//========== GRAPHICS FUNCTIONS ==========

static inline void _toggle_render_state(GLenum cap, int enable) {
    switch(cap) {
    case GL_CULL_FACE:
        cull_enable = enable;
        dirty_flags |= DIRTY_FLAGS_CULL;
        break;
    case GL_BLEND:
        blend_enable = enable;
        dirty_flags |= DIRTY_FLAGS_BLEND;
        break;
    case GL_DEPTH_TEST:
        depth_enable = enable;
        dirty_flags |= DIRTY_FLAGS_DEPTH;
        break;
    case GL_ALPHA_TEST:
        alpha_enable = enable;
        dirty_flags |= DIRTY_FLAGS_ALPHA;
        break;
    case GL_SCISSOR_TEST:
        scissor_enable = enable;
        dirty_flags |= DIRTY_FLAGS_SCISSOR;
        break;
    case GL_FOG:
        fog_enable = enable;
        dirty_flags |= DIRTY_FLAGS_FOG;
        break;
    }
}

void glEnable(GLenum cap) {
    _toggle_render_state(cap, 1);
}

void glDisable(GLenum cap) {
    _toggle_render_state(cap, 0);
}

static inline GPU_CULLMODE _gl_to_c3d_cull(GLenum mode) {
    GPU_CULLMODE ret;

    switch(mode) {
    case GL_FRONT:
        ret = GPU_CULL_FRONT_CCW;
        break;
    default: // GL_BACK
        ret = GPU_CULL_BACK_CCW;
        break;
    }

    return ret;
}

void glCullFace(GLenum mode) {
    cull_mode = mode;
    dirty_flags |= DIRTY_FLAGS_CULL;
}

static inline GPU_BLENDFACTOR _gl_to_c3d_blend(GLenum gl_factor) {
    GPU_BLENDFACTOR ret;

    switch(gl_factor) {
    case GL_ZERO: ret = GPU_ZERO; break;
    case GL_ONE: ret = GPU_ONE; break;
    case GL_SRC_COLOR: ret = GPU_SRC_COLOR; break;
    case GL_ONE_MINUS_SRC_COLOR: ret = GPU_ONE_MINUS_SRC_COLOR; break;
    case GL_SRC_ALPHA: ret = GPU_SRC_ALPHA; break;
    case GL_ONE_MINUS_SRC_ALPHA: ret = GPU_ONE_MINUS_SRC_ALPHA; break;
    case GL_DST_ALPHA: ret = GPU_DST_ALPHA; break;
    case GL_ONE_MINUS_DST_ALPHA: ret = GPU_ONE_MINUS_DST_ALPHA; break;
    case GL_DST_COLOR: ret = GPU_DST_COLOR; break;
    case GL_ONE_MINUS_DST_COLOR: ret = GPU_ONE_MINUS_DST_COLOR; break;
    case GL_SRC_ALPHA_SATURATE: ret = GPU_SRC_ALPHA_SATURATE; break;
    }

    return ret;
}

void glBlendFunc(GLenum sfactor, GLenum dfactor) {
    blend_sfactor = sfactor;
    blend_dfactor = dfactor;
    dirty_flags |= DIRTY_FLAGS_BLEND;
}

static inline GPU_TESTFUNC _gl_to_c3d_testfunc(GLenum gl_testfunc) {
    GPU_TESTFUNC ret = GPU_LESS;

    switch(gl_testfunc) {
    case GL_NEVER: ret = GPU_NEVER; break;
    case GL_LESS: ret = GPU_LESS; break;
    case GL_EQUAL: ret = GPU_EQUAL; break;
    case GL_LEQUAL: ret = GPU_LEQUAL; break;
    case GL_GREATER: ret = GPU_GREATER; break;
    case GL_NOTEQUAL: ret = GPU_NOTEQUAL; break;
    case GL_GEQUAL: ret = GPU_GEQUAL; break;
    case GL_ALWAYS: ret = GPU_ALWAYS; break;
    }

    return ret;
}

void glDepthFunc(GLenum func) {
    depth_func = func;
    dirty_flags |= DIRTY_FLAGS_DEPTH;
}

void glDepthMask(GLboolean flag) {
    depth_mask = flag;
    dirty_flags |= DIRTY_FLAGS_DEPTH;
}

void glAlphaFunc(GLenum func, GLclampf ref) {
    alpha_func = func;
    alpha_ref = ref;
    dirty_flags |= DIRTY_FLAGS_ALPHA;
}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    clear_color_r = red;
    clear_color_g = green;
    clear_color_b = blue;
    clear_color_a = alpha;
}

void glClearDepth(GLclampd depth) {
    clear_depth = -depth;
}

void glClear(GLbitfield mask) {
    GPU_WRITEMASK write_mask = 0;

    if(mask & GL_COLOR_BUFFER_BIT)
        write_mask |= GPU_WRITE_COLOR;
    if(mask & GL_DEPTH_BUFFER_BIT)
        write_mask |= GPU_WRITE_DEPTH;

    C3D_CullFace(GPU_CULL_FRONT_CCW);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_ALWAYS, write_mask);

    C3D_SetViewport(0, 0, 240, 400);
    C3D_SetScissor(scissor_enable ? GPU_SCISSOR_NORMAL : GPU_SCISSOR_DISABLE, scissor_x, scissor_y, scissor_x + scissor_width, scissor_y + scissor_height);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);

    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
    C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, 0);
    C3D_TexEnvOpAlpha(env, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, 0);

    C3D_FogGasMode(GPU_NO_FOG, GPU_DEPTH_DENSITY, true);

    C3D_Mtx ident_mtx;
    Mtx_Identity(&ident_mtx);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uloc_mv_mtx, &ident_mtx);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uloc_p_mtx,  &ident_mtx);

    mtx_modelview.isDirty = true;
    mtx_projection.isDirty = true;

    C3D_ImmDrawBegin(GPU_TRIANGLE_FAN);

    C3D_ImmSendAttrib(clear_color_r, clear_color_g, clear_color_b, clear_color_a);
    C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 1.0f);
    C3D_ImmSendAttrib(-1.0f, -1.0f, clear_depth, 1.0f);

    C3D_ImmSendAttrib(clear_color_r, clear_color_g, clear_color_b, clear_color_a);
    C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 1.0f);
    C3D_ImmSendAttrib(-1.0f, 1.0f, clear_depth, 1.0f);

    C3D_ImmSendAttrib(clear_color_r, clear_color_g, clear_color_b, clear_color_a);
    C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 1.0f);
    C3D_ImmSendAttrib(1.0f, 1.0f, clear_depth, 1.0f);

    C3D_ImmSendAttrib(clear_color_r, clear_color_g, clear_color_b, clear_color_a);
    C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 1.0f);
    C3D_ImmSendAttrib(1.0f, -1.0f, clear_depth, 1.0f);

    C3D_ImmDrawEnd();

    dirty_flags = 0xffffffff;
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    viewport_x = y;
    viewport_y = x;
    viewport_width = height;
    viewport_height = width;
    dirty_flags |= (DIRTY_FLAGS_VIEWPORT | DIRTY_FLAGS_SCISSOR);
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    scissor_x = y;
    scissor_y = x;
    scissor_width = height;
    scissor_height = width;
    dirty_flags |= DIRTY_FLAGS_SCISSOR;
}

static inline GPU_COMBINEFUNC _gl_to_c3d_tev_combine_func(GLenum gl_combine_func) {
    GPU_COMBINEFUNC ret;

    switch(gl_combine_func) {
    case GL_DOT3_RGB:
        ret = GPU_DOT3_RGB;
        break;
    case GL_MODULATE:
        ret = GPU_MODULATE;
        break;
    default: // GL_REPLACE
        ret = GPU_REPLACE;
        break;
    }

    return ret;
}

static inline GPU_TEVSRC _gl_to_c3d_tev_src(GLenum gl_tev_src) {
    GPU_TEVSRC ret;

    switch(gl_tev_src) {
    case GL_TEXTURE:
    case GL_TEXTURE0:
        ret = GPU_TEXTURE0;
        break;
    default: // GL_PRIMARY_COLOR:
        ret = GPU_PRIMARY_COLOR;
        break;
    }

    return ret;
}

static inline void _update_dirty_render_states() {
    if(dirty_flags & DIRTY_FLAGS_CULL)
    {
        C3D_CullFace(cull_enable ? _gl_to_c3d_cull(cull_mode) : GPU_CULL_NONE);
    }
    if(dirty_flags & DIRTY_FLAGS_BLEND)
    {
        if(blend_enable)
        {
            GPU_BLENDFACTOR c3d_sfactor = _gl_to_c3d_blend(blend_sfactor);
            GPU_BLENDFACTOR c3d_dfactor = _gl_to_c3d_blend(blend_dfactor);
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, c3d_sfactor, c3d_dfactor, c3d_sfactor, c3d_dfactor);
        }
        else
        {
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
        }
    }
    if(dirty_flags & DIRTY_FLAGS_DEPTH)
    {
        C3D_DepthTest(depth_enable, _gl_to_c3d_testfunc(depth_func), depth_mask ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
    }
    if(dirty_flags & DIRTY_FLAGS_ALPHA)
    {
        C3D_AlphaTest(alpha_enable, _gl_to_c3d_testfunc(alpha_func), (int)(alpha_ref * 255.0f));
    }
    if(dirty_flags & DIRTY_FLAGS_VIEWPORT)
    {
        C3D_SetViewport(viewport_x, viewport_y, viewport_width, viewport_height);
    }
    if(dirty_flags & DIRTY_FLAGS_SCISSOR)
    {
        C3D_SetScissor(scissor_enable ? GPU_SCISSOR_NORMAL : GPU_SCISSOR_DISABLE, scissor_x, scissor_y, scissor_x + scissor_width, scissor_y + scissor_height);
    }
    if(dirty_flags & DIRTY_FLAGS_TEV)
    {
        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);

        C3D_TexEnvFunc(env, C3D_RGB, _gl_to_c3d_tev_combine_func(tev_combine_func_rgb));
        C3D_TexEnvSrc(env, C3D_RGB, _gl_to_c3d_tev_src(tev_source0_rgb), _gl_to_c3d_tev_src(tev_source1_rgb), 0);
        C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, 0);

        C3D_TexEnvFunc(env, C3D_Alpha, _gl_to_c3d_tev_combine_func(tev_combine_func_alpha));
        C3D_TexEnvSrc(env, C3D_Alpha, _gl_to_c3d_tev_src(tev_source0_alpha), _gl_to_c3d_tev_src(tev_source1_alpha), 0);
        C3D_TexEnvOpAlpha(env, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, 0);
    }
    if(dirty_flags & DIRTY_FLAGS_FOG)
    {
        FogLut_Exp(&fog_Lut, fog_density * 12.0f, 1.0f, 0.01f, 1.0f);
        C3D_FogGasMode(fog_enable ? GPU_FOG : GPU_NO_FOG, GPU_DEPTH_DENSITY, true);

        C3D_FogColor( ((int)(fog_color[0] * 255) << 16) |
                      ((int)(fog_color[1] * 255) << 8) |
                      ((int)(fog_color[2] * 255)) );

        C3D_FogLutBind(&fog_Lut);
    }

    // Reset all dirty flags
    dirty_flags = 0;
}

void glBegin(GLenum mode) {
    MtxStack_Update(&mtx_modelview);
    MtxStack_Update(&mtx_projection);
    MtxStack_Update(&mtx_texture);

    _update_dirty_render_states();

    if(cur_texture)
        C3D_TexBind(0, &cur_texture->c3d_tex);
    else
        C3D_TexBind(0, NULL);

    switch(mode) {
    case GL_TRIANGLE_STRIP:
        C3D_ImmDrawBegin(GPU_TRIANGLE_STRIP);
        break;
    case GL_TRIANGLE_FAN:
        C3D_ImmDrawBegin(GPU_TRIANGLE_FAN);
        break;
    default: // GL_TRIANGLES
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        break;
    }
}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    cur_color[0] = red;
    cur_color[1] = green;
    cur_color[2] = blue;
    cur_color[3] = alpha;
}

void glColor4ubv(const GLubyte *v) {
    cur_color[0] = v[0] / 255.0f;
    cur_color[1] = v[1] / 255.0f;
    cur_color[2] = v[2] / 255.0f;
    cur_color[3] = v[3] / 255.0f;
}

void glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    cur_color[0] = red;
    cur_color[1] = green;
    cur_color[2] = blue;
    cur_color[3] = 1.0f;
}

void glTexCoord2f(GLfloat s, GLfloat t) {
    cur_texcoord[0] = s;
    cur_texcoord[1] = t;
    cur_texcoord[2] = 0.0f;
    cur_texcoord[3] = 1.0f;
}

void glTexCoord2fv(const GLfloat *v) {
    cur_texcoord[0] = v[0];
    cur_texcoord[1] = v[1];
    cur_texcoord[2] = 0.0f;
    cur_texcoord[3] = 1.0f;
}

void glVertex2i(GLint x, GLint y) {
    C3D_ImmSendAttrib(cur_color[0], cur_color[1], cur_color[2], cur_color[3]);
    C3D_ImmSendAttrib(cur_texcoord[0], cur_texcoord[1], cur_texcoord[2], cur_texcoord[3]);
    C3D_ImmSendAttrib((GLfloat)x, (GLfloat)y, 0.0f, 1.0f);
}

void glVertex2f(GLfloat x, GLfloat y) {
    C3D_ImmSendAttrib(cur_color[0], cur_color[1], cur_color[2], cur_color[3]);
    C3D_ImmSendAttrib(cur_texcoord[0], cur_texcoord[1], cur_texcoord[2], cur_texcoord[3]);
    C3D_ImmSendAttrib(x, y, 0.0f, 1.0f);
}

void glVertex3f(GLfloat x,GLfloat y,GLfloat z) {
    C3D_ImmSendAttrib(cur_color[0], cur_color[1], cur_color[2], cur_color[3]);
    C3D_ImmSendAttrib(cur_texcoord[0], cur_texcoord[1], cur_texcoord[2], cur_texcoord[3]);
    C3D_ImmSendAttrib(x, y, z, 1.0f);
}

void glVertex3fv(const GLfloat *v) {
    C3D_ImmSendAttrib(cur_color[0], cur_color[1], cur_color[2], cur_color[3]);
    C3D_ImmSendAttrib(cur_texcoord[0], cur_texcoord[1], cur_texcoord[2], cur_texcoord[3]);
    C3D_ImmSendAttrib(v[0], v[1], v[2], 1.0f);
}

void glEnd(void) {
    C3D_ImmDrawEnd();
}

static inline GLuint _gen_texture() {
    if(!gl_c3d_tex_head) // Linked list has no entries?
    {
        gl_c3d_tex_head = malloc(sizeof(gl_c3d_tex));

        gl_c3d_tex_head->prev = NULL;
        gl_c3d_tex_head->next = NULL;

        gl_c3d_tex_base = gl_c3d_tex_head;
    }
    else // Create new entry at the head of the list
    {
        gl_c3d_tex_head->next = malloc(sizeof(gl_c3d_tex));

        gl_c3d_tex_head->next->prev = gl_c3d_tex_head;
        gl_c3d_tex_head->next->next = NULL;

        gl_c3d_tex_head = gl_c3d_tex_head->next;
    }

    // Set default C3D texture parameters
    gl_c3d_tex_head->c3d_tex.width = 0;
    gl_c3d_tex_head->c3d_tex.height = 0;
    gl_c3d_tex_head->c3d_tex.data = NULL;
    gl_c3d_tex_head->c3d_tex.fmt = GPU_RGBA8;
    gl_c3d_tex_head->c3d_tex.size = 0;
    gl_c3d_tex_head->c3d_tex.param = GPU_TEXTURE_MODE(GPU_TEX_2D);
    gl_c3d_tex_head->c3d_tex.border = 0;
    gl_c3d_tex_head->c3d_tex.lodBias = 0;
    gl_c3d_tex_head->c3d_tex.maxLevel = 0;
    gl_c3d_tex_head->c3d_tex.minLevel = 0;

    // Force mip linear for fog
    gl_c3d_tex_head->c3d_tex.param &= ~GPU_TEXTURE_MIP_FILTER(1);
    gl_c3d_tex_head->c3d_tex.param |= GPU_TEXTURE_MIP_FILTER(GPU_LINEAR);

    // The pointer to the new texture acts as the GL texture ID
    return (GLuint)gl_c3d_tex_head;
}

void glGenTextures(GLsizei n, GLuint *textures) {
    for(int i = 0; i < n; i++)
        textures[i] = _gen_texture();
}

static inline gl_c3d_tex *_try_find_texture(GLuint texture) {
    gl_c3d_tex *cur_entry = gl_c3d_tex_base;

    while(cur_entry) {
        if((GLuint)cur_entry == texture)
            return cur_entry;

        cur_entry = cur_entry->next;
    }

    return NULL;
}

void glBindTexture(GLenum target, GLuint texture) {
    if(!texture) {
        cur_texture = NULL;
        return;
    }

    // Find the corresponding C3D texture
    gl_c3d_tex *gl_bind_tex = _try_find_texture(texture);

    if(!gl_bind_tex)
        return;

    cur_texture = gl_bind_tex;
}

static inline GPU_TEXTURE_WRAP_PARAM _gl_to_c3d_texwrap(GLenum mode) {
    GPU_TEXTURE_WRAP_PARAM ret;

    switch(mode) {
    case GL_CLAMP:
        ret = GPU_CLAMP_TO_EDGE;
        break;
    default: // GL_REPEAT
        ret = GPU_REPEAT;
        break;
    }

    return ret;
}

static inline GPU_TEXTURE_FILTER_PARAM _gl_to_c3d_texfilter(GLenum mode) {
    GPU_TEXTURE_FILTER_PARAM ret;

    switch(mode) {
    case GL_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_NEAREST_MIPMAP_NEAREST:
        ret = GPU_NEAREST;
        break;
    default: // GL_LINEAR
        ret = GPU_LINEAR;
        break;
    }

    return ret;
}

void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if(!cur_texture)
        return;

    switch(pname) {
    case GL_TEXTURE_WRAP_S:
        cur_texture->c3d_tex.param &= ~(GPU_TEXTURE_WRAP_S(3));
        cur_texture->c3d_tex.param |= GPU_TEXTURE_WRAP_S(_gl_to_c3d_texwrap(param));
        break;
    case GL_TEXTURE_WRAP_T:
        cur_texture->c3d_tex.param &= ~(GPU_TEXTURE_WRAP_T(3));
        cur_texture->c3d_tex.param |= GPU_TEXTURE_WRAP_T(_gl_to_c3d_texwrap(param));
        break;
    case GL_TEXTURE_MAG_FILTER:
        cur_texture->c3d_tex.param &= ~(GPU_TEXTURE_MAG_FILTER(1));
        cur_texture->c3d_tex.param |= GPU_TEXTURE_MAG_FILTER(_gl_to_c3d_texfilter(param));
        break;
    case GL_TEXTURE_MIN_FILTER:
        cur_texture->c3d_tex.param &= ~(GPU_TEXTURE_MIN_FILTER(1));
        cur_texture->c3d_tex.param |= GPU_TEXTURE_MIN_FILTER(_gl_to_c3d_texfilter(param));
        break;
    }
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {
    if(!cur_texture)
        return;

    if(cur_texture->c3d_tex.data) {
        linearFree(cur_texture->c3d_tex.data);
        cur_texture->c3d_tex.data = NULL;
    }

    u32 size = width * height * 4;
    gl_c3d_tex_head->c3d_tex.width = width;
    gl_c3d_tex_head->c3d_tex.height = height;
    gl_c3d_tex_head->c3d_tex.data = linearAlloc(size);
    gl_c3d_tex_head->c3d_tex.fmt = GPU_RGBA8;
    gl_c3d_tex_head->c3d_tex.size = size;

    // We can safely leave now if no pixel data (NULL) was provided
    if(!pixels)
        return;

    u32 *swizzle_buf = malloc(size);
    SwizzleTexBufferRGBA8((u32*)pixels, swizzle_buf, width, height);

    // Reverse the order of the color components
    for(int i = 0; i < width * height; i++)
    {
        swizzle_buf[i] = ((swizzle_buf[i] & 0xff000000) >> 24) |
                         ((swizzle_buf[i] & 0x00ff0000) >> 8) |
                         ((swizzle_buf[i] & 0x0000ff00) << 8) |
                         ((swizzle_buf[i] & 0x000000ff) << 24);
    }

    memcpy(gl_c3d_tex_head->c3d_tex.data, swizzle_buf, size);

    free(swizzle_buf);
}

void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    if(!cur_texture)
        return;

    u32 *screen = (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    u32 *texbuf = (u32*)gl_c3d_tex_head->c3d_tex.data;

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            texbuf[(y * cur_texture->c3d_tex.width) + x] = screen[(x * height) + y];
        }
    }

    u32 *swizzle_buf = malloc(cur_texture->c3d_tex.size);
    SwizzleTexBufferRGBA8(texbuf, swizzle_buf, cur_texture->c3d_tex.width, cur_texture->c3d_tex.height);

    memcpy(gl_c3d_tex_head->c3d_tex.data, swizzle_buf, gl_c3d_tex_head->c3d_tex.size);

    free(swizzle_buf);
}

static inline void _delete_texture(GLuint texture) {
    // Find the corresponding C3D texture
    gl_c3d_tex *gl_delete_tex = _try_find_texture(texture);

    if(!gl_delete_tex)
        return;

    if(gl_delete_tex->c3d_tex.data) {
        linearFree(gl_delete_tex->c3d_tex.data);
        gl_delete_tex->c3d_tex.data = NULL;
    }

    if(gl_delete_tex == cur_texture)
        cur_texture = NULL;

    gl_c3d_tex *prev = gl_delete_tex->prev;
    gl_c3d_tex *next = gl_delete_tex->next;

    // Delete (free) the texture
    free(gl_delete_tex);

    // The deleted texture was...
    if(!prev && next) // ...at the start of the list
    {
        next->prev = NULL;
        gl_c3d_tex_base = next;
    }
    else if(prev && next) // ...between 2 list entries
    {
        prev->next = next;
        next->prev = prev;
    }
    else if(prev && !next) // ...at the end of the list
    {
        prev->next = NULL;
        gl_c3d_tex_head = prev;
    }
    else // ...the only entry in the list
    {
        gl_c3d_tex_base = NULL;
        gl_c3d_tex_head = NULL;
    }
}

void glDeleteTextures(GLsizei n, const GLuint *textures) {
    for(int i = 0; i < n; i++)
        _delete_texture(textures[i]);
}

void glFogi(GLenum pname, GLint param) {
    // PrBoom+ always uses GL_EXP

    dirty_flags |= DIRTY_FLAGS_FOG;
}

void glFogf(GLenum pname, GLfloat param) {
    switch(pname) {
    case GL_FOG_DENSITY:
        fog_density = param;
        break;
    }

    dirty_flags |= DIRTY_FLAGS_FOG;
}

void glFogfv(GLenum pname, const GLfloat *params) {
    switch(pname) {
    case GL_FOG_COLOR:
        fog_color[0] = params[0];
        fog_color[1] = params[1];
        fog_color[2] = params[2];
        fog_color[3] = params[3];
        break;
    }

    dirty_flags |= DIRTY_FLAGS_FOG;
}

void glTexEnvi(GLenum target, GLenum pname, GLint param) {
    if(pname == GL_TEXTURE_ENV_MODE)
    {
        switch(param) {
        case GL_MODULATE:
            tev_combine_func_rgb = tev_combine_func_alpha = GL_MODULATE;
            tev_source0_rgb = tev_source0_alpha = GL_TEXTURE0;
            tev_source1_rgb = tev_source1_alpha = GL_PRIMARY_COLOR;
            break;
        }
    }
    else
    {
        switch(pname) {
        case GL_COMBINE_RGB: tev_combine_func_rgb = param; break;
        case GL_SOURCE0_RGB: tev_source0_rgb = param; break;
        case GL_SOURCE1_RGB: tev_source1_rgb = param; break;
        case GL_COMBINE_ALPHA: tev_combine_func_alpha = param; break;
        case GL_SOURCE0_ALPHA: tev_source0_alpha = param; break;
        case GL_SOURCE1_ALPHA: tev_source1_alpha = param; break;
        }
    }

    dirty_flags |= DIRTY_FLAGS_TEV;
}

void glGetIntegerv(GLenum pname, GLint *params) {
    switch(pname) {
    case GL_BLEND_DST:
        params[0] = blend_dfactor;
        break;
    case GL_BLEND_SRC:
        params[0] = blend_sfactor;
        break;
    }
}

void glGetFloatv(GLenum pname, GLfloat *params) {
    switch(pname) {
    case GL_CURRENT_COLOR:
        params[0] = cur_color[0];
        params[1] = cur_color[1];
        params[2] = cur_color[2];
        params[3] = cur_color[3];
        break;
    }
}

void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {
    if(!cur_texture)
        return;

    switch(pname) {
    case GL_TEXTURE_WIDTH:
        params[0] = cur_texture->c3d_tex.width;
        break;
    case GL_TEXTURE_HEIGHT:
        params[0] = cur_texture->c3d_tex.height;
        break;
    }
}

void glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels) {
    if(!cur_texture)
        return;

    int tex_width = cur_texture->c3d_tex.width;
    int tex_height = cur_texture->c3d_tex.height;

    // Unswizzle the texture into temporary buffer
    u32 *unswizzle_buf = malloc(cur_texture->c3d_tex.size);
    SwizzleTexBufferRGBA8((u32*)cur_texture->c3d_tex.data, unswizzle_buf, tex_width, tex_height);

    // Copy texture pixels to destination buffer
    u32 *pixel_ptr = (u32*)pixels;
    for(int i = 0; i < tex_width * tex_height; i++)
    {
        pixel_ptr[i] = ((unswizzle_buf[i] & 0xff000000) >> 24) |
                       ((unswizzle_buf[i] & 0x00ff0000) >> 8) |
                       ((unswizzle_buf[i] & 0x0000ff00) << 8) |
                       ((unswizzle_buf[i] & 0x000000ff) << 24);
    }

    free(unswizzle_buf);
}

void glFlush(void) {
    // HACK: Because C3D_FrameEnd() swaps the gfx buffers, we need to swap them
    // back straight aftwards to prevent a brief flicker of the next frame
    // (which, because we're in glFlush(), we don't want to present just yet)
    C3D_FrameEnd(0);
    gfxScreenSwapBuffers(GFX_TOP, true);

    // Now that the C3D frame has "ended", we can safely transfer the
    // framebuffer in VRAM back to the main framebuffer in main RAM
    u32 dim = GX_BUFFER_DIM(hw_screen_l->frameBuf.width, hw_screen_l->frameBuf.height);
    C3D_SyncDisplayTransfer((u32*)hw_screen_l->frameBuf.colorBuf, dim, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), dim, DISPLAY_TRANSFER_FLAGS);

    // Resume the C3D frame
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
}

void glFinish(void) {

}


//========== MATRIX FUNCTIONS ==========

void glMatrixMode(GLenum mode) {
    switch(mode) {
    case GL_MODELVIEW:
        cur_mtxstack = &mtx_modelview;
        break;
    case GL_PROJECTION:
        cur_mtxstack = &mtx_projection;
        break;
    case GL_TEXTURE:
        cur_mtxstack = &mtx_texture;
        break;
    }
}

void glLoadIdentity(void) {
    Mtx_Identity(MtxStack_Cur(cur_mtxstack));
}

void glLoadMatrixf(const GLfloat *m) {
    float *out = MtxStack_Cur(cur_mtxstack)->m;

    // (3DS GPU expects matrix row permutation to be WZYX, not XYZW!)
    out[0] = m[12]; out[1] = m[8]; out[2] = m[4]; out[3] = m[0];
    out[4] = m[13]; out[5] = m[9]; out[6] = m[5]; out[7] = m[1];
    out[8] = m[14]; out[9] = m[10]; out[10] = m[6]; out[11] = m[2];
    out[12] = m[15]; out[13] = m[11]; out[14] = m[7]; out[15] = m[3];
}

void glPushMatrix(void) {
    MtxStack_Push(cur_mtxstack);
}

void glPopMatrix(void) {
    MtxStack_Pop(cur_mtxstack);
}

void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    Mtx_OrthoTilt(MtxStack_Cur(cur_mtxstack), (float)left, (float)right, (float)bottom, (float)top, (float)zFar, (float)zNear, false);
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    Mtx_Translate(MtxStack_Cur(cur_mtxstack), x, y, z, true);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    // TODO: This is really lazy, but it works, and
    // Mtx_Rotate was giving me shit for some reason...
    if(x == 1.0f)
        Mtx_RotateX(MtxStack_Cur(cur_mtxstack), DEG2RAD(angle), true);
    else if(y == 1.0f)
        Mtx_RotateY(MtxStack_Cur(cur_mtxstack), DEG2RAD(angle), true);
    else if(z == 1.0f)
        Mtx_RotateZ(MtxStack_Cur(cur_mtxstack), DEG2RAD(angle), true);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    Mtx_Scale(MtxStack_Cur(cur_mtxstack), x, y, z);
}
