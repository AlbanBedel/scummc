
// code.c
   
// create an smap out from a bitmap. It simply try every codec for each
// stripe and take the best one. Note that in the doot data at least some
// room aren't using the best encoding.

int scc_code_image(uint8_t* src, int src_stride,
                   int width,int height,int transparentColor,
                   uint8_t** smap_p);

int scc_code_zbuf(uint8_t* src, int src_stride,
                  int width,int height,
                  uint8_t** smap_p);

// decode.c

int scc_decode_image(uint8_t* dst, int dst_stride,
                     int width,int height,
                     uint8_t* smap,uint32_t smap_size,
                     int transparentColor);

int scc_decode_zbuf(uint8_t* dst, int dst_stride,
                    int width,int height,
                    uint8_t* zmap,uint32_t zmap_size,int or);

