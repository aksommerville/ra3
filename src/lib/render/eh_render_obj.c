#include "eh_render_internal.h"
#include "eh_screencap.h"

/* Delete.
 */

void eh_render_del(struct eh_render *render) {
  if (!render) return;
  if (render->texid) glDeleteTextures(1,&render->texid);
  if (render->fbrgb) free(render->fbrgb);
  free(render);
}

/* Calculate RGB shifts.
 */
 
static void eh_render_calculate_shifts(struct eh_render *render) {
  switch (eh.delegate.video_format) {

    case EH_VIDEO_FORMAT_RGB16:
    case EH_VIDEO_FORMAT_RGB16SWAP: {
        render->chr_r=eh_render_chr16(eh.delegate.rmask);
        render->chr_g=eh_render_chr16(eh.delegate.gmask);
        render->chr_b=eh_render_chr16(eh.delegate.bmask);
      } break;

    case EH_VIDEO_FORMAT_RGB24: {
      } break;
      
    case EH_VIDEO_FORMAT_RGB32: {
        uint32_t q;
        if (q=eh.delegate.rmask) for (;!(q&1);q>>=1,render->rshift++) ;
        if (q=eh.delegate.gmask) for (;!(q&1);q>>=1,render->gshift++) ;
        if (q=eh.delegate.bmask) for (;!(q&1);q>>=1,render->bshift++) ;
      } break;
  }
}

/* Initialize framebuffer conversion.
 */
 
static int eh_render_init_conversion(struct eh_render *render) {
  
  /* When the delegate's framebuffer dimensions are (0,0), client is rendering on its own.
   * Really nothing for us to do in this case.
   */
  if (!eh.delegate.video_width) return 0;
  
  // Allocate the texture.
  glGenTextures(1,&render->texid);
  if (!render->texid) {
    glGenTextures(1,&render->texid);
    if (!render->texid) return -1;
  }
  glBindTexture(GL_TEXTURE_2D,render->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  
  // If it's an indexed format, initialize the color table to grayscale. Just in case they forget to.
  uint8_t *dst=render->ctab;
  switch (eh.delegate.video_format) {
    case EH_VIDEO_FORMAT_I1: {
        dst[0]=dst[1]=dst[2]=0x00;
        dst[3]=dst[4]=dst[5]=0xff;
      } break;
    case EH_VIDEO_FORMAT_I2: {
        dst[0]=dst[1]=dst[2]=0x00;
        dst[3]=dst[4]=dst[5]=0x55;
        dst[6]=dst[7]=dst[8]=0xaa;
        dst[9]=dst[10]=dst[11]=0xff;
      } break;
    case EH_VIDEO_FORMAT_I4: {
        int i=0; for (;i<16;i++,dst+=3) {
          dst[0]=dst[1]=dst[2]=i|(i<<4);
        }
      } break;
    case EH_VIDEO_FORMAT_I8: {
        int i=0; for (;i<256;i++,dst+=3) {
          dst[0]=dst[1]=dst[2]=i;
        }
      } break;
  }
  
  uint32_t botest=0x04030201;
  int bigendian=(*(uint8_t*)&botest==0x04);
  
  // Canonical RGB: Use as-is.
  if (
    (eh.delegate.video_format==EH_VIDEO_FORMAT_RGB24)&&
    (eh.delegate.rmask==0xff0000)&&
    (eh.delegate.gmask==0x00ff00)&&
    (eh.delegate.bmask==0x0000ff)
  ) {
    render->fb_gl_format=GL_RGB;
    render->fb_gl_type=GL_UNSIGNED_BYTE;
    return 0;
  }
    
  // Canonical RGBA: Use as-is. But GL will drop the alpha channel for us.
  if (
    (eh.delegate.video_format==EH_VIDEO_FORMAT_RGB32)&&
    ((bigendian&&(
      (eh.delegate.rmask==0xff000000)&&
      (eh.delegate.gmask==0x00ff0000)&&
      (eh.delegate.bmask==0x0000ff00)
    ))||(!bigendian&&(
      (eh.delegate.rmask==0x000000ff)&&
      (eh.delegate.gmask==0x0000ff00)&&
      (eh.delegate.bmask==0x00ff0000)
    )))
  ) {
    render->fb_gl_format=GL_RGBA;
    render->fb_gl_type=GL_UNSIGNED_BYTE;
    return 0;
  }
    
  //TODO GL does support byte-swapping 32-bit RGBA. Look into that.
  //TODO Also I think there are 16-bit RGB formats in GL?

  // Generic formats: Convert in software.
  render->fb_gl_format=GL_RGB;
  render->fb_gl_type=GL_UNSIGNED_BYTE;
  if (!(render->fbrgb=malloc(eh.delegate.video_width*3*eh.delegate.video_height))) return -1;
  switch (eh.delegate.video_format) {
    case EH_VIDEO_FORMAT_I1: render->fbcvt=eh_fbcvt_i1; break;
    case EH_VIDEO_FORMAT_I2: render->fbcvt=eh_fbcvt_i2; break;
    case EH_VIDEO_FORMAT_I4: render->fbcvt=eh_fbcvt_i4; break;
    case EH_VIDEO_FORMAT_I8: render->fbcvt=eh_fbcvt_i8; break;
    case EH_VIDEO_FORMAT_RGB16: render->fbcvt=eh_fbcvt_rgb16; eh_render_calculate_shifts(render); break;
    case EH_VIDEO_FORMAT_RGB24: render->fbcvt=eh_fbcvt_rgb24; eh_render_calculate_shifts(render); break;
    case EH_VIDEO_FORMAT_RGB32: render->fbcvt=eh_fbcvt_rgb32; eh_render_calculate_shifts(render); break;
    case EH_VIDEO_FORMAT_RGB16SWAP: render->fbcvt=eh_fbcvt_rgb16swap; eh_render_calculate_shifts(render); break;
  }
  if (!render->fbcvt) return -1;
  
  return 0;
}

/* GLSL.
 */
 
static const char eh_vshader[]=
  "attribute vec2 aposition;\n"
  "attribute vec2 atexcoord;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
  "  gl_Position=vec4(aposition,0.0,1.0);\n"
  "  vtexcoord=atexcoord;\n"
  "}\n"
"";

static const char eh_fshader[]=
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
  "  gl_FragColor=texture2D(sampler,vtexcoord);\n"
  "}\n"
"";

/* Compile shader.
 */
 
static int eh_render_compile(struct eh_render *render,const char *src,int srcc,int type) {

  char version[256];
  int versionc;
  versionc=snprintf(version,sizeof(version),"#version %d\n",eh.glsl_version);
  if ((versionc<1)||(versionc>=sizeof(version))) return -1;
  const char *srcv[2]={version,src};
  GLint lenv[2]={versionc,srcc};

  GLuint id=glCreateShader(type);
  if (!id) return -1;
  glShaderSource(id,2,srcv,lenv);
  glCompileShader(id);

  GLint status=0;
  glGetShaderiv(id,GL_COMPILE_STATUS,&status);
  if (status) {
    glAttachShader(render->programid,id);
    glDeleteShader(id);
    return 0;
  }

  int err=-1;
  GLint loga=0;
  glGetShaderiv(id,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetShaderInfoLog(id,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        fprintf(stderr,"Error compiling %s shader:\n%.*s\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment",logc,log);
        err=-2;
      }
      free(log);
    }
  }
  glDeleteShader(id);
  return err;
}

