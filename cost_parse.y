/* ScummC
 * Copyright (C) 2005-2006  Alban Bedel
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/**
 * @file cost_parse.y
 * @ingroup scumm
 * @brief Costume compiler
 */

%{

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#ifndef IS_MINGW
#include <glob.h>
#endif


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "scc_fd.h"
#include "scc_util.h"
#include "scc_param.h"
#include "scc_img.h"
#include "cost_parse.tab.h"
#include "scc_lex.h"

#define YYERROR_VERBOSE 1

  int yylex(void);
  int yyerror (const char *s);

#define COST_ABORT(at,msg...)  { \
  printf("Line %d, column %d: ",at.first_line,at.first_column); \
  printf(msg); \
  YYABORT; \
}

  typedef struct cost_pic cost_pic_t;
  struct cost_pic {
    cost_pic_t* next;
    // name from the sym
    char* name;
    // number of references from limbs
    unsigned ref;
    // offset where it will be written
    unsigned offset;
    // global index for akos
    unsigned idx;
    // path/glob
    char* path;
    int is_glob;
    // params, they will be copied for globs
    int16_t rel_x,rel_y;
    int16_t move_x,move_y;

    uint16_t width,height;
    uint8_t* data;
    uint32_t data_size;
  };

  static cost_pic_t* cur_pic = NULL;
  static cost_pic_t* pic_list = NULL;

#define COST_MAX_LIMBS 16
#define LIMB_MAX_PICTURES 0x70
#define COST_MAX_DIR 4

  typedef struct cost_limb {
    char* name;
    unsigned num_pic;
    cost_pic_t* pic[LIMB_MAX_PICTURES];
  } cost_limb_t;
  

  static cost_limb_t limbs[COST_MAX_LIMBS];
  static cost_limb_t* cur_limb = NULL;

#define COST_MAX_ANIMS 0x3F
#define COST_MAX_LIMB_CMDS 0x7F

  typedef struct cost_limb_anim {
    unsigned len;
    unsigned flags;
    uint8_t cmd[COST_MAX_LIMB_CMDS];
  } cost_limb_anim_t;

  typedef struct cost_anim_dir {
    unsigned limb_mask;
    cost_limb_anim_t limb[COST_MAX_LIMBS];
  } cost_anim_dir_t;

  typedef struct cost_anim {
    char* name;
    cost_anim_dir_t dir[COST_MAX_DIR];
  } cost_anim_t;
  
  static cost_anim_t anims[COST_MAX_ANIMS];
  static cost_anim_t* cur_anim = NULL;
  static cost_anim_dir_t* cur_dir = NULL;

#define COST_MAX_PALETTE_SIZE 32
  static unsigned pal_size = 0;
  static uint8_t pal[COST_MAX_PALETTE_SIZE];

  // Default to no flip
  static unsigned cost_flags = 0x80;

  static char* img_path = NULL;
  static char* cost_output = NULL;
  static scc_fd_t* out_fd = NULL;

  // Output a AKOS instead of COST
  static int akos = 0;

  // Ouput a header
  static char* header_name = NULL;
  static char* symbol_prefix = NULL;

  static int cost_pic_load(cost_pic_t* pic,char* file);

  static cost_pic_t* find_pic(char* name) {
    cost_pic_t* p;
    for(p = pic_list ; p ; p = p->next)
      if(!strcmp(name,p->name)) break;
    return p;
  }

  struct dir_map {
    char* name;
    unsigned id;
  } dir_map[] = {
    { "W",   0 },
    { "E",   1 },
    { "S",   2 },
    { "N",   3 },
    { NULL,  0 }
  };

  struct anim_map {
    char* name;
    unsigned id;
  } anim_map[] = {
    { "init",       1 },
    { "walk",       2 },
    { "stand",      3 },
    { "talkStart",  4 },
    { "talkStop",   5 },
    { NULL, 0 }
  };
  
#define ANIM_USER_START 6

#define COST_CMD_SOUND 0x70
#define COST_CMD_STOP  0x79
#define COST_CMD_START 0x7A
#define COST_CMD_HIDE  0x7B
#define COST_CMD_SKIP  0x7C
  
%}


%union {
  int integer;    // INTEGER, number
  char* str;      // STRING, ID
  int intpair[2];
  uint8_t* intlist;
}

%token PALETTE
%token FLAGS
%token LOOP FLIP
%token PICTURE
%token LIMB
%token ANIM
%token <integer> INTEGER
%token <str> STRING
%token <str> SYM
%token PATH GLOB
%token POSITION
%token MOVE
%token START STOP HIDE SKIP SOUND
%token ERROR

