

INCLUDE = 
CFLAGS  = -g -Wall

GTK_CFLAGS=`pkg-config gtk+-2.0  --cflags`
GTK_LDFLAGS=`pkg-config gtk+-2.0  --libs`

## Newer readline need ncurse
LDFLAGS = 

.SUFFIXES: .c .o

BASE = code.c  decode.c  read.c write.c scc_util.c scc_fd.c scc_cost.c

RD_SRCS = $(BASE) rd.c
RD_OBJS = $(RD_SRCS:.c=.o)

COSTVIEW_SRCS = $(BASE) costview.c scc_param.c
COSTVIEW_OBJS = $(COSTVIEW_SRCS:.c=.o)

# .PHONY: all clean

all: scc sld zpnn2bmp costview boxedit cost

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

costview.o: costview.c
	$(CC) -c $(CFLAGS) $(GTK_CFLAGS) -o $@ $<

costview: $(COSTVIEW_OBJS)
	$(CC) $(CFLAGS) $(COSTVIEW_OBJS) -o costview $(LDFLAGS) $(GTK_LDFLAGS)

rd: $(RD_OBJS)
	$(CC) $(CFLAGS) $(RD_OBJS) -o rd $(LDFLAGS)

scc_parse.tab.c: scc_parse.y scc_func.h
	bison -v scc_parse.y

scc_parse.tab.h: scc_parse.y
	bison -v scc_parse.y

scc_lex.c: scc_lex.y scc_parse.tab.h
	lex -oscc_lex.c scc_lex.y

PARSER_SRCS= scc_parse.tab.c scc_util.c scc_ns.c scc_roobj.c scc_img.c scc_code.c code.c write.c scc_lex.c scc_fd.c scc_param.c
PARSER_OBJS = $(PARSER_SRCS:.c=.o)


scc: $(PARSER_OBJS)
	gcc -o scc $(PARSER_OBJS)

LINKER_SCRS= scc_ld.c scc_ns.c scc_code.c scc_fd.c scc_param.c
LINKER_OBJS= $(LINKER_SCRS:.c=.o)

sld: $(LINKER_OBJS)
	gcc -o sld $(LINKER_OBJS)

ZPNN2BMP_SCRS= zpnn2bmp.c scc_fd.c scc_param.c decode.c scc_img.c
ZPNN2BMP_OBJS= $(ZPNN2BMP_SCRS:.c=.o)

zpnn2bmp: $(ZPNN2BMP_OBJS)
	gcc -o zpnn2bmp $(ZPNN2BMP_OBJS)

BE_SCRS= read.c write.c scc_fd.c scc_cost.c scc_img.c scc_param.c boxedit.c
BE_OBJS= $(BE_SCRS:.c=.o)

boxedit.o: boxedit.c
	$(CC) -c $(CFLAGS) $(GTK_CFLAGS) -o $@ $<

boxedit: $(BE_OBJS)
	gcc -o boxedit $(BE_OBJS)  $(LDFLAGS) $(GTK_LDFLAGS)

imgtest: scc_img.c scc_fd.c
	$(CC) $(CFLAGS) -DSCC_IMG_TEST  -o $@ scc_img.c scc_fd.c


cost_parse.tab.c: cost_parse.y
	bison -v cost_parse.y

cost_parse.tab.h: cost_parse.y
	bison -v cost_parse.y

cost_lex.c: cost_lex.y cost_parse.tab.h
	lex -ocost_lex.c cost_lex.y

COST_SRCS= cost_lex.c cost_parse.tab.c scc_fd.c scc_param.c scc_img.c
COST_OBJS=$(COST_SRCS:.c=.o)

cost: $(COST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(COST_OBJS)


clean:
	rm -f *.o *.a *~ *.tab.[ch] scc_lex.c cost_lex.c

distclean: clean
	rm -f Makefile.bak .depend

dep:    depend

depend: $(PARSER_SRCS) $(LINKER_SCRS) $(ZPNN2BMP_SCRS) $(BE_SCRS) $(COST_SRCS)
	$(CC) -MM $(CFLAGS) $(GTK_CFLAGS) $(PARSER_SRCS) $(LINKER_SCRS) \
	      $(ZPNN2BMP_SCRS) $(BE_SCRS) $(COST_SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
