
// We can probably kick some since the testing code left us.
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "scc_fd.h"
#include "scc_util.h"
#include "scc.h"

scc_res_list_t* scc_read_res_list(scc_fd_t* fd) {
  scc_res_list_t* list;
  int i;

  list = calloc(1,sizeof(scc_res_list_t));
  list->size = scc_fd_r16le(fd);
  if(list->size == 0) return list;
  list->room_no = malloc(list->size);
  list->room_off = malloc(list->size*sizeof(uint32_t));
  for(i = 0 ; i < list->size ; i++)
    list->room_no[i] = scc_fd_r8(fd);
  for(i = 0 ; i < list->size ; i++)
    list->room_off[i] = scc_fd_r32le(fd);

  return list;
}

void scc_print_res_list(scc_res_list_t* l) {
  int i;

  printf("The list have %d elements\n",l->size);
  for(i = 0; i < l->size ; i++)
    printf(" %4.4d : %d  (%d)\n",i,l->room_no[i],l->room_off[i]);
}

scc_res_idx_t* scc_read_res_idx(scc_fd_t* fd) {
  uint32_t blocktype,itemsize;
  int pos = 0;
  scc_res_idx_t* res = calloc(sizeof(scc_res_idx_t),1);
  scc_aary_t *aary,*aary_tail=NULL;

  while (1) {
    blocktype = scc_fd_r32(fd);
    itemsize = scc_fd_r32be(fd);
    pos += 8;

    if(blocktype == 0 || itemsize < 8) break;
    itemsize -= 8;

    printf("Found item %c%c%c%c (%d bytes)\n",
	   (uint8_t)(blocktype >> 24),
	   (uint8_t)(blocktype >> 16),
	   (uint8_t)(blocktype >> 8),
	   (uint8_t)(blocktype >> 0),
	   itemsize);

    switch(blocktype) {
      // This is the first block in dott index
    case MKID('R','N','A','M'): {
      uint8_t b = scc_fd_r8(fd);
      printf("MANR: %c (0x%x)\n",b,b);
      pos++;
    } break;   
    case MKID('M','A','X','S'): {
      // We do version 6 only atm
      if(itemsize != 30) {
	printf("Bad version probably, we need a 30 bytes maxs block.\n");
	return NULL;
      }

      res->num_vars = scc_fd_r16le(fd);
      res->unk1 = scc_fd_r16le(fd);
      res->num_bit_vars = scc_fd_r16le(fd);
      res->num_local_objs = scc_fd_r16le(fd);
      res->num_array = scc_fd_r16le(fd);
      res->unk2 = scc_fd_r16le(fd);
      res->num_verbs = scc_fd_r16le(fd);
      res->num_fl_objs = scc_fd_r16le(fd);
      res->num_inv = scc_fd_r16le(fd);
      res->num_rooms = scc_fd_r16le(fd);
      res->num_scr = scc_fd_r16le(fd);
      res->num_snds = scc_fd_r16le(fd);
      res->num_chst =  scc_fd_r16le(fd);
      res->num_cost = scc_fd_r16le(fd);
      res->num_glob_objs = scc_fd_r16le(fd);
      
      res->num_new_names = 50;
      res->shd_pal_size = 256;

      pos += 30;

#if 1
      printf("MAXS:\n");
      printf("vars: %d\n",res->num_vars);
      printf("bit vars: %d\n",res->num_bit_vars);
      printf("local objs: %d\n",res->num_local_objs);
      printf("arrays: %d\n",res->num_array);
      printf("verbs: %d\n",res->num_verbs);
      printf("fl objs: %d\n",res->num_fl_objs);
      printf("inv: %d\n",res->num_inv);
      printf("rooms: %d\n",res->num_rooms);
      printf("scripts: %d\n",res->num_scr);
      printf("sounds: %d\n",res->num_snds);
      printf("charset: %d\n",res->num_chst);
      printf("costumes: %d\n",res->num_cost);
      printf("global objs: %d\n",res->num_glob_objs);
#endif

    } break;
    case MKID('D','R','O','O'):{
      res->room_list = scc_read_res_list(fd);
      pos += itemsize;
      scc_print_res_list(res->room_list);
    } break;
    case MKID('D','S','C','R'):{
      res->scr_list = scc_read_res_list(fd);
      pos += itemsize;
      scc_print_res_list(res->scr_list);
    } break;
    case MKID('D','S','O','U'):{
      res->snd_list = scc_read_res_list(fd);
      pos += itemsize;
      scc_print_res_list(res->snd_list);
    } break;
    case MKID('D','C','O','S'):{
      res->cost_list = scc_read_res_list(fd);
      pos += itemsize;
      scc_print_res_list(res->cost_list);
    } break;

    case MKID('D','C','H','R'): {
      res->chst_list = scc_read_res_list(fd);
      pos += itemsize;
      scc_print_res_list(res->chst_list);
    } break;

    case MKID('D','O','B','J'): {
      res->num_obj_owners = scc_fd_r16le(fd);
      if(res->num_obj_owners != res->num_glob_objs) 
	printf("Warning obj list size doesn't match number of global objects\n");
      if(!res->num_obj_owners) break;
      res->obj_owners = malloc(res->num_obj_owners);
      if(scc_fd_read(fd,res->obj_owners,res->num_obj_owners) != res->num_obj_owners) {
	printf("Read error ???\n");
	return 0;
      }
      res->class_data = malloc(res->num_obj_owners * sizeof(uint32_t));
      if(scc_fd_read(fd,res->class_data,res->num_obj_owners * sizeof(uint32_t)) != res->num_obj_owners * sizeof(uint32_t)) {
	printf("Read error ???\n");
	return 0;
      }
      pos += itemsize;
      {
	int i;
	for(i = 0 ; i < res->num_obj_owners && i < 20 ; i++)
	  printf("Obj %d: owner 0x%x class 0x%x\n",i,res->obj_owners[i],
		 res->class_data[i]);
      }
    } break;

    case MKID('A','A','R','Y'): {
      int num;
      while((num = scc_fd_r16le(fd)) != 0) {
	aary = malloc(sizeof(scc_aary_t));
	aary->idx = num;
	aary->a = scc_fd_r16le(fd);
	aary->b = scc_fd_r16le(fd);
	aary->c = scc_fd_r16le(fd);
	pos += 4*2;
	
	printf("Got array %d: %dx%d (%d)\n",aary->idx,aary->a,aary->b,aary->c);
	if(aary_tail) aary_tail->next = aary;
	else res->aary = aary;
	aary_tail = aary;
      }
      pos += 2;
    } break;

    case MKID('D','I','R','F'):

    case MKID('D','L','F','L'):
    case MKID('D','I','R','I'):
    case MKID('A','N','A','M'):

    case MKID('D','I','R','R'):
    case MKID('D','R','S','C'):

    case MKID('D','I','R','S'):

    case MKID('D','I','R','C'):  


    case MKID('D','I','R','N'):

      printf("Such id should be handled\n");
      if(lseek(fd->fd,itemsize,SEEK_CUR) != pos + itemsize) return NULL;
      pos += itemsize;
      break;
    default:
      printf("Bad ID %c%c%c%c found in directory!\n",
	    (uint8_t)(blocktype >> 0),
	    (uint8_t)(blocktype >> 8),
	    (uint8_t)(blocktype >> 16),
	    (uint8_t)(blocktype >> 24));
      return NULL;
      
    }
  }

  return res;
}