%type <integer> number location
%type <intpair> numberpair
%type <intlist> intlist intlistitem cmdlist cmdlistitem
%type <intpair> limbanimflags flaglist flag flagvalue

%%

srcfile: /* empty */
| costume
;

// the palette declaration is requiered and must be
// first.
costume: palette costflags statementlist
;

statementlist: ';'
| statement ';'
| statementlist statement ';'
;

statement: dec
| picture
| limb
| anim
;

dec: picturedec
{
  cur_pic = NULL;
}
| limbdec
{
  cur_limb = NULL;
}
| animdec
{
  cur_anim = NULL;
}
;

picture: picturedec '=' '{' picparamlist '}'
{
  // check that the pic have an image
  if(!cur_pic->path)
    COST_ABORT(@1,"Picture %s has no path defined.\n",
               cur_pic->name);
  
  if(cur_pic->is_glob) {
    glob_t gl;
    int n,nlen = strlen(cur_pic->name);
    char name[nlen+16];
    cost_pic_t* p;
    
    // expand the glob
    if(glob(cur_pic->path,0,NULL,&gl))
      COST_ABORT(@1,"Glob pattern error: %s\n",cur_pic->path);
    if(!gl.gl_pathc)
      COST_ABORT(@1,"Glob pattern didn't match anything: %s\n",cur_pic->path);
    
    // detach the current pic from the list
    if(pic_list == cur_pic)
      pic_list = cur_pic->next;
    else {
      for(p = pic_list ; p && p->next != cur_pic ; p = p->next);
      if(!p) COST_ABORT(@1,"Internal error: failed to find current pic in pic list.\n");
      p->next = cur_pic->next;
    }
    cur_pic->next = NULL;
    
    // create the new pictures
    for(n = 0 ; n < gl.gl_pathc ; n++) {
      sprintf(name,"%s%02d",cur_pic->name,n);
      p = find_pic(name);
      if(p) {
        if(p->path)
          COST_ABORT(@1,"Glob expansion failed: %s is already defined\n",name);
      } else {
        p = calloc(1,sizeof(cost_pic_t));
        p->name = strdup(name);
      }
      // load it up
      p->path = strdup(gl.gl_pathv[n]);
      if(!cost_pic_load(p,p->path))
        COST_ABORT(@1,"Failed to load %s.\n",p->path);
      // copy the position, etc
      p->rel_x = cur_pic->rel_x;
      p->rel_y = cur_pic->rel_y;
      p->move_x = cur_pic->move_x;
      p->move_y = cur_pic->move_y;
      
      // append it to the list
      p->next = pic_list;
      pic_list = p;
    }
    
    // free the cur_pic
    free(cur_pic->name);
    free(cur_pic->path);
    free(cur_pic);
    globfree(&gl);
  } else {  
    // load it up
    if(!cost_pic_load(cur_pic,cur_pic->path))
      COST_ABORT(@1,"Failed to load %s.\n",cur_pic->path);
  }

  cur_pic = NULL;
}
;

picturedec: PICTURE SYM
{
  cost_pic_t* p = find_pic($2);

  // alloc the pic
  if(!p) {
    p = calloc(1,sizeof(cost_pic_t));
    p->name = $2;
    p->next = pic_list;
    pic_list = p;
  } else
    free($2);
  // set it as the current pic
  cur_pic = p;
}
;

picparamlist: picparam
| picparamlist ',' picparam
;

picparam: PATH '=' STRING
{
  if(cur_pic->path)
    COST_ABORT(@1,"This picture already has a path defined.\n");

  if(img_path)
    asprintf(&cur_pic->path,"%s/%s",img_path,$3);
  else
    cur_pic->path = $3;
}
| GLOB '=' STRING
{
  if(cur_pic->path)
    COST_ABORT(@1,"This picture already has a path defined.\n");

  if(cur_pic->ref)
    COST_ABORT(@1,"This picture has already been referenced, we can't expand it to a glob anymore.\n");

  if(img_path)
    asprintf(&cur_pic->path,"%s/%s",img_path,$3);
  else
    cur_pic->path = $3;
  cur_pic->is_glob = 1;
}
| POSITION '=' numberpair
{
  cur_pic->rel_x = $3[0];
  cur_pic->rel_y = $3[1];
}
| MOVE '=' numberpair
{
  cur_pic->move_x = $3[0];
  cur_pic->move_y = $3[1];
}
;


limb: limbdec '=' '{' piclist '}'
;

