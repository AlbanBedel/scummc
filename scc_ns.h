
// SCC name space implementation
typedef struct scc_ns_st {
  // global symbol tree
  scc_symbol_t *glob_sym;
  // current start point in the tree.
  // NULL is global
  scc_symbol_t *cur;
  // this is used for rid allocation
  uint16_t rids[SCC_RES_LAST];
  // this represent the address spaces
  uint8_t as[SCC_RES_LAST][8192];
} scc_ns_t;

scc_ns_t* scc_ns_new(void);

void scc_ns_free(scc_ns_t* ns);

scc_symbol_t* scc_ns_get_sym(scc_ns_t* ns, char* room, char* sym);

scc_symbol_t* scc_ns_get_sym_with_id(scc_ns_t* ns,int type, int id);

scc_symbol_t* scc_ns_add_sym(scc_ns_t* ns, char* sym, int type, int subtype,
			     int addr,char status);

scc_symbol_t* scc_ns_decl(scc_ns_t* ns, char* room, char* sym, 
			  int type, int subtype, int addr);

int scc_ns_get_rid(scc_ns_t* ns, scc_symbol_t* s);

int scc_ns_push(scc_ns_t* ns, scc_symbol_t* s);

int scc_ns_pop(scc_ns_t* ns);

int scc_ns_set_sym_addr(scc_ns_t* ns, scc_symbol_t* s,int addr);

int scc_ns_alloc_addr(scc_ns_t* ns);

int scc_ns_get_addr_from(scc_ns_t* ns,scc_ns_t* from);

int scc_ns_res_max(scc_ns_t* ns,int type);

int scc_ns_is_addr_alloc(scc_ns_t* ns,int type,int addr);

scc_symbol_t* scc_ns_get_sym_at(scc_ns_t* ns,int type,int addr);

