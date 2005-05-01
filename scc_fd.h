
typedef struct scc_fd {
  int fd;
  uint8_t enckey;
  char* filename;
} scc_fd_t;

scc_fd_t* new_scc_fd(char* path,int flags,uint8_t key);

int scc_fd_close(scc_fd_t* f);

int scc_fd_read(scc_fd_t* f,void *buf, size_t count);

uint8_t* scc_fd_load(scc_fd_t* f,size_t count);

int scc_fd_dump(scc_fd_t* f,char* path,int size);

off_t scc_fd_seek(scc_fd_t* f, off_t offset, int whence);

off_t scc_fd_pos(scc_fd_t* f);

uint8_t scc_fd_r8(scc_fd_t* f);

uint16_t scc_fd_r16le(scc_fd_t* f);
uint32_t scc_fd_r32le(scc_fd_t* f);

uint16_t scc_fd_r16be(scc_fd_t* f);
uint32_t scc_fd_r32be(scc_fd_t* f);

int scc_fd_write(scc_fd_t* f,void *buf, size_t count);

int scc_fd_w8(scc_fd_t*f,uint8_t a);

int scc_fd_w16le(scc_fd_t*f,uint16_t a);
int scc_fd_w32le(scc_fd_t*f,uint32_t a);

int scc_fd_w16be(scc_fd_t*f,uint16_t a);
int scc_fd_w32be(scc_fd_t*f,uint32_t a);

#ifndef BIG_ENDIAN
#define scc_fd_r16(f) scc_fd_r16le(f)
#define scc_fd_r32(f) scc_fd_r32le(f)
#define scc_fd_w16(f,a) scc_fd_w16le(f,a)
#define scc_fd_w32(f,a) scc_fd_w32le(f,a)
#else
#define scc_fd_r16(f) scc_fd_r16be(f)
#define scc_fd_r32(f) scc_fd_r32be(f)
#define scc_fd_w16(f,a) scc_fd_w16be(f,a)
#define scc_fd_w32(f,a) scc_fd_w32be(f,a)
#endif
