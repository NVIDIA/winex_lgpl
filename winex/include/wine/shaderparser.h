#ifndef SHADER_PARSER_H_INCLUDED
#define SHADER_PARSER_H_INCLUDED

#include "wine/d3dhalp.h"
#include "wine/wine_gl.h"

#include <stdio.h>

#define OUTPUT_POSITION          0
#define OUTPUT_FOG_COORD         1
#define OUTPUT_POINT_SIZE        2
#define OUTPUT_COLOR             3
#define OUTPUT_COLOR_SECONDARY   4
#define OUTPUT_TEX_COORD         5
#define OUTPUT_FRAG_COLOR        24
#define OUTPUT_FRAG_COLOR_MASK   0x3f000000 /* Allow for 8 tracking MRTs */
#define D3DSIO_NUMOPCODES        97

/* Flags to determine which instructions are implemented in our multiple shader pathways */
#define IMP_SOFTWARE      0x1
#define IMP_ARB           0x2
#define IMP_GLSL          0x4

/* Macros to check what type of shader based on stream start dword */
#define D3DSHADER_TYPE_VERTEX(x)  ((x & 0xFFFF0000) == 0xFFFE0000)
#define D3DSHADER_TYPE_PIXEL(x)   ((x & 0xFFFF0000) == 0xFFFF0000)

typedef struct {
  GLfloat data[4];
} floatVec;

typedef struct _ShaderConstantDef {
  D3DSHADER_PARAM_REGISTER_TYPE regtype;
  GLuint regnum;
  GLfloat val[4];
  struct _ShaderConstantDef *prev;
  struct _ShaderConstantDef *next;
} ShaderConstantDef;

typedef struct _ShaderSrcModifier {
  GLboolean composite;
  GLboolean negate;
  GLboolean absolute;
  GLint bitwise_shift;
  GLboolean bias;
  /*GLboolean saturate;*/
  GLboolean dzb,dwa;
  GLboolean mipmap_bias;
} ShaderSrcModifier;

typedef struct _ShaderDstModifier {
  GLint bitwise_shift;
  GLboolean saturate;
  GLboolean partial_precision;
  GLboolean centroid;
} ShaderDstModifier;

typedef struct _ShaderRegister {
  D3DSHADER_PARAM_REGISTER_TYPE type;
  GLint num;
  GLboolean address;
  GLboolean expanded;
} ShaderRegister;

typedef struct _ShaderSrcRegister {
  ShaderRegister reg;
  ShaderRegister rel; /* relative addressing */
  ShaderSrcModifier mod;
  GLuint swizzle[4];
  GLuint relcomp;
} ShaderSrcRegister;

typedef struct _ShaderDstRegister {
  ShaderRegister reg;
  ShaderDstModifier mod;
  GLboolean writemask[4];
} ShaderDstRegister;

typedef struct _ShaderComparison {
  DWORD comp;
  const char* name;
  const char* symbol;
} ShaderComparison;

typedef struct _ShaderInstPredication {
  GLboolean predicated;
  int comparison;		/* see D3DSHADER_COMPARISON */
  GLboolean compmask[4];
} ShaderInstPredication;

typedef struct _ShaderInstruction {
  DWORD opcode;			/* d3d stream opcode */
  GLboolean coissue;
  ShaderInstPredication pred;
  ShaderDstRegister dst[1];
  ShaderSrcRegister src[4];
  struct _ShaderInstruction *prev;
  struct _ShaderInstruction *next;
} ShaderInstruction;

typedef struct _ShaderDeclaration {
  ShaderDstRegister dst;
  GLuint type;
  GLuint index;
  struct _ShaderDeclaration *prev;
  struct _ShaderDeclaration *next;
} ShaderDeclaration;