scc_res_list_t* scc_get_res_idx_list(scc_res_idx_t* idx,uint32_t type) {

  switch(type) {
  case MKID('D','R','O','O'):
    return idx->room_list;
  case MKID('D','S','C','R'):
  case MKID('S','C','R','P'):
    return idx->scr_list;
  case MKID('D','S','O','U'):
  case MKID('S','O','U','N'):
    return idx->snd_list;
  case MKID('D','C','O','S'):
  case MKID('C','O','S','T'):
    return idx->cost_list;
  case MKID('D','C','H','R'):
  case MKID('C','H','A','R'):
    return idx->chst_list;
  default:
    return NULL;
  }
}

scc_res_t* scc_open_res_file(scc_fd_t* fd,scc_res_idx_t* idx) {
  scc_res_t* res;
  int i;
  uint8_t room,num;

  res = malloc(sizeof(scc_res_t));

  scc_fd_seek(fd,16,SEEK_SET);

  res->fd = fd;
  res->idx = idx;

  num = scc_fd_r8(fd);
  printf("%d rooms found:\n",num);
  for(i = 0 ; i < num ; i++) {
    room = scc_fd_r8(fd);
    idx->room_list->room_off[room] = scc_fd_r32le(fd);
    printf(" Room %4.4d : 0x%x\n",room,idx->room_list->room_off[room]);
  }

  return res;
}

int scc_res_idx_pos2room(scc_res_idx_t* idx,off_t pos) {
  int i;

  for(i = 0 ; i < idx->room_list->size ; i++) {
    if(idx->room_list->room_off[i] == pos) return i;
  }
  return -1;
}

int scc_res_idx_pos2idx(scc_res_idx_t* idx,scc_res_list_t* list,
			uint8_t room,off_t pos) {
  int i;

  if(room >= idx->room_list->size) return -1;
  pos -= idx->room_list->room_off[room];
  for(i = 0 ; i < list->size ; i++) {
    if(list->room_no[i] == room && list->room_off[i] == pos) return i;
  }
  return -1;

}

int check_dir(char* path) {
  struct stat st;

  if(stat(path,&st) < 0) {
    if(errno == ENOENT) { // Try to create it
      if(mkdir(path,0755) < 0) { // rwx r-x r-x
	printf("Failed to create dir %s: %s\n",path,strerror(errno));
	return 0;
      }
      return 1;
    }
    printf("Failed to stat %s: %s\n",path,strerror(errno));
    return 0;
  }
  if(!S_ISDIR(st.st_mode)) {
    printf("%s is not a directory !!!\n",path);
    return 0;
  }
  return 1;
}

scc_data_t* scc_read_data(scc_fd_t* fd,int len) {
  int rd;
  scc_data_t* r = malloc(sizeof(scc_data_t)+len);
  r->size = len;

  while(len > 0) {
    rd = scc_fd_read(fd,r->data+r->size-len,len);
    if(rd < 0) {
      printf("read error :(\n");
      free(r);
      return NULL;
    }
    len -= rd;
  }
  return r;
}

scc_data_list_t* scc_read_data_list(scc_fd_t* fd,int idx,int len) {
  int rd;
  scc_data_list_t* r = calloc(1,sizeof(scc_data_list_t)+len);
  r->idx = idx;
  r->size = len;

  while(len > 0) {
    rd = scc_fd_read(fd,r->data+r->size-len,len);
    if(rd < 0) {
      printf("read error :(\n");
      free(r);
      return NULL;
    }
    len -= rd;
  }
  return r;
}

scc_cycl_t* scc_parse_cycl(scc_fd_t* fd,int len) {
  scc_cycl_t *head = NULL,*tail=NULL;
  scc_cycl_t * c;
  int pos = 0;
  uint8_t idx;
  
  while(pos < len) {
    idx = scc_fd_r8(fd);
    pos++;
    if(!idx) break;
    c = calloc(1,sizeof(scc_cycl_t));
    c->idx = idx;
    c->unk = scc_fd_r16be(fd); // skip unknow
    c->freq = scc_fd_r16be(fd);
    c->flags = scc_fd_r16be(fd);
    c->start = scc_fd_r8(fd);
    c->end = scc_fd_r8(fd);

    if(!head) head = c;
    if(tail) tail->next = c;
    tail = c;

    pos += 8;
  }
  if(pos < len) scc_fd_seek(fd,len - pos,SEEK_CUR);

  if(scc_cycl_size(head) != len)
    printf("CYCL block size failed: (%d/%d)\n",scc_cycl_size(head),len);

  return head;
}

scc_pal_t* scc_parse_pals(scc_fd_t* fd,int len) {
  scc_pal_t *head=NULL,*tail=NULL;
  scc_pal_t *pal;
  uint32_t type,size,off_start;
  int pos = 0;

  if(len <= 16) {
    if(len > 0) scc_fd_seek(fd,len,SEEK_CUR);
    return NULL;
  }
  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  if(type != MKID('W','R','A','P')) {
    scc_fd_seek(fd,len-8,SEEK_CUR);
    return NULL;
  }
  pos = 8;
  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  if(type != MKID('O','F','F','S')) {
    scc_fd_seek(fd,len-16,SEEK_CUR);
    return NULL;
  }
  off_start = pos = 16;
  size -= 8;

  if(!size || len < pos+size+8) {
    printf("Warning PALS block with invalid size\n");
    scc_fd_seek(fd,len - pos,SEEK_CUR);
    return NULL;
  } else { // Read the OFFS block
    int num_pal = size/4;
    uint32_t offset[num_pal];
    int r,w=0;

    while(w < size) {
      r = scc_fd_read(fd,offset+w,size-w);
      if(r < 0) return NULL;
      w += r;
    }
    //for(r = 0 ; r < num_pal ; r++)
    //  printf("Palette %d at 0x%x\n",r+1,offset[r]);
    pos += size;

    //printf("We should find %d pals (%d/4)\n",size/4,size);

    if(pos != size+16) {
      printf("We missed somethg in the OFFS block ??\n");
      scc_fd_seek (fd,size+16-pos,SEEK_CUR);
      pos = size+16;
    }

    r = 0;
    while(pos < len) {
      int i;

      type = scc_fd_r32(fd);
      size = scc_fd_r32be(fd);
      pos += 8; size -= 8;

      if(!r) size = 256;
      else size /= 3;

      if(type != MKID('A','P','A','L') ||
	 pos + 3*size  > len) {
	scc_fd_seek(fd,len - pos,SEEK_CUR);
	return NULL;
      }

      if(r >= num_pal) break;

      if(pos != offset[r] + off_start) {
	printf("Warning missed some pal data ???\n");
	scc_fd_seek(fd,(offset[r] + off_start)-pos,SEEK_CUR);
	pos = offset[r] + off_start;
      }

      pal = malloc(sizeof(scc_pal_t));
      for(i = 0 ; i < size ; i++) {
	pal->r[i] = scc_fd_r8(fd);
	pal->g[i] = scc_fd_r8(fd);
	pal->b[i] = scc_fd_r8(fd);
	pos += 3;
      }
      pal->next = NULL;
      if(!head) head = pal;
      if(tail) tail->next = pal;
      tail = pal;
      //printf("Palette parsed (0x%x)...\n",pos-off_start);
      r++;
    }
    
    if(pos < len) {
      printf("Warning pals parsing finished unaligned (%d/%d) ???\n",pos,len);
      scc_fd_seek(fd,len - pos,SEEK_CUR);
    }

  }

  if(scc_pals_size(head) != len)
    printf("PALS size failed (%d/%d)\n",scc_pals_size(head),len);

  return head;  
}