/* Link shader.
 */
 
static int eh_render_link(struct eh_render *render) {

  glLinkProgram(render->programid);
  GLint status=0;
  glGetProgramiv(render->programid,GL_LINK_STATUS,&status);
  if (status) return 0;

  /* Link failed. */
  int err=-1;
  GLint loga=0;
  glGetProgramiv(render->programid,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetProgramInfoLog(render->programid,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        fprintf(stderr,"Error linking shader:\n%.*s\n",logc,log);
        err=-2;
      }
      free(log);
    }
  }
  return err;
}

/* Initialize shader.
 */
 
static int eh_render_init_shader(struct eh_render *render) {

  if (!(render->programid=glCreateProgram())) return -1;
  if (eh_render_compile(render,eh_vshader,sizeof(eh_vshader),GL_VERTEX_SHADER)<0) return -1;
  if (eh_render_compile(render,eh_fshader,sizeof(eh_fshader),GL_FRAGMENT_SHADER)<0) return -1;
  
  glBindAttribLocation(render->programid,0,"aposition");
  glBindAttribLocation(render->programid,1,"atexcoord");
  
  if (eh_render_link(render)<0) return -1;

  return 0;
}

/* New.
 */
 
struct eh_render *eh_render_new() {
  struct eh_render *render=calloc(1,sizeof(struct eh_render));
  if (!render) return 0;
  
  render->dstr_dirty=1;
  
  if (eh_render_init_conversion(render)<0) {
    eh_render_del(render);
    return 0;
  }
  
  if (eh_render_init_shader(render)<0) {
    eh_render_del(render);
    return 0;
  }
  
  return render;
}

/* Trivial accessors.
 */
 
void eh_render_bounds_dirty(struct eh_render *render) {
  render->dstr_dirty=1;
}

void eh_render_before(struct eh_render *render) {
  render->srcfb=0;
}

void eh_render_after(struct eh_render *render) {
  if (render->gx_in_progress) {
    render->gx_in_progress=0;
    eh.video->type->end(eh.video);
  }
  if (render->srcfb) {
    eh.video->type->begin(eh.video);
    eh_render_commit(render);
    eh.video->type->end(eh.video);
    render->srcfb=0;
  }
}

/* Public API.
 */
 
void eh_video_write(const void *fb) {
  eh.render->srcfb=fb;
  if (eh.screencap_requested) {
    eh.screencap_requested=0;
    eh_screencap_send_from_fb(fb);
  }
}

void eh_video_begin() {
  if (eh.render->gx_in_progress) return;
  eh.render->gx_in_progress=1;
  eh.video->type->begin(eh.video);
}

void eh_video_end() {
  if (!eh.render->gx_in_progress) return;
  eh.render->gx_in_progress=0;
  if (eh.screencap_requested) {
    eh.screencap_requested=0;
    eh_screencap_send_from_opengl();
  }
  eh.video->type->end(eh.video);
}

void eh_video_get_size(int *w,int *h) {
  *w=eh.video->w;
  *h=eh.video->h;
}

void eh_ctab_write(int p,int c,const void *rgb) {
  if (!rgb) return;
  if (c<1) return;
  if (p<0) { if ((c+=p)<1) return; p=0; }
  if (p>256-c) { if ((c=256-p)<1) return; }
  memcpy(eh.render->ctab+p*3,rgb,c*3);
}

void eh_ctab_read(void *rgb,int p,int c) {
  if (!rgb) return;
  if (c<1) return;
  if (p<0) { if ((c+=p)<1) return; p=0; }
  if (p>256-c) { if ((c=256-p)<1) return; }
  memcpy(rgb,eh.render->ctab+p*3,c*3);
}
