
all=ccjson

.PHONY:main
main:$(all)

OBJS=main.o dict.o cJSON.o ccjson.o

ccjson: $(OBJS)
	cc $(OBJS) -o ccjson

main.o: dict.h cJSON.h dict.c cJSON.c ccjson.h ccjson.c ccjsontest.h

.PHONY:clean
clean:
	rm $(OBJS) $(all)