limbdec: LIMB SYM location
{
  int n;

  if($3 >= 0) {
    // valid index ??
    if($3 >= COST_MAX_LIMBS)
      COST_ABORT(@3,"Limb index %d is invalid. Costumes can only have up to %d limbs.\n",
                 $3,COST_MAX_LIMBS);
    n = $3;
    // name mismatch ???
    if(limbs[n].name && strcmp(limbs[n].name,$2))
       COST_ABORT(@3,"Limb %d is already defined with the name %s.\n",
                  n,limbs[n].name);
  } else {
    // look if there a limb with that name
    for(n = 0 ; n < COST_MAX_LIMBS ; n++) {
      if(!limbs[n].name) continue;
      if(!strcmp(limbs[n].name,$2)) break;
    }
    // no limb with that name then look for a free index
    if(n == COST_MAX_LIMBS) {
      for(n = 0 ; n < COST_MAX_LIMBS ; n++)
        if(!limbs[n].name) break;
      if(n == COST_MAX_LIMBS)
        COST_ABORT(@1,"Too many limbs defined. Costumes can only have up to %d limbs.\n",
                   COST_MAX_LIMBS);
    }
  }

  // alloc it
  if(!limbs[n].name)
    limbs[n].name = strdup($2);

  cur_limb = &limbs[n];
}
;

piclist: SYM
{
  cost_pic_t* p = find_pic($1);

  if(!p)
    COST_ABORT(@1,"There is no picture named %s.\n",$1);
    
  cur_limb->pic[0] = p;
  cur_limb->num_pic = 1;
  p->ref++;
}
| piclist ',' SYM
{
  cost_pic_t* p = find_pic($3);

  if(!p)
    COST_ABORT(@3,"There is no picture named %s.\n",$3);

  if(cur_limb->num_pic >= LIMB_MAX_PICTURES)
    COST_ABORT(@3,"Too many pictures in limb. A limb can only have up to %d pictures.\n",
               LIMB_MAX_PICTURES);

  cur_limb->pic[cur_limb->num_pic] = p;
  cur_limb->num_pic++;
  p->ref++;
}
;


anim: animdec '=' '{' animdirlist '}'
{
  cur_anim = NULL;
}
;

animdec: ANIM SYM location
{
  int n;

  if($3 >= 0) {
    // valid index ??
    if($3 >= COST_MAX_ANIMS)
      COST_ABORT(@3,"Anim index %d is invalid. Costumes can only have up to %d anims.\n",
                 $3,COST_MAX_ANIMS);
    n = $3;
    // name mismatch ???
    if(anims[n].name && strcmp(anims[n].name,$2))
       COST_ABORT(@3,"Anim %d is already defined with the name %s.\n",
                  n,anims[n].name);
  } else {
    // look if there a anim with that name
    for(n = 0 ; n < COST_MAX_ANIMS ; n++) {
      if(!anims[n].name) continue;
      if(!strcmp(anims[n].name,$2)) break;
    }
    // no anim with that name then look for a free index
    if(n == COST_MAX_ANIMS) {
      // look if it's a predefined name
      for(n = 0 ; anim_map[n].name ; n++)
        if(!strcmp(anim_map[n].name,$2)) break;

      if(anim_map[n].name) n = anim_map[n].id;
      else {
        for(n = ANIM_USER_START ; n < COST_MAX_ANIMS ; n++)
          if(!anims[n].name) break;
        if(n == COST_MAX_ANIMS)
          COST_ABORT(@1,"Too many anims defined. Costumes can only have up to %d anims.\n",
                     COST_MAX_ANIMS);
      }
    }
  }

  // alloc it
  if(!anims[n].name)
    anims[n].name = strdup($2);

  cur_anim = &anims[n];

}
;

animdirlist: animdir ';'
| animdirlist animdir ';'
;

animdir: animdirname '=' '{' limbanimlist  '}'
;

animdirname: SYM
{
  int n;
  for(n = 0 ; dir_map[n].name ; n++)
    if(!strcmp(dir_map[n].name,$1)) break;

  if(!dir_map[n].name)
    COST_ABORT(@1,"Bad direction name.\n");

  cur_dir = cur_anim->dir+dir_map[n].id;
}
;

limbanimlist: limbanim
| limbanimlist ',' limbanim
;