typedef struct _ShaderProgram {
  DWORD shader_version;         /* raw shader version number used by d3d */
  int shader_type;		/* 1 = vs, 2 = ps*/
  int ver_major,ver_minor;	/* shader version */
  int shader_length;		/* number of shader instructions */
  ShaderDeclaration *head_d;
  ShaderDeclaration *tail_d;
  int num_constants;		/* total number of constants */
  int max_constants;		/* max number of float constants for this shader version */
  int max_temps;            /* max number of temps for this shader version */
  ShaderConstantDef *head_c;
  ShaderConstantDef *tail_c;
  int num_instructions;		/* total number of instructions */
  ShaderInstruction *head;
  ShaderInstruction *tail;
  GLuint addr_regs_used;	/* bitfield for which address registers are used */
  GLuint *temp_regs_used; 	/* bitfield for which temp registers are used (max taken from caps) */
  GLuint *const_regs_used; 	/* bitfield for which constant registers are used (max taken from caps) */
  GLuint constint_regs_used;	/* bitfield for which constant int registers are used */
  GLuint constbool_regs_used;	/* bitfield for which constant boolean registers are used */
  int max_const_regs_used;
  int max_temp_regs_used;
  int max_constint_regs_used;
  int max_constbool_regs_used;
  GLuint instructions_used[(D3DSIO_NUMOPCODES+31)/32];  /* bitfield for instructions used */
  GLuint inputs_read;		/* bitfield for which input registers are read from by the shader */
  GLuint outputs_written;	/* bitfield for which output registers are written to by the shader */
  GLubyte texcomp_written[8];	/* 4 component bitfield for each texture coordinate */
  GLubyte texcomp_read[8];	/* 4 component bitfield for each texture coordinate */
  unsigned char *component_masks; /* bitfield for the 16 DCL component masks */
  GLuint id;
  int position_invariant;
  BOOL has_loop;
  BOOL uses_rel_address_on_varyings;
  BOOL needs_user_declared_varyings;
  GLint max_builtin_texture_coords;
  GLuint expanded_temp_regs_used;/* number of expanded temporaries used */
  GLuint swzl_instructions_used; /* number of internal SWZL instructions used */
} ShaderProgram;

typedef struct _ShaderOpcodeStruct {
    BOOL valid;
    const char*name;
    int dst_args,src_args;
    DWORD versions_valid;
    DWORD vs_implemented;
    DWORD ps_implemented;
} ShaderOpcodeStruct;

/* functions to find out about a given op */
const ShaderOpcodeStruct* Shader_GetShaderOpcode( DWORD opcode );
int Opcode_GetDstArgs( DWORD opcode );
int Opcode_GetSrcArgs( const DWORD shader_version, DWORD opcode );
const char * Opcode_GetName( DWORD opcode );
const ShaderComparison* Shader_GetComparison( int comparison );

HRESULT ParseDirect3DShader (struct GL_shader_caps * glcaps, const DWORD * shaderStream, ShaderProgram ** pProg, UINT * pLen);

void SP_AllocShaderDefBlock( const DWORD *stream, ShaderDefBlock *b );

/* functions to manipulate ShaderPrograms */
void PrintShaderProgram( const ShaderProgram* prog );
void PrintShaderDeclaration( const ShaderProgram *prog, const ShaderDeclaration *dcl );
void PrintShaderInstruction( const ShaderProgram *prog, const ShaderInstruction *ins );
void DeallocateShaderProgram( ShaderProgram *program );
ShaderProgram* CloneShaderProgram( ShaderProgram *program );
ShaderInstruction* AllocShaderInstruction();
ShaderInstruction* DuplicateShaderInstruction(ShaderInstruction *inst);
void FreeShaderInstruction( ShaderInstruction *inst );
void AddInstructionBefore( ShaderProgram *prog, ShaderInstruction *before, ShaderInstruction *inst );
void AddInstructionAfter( ShaderProgram *prog, ShaderInstruction *after, ShaderInstruction *inst );
void RemoveInstruction( ShaderProgram *prog, ShaderInstruction *inst );

void SP_CalculateProgramRequirements( ShaderProgram *prog );

void add_declaration( ShaderProgram *prog, ShaderDeclaration *sd );

/* generic utility functions -- should these really be here? */

int VS_max_constants( DWORD major_version, D3DSHADER_PARAM_REGISTER_TYPE type );
int PS_max_constants( DWORD major_version, D3DSHADER_PARAM_REGISTER_TYPE type );

void fcodedump (FILE * stream, const char * source);

/* TESTCASE framework */
/* 
   Exported functions which allow a test case to access the 
   assembly text created from a shader binary 
*/
char * WINAPI TEST_CreateTextFromShaderBinary(const DWORD *data, int *length, BOOL post_gen_text);
void WINAPI TEST_FreeTextFromShaderBinary(char *text); 

#endif
