# 使用方法
    拷贝 cJSON.h cJSON.c dict.h dict.c ccjson.h ccjson.c 
    到自己的工程里面就可以用了

# 建议集中存放自己需要序列化的结构体的定义
    放到一个.inl 文件里面，定义协议结构体
    然后顺便把结构体的定义和实现合并了，并定义好成员索引

    具体做法参考 : cjsonstruct.h cjsonstruct.c cjsonstruct.inl