scc_rmim_t* scc_parse_rmim(scc_fd_t* fd,int len) {
  scc_rmim_t* rmim = NULL;
  uint32_t type,size;
  int pos = 0,r,w,b;



  if(len < 18) return NULL;

  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  pos += 8; size -= 8;

  if(type == MKID('R','M','I','H')) {
    if(size != 2) {
      printf("Invalid RMIH block !!!\n");
      scc_fd_seek(fd,len - pos,SEEK_CUR);
      return NULL;
    }
    rmim = calloc(1,sizeof(scc_rmim_t));
    rmim->num_z_buf = scc_fd_r16le(fd);
    pos += 2;

    if(pos == len) return rmim;
    
    type = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    pos += 8; size -= 8;
  }

  if(type != MKID('I','M','0','0')) {
    printf("No IM00 block found ???\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return rmim;
  }

  if(size < 8) {
    printf("Invalid IM00 block ???\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return rmim;
  }

  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  pos += 8; size -= 8;

  if(type != MKID('S','M','A','P')) {
    printf("No SMAP block found ???\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return rmim;
  }

  if(pos + size > len) {
    printf("Too short RMIM block ???\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return rmim;
  }

  if(!rmim) rmim = calloc(1,sizeof(scc_rmim_t));
  rmim->smap = malloc(size);
  rmim->smap_size = size;
  w = 0;
  while(w < size) {
    r = scc_fd_read(fd,rmim->smap + w,size-w);
    if(r < 0) {
      printf("Read error !!!!\n");
      free(rmim->smap);
      rmim->smap = NULL;
      scc_fd_seek(fd,len-pos,SEEK_CUR);
      return rmim;
    }
    pos += r;
    w += r;
  }

  while(pos < len) {
    type = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    pos += 8; size -= 8;

    if(pos + size > len) break;

    if(type >> 8 != MKID(0,'Z','P','0')) {
      printf("Unknow block in IM00 ????\n");
      scc_fd_seek(fd,size,SEEK_CUR);
      pos += size;
      continue;
    }
    
    b = (type & 0xFF)-'0';
    if(b < 0 || b >= SCC_MAX_IM_PLANES) {
      printf("Invalid RMIM ZPnn index: %d\n",b);
      scc_fd_seek(fd,size,SEEK_CUR);
      pos += size;
      continue;
    }

    rmim->z_buf[b] = malloc(size);
    rmim->z_buf_size[b] = size;
    w = 0;
    while(w < size) {
      r = scc_fd_read(fd,rmim->z_buf[b] + w,size-w);
      if(r <= 0) {
	printf("Read error !!!!\n");
	free(rmim->z_buf[b]);
	rmim->z_buf[b] = NULL;
	break;
      }
      pos += r;
      w += r;
    }
  }

  if(pos != len) scc_fd_seek(fd,len-pos,SEEK_CUR);

  if(scc_rmim_size(rmim) != len)
    printf("RMIM size failed (%d/%d)\n",scc_rmim_size(rmim),len);

  return rmim;
}

scc_imnn_t* scc_parse_imnn(scc_fd_t* fd,uint8_t idx,int len) {
  uint32_t type,size;
  int pos = 0,r,w,b;
  scc_imnn_t * imnn;

  if(len < 8) return NULL;

  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  pos += 8; size -= 8;

  if(type != MKID('S','M','A','P')) {
    printf("No SMAP block found ???\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return NULL;
  }

  if(pos + size > len) {
    printf("Too short RMIM block ???\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return NULL;
  }

  imnn = calloc(1,sizeof(scc_imnn_t));
  imnn->idx = idx;
  imnn->smap = malloc(size);
  imnn->smap_size = size;
  w = 0;
  while(w < size) {
    r = scc_fd_read(fd,imnn->smap + w,size-w);
    if(r < 0) {
      printf("Read error !!!!\n");
      free(imnn->smap);
      free(imnn);
      scc_fd_seek(fd,len-pos,SEEK_CUR);
      return NULL;
    }
    pos += r;
    w += r;
  }

  if(pos != size+8) {
    printf("Warning missed SMAP block end\n");
    scc_fd_seek(fd,size+8-pos,SEEK_CUR);
    pos = size+8;
  }

  while(pos < len) {
    type = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    pos += 8; size -= 8;

    if(pos + size > len) break;

    if(type >> 8 != MKID(0,'Z','P','0')) {
      printf("Unknow block in IM00 ????\n");
      scc_fd_seek(fd,pos+size,SEEK_CUR);
      pos += size;
      continue;
    }
    
    b = (type & 0xFF)-'0';
    if(b < 0 || b >= SCC_MAX_IM_PLANES) {
      printf("Invalid RMIM ZPnn index: %d\n",b);
      scc_fd_seek(fd,pos+size,SEEK_CUR);
      pos += size;
      continue;
    }

    imnn->z_buf[b] = malloc(size);
    imnn->z_buf_size[b] = size;
    w = 0;
    while(w < size) {
      r = scc_fd_read(fd,imnn->z_buf[b] + w,size-w);
      if(r < 0) {
	printf("Read error !!!!\n");
	free(imnn->z_buf[b]);
	imnn->z_buf[b] = NULL;
	break;
      }
      pos += r;
      w += r;
    }
    b++;
  }

  if(pos != len) {
    printf("Missed IMnn block end\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
  }
  return imnn;
}

scc_obim_t* scc_parse_obim(scc_fd_t* fd,int len) {
  uint32_t type,size;
  int pos = 0,i;
  scc_obim_t *obim;
  scc_imnn_t *imnn,*imnn_tail = NULL;

  if(len < 26) {
    scc_fd_seek(fd,len,SEEK_CUR);
    return NULL;
  }
  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  pos += 8; size -= 8;

  if(type != MKID('I','M','H','D')) {
    printf("Warning OBIM lock without IMHD\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return NULL;
  }
  
  if(size < 18) {
    printf("Invalid IMHD block ???\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return NULL;
  }

  obim = calloc(1,sizeof(scc_obim_t));
  obim->idx = scc_fd_r16le(fd);
  obim->num_imnn = scc_fd_r16le(fd);
  obim->num_zpnn = scc_fd_r16le(fd);
  obim->unk = scc_fd_r16le(fd);
  obim->x = scc_fd_r16le(fd);
  obim->y = scc_fd_r16le(fd);
  obim->width = scc_fd_r16le(fd);
  obim->height = scc_fd_r16le(fd);
  obim->num_hotspots = scc_fd_r16le(fd);
  pos += 18;
  if(obim->num_hotspots != (size-18)/4) {
    printf("Warning invalid num hotspots: %d/%d\n",
	   obim->num_hotspots,(size-18)/4);
    if(obim->num_hotspots > (size-18)/4)
      obim->num_hotspots = (size-18)/4;
  }
  obim->hotspots = malloc(obim->num_hotspots*4);
  for(i = 0 ; i < obim->num_hotspots ; i++) {
    obim->hotspots[2*i] = scc_fd_r16le(fd);
    obim->hotspots[2*i+1] = scc_fd_r16le(fd);
    pos += 4;
  }
  
  if(pos != size+8) {
    printf("Warning missing some hotspot data ??\n");
    scc_fd_seek(fd,size+8-pos,SEEK_CUR);
    pos = size+8;
  }

  //  scc_fd_seek(fd,len-pos,SEEK_CUR);
  // return obim;

  while(pos < len) {
    type = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    pos += size; size -= 8;

    if(type>>8 != MKID(0,'I','M','0')) {
      printf("Got unknow block in OBIM\n");
      scc_fd_seek(fd,size,SEEK_CUR);
      continue;
    }

    if(size < 8) {
      printf("Invalid IMnn block ???\n");
      scc_fd_seek(fd,size,SEEK_CUR);
      continue;
    }
    imnn = scc_parse_imnn(fd,(type & 0xFF)-'0',size);
    if(!imnn) {
      printf("Failed to parse IMnn block\n");
      continue;
    }
    if(imnn_tail) imnn_tail->next = imnn;
    else obim->img = imnn;
    imnn_tail = imnn;
  }

  if(pos != len) {
    printf("Missed end of OBIM block\n");
    scc_fd_seek(fd,pos-len,SEEK_CUR);
  }

  if(scc_obim_size(obim) != len)
    printf("OBIM size failed (%d/%d)\n",scc_obim_size(obim),len);

  return obim;
}

scc_verb_t* scc_parse_verb(scc_fd_t* fd, int len) {
  struct {
    uint8_t idx;
    uint16_t off;
  } *map = NULL;
  int map_size = 0,map_pos = 0;
  scc_verb_t *verb = NULL,*verb_tail = NULL,*v;
  int pos = 0;
  int i,j;

  while(1) {
    if(len < pos + 1) {
      printf("Too small VERB block ???\n");
      if(map) free(map);
      return NULL;
    }

    if(map_pos >= map_size) {
      map_size += 16;
      map = realloc(map,map_size*sizeof(map[0]));
    }

    map[map_pos].idx = scc_fd_r8(fd); pos++;
    if(map[map_pos].idx == 0) break;

    if(len >= pos + 2) {
      map[map_pos].off = scc_fd_r16le(fd); pos += 2;
    } else if(map[map_pos].idx != 0) {
      printf("Too small VERB block ???\n");
      if(map) free(map);
      return NULL;
    }

    //    if(map[map_pos].idx == 0xFF || map[map_pos].idx == 0) break;

    map_pos++;
  }

  for(i = 0 ; i < map_pos ; i++) {

    for(j = 0 ; j < i ; j++) {
      if(map[j].off == map[i].off) break;
    }
    v = calloc(1,sizeof(scc_verb_t));
    v->idx = map[i].idx;

    // duplicate code entry ?
    if(j < i) {
      v->code_idx = j;
      verb_tail->next = v;
      //else verb = v;
      verb_tail = v;
      continue;
    }

    if(map[i].off-8 >= len) {
      printf("Bad verb offset ??? %d/%d\n",map[i].off-8,len);
      continue;
    }

    if(pos+8 != map[i].off) {
      scc_fd_seek(fd,map[i].off-pos-8,SEEK_CUR);
      pos = map[i].off-8;
    }

    v->len = len-pos;

    for(j = 0 ; j < map_pos ; j++) {
      if(map[j].off > map[i].off && map[j].off-map[i].off < v->len )
	v->len = map[j].off-map[i].off;	
    }

    if(pos+v->len > len) {
      printf("Not enouth data left for the code block %d/%d???\n",v->len,len-pos);
      break;
    }

    v->code = malloc(v->len);
    scc_fd_read(fd,v->code,v->len); pos += v->len;
    if(v->code[v->len-1] != 0x65) printf("Verb %d: code without return ???\n",v->idx);

    if(verb_tail) verb_tail->next = v;
    else verb = v;
    verb_tail = v;
  }

  if(pos < len) scc_fd_seek(fd,len-pos,SEEK_CUR);

  if(map) free(map);
  return verb;
}

scc_obcd_t* scc_parse_obcd(scc_fd_t* fd,int len) {
  scc_obcd_t* obcd;
  uint32_t type,size;
  int pos = 0;

  if(len < 17+8) {
    printf("Invalid OBCD block (too short)\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return NULL;
  }

  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  pos += 8;
  size -= 8;

  if(type != MKID('C','D','H','D')) {
    printf("OBCD block not starting with a CDHD block ???\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return NULL;
  }
  
  if(size != 17) {
    printf("Invalid OBCD block (bad size)\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return NULL;
  }

  obcd = calloc(1,sizeof(scc_obcd_t));
  obcd->idx = scc_fd_r16le(fd);
  obcd->x = scc_fd_r16le(fd);
  obcd->y = scc_fd_r16le(fd);
  obcd->width = scc_fd_r16le(fd);
  obcd->height = scc_fd_r16le(fd);
  obcd->flags = scc_fd_r8(fd);
  obcd->parent = scc_fd_r8(fd);
  obcd->unk1 = scc_fd_r16le(fd);
  obcd->unk2 = scc_fd_r16le(fd);
  printf("OBCD %d: %x %d %d\n",obcd->idx,obcd->flags,obcd->unk1,obcd->unk2);
  obcd->actor_dir = scc_fd_r8(fd);
  pos += 17;

  while(pos < len) {
    
    type = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    pos += size;
    size -= 8;

    switch(type) {
    case MKID('V','E','R','B'):
      if(obcd->verb) {
	printf("Warning we alredy have a VERB block !!!\n");
	break;
      }
      obcd->verb = scc_parse_verb(fd,size);
      //if(!obcd->verb)
      //	printf("Failed to parse VERB block ??\n");
      break;
    case MKID('O','B','N','A'): {
      int r,w=0;
      if(obcd->name) {
	printf("Warning we alredy have a OBNA block !!!\n");
	break;
      }
      obcd->name = malloc(size);
      while(w < size) {
	r = scc_fd_read(fd,obcd->name+w,size-w);
	if(r < 0) {
	  printf("Read error\n");
	  break;
	}
	w += r;
      }
      obcd->name[size-1] = '\0'; // Just to make sure
    } break;
    default:
      printf("Got unknow block in OBCD %c%c%c%c\n",
	     (uint8_t)(type >> 24),
	     (uint8_t)(type >> 16),
	     (uint8_t)(type >> 8),
	     (uint8_t)(type >> 0)
	     );
    }
  }

  if(pos != len) {
    printf("Warning misaligned end in OBCD !!!!\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
  }

  if(scc_obcd_size(obcd) != len)
    printf("OBCD %d size failed (%d)\n",obcd->idx,len-scc_obcd_size(obcd));

  return obcd;
}

scc_boxd_t* scc_parse_boxd(scc_fd_t* fd,int len) {
  uint16_t num;
  int pos = 0;
  scc_boxd_t *boxd_head = NULL,*boxd,*boxd_tail = NULL;

  if(len < 2+20) {
    printf("Invalid BOXD block (too short)\n");
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return NULL;
  }

  num = scc_fd_r16le(fd);
  pos += 2;

  if(num*20+2 != len) printf("Warning BOXD block have invalid size\n");

  while(pos+20 <= len) {
    boxd = malloc(sizeof(scc_boxd_t));
    boxd->next = NULL;
    boxd->ulx = (int16_t)scc_fd_r16le(fd);
    boxd->uly = (int16_t)scc_fd_r16le(fd);
    boxd->urx = (int16_t)scc_fd_r16le(fd);
    boxd->ury = (int16_t)scc_fd_r16le(fd);
    boxd->lrx = (int16_t)scc_fd_r16le(fd);
    boxd->lry = (int16_t)scc_fd_r16le(fd);
    boxd->llx = (int16_t)scc_fd_r16le(fd);
    boxd->lly = (int16_t)scc_fd_r16le(fd);
    boxd->mask = scc_fd_r8(fd);
    boxd->flags = scc_fd_r8(fd);
    boxd->scale = scc_fd_r16le(fd);

    if(!boxd_head) boxd_head = boxd;
    if(!boxd_tail) boxd_tail = boxd;
    else boxd_tail->next = boxd;
    boxd_tail = boxd;
    pos += 20;
  }

  return boxd_head;
}

uint8_t** scc_parse_boxm(scc_fd_t* fd,int num_box, int len) {
  int tbl = (num_box+1)*sizeof(uint8_t*);
  uint8_t* buf = malloc(tbl+num_box*num_box);
  uint8_t** boxm = (uint8_t**)buf;
  int i,j;
  uint8_t s,e,d;

  // Fill up the ptr table
  for(i = 0 ; i < num_box ; i++)
    boxm[i] = &buf[tbl+(num_box*i)];
  boxm[num_box] = NULL;
  memset(&buf[tbl],0xFF,num_box*num_box);
  
  for (i = 0; i < num_box ; i++) {
    while(1) {
      s = scc_fd_r8(fd); len--;
      
      if(s == 0xff) break;
      if(len <= 0 || s > num_box) goto err;

      e = scc_fd_r8(fd); len--;
      if(len <= 0 || e < s || e > num_box) goto err;

      d = scc_fd_r8(fd); len--;
      if(len < 0 || d > num_box) goto err;
      
      for(j = s ; j <= e ; j++) boxm[i][j] = d;

      if(len == 0) break;      
    }
  }

  return boxm;
 
  err:
    printf("Error while reading boxm: %d\n",len);
    free(boxm);
    return NULL;
}


int scc_parse_room(scc_fd_t* fd,scc_room_t* room,int len) {
  uint32_t type,size;
  int pos = 0;
  scc_obim_t *obim,*obim_tail=NULL;
  scc_obcd_t *obcd,*obcd_tail=NULL;
  scc_data_list_t *lscr,*lscr_tail=NULL;
 
  while(pos < len) {
    type = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    pos += size;
    size -= 8;

#if 0
       printf("ROOM block : %c%c%c%c (%d)\n",
	      (uint8_t)(type >> 24),
	      (uint8_t)(type >> 16),
	      (uint8_t)(type >> 8),
	      (uint8_t)(type >> 0),
	      size);
#endif

    switch(type) {
    case MKID('R','M','H','D'):
      if(size != 6) {
	printf("Invalid room header size !!!!\n");
	return 0;
      }
      room->width = scc_fd_r16le(fd);
      room->height = scc_fd_r16le(fd);
      room->num_objs = scc_fd_r16le(fd);
      //printf("Parsed room header (%d)\n",pos);
      break;
    case MKID('C','Y','C','L'):
      room->cycl = scc_parse_cycl(fd,size);
      break;
    case MKID('T','R','N','S'):
      room->transp = scc_fd_r16le(fd);
      break;
    case MKID('P','A','L','S'):
      room->pals = scc_parse_pals(fd,size);
      break;
    case MKID('R','M','I','M'):
      room->rmim = scc_parse_rmim(fd,size);
      break;
    case MKID('O','B','I','M'): {
      obim = scc_parse_obim(fd,size);
      if(!obim) {
	printf("Failed to parse OBIM block\n");
	break;
      }
      if(obim_tail) obim_tail->next = obim;
      else room->obim = obim;
      obim_tail = obim;
    } break;
    case MKID('O','B','C','D'): {
      obcd = scc_parse_obcd(fd,size);
      if(!obcd) {
	printf("Failed to parse OBCD block\n");
	break;
      }
      if(obcd_tail) obcd_tail->next = obcd;
      else room->obcd = obcd;
      obcd_tail = obcd;
    } break;
   case MKID('E','X','C','D'):
     if(room->excd) {
       printf("Warning we alredy have a EXCD\n");
       break;
     }
     room->excd = scc_read_data(fd,size);
     if(!room->excd)
       printf("Warning failed to parse EXCD block\n");
     break;
   case MKID('E','N','C','D'):
     if(room->encd) {
       printf("Warning we alredy have a ENCD\n");
       break;
     }
     room->encd = scc_read_data(fd,size);
     if(!room->encd)
       printf("Warning failed to parse ENCD block\n");
     break;
   case MKID('N','L','S','C'):
     if(room->nlsc) {
       printf("Warning we alredy have a NLSC\n");
       break;
     }
     if(size != 2) {
       printf("Warning invalid NLSC block\n");
       break;
     }
     room->nlsc = scc_fd_r16le(fd);
     break;
    case MKID('L','S','C','R'): {
      lscr = scc_read_data_list(fd,scc_fd_r8(fd),size-1);
      if(!lscr) {
	printf("Warning failed to parse LSCR block\n");
	break;
      }
      if(lscr_tail) lscr_tail->next = lscr;
      else room->lscr = lscr;
      lscr_tail = lscr;
    } break;
    case MKID('B','O','X','D'):
     if(room->boxd) {
       printf("Warning we alredy have a BOXD\n");
       break;
     }
     room->boxd = scc_parse_boxd(fd,size);
     if(!room->boxd)
       printf("Warning failed to parse BOXD block\n");
     break;
    case MKID('B','O','X','M'):
     if(room->boxm) {
       printf("Warning we alredy have a BOXM\n");
       break;
     }
     if(!room->boxd) {
       printf("Error got a BOXM block before the BOXD block.\n");
       break;
     } else {
       int num;
       scc_boxd_t* b;
       for(num = 0, b = room->boxd ; b ; b = b->next) num++;
       room->boxm = scc_parse_boxm(fd,num,size); 
     }
     if(!room->boxm)
       printf("Warning failed to parse BOXM block\n");
     break;
    case MKID('S','C','A','L'):
     if(room->scal) {
       printf("Warning we alredy have a SCAL\n");
       break;
     }
     if(size != 32) printf("SCAL block with non standard size ???\n");
     room->scal = scc_read_data(fd,size);
     if(!room->scal)
       printf("Warning failed to parse SCAL block\n");
     break;
    default:
      printf("Skeeping %c%c%c%c block\n",
	     (uint8_t)(type >> 24),
	     (uint8_t)(type >> 16),
	     (uint8_t)(type >> 8),
	     (uint8_t)(type >> 0));
      scc_fd_seek(fd,size,SEEK_CUR);
      break;
    }
  }

  if(scc_room_size(room) != len)
    printf("ROOM %d size failed (%d/%d)\n",room->idx,scc_room_size(room),len);

  return 1;
}

scc_cost_t* scc_parse_cost(scc_fd_t* fd,int len) {
  uint8_t num_anim;
  uint16_t cmds_off,mask,cmask = 0;
  uint16_t limbs[16],pict_off = 0;
  int i,j,pos = 0,cmd_end;
  uint32_t unk;
  scc_cost_t* cost = calloc(1,sizeof(scc_cost_t));
  scc_cost_anim_t* ani;

  unk = scc_fd_r32le(fd);
  if(scc_fd_r16le(fd) != 0x4f43) printf("Bad CO header ???\n");

  pos += 3*2;
  // number of anims
  num_anim = scc_fd_r8(fd); pos += 1;
  if(unk) { 
    num_anim += 1;
  }
  
  // format, tell palette size
  if(unk)
    cost->flags |= SCC_COST_HAS_SIZE;
  //  cost->size = unk;
  cost->format = scc_fd_r8(fd); pos += 1;
  cost->pal_size = (cost->format & 1) ? 32 : 16;

  // skip palette
  cost->pal = scc_fd_load(fd,cost->pal_size); pos += cost->pal_size;

  cmds_off = scc_fd_r16le(fd); pos += 2;

  // read limbs table
  for(i = 15 ; i >= 0 ; i--)
    limbs[i] = scc_fd_r16le(fd);
  pos += 16*2;

  if(num_anim > 0) {
    uint16_t aoff[num_anim];
    uint16_t alen;

    for(i = 0 ; i < num_anim ; i++) {
      aoff[i] = scc_fd_r16le(fd); pos += 2;
    }

    // compute the global anim mask (ie which limbs are used)
    for(i = 0 ; i < num_anim ; i++) {
      if(!aoff[i]) continue;
      if(pos != aoff[i]) {
	// we migth need to seek back as sometimes several entries in the
	// offset table give the same value.
	scc_fd_seek(fd,aoff[i]-pos,SEEK_CUR); pos = aoff[i];
      }

      mask = scc_fd_r16le(fd); pos += 2;
      cmask |= mask;

    }
    
    if(!cmask) {
      printf("Costume with no limb ????\n");
      scc_fd_seek(fd,len-pos,SEEK_CUR);
      return cost;
    }

    // Read the commands
    // find the first limb (yep it go backward)
    for(i = 15 ; ! ( (cmask & (1<<i)) && limbs[i] ) ; i--);
    cmd_end = limbs[i];
    scc_fd_seek(fd,cmds_off-pos,SEEK_CUR);
    pos = cmds_off;

    cost->cmds_size = cmd_end-cmds_off;
    cost->cmds = scc_fd_load(fd,cost->cmds_size); pos += cost->cmds_size;
    
    // Read the anim definitions
    for(i = 0 ; i < num_anim ; i++) {
      if(!aoff[i]) continue;

      if(pos != aoff[i]) {
	// we migth need to seek back as sometimes several entries in the
	// offset table give the same value.
	scc_fd_seek(fd,aoff[i]-pos,SEEK_CUR); pos = aoff[i];
      }

      ani = scc_cost_new_anim(cost,i);
      if(!ani) continue;

      for(j = 0 ; j < i ; j++) {
	if(aoff[j] == aoff[i]) break;
      }

      if(j < i) {
	ani->redir = j;
     } else {

	ani->mask = mask = scc_fd_r16le(fd); pos += 2;

	for(j = 15 ; j >= 0 ; j--) {
	  
	  if(mask & 0x8000) {
	    ani->limb[j].start = scc_fd_r16le(fd); pos += 2;
	    if(ani->limb[j].start != 0xFFFF) {
	      alen = scc_fd_r8(fd); pos += 1;
	      ani->limb[j].end = ani->limb[j].start + (alen & 0x7F);
	      ani->limb[j].flags = alen>>7;	      
	    }
	  } else
	    ani->limb[j].start = 0xFFFF;

	  mask <<= 1;
	}
      }
    }
  }    

        
  // read the limbs
  for(i = 15 ; i >= 0 ; i--) {
    int limb_size;
    if(!(cmask & (1<<i))) continue;

    // look if it's a duplicate limb
    for(j = 15 ; j > i ; j--) {
      if((cmask & (1<<j)) && limbs[j] == limbs[i]) break;
    }

    // it's a dup, so add a special pic
    if(j > i) {
      scc_cost_pic_t* pic = calloc(1,sizeof(scc_cost_pic_t));
      pic->id = 0xFF;
      pic->redir_limb = j;
      cost->limb_pic[i] = pic;
      continue;
    }

    // Got to the limb table
    scc_fd_seek(fd,limbs[i]-pos,SEEK_CUR);
    pos = limbs[i];

    // Try to find out the table entry size
    for(j = i - 1 ; j >= 0 ; j--) {
      if(cmask & (1<<j) && limbs[j] != limbs[i]) break;
    }

    if(j >= 0)  limb_size = limbs[j]-limbs[i];
    else {
      while(!pict_off) {

	  for(j = 15 ; j >= 0 ; j--) {
	    if((cmask & (1<<j)) && limbs[j]) break;
	  }
	 
	  scc_fd_seek(fd,limbs[j]-pos,SEEK_CUR); pos = limbs[j];
	  while(!pict_off) {
	    pict_off = scc_fd_r16le(fd); pos += 2;
	  }
      }
      limb_size = pict_off-limbs[i];
    }

    if(!(limb_size && limbs[i])) continue;

    // parse the pictures    
    for(j = 0 ; j < limb_size/2 ; j++) {
      uint16_t poff;
      scc_cost_pic_t* pic;
      
      scc_fd_seek(fd,limbs[i]+j*2-pos,SEEK_CUR); pos = limbs[i]+j*2;
      poff = scc_fd_r16le(fd); pos += 2;


      if(poff > 0) {
	//printf("  pict %d %d: 0x%x \n",i,j,poff);      
	scc_fd_seek(fd,poff-pos,SEEK_CUR); pos = poff;
	
	pic = calloc(1,sizeof(scc_cost_pic_t));
	pic->id = j;
	pic->width = scc_fd_r16le(fd); pos += 2;
	pic->height = scc_fd_r16le(fd); pos += 2;
	pic->rel_x = (int16_t)scc_fd_r16le(fd); pos += 2;
	pic->rel_y = (int16_t)scc_fd_r16le(fd); pos += 2;
	pic->move_x = (int16_t)scc_fd_r16le(fd); pos += 2;
	pic->move_y = (int16_t)scc_fd_r16le(fd); pos += 2;
	
	if((cost->format & ~(0x81)) == 0x60) {	  
	  pic->redir_limb = scc_fd_r8(fd); pos++;
	  pic->redir_pic = scc_fd_r8(fd); pos++;	
	} else {
	  pic->redir_limb = pic->redir_pic = 0xFF;
	}

	if(pic->redir_limb == 0xFF && pic->redir_pic == 0xFF) {
	  if(!scc_read_cost_pic(fd,cost,pic,len,&pos))
	    printf("Failed to decode picture %d of limb %d\n",j,i);    
	}
	scc_cost_add_pic(cost,i,pic);
      }
    }
  }
    
  scc_fd_seek(fd,len-pos,SEEK_CUR);

  i = scc_cost_size(cost);
  if(i != len)
    printf("COST size failed (%d/%d)\n",i,len);

  return cost;
}

scc_lfl_t* scc_parse_lfl(scc_fd_t* fd,scc_res_idx_t* idx,int len) {
  uint32_t type,size;
  int pos = 0;
  scc_lfl_t* lfl;
  scc_data_list_t* item;
  scc_data_list_t *scrp_tail=NULL,*soun_tail=NULL;
  scc_data_list_t  *chr_tail=NULL;
  scc_cost_t *cost,*cost_tail=NULL;
  if(len < 8) return NULL;

  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);


  if(type != MKID('R','O','O','M')) {
    printf("LFLF block without ROOM block ???\n");
    return NULL;
  }

  lfl = calloc(1,sizeof(scc_lfl_t));
  lfl->room.idx = scc_res_idx_pos2room(idx,scc_fd_pos(fd)-8);

  pos += size;
  size -= 8;
  //printf("Parsing ROOM %d (%d)\n",lfl->room.idx,size);

  if(!scc_parse_room(fd,&lfl->room,size)) {
    printf("Failed to parse room block.\n");
    free(lfl);
    scc_fd_seek(fd,len-pos,SEEK_CUR);
    return NULL;
  }
 

  while(pos < len) {
    scc_res_list_t* l;
    int i;
    
    type = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    pos += 8;
    size -= 8;

    if(pos+size > len) {
      printf("Too short lflf block ??\n");
      break;
    }

    pos += size;
    if(type == MKID('C','O','S','T')) {
      cost = scc_parse_cost(fd,size);
      if(cost) {
	if(lfl->cost) cost_tail->next = cost;
	else lfl->cost = cost;
	cost_tail = cost;
      }
      continue;
    }

    l = scc_get_res_idx_list(idx,type);
    if(!l) {
      printf("Got unknow block in lfl: %c%c%c%c \n",
	     (uint8_t)(type >> 24),
	     (uint8_t)(type >> 16),
	     (uint8_t)(type >> 8),
	     (uint8_t)(type >> 0));
      scc_fd_seek(fd,size,SEEK_CUR);
      continue;
    }
    i = scc_res_idx_pos2idx(idx,l,lfl->room.idx,pos-8);
    if(!i) {
      printf("Failed to find object index :((\n");
      scc_fd_seek(fd,size,SEEK_CUR);
      continue;
    }

    item = scc_read_data_list(fd,i,size);
    if(!item) break;
    
    switch(type) {
    case MKID('S','C','R','P'):
      if(scrp_tail) scrp_tail->next = item;
      else lfl->scr = item;
      scrp_tail = item;
      break;
    case MKID('S','O','U','N'):
      if(soun_tail) soun_tail->next = item;
      else lfl->snd = item;
      soun_tail = item;
      break;
      //    case MKID('C','O','S','T'):
      //     if(cost_tail) cost_tail->next = item;
      //      else lfl->cost = item;
      //      cost_tail = item;
      //      break;
    case MKID('C','H','A','R'):
      if(chr_tail) chr_tail->next = item;
      else lfl->chr = item;
      chr_tail = item;
      break;
    default:
      printf("Got unknow block in lfl %c%c%c%c  (but type is indexed)\n",
	     (uint8_t)(type >> 24),
	     (uint8_t)(type >> 16),
	     (uint8_t)(type >> 8),
	     (uint8_t)(type >> 0));
	     
      free(item);
      break;
    }
  }

  if(pos != len) scc_fd_seek(fd,len-pos,SEEK_CUR);

  if(scc_lfl_size(lfl) != len)
    printf("LFLF size failed (%d/%d)\n",scc_lfl_size(lfl),len);

  return lfl;
}

int scc_parse_loff(scc_fd_t* fd,scc_res_idx_t* idx,int len) {
  uint8_t num = scc_fd_r8(fd);

  if(len-1 < 5*num) {
    printf("Too short LOFF block ???\n");
    return 0;
  } else {
    struct  __attribute__((__packed__)) {
      uint8_t idx;
      uint32_t off;
    } offs[num];
    uint32_t max_idx = 0;
    int r,w=0;

    while(w < 5*num) {
      r = scc_fd_read(fd,((uint8_t*)offs)+w,5*num-w);
      if(r <= 0) {
	printf("Read error\n");
	return 0;
      }
      w += r;

      for(r = 0 ; r < num ; r++) {
	if(offs[r].idx > max_idx) max_idx = offs[r].idx;
      }
      idx->room_list = calloc(1,sizeof(scc_res_list_t));
      idx->room_list->size = max_idx+1;
      idx->room_list->room_no = calloc(1,max_idx+1);
      idx->room_list->room_off = calloc((max_idx+1),4);
      for(r = 0 ; r < num ; r++) {
	idx->room_list->room_off[offs[r].idx] = offs[r].off;
	idx->room_list->room_no[offs[r].idx] = 1; // disk no ??
      }
      	
    }
    
    if(len != 5*num+1) scc_fd_seek(fd,len-(5*num+1),SEEK_CUR);
    return 1;
  }
  
}

scc_res_t* scc_parse_res(scc_fd_t* fd,scc_res_idx_t* idx) {
  uint32_t type,size,lecf_size;
  int pos = 0;
  scc_res_t* res;
  scc_lfl_t *tail=NULL,*lfl;
  int n = 0;

  scc_fd_seek(fd,0,SEEK_SET);

  type = scc_fd_r32(fd);
  lecf_size = scc_fd_r32be(fd);
  pos += 8;

  if(type !=  MKID('L','E','C','F')) {
    printf("This doesn't look like scumm resource file.\n");
    return NULL;
  }

  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  pos += 8;
  size -= 8;

  if(type != MKID('L','O','F','F')) {
    printf("Missing LOFF block ????\n");
    return NULL;
  }
  printf("The whole file should be %d bytes\n",size);

  res = calloc(1,sizeof(scc_res_t));
  res->fd = fd;
  res->idx = idx;
  
  if(!scc_parse_loff(fd,idx,size)) {
    printf("Failed to parse room index.\n");
    free(res);
    return NULL;
  }
  pos += size;

  while(pos < lecf_size) {
    type = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    pos += 8;
    size -= 8;
    
    if(pos + size > lecf_size) {
      printf("Invalid block length ???\n");
      break;
    }
    pos += size;

    if(type != MKID('L','F','L','F')) {
      printf("Non LFLF block ????\n");
      scc_fd_seek(fd,size,SEEK_CUR);

      continue;
    }
    lfl = scc_parse_lfl(fd,idx,size);
    if(!lfl) {
      printf("Invalid lfl block ???\n");
      continue;
    }
    if(!res->lfl) res->lfl = lfl;
    if(tail) {
      tail->next = lfl;
      lfl->prev = tail;
    }
    tail = lfl;
    n++;
    //if(n > 2) break;
  }

  printf("And we compute a size of %d bytes\n",scc_lecf_size(res) + 8);

  return res;
}

typedef struct scc_res_cont scc_res_cont_t;
struct scc_res_cont {
  scc_res_cont_t* prev;
  uint32_t type,pos,size;
};

void scc_dump_res(scc_res_t* res) {
  uint32_t type,size;
  int pos = 0;
  scc_res_cont_t* stack = NULL;
  int room = -1,idx;
  char path[255];

  scc_fd_seek(res->fd,0,SEEK_SET);
  
  while(1) {
    if(stack && stack->pos + stack->size <= pos) {
      scc_res_cont_t* c = stack->prev;
      printf("%c%c%c%c end\n\n",
	     (uint8_t)(stack->type >> 24),
	     (uint8_t)(stack->type >> 16),
	     (uint8_t)(stack->type >> 8),
	     (uint8_t)(stack->type >> 0));
      if(stack->type == MKID('L','F','L','F')) room = -1;
      free(stack);
      stack = c;
      continue;
    }

    type = scc_fd_r32(res->fd);
    size = scc_fd_r32be(res->fd);
    pos += 8;

    if(type == 0 || size < 8) break;
    size -= 8;

    if(stack && stack->type == MKID('O','B','I','M') &&
       type >> 8 == (('I' << 16) | ('M' << 8)| '0'))
      type = MKID('I','M','0','0');

    switch(type) {
    case MKID('R','O','O','M'):
      room = scc_res_idx_pos2room(res->idx,pos-8);
      if(room < 0)
	printf("Failed to find room number :(\n");
      else {
	printf("Room %d\n",room);
	sprintf(path,"dump/lfl.%3.3d",room);
	check_dir(path);
	sprintf(path,"dump/lfl.%3.3d/room",room);
	check_dir(path);
      }
      //pos += size;
      //scc_fd_seek(res->fd,size,SEEK_CUR);
      //break;

    case MKID('L','F','L','F'):
      

    case MKID('O','B','I','M'):
    case MKID('O','B','C','D'):
    case MKID('I','M','0','0'):
          case MKID('R','M','I','M'):
      //    case MKID('P','A','L','S'):
      //    case MKID('W','R','A','P'):

    case MKID('L','E','C','F'): {
      scc_res_cont_t* c = malloc(sizeof(scc_res_cont_t));
      c->pos = pos;
      c->type = type;
      c->size = size;
      c->prev = stack;
      stack = c;

      printf("Container block %c%c%c%c (%d bytes @ 0x%x)\n",
	     (uint8_t)(type >> 24),
	     (uint8_t)(type >> 16),
	     (uint8_t)(type >> 8),
	     (uint8_t)(type >> 0),
	     size,pos-8);
      
    } break;
    default:
      idx = -1;
      if(room >= 0) {
	scc_res_list_t* l = scc_get_res_idx_list(res->idx,type);
	if(l) {
	  idx = scc_res_idx_pos2idx(res->idx,l,room,pos-8);
	  if(idx < 0)
	    printf("Failed to find object index :((\n");
	  else {
	    printf("Found %c%c%c%c %3.3d (%d bytes)\n",
		   (uint8_t)(type >> 24),
		   (uint8_t)(type >> 16),
		   (uint8_t)(type >> 8),
		   (uint8_t)(type >> 0),
		   idx,
		   size);
	    if(stack && stack->type == MKID('R','O','O','M')) {
	      sprintf(path,"dump/lfl.%3.3d/room/%c%c%c%c.%3.3d",
		      room,
		      (uint8_t)(type >> 24),
		      (uint8_t)(type >> 16),
		      (uint8_t)(type >> 8),
		      (uint8_t)(type >> 0),
		      idx);
	    } else {
	      sprintf(path,"dump/lfl.%3.3d/%c%c%c%c.%3.3d",
		      room,
		      (uint8_t)(type >> 24),
		      (uint8_t)(type >> 16),
		      (uint8_t)(type >> 8),
		      (uint8_t)(type >> 0),
		      idx);
	    }
	    if(!scc_fd_dump(res->fd,path,size)) {
	      scc_fd_seek(res->fd,pos,SEEK_SET);
	      printf("Dump failed :((\n");
	    } else {
	      pos += size;
	      break;
	    }
	  }
	} else {
	  printf("Found item %c%c%c%c (%d bytes)\n",
		 (uint8_t)(type >> 24),
		 (uint8_t)(type >> 16),
		 (uint8_t)(type >> 8),
		 (uint8_t)(type >> 0),
		 size);
	  //printf("Failed to find the list for this obj type\n");
	  
	}
      } else 
	printf("Found item %c%c%c%c (%d bytes)\n",
	       (uint8_t)(type >> 24),
	       (uint8_t)(type >> 16),
	       (uint8_t)(type >> 8),
	       (uint8_t)(type >> 0),
	       size);
      pos += size;
      scc_fd_seek(res->fd,size,SEEK_CUR);
    }
  }
      
}



int scc_res_find(scc_res_t* res,uint32_t type,uint32_t idx) {
  scc_res_list_t* list;
  uint32_t room,off,t2,size;

  list = scc_get_res_idx_list(res->idx,type);
  if(!list) return -1;

  if(idx >= list->size) return -1;

  room = list->room_no[idx];

  if(room == 0) {
    printf("TODO, sorry ;)\n");
    return -1;
  } else if(room >= res->idx->room_list->size) {
    fprintf(stderr,"Invalid room number\n");
    return -1;
  }

  if(type == MKID('D','R','O','O'))
    off = 0;
  else
    off = list->room_off[idx];

  off += res->idx->room_list->room_off[room];

  fprintf(stderr,"Seeking to 0x%x\n",off);
  scc_fd_seek(res->fd,off,SEEK_SET);

  t2 = scc_fd_r32(res->fd);
  if(t2 != type) {
    fprintf(stderr,"Oups we didn't found the awaited data: 0x%x\n",t2);
    return -1;
  }
  size = scc_fd_r32be(res->fd);
  if(size < 8) return -1;
  return size - 8;
}

