
#define SCC_PARAM_FLAG 1
#define SCC_PARAM_INT  2
#define SCC_PARAM_STR  3
#define SCC_PARAM_DBL  4

#define SCC_PARAM_TYPE_NO_ARG 1

#define SCC_PARAM_UNKNOWN      -1
#define SCC_PARAM_NEED_ARG     -2
#define SCC_PARAM_INVALID      -3
#define SCC_PARAM_OUT_OF_RANGE -4

typedef struct scc_param_st {
  char* name;
  int type;
  int min,max;
  void* ptr;
} scc_param_t;

typedef struct scc_cl_arg_st scc_cl_arg_t;
struct scc_cl_arg_st {
  scc_cl_arg_t* next;
  char* val;
};

int scc_param_parse(scc_param_t* params,char* k,char* v);

scc_cl_arg_t* scc_param_parse_argv(scc_param_t* params,int argc,char** argv);