limbanim: SYM '(' cmdlist ')' limbanimflags
{
  int n;
  // find the limb
  for(n = 0 ; n < COST_MAX_LIMBS ; n++)
    if(limbs[n].name && !strcmp(limbs[n].name,$1)) break;
  if(n == COST_MAX_LIMBS)
    COST_ABORT(@1,"There is no limb named %s.\n",$1);

  if($3[0] > COST_MAX_LIMB_CMDS)
    COST_ABORT(@3,"Too many commands. A limb anim can have only %d commands.\n",
               COST_MAX_LIMB_CMDS);

  if(cur_dir->limb_mask & (1 << n))
    COST_ABORT(@1,"Anim for limb %s is already defined.\n",
               limbs[n].name);

  cur_dir->limb_mask |= (1 << n);
  cur_dir->limb[n].len = $3[0];
  cur_dir->limb[n].flags |=  $5[0];
  cur_dir->limb[n].flags &= ~$5[1];
  memcpy(cur_dir->limb[n].cmd,$3+1,$3[0]);
  free($3);
}
| SYM '(' ')' limbanimflags
{
  int n;
  // find the limb
  for(n = 0 ; n < COST_MAX_LIMBS ; n++)
    if(limbs[n].name && !strcmp(limbs[n].name,$1)) break;
  if(n == COST_MAX_LIMBS)
    COST_ABORT(@1,"There is no limb named %s.\n",$1);

  if(cur_dir->limb_mask & (1 << n))
    COST_ABORT(@1,"Anim for limb %s is already defined.\n",
               limbs[n].name);
  cur_dir->limb_mask |= (1 << n);
  cur_dir->limb[n].flags |=  $4[0];
  cur_dir->limb[n].flags &= ~$4[1];
}
;

cmdlist: cmdlistitem
| cmdlist ',' cmdlistitem
{
  int l = $1[0] + $3[0];

  $$ = realloc($1,l+1);
  memcpy($$+$$[0]+1,$3+1,$3[0]);
  $$[0] = l;
  free($3);
}
;

cmdlistitem: intlistitem
| START
{
  $$ = malloc(2);
  $$[0] = 1;
  $$[1] = COST_CMD_START;
}
| STOP
{
  $$ = malloc(2);
  $$[0] = 1;
  $$[1] = COST_CMD_STOP;
}
| HIDE
{
  $$ = malloc(2);
  $$[0] = 1;
  $$[1] = COST_CMD_HIDE;
}
| SKIP
{
  $$ = malloc(2);
  $$[0] = 1;
  $$[1] = COST_CMD_SKIP;
}
| SOUND '(' INTEGER ')'
{
  if($3 < 1 || $3 > 8)
    COST_ABORT(@3,"Invalid costume sound index: %d.\n",$3);
  
  $$ = malloc(2);
  $$[0] = 1;
  $$[1] = COST_CMD_SOUND + $3;
}
;

palette: PALETTE '(' intlist ')' ';'
{
  if($3[0] != 16 &&
     $3[0] != 32)
    COST_ABORT(@1,"Invalid palette size. Must be 16 or 32.\n");
  pal_size = $3[0];
  memcpy(pal,$3+1,pal_size);
  free($3);
}
;

intlist: intlistitem
| intlist ',' intlistitem
{
  int l = $1[0] + $3[0];

  $$ = realloc($1,l+1);
  memcpy($$+$$[0]+1,$3+1,$3[0]);
  $$[0] = l;
  free($3);
}
;

intlistitem: INTEGER
{
  if($1 < 0 || $1 > 255)
    COST_ABORT(@1,"List items can't be greater than 255.\n");

  $$ = malloc(2);
  $$[0] = 1;
  $$[1] = $1;
}
| '[' INTEGER '-' INTEGER ']'
{
  int i,s,l = $4-$2+1;

  if($2 < 0 || $2 > 255)
    COST_ABORT(@2,"List items can't be greater than 255.\n");
  if($4 < 0 || $4 > 255)
    COST_ABORT(@4,"List items can't be greater than 255.\n");

  if($4 >= $2)
    l = $4-$2+1, s = 1;
  else
    l = $2-$4+1, s = -1;
    
  $$ = malloc(l+1);
  $$[0] = l;
  for(i = 0 ; i < l ; i++)
    $$[i+1] = $2+s*i;
}
;

costflags: /* empty */
| FLAGS '=' flaglist ';'
{
  cost_flags |=  $3[0];
  cost_flags &= ~$3[1];
}
;

limbanimflags: /* empty */
{
  $$[0] = 0;
  $$[1] = 0;
}
| flaglist
;

flaglist: flag
| flaglist ',' flag
{
  $$[0] |= $3[0];
  $$[1] |= $3[1];
}
;

flag: flagvalue
{
  $$[0] = $1[0];
  $$[1] = $1[1];
}
| '!' flagvalue
{
  $$[0] = $2[1];
  $$[1] = $2[0];
}
;

flagvalue: FLIP
{
  $$[0] = 0;
  $$[1] = 0x80;
}
| LOOP
{
  $$[0] = 0;
  $$[1] = 0x80;
}
;

numberpair: '{' number ',' number '}'
{
  $$[0] = $2;
  $$[1] = $4;
}
;

