

scc_code_t* scc_code_new(int len);

void scc_code_free(scc_code_t* c);

void scc_code_free_all(scc_code_t* c);

scc_script_t* scc_script_new(scc_ns_t* ns, scc_instruct_t* inst);

void scc_script_free(scc_script_t* scr);

void scc_script_list_free(scc_script_t* scr);
