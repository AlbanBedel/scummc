
typedef struct scc_char_st {
  uint8_t w,h;
  int8_t  x,y;
  uint8_t* data;
} scc_char_t;


#define SCC_MAX_CHAR 8192

typedef struct scc_charmap_st {
  uint8_t    pal[15];             // palette used
  uint8_t    bpp;                 // bpp used for coding
  uint8_t    height;              // font height
  uint8_t    width;               // max width
  uint16_t   max_char;            // index of the last good char
  scc_char_t chars[SCC_MAX_CHAR];
} scc_charmap_t;