number: INTEGER
| '-' INTEGER
{
  $$ = - $2;
}
| '+' INTEGER
{
  $$ = $2;
}
;

location: /* empty */
{
  $$ = -1;
}
| '@' INTEGER
{
  $$ = $2;
}
;

%%

// load an image and encode it with the strange vertical RLE
static int cost_pic_load(cost_pic_t* pic,char* file) {  
  scc_img_t* img = scc_img_open(file);
  int color,rep,shr,max_rep,x,y;
  uint8_t* dst;

  if(!img) return 0;

  if(img->ncol != pal_size)
    printf("Warning, image %s doesn't have the same number of colors as the palette: %d != %d.\n",file,img->ncol,pal_size);

  pic->width = img->w;
  pic->height = img->h;

  // encode the pic
  // alloc enouth mem for the worst case
  dst = pic->data = malloc(img->w*img->h);

  // set the params
  switch(pal_size) {
  case 16:
    shr = 4;
    max_rep = 0x0F;
    break;
  case 32:
    shr = 3;
    max_rep = 0x07;
    break;
  }

  // take the initial color
  color = img->data[0];
  if(color >= pal_size) color = pal_size-1;
  rep = 0;
  for(x = 0 ; x < img->w ; x++)
    for(y = 0 ; y < img->h ; y++) {
      if(rep < 255 && // can repeat ??
         img->data[y*img->w+x] == color) {
        rep++;
        continue;
      }
      // write the repetition
      if(rep > max_rep) {
        pic->data[pic->data_size] = (color << shr);
        pic->data_size++;
        pic->data[pic->data_size] = rep;
        pic->data_size++;
      } else {
        pic->data[pic->data_size] = (color << shr) | rep;
        pic->data_size++;
      }

      color = img->data[y*img->w+x];
      if(color >= pal_size) color = pal_size-1;
      rep = 1;
    }
  // write the last repetition
  if(rep > 0) {
    if(rep > max_rep) {
      pic->data[pic->data_size] = (color << shr);
      pic->data_size++;
      pic->data[pic->data_size] = rep;
      pic->data_size++;
    } else {
      pic->data[pic->data_size] = (color << shr) | rep;
      pic->data_size++;
    }
  }

  if(pic->data_size < img->w*img->h)
    pic->data = realloc(pic->data,pic->data_size);

  scc_img_free(img);
  return 1;
}

// compute the whole size of the costume
static int cost_get_size(int *na,unsigned* coff,unsigned* aoff,unsigned* loff) {
  cost_pic_t* p;
  int i,j,num_anim,clen = 0;
  int size = 4 + // size
    2 + // header
    1 + // num anim
    1 + // format
    pal_size + // palette
    2 + // cmds offset
    2*16; // limbs offset
    
  // get the highest anim id
  for(i = COST_MAX_ANIMS-1 ; i >= 0 ; i--)
    if(anims[i].name) break;
  num_anim = i+1;

  size += 2*COST_MAX_DIR*num_anim; // anim offsets

  for(i = 0 ; i < num_anim*COST_MAX_DIR ; i++) {
    cost_anim_dir_t* anim = &anims[i/COST_MAX_DIR].dir[i%COST_MAX_DIR];
    if(aoff) {
      if(anim->limb_mask)
        aoff[i] = size-clen;
      else
        aoff[i] = 0;
    }
    size += 2; // limb mask
    if(!anim->limb_mask) continue;
    for(j = COST_MAX_LIMBS-1 ; j >= 0 ; j--) {
      if(!(anim->limb_mask & (1 << j))) continue;
      if(anim->limb[j].len) {
        size += 2 + 1 + anim->limb[j].len; // limb start/len + cmds
        clen += anim->limb[j].len;
      } else
        size += 2;
    }
  }
  if(coff) coff[0] = size-clen;

  // limbs table
  for(i = COST_MAX_LIMBS-1 ; i >= 0 ; i--) {
    if(loff) {
      if(limbs[i].num_pic > 0)
        loff[i] = size;
      else
        loff[i] = 0;
    }
    size += 2*limbs[i].num_pic;
  }
    
  // pictures
  //  for(p = pic_list ; p ; p = p->next)
  for(i = COST_MAX_LIMBS-1 ; i >= 0 ; i--) // pictures
    for(j = 0 ; j < limbs[i].num_pic ; j++) {
      if(!(p = limbs[i].pic[j])) continue;
      if(p->ref && !p->offset) {
        p->offset = size;
        size += p->data_size + 6*2;
      }
    }

  na[0] = num_anim;
  return size;
}

