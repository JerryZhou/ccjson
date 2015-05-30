
all=ccjson

.PHONY:main
main:$(all)

OBJS=main.o dict.o cJSON.o ccjson.o cjsonstruct.o

ccjson: $(OBJS)
	cc $(OBJS) -o ccjson

main.o: dict.h cJSON.h dict.c cJSON.c ccjson.h ccjson.c ccjsontest.h cjsonstruct.inl cjsonstruct.h cjsonstruct.c

cjsonstruct.o: cjsonstruct.h cjsonstruct.inl cjsonstruct.c

ccjson.o: ccjson.h cJSON.h cJSON.c dict.h dict.c

.PHONY:clean
clean:
	rm $(OBJS) $(all)
