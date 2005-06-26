
/*
 * A little lib to open image files. We are only intersted in
 * paletted format atm. Currently only uncompressed bmp is supported.
 *
 */

typedef struct scc_img {
  unsigned int w,h;
  unsigned int ncol,trans;
  uint8_t* pal;
  uint8_t* data;
} scc_img_t;

// create a new empty image of the given size
scc_img_t* scc_img_new(int w,int h,int ncol);

void scc_img_free(scc_img_t* img);

int scc_img_write_bmp(scc_img_t* img,scc_fd_t* fd);

int scc_img_save_bmp(scc_img_t* img,char* path);

scc_img_t* scc_img_open(char* path);