static int cost_write(scc_fd_t* fd) {
  int size,num_anim,i,j,pad = 0;
  cost_pic_t* p;
  uint8_t fmt = 0x58 | cost_flags;
  unsigned coff,cpos = 0;
  unsigned aoff[COST_MAX_ANIMS*COST_MAX_DIR];
  unsigned loff[COST_MAX_LIMBS];

  if(pal_size == 32) fmt |= 1;

  size = cost_get_size(&num_anim,&coff,aoff,loff);

  // round up to next 2 multiple
  if(size & 1) {
    size++;
    pad = 1;
  }

  scc_fd_w32(fd,MKID('C','O','S','T'));
  scc_fd_w32be(fd,size+8);


  scc_fd_w32le(fd,size);  // size

  scc_fd_w8(fd,'C'); // header
  scc_fd_w8(fd,'O');

  scc_fd_w8(fd,num_anim*COST_MAX_DIR-1); // last anim

  scc_fd_w8(fd,fmt); // format

  scc_fd_write(fd,pal,pal_size); // palette

  scc_fd_w16le(fd,coff); // cmd offset

  for(i = COST_MAX_LIMBS-1 ; i >= 0 ; i--) // limbs offset
    scc_fd_w16le(fd,loff[i]);

  for(i = 0 ; i < num_anim*COST_MAX_DIR ; i++) // anim offsets
    scc_fd_w16le(fd,aoff[i]);

  for(i = 0 ; i < num_anim*COST_MAX_DIR ; i++) { // anims
    cost_anim_dir_t* anim = &anims[i/COST_MAX_DIR].dir[i%COST_MAX_DIR];
    scc_fd_w16le(fd,anim->limb_mask); // limb mask
    if(!anim->limb_mask) continue;
    for(j = COST_MAX_LIMBS-1 ; j >= 0 ; j--) {
      if(!(anim->limb_mask & (1 << j))) continue;
      if(anim->limb[j].len) {
        scc_fd_w16le(fd,cpos);  // cmd start
        cpos += anim->limb[j].len;
        // last cmd offset
        scc_fd_w8(fd,((anim->limb[j].len-1)&0x7F) |
                    (anim->limb[j].flags&0x80));
      } else
        scc_fd_w16le(fd,0xFFFF); // stoped limb
    }
  }

  for(i = 0 ; i < num_anim*COST_MAX_DIR ; i++) { // anim cmds
    cost_anim_dir_t* anim = &anims[i/COST_MAX_DIR].dir[i%COST_MAX_DIR];
    for(j = COST_MAX_LIMBS-1 ; j >= 0 ; j--) {
      if(!anim->limb[j].len ) continue;
      scc_fd_write(fd,anim->limb[j].cmd,anim->limb[j].len);
    }
  }

  for(i = COST_MAX_LIMBS-1 ; i >= 0 ; i--)  // limbs
    for(j = 0 ; j < limbs[i].num_pic ; j++) {
      if(limbs[i].pic[j]) scc_fd_w16le(fd,limbs[i].pic[j]->offset);
      else scc_fd_w16le(fd,0);
    }

  // i would prefer to just go along the list,
  // but that should give results closer to the original.
  for(i = COST_MAX_LIMBS-1 ; i >= 0 ; i--) // pictures
    for(j = 0 ; j < limbs[i].num_pic ; j++) {
      if(!(p = limbs[i].pic[j])) continue;
      if(!p->ref) continue;
      if(!p->width || !p->height) printf("Bad pic?\n");
      //printf("Write pic: %dx%d\n",p->width,p->height);
      scc_fd_w16le(fd,p->width);
      scc_fd_w16le(fd,p->height);
      scc_fd_w16le(fd,p->rel_x);
      scc_fd_w16le(fd,p->rel_y);
      scc_fd_w16le(fd,p->move_x);
      scc_fd_w16le(fd,p->move_y);
      scc_fd_write(fd,p->data,p->data_size);
      p->ref = 0;
    }

  if(pad) scc_fd_w8(fd,0);

  return 1;
}


