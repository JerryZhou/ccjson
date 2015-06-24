
all=ccjson

.PHONY:main
main:$(all)

OBJS=main.o ccjson.o ccjsonstruct.o

ccjson: $(OBJS)
	cc $(OBJS) -o ccjson -fno-exceptions -fno-rtti

main.o: ccjson.h ccjson.c ccjsontest.h ccjsonstruct.inl ccjsonstruct.h ccjsonstruct.c

ccjsonstruct.o: ccjsonstruct.h ccjsonstruct.inl ccjsonstruct.c

.PHONY:clean
clean:
	rm $(OBJS) $(all)
