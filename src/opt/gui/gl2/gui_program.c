#include "../gui_internal.h"

/* Delete.
 */

void gui_program_del(struct gui_program *program) {
  if (!program) return;
  if (program->refc-->1) return;
  glDeleteProgram(program->programid);
  free(program);
}

/* Retain.
 */
 
int gui_program_ref(struct gui_program *program) {
  if (!program) return -1;
  if (program->refc<1) return -1;
  if (program->refc==INT_MAX) return -1;
  program->refc++;
  return 0;
}

/* Compile one shader and attach it.
 */
 
static int gui_program_compile(
  struct gui_program *program,
  const char *src,int srcc,
  int type
) {

  char version[256];
  int versionc;
  versionc=snprintf(version,sizeof(version),"#version %d\n",GUI_GLSL_VERSION);
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
    glAttachShader(program->programid,id);
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
        fprintf(stderr,
          "Error compiling '%s' %s shader:\n%.*s\n",
          program->name,(type==GL_VERTEX_SHADER)?"vertex":"fragment",logc,log
        );
        err=-2;
      }
      free(log);
    }
  }
  glDeleteShader(id);
  return err;
}

/* Link program, after both shaders are attached.
 */
 
static int gui_program_link(struct gui_program *program) {

  glLinkProgram(program->programid);
  GLint status=0;
  glGetProgramiv(program->programid,GL_LINK_STATUS,&status);
  if (status) return 0;

  /* Link failed. */
  int err=-1;
  GLint loga=0;
  glGetProgramiv(program->programid,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetProgramInfoLog(program->programid,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        fprintf(stderr,
          "Error linking '%s' shader:\n%.*s\n",
          program->name,logc,log
        );
        err=-2;
      }
      free(log);
    }
  }
  return err;
}

/* Bind attributes from program text.
 */
 
static int gui_program_bind_attributes(struct gui_program *program,const char *src,int srcc) {
  int srcp=0,attrid=0;
  while (srcp<srcc) {
    const char *line=src+srcp;
    int linec=0;
    while ((srcp<srcc)&&(src[srcp++]!=0x0a)) linec++;
    if ((linec>=10)&&!memcmp(line,"attribute ",10)) {
      #define IDCH(ch) (((ch>=0x30)&&(ch<=0x39))||((ch>=0x41)&&(ch<=0x5a))||((ch>=0x61)&&(ch<=0x7a)))
      while (!IDCH(line[linec-1])) linec--;
      const char *name=line+linec;
      int namec=0;
      while ((name>line)&&IDCH(name[-1])) { name--; namec++; }
      #undef IDCH
      if (namec) {
        char zname[32];
        if (namec>sizeof(zname)) return -1;
        memcpy(zname,name,namec);
        zname[namec]=0;
        glBindAttribLocation(program->programid,attrid,zname);
        attrid++;
      }
    }
  }
  program->attrc=attrid;
  return 0;
}

/* Init.
 */
 
static int gui_program_init(struct gui_program *program,const char *vsrc,int vsrcc,const char *fsrc,int fsrcc) {
  int err;
  if (!(program->programid=glCreateProgram())) {
    if (!(program->programid=glCreateProgram())) {
      return -1;
    }
  }
  if (!vsrc) vsrcc=0; else if (vsrcc<0) { vsrcc=0; while (vsrc[vsrcc]) vsrcc++; }
  if (!fsrc) fsrcc=0; else if (fsrcc<0) { fsrcc=0; while (fsrc[fsrcc]) fsrcc++; }
  
  if ((err=gui_program_compile(program,vsrc,vsrcc,GL_VERTEX_SHADER))<0) return err;
  if ((err=gui_program_compile(program,fsrc,fsrcc,GL_FRAGMENT_SHADER))<0) return err;
  
  if ((err=gui_program_bind_attributes(program,vsrc,vsrcc))<0) return err;
  
  if ((err=gui_program_link(program))<0) return err;
  
  program->loc_screensize=glGetUniformLocation(program->programid,"screensize");
  program->loc_texsize=glGetUniformLocation(program->programid,"texsize");
  program->loc_sampler=glGetUniformLocation(program->programid,"sampler");

  return 0;
}

/* New.
 */

struct gui_program *gui_program_new(const char *name,const char *vsrc,int vsrcc,const char *fsrc,int fsrcc) {
  struct gui_program *program=calloc(1,sizeof(struct gui_program));
  if (!program) return 0;
  program->refc=1;
  program->name=name;
  if (gui_program_init(program,vsrc,vsrcc,fsrc,fsrcc)<0) {
    gui_program_del(program);
    return 0;
  }
  return program;
}

/* Use.
 */

void gui_program_use(struct gui_program *program) {
  if (!program) {
    glUseProgram(0);
  } else {
    glUseProgram(program->programid);
  }
}

/* Uniforms.
 */
 
unsigned int gui_program_get_uniform(const struct gui_program *program,const char *name) {
  return glGetUniformLocation(program->programid,name);
}
 
void gui_program_set_screensize(struct gui_program *program,int w,int h) {
  glUniform2f(program->loc_screensize,w,h);
}

void gui_program_set_texsize(struct gui_program *program,int w,int h) {
  glUniform2f(program->loc_texsize,w,h);
}

void gui_program_set_texsize_obj(struct gui_program *program,struct gui_texture *texture) {
  glUniform2f(program->loc_texsize,texture->w,texture->h);
}

void gui_program_set_sampler(struct gui_program *program,int sampler) {
  glUniform1f(program->loc_sampler,sampler);
}