static int akos_write(scc_fd_t* fd) {
  int akhd_size = 8 + 2 + 1 + 1 + 2 + 2 + 2;
  int akpl_size = 8 + pal_size;
  int aksq_size = 8;
  int akch_size = 8;
  int akof_size = 8;
  int akci_size = 8;
  int akcd_size = 8;
  int akos_size;
  int i,j,d;
  int cpos = 0;
  int aoff[COST_MAX_DIR*COST_MAX_ANIMS];
  cost_pic_t* pic;
  int num_anim = 0, num_pic = 0;
  int data_off = 0, header_off = 0;


  // get the highest anim id
  for(i = COST_MAX_ANIMS-1 ; i >= 0 ; i--)
    if(anims[i].name) break;
  num_anim = i+1;

  // Compute the number of picture and setup the ids
  for(pic = pic_list ; pic ; pic = pic->next) {
    if(!pic->ref) continue;
    pic->idx = num_pic;
    num_pic++;
  }

  // Compute the size needed for all commands
  for(i = 0 ; i < num_anim ; i++)
    for(d = 0 ; d < COST_MAX_DIR ; d++)
      for(j = COST_MAX_LIMBS-1 ; j >= 0 ; j--)
        aksq_size += anims[i].dir[d].limb[j].len;

  // Compute the size needed for the anim definitions
  akch_size += 2*COST_MAX_DIR*num_anim;
  for(i = 0 ; i < num_anim*COST_MAX_DIR ; i++) {
    cost_anim_dir_t* anim = &anims[i/COST_MAX_DIR].dir[i%COST_MAX_DIR];
    if(!anim->limb_mask) {
      aoff[i] = 0;
      continue;
    }
    aoff[i] = akch_size-8;
    akch_size += 2;
    for(j = COST_MAX_LIMBS-1 ; j >= 0 ; j--) {
      if(!(anim->limb_mask & (1 << j))) continue;
      akch_size += 1 + 2 + 2;
    }
  }

  akof_size += (4 + 2)*num_pic;

  akci_size += (2+2 + 2+2 + 2+2)*num_pic;

  // Compute the size needed for the picture data
  for(pic = pic_list ; pic ; pic = pic->next) {
    if(!pic->ref) continue;
    akcd_size += pic->data_size;
  }

  akos_size = 8 + akhd_size + akpl_size + aksq_size +
    akch_size + akof_size + akci_size + akcd_size;

  scc_fd_w32(fd,MKID('A','K','O','S'));
  scc_fd_w32be(fd,akos_size);

  // Write the header
  scc_fd_w32(fd,MKID('A','K','H','D'));
  scc_fd_w32be(fd,akhd_size);
  // Version ?
  scc_fd_w16le(fd,0x0001);
  // Flags
  scc_fd_w8(fd,cost_flags>>7);
  // Unknown
  scc_fd_w8(fd,0x80);
  // Num anim
  scc_fd_w16le(fd,num_anim*COST_MAX_DIR);
  // Num frame
  scc_fd_w16le(fd,num_pic);
  // Codec
  scc_fd_w16le(fd,1);

  // Write the palette
  scc_fd_w32(fd,MKID('A','K','P','L'));
  scc_fd_w32be(fd,akpl_size);
  scc_fd_write(fd,pal,pal_size);

  // Write the commands, a bit tricky because
  // need to relocate the picture id from limb picture
  // to global picture.
  scc_fd_w32(fd,MKID('A','K','S','Q'));
  scc_fd_w32be(fd,aksq_size);
  for(i = 0 ; i < num_anim*COST_MAX_DIR ; i++) {
    cost_anim_dir_t* anim = &anims[i/COST_MAX_DIR].dir[i%COST_MAX_DIR];
    for(j = COST_MAX_LIMBS-1 ; j >= 0 ; j--) {
      int c;
      for(c = 0 ; c < anim->limb[j].len ; c++) {
        int cmd = anim->limb[j].cmd[c];
        // Ignore high id atm, that include commands
        if(cmd >= 0x80) {
          scc_fd_w8(fd,cmd), c++;
          scc_fd_w8(fd,anim->limb[j].cmd[c]);
          continue;
        }
        if(cmd >= limbs[j].num_pic ||
           !limbs[j].pic[cmd]) {
          printf("Warning: Bad picture index in anim %s, "
                 "limb %s at %d: %d\n",
                 anims[i/COST_MAX_DIR].name,limbs[j].name,c,cmd);
          scc_fd_w8(fd,0);
        } else
          scc_fd_w8(fd,limbs[j].pic[cmd]->idx);
      }
    }
  }

  // Write the anims
  scc_fd_w32(fd,MKID('A','K','C','H'));
  scc_fd_w32be(fd,akch_size);
  // offsets
  for(i = 0 ; i < num_anim*COST_MAX_DIR ; i++)
    scc_fd_w16le(fd,aoff[i]);
  // definitions
  for(i = 0 ; i < num_anim*COST_MAX_DIR ; i++) {
    cost_anim_dir_t* anim = &anims[i/COST_MAX_DIR].dir[i%COST_MAX_DIR];
    if(!anim->limb_mask) continue;
    // limb mask
    scc_fd_w16le(fd,anim->limb_mask);
    for(j = COST_MAX_LIMBS-1 ; j >= 0 ; j--) {
      if(!(anim->limb_mask & (1 << j))) continue;
      // mode
      scc_fd_w8(fd,(anim->limb[j].flags&0x80) ? 0x03 : 0x02);
      // cmd start
      scc_fd_w16le(fd,cpos);
      cpos += anim->limb[j].len;
      // len
      scc_fd_w16le(fd,anim->limb[j].len-1);
    }
  }

  // Write the offset table
  scc_fd_w32(fd,MKID('A','K','O','F'));
  scc_fd_w32be(fd,akof_size);
  for(pic = pic_list ; pic ; pic = pic->next) {
    if(!pic->ref) continue;
    // Data offset
    scc_fd_w32le(fd,data_off);
    data_off += pic->data_size;
    // Header offset
    scc_fd_w16le(fd,header_off);
    header_off += 2+2 + 2+2 + 2+2;
  }

  // Write the pic headers
  scc_fd_w32(fd,MKID('A','K','C','I'));
  scc_fd_w32be(fd,akci_size);
  for(pic = pic_list ; pic ; pic = pic->next) {
    if(!pic->ref) continue;
    // width / height
    scc_fd_w16le(fd,pic->width);
    scc_fd_w16le(fd,pic->height);
    scc_fd_w16le(fd,pic->rel_x);
    scc_fd_w16le(fd,pic->rel_y);
    scc_fd_w16le(fd,pic->move_x);
    scc_fd_w16le(fd,pic->move_y);
  }

  // Write the pic data
  scc_fd_w32(fd,MKID('A','K','C','D'));
  scc_fd_w32be(fd,akcd_size);
  for(pic = pic_list ; pic ; pic = pic->next) {
    if(!pic->ref) continue;
    scc_fd_write(fd,pic->data,pic->data_size);
  }

  return 1;
}

