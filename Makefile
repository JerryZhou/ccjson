
all=ccjson

.PHONY:main
main:$(all)

OBJS=main.o ccjson.o cjsonstruct.o

ccjson: $(OBJS)
	cc $(OBJS) -o ccjson

main.o: ccjson.h ccjson.c ccjsontest.h cjsonstruct.inl cjsonstruct.h cjsonstruct.c

cjsonstruct.o: cjsonstruct.h cjsonstruct.inl cjsonstruct.c

.PHONY:clean
clean:
	rm $(OBJS) $(all)