static int header_write(scc_fd_t* fd,char *prefix) {
  int i;
  scc_fd_printf(fd,"/* This file was generated do not edit */\n\n");
  for(i = 0 ; i < COST_MAX_ANIMS ; i++) {
    if(!anims[i].name) continue;
    scc_fd_printf(fd,"#define %s%s %d\n",prefix,anims[i].name,i);
  }
  return 1;
}

static scc_lex_t* cost_lex;
extern int cost_main_lexer(YYSTYPE *lvalp, YYLTYPE *llocp,scc_lex_t* lex);

static void set_start_pos(YYLTYPE *llocp,int line,int column) {
  llocp->first_line = line+1;
  llocp->first_column = column;
}

static void set_end_pos(YYLTYPE *llocp,int line,int column) {
  llocp->last_line = line+1;
  llocp->last_column = column;
}

int yylex(void) {
  return scc_lex_lex(&yylval,&yylloc,cost_lex);
}

int yyerror (const char *s)  /* Called by yyparse on error */
{
  scc_log(LOG_ERR,"%s: %s\n",scc_lex_get_file(cost_lex),
          cost_lex->error ? cost_lex->error : s);
  return 0;
}

static void usage(char* prog) {
  printf("Usage: %s [-o output] input.scost\n",prog);
  exit(-1);
}

static scc_param_t scc_parse_params[] = {
  { "o", SCC_PARAM_STR, 0, 0, &cost_output },
  { "I", SCC_PARAM_STR, 0, 0, &img_path },
  { "akos", SCC_PARAM_FLAG, 0, 1, &akos },
  { "prefix", SCC_PARAM_STR, 0, 0, &symbol_prefix },
  { "header", SCC_PARAM_STR, 0, 0, &header_name },
  { NULL, 0, 0, 0, NULL }
};

int main (int argc, char** argv) {
  scc_cl_arg_t* files;
  char* out;

  if(argc < 2) usage(argv[0]);

  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);

  if(!files) usage(argv[0]);

  out = cost_output ? cost_output : "output.cost";
  out_fd = new_scc_fd(out,O_WRONLY|O_CREAT|O_TRUNC,0);
  if(!out_fd) {
    printf("Failed to open output file %s.\n",out);
    return -1;
  }

  cost_lex = scc_lex_new(cost_main_lexer,set_start_pos,set_end_pos,NULL);
  if(!scc_lex_push_buffer(cost_lex,files->val)) return -1;

  if(yyparse()) return -1;

  if(akos)
    akos_write(out_fd);
  else
    cost_write(out_fd);

  scc_fd_close(out_fd);

  if(header_name) {
    out_fd = new_scc_fd(header_name,O_WRONLY|O_CREAT|O_TRUNC,0);
    if(!out_fd) {
      printf("Failed to open output file %s.\n",out);
      return -1;
    }
    header_write(out_fd,symbol_prefix);
    scc_fd_close(out_fd);
  }

  return 0;
}
