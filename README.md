# 解析性能
    如单元测试里面的简单压测所示：
    解析 10000 Json Obj Take 36682 nanos(0.036682s)
    平均每秒钟可以解析30W的JSON对象

# 使用方法
    拷贝 ccjson.h ccjson.c 
    到自己的工程里面就可以用了

# 建议集中存放自己需要序列化的结构体的定义
    放到一个.inl 文件里面，定义协议结构体
    然后顺便把结构体的定义和实现合并了，并定义好成员索引

    具体做法参考 : ccjsonstruct.h ccjsonstruct.c ccjsonstruct.inl

# 具体用法如下：

### 定义结构体
    __cc_type_begin(cstruct)
        __cc_type_member(cstruct, ccstring, str) 
        __cc_type_member(cstruct, int, i) 
        __cc_type_member_array(cstruct, int, array) 
    __cc_type_end(cstruct)

### 把JSON 解析到结构体
    const char* json = "{\"str\":\"hello\", \"i\":123, \"array\":[1, 2, 3]}";
    cstruct c = {0}
    ccparsefrom(&cctypeofmeta(cstruct), &c, json);
    ccobjrelease(&c);

### 把结构体打印成JSON
    cstruct c = {0}
    c.str = (char*)"hello";    
    ccobjset(&c, cctypeofmindex(cstruct, str));

    c.i = 2;
    ccobjset(&c, cctypeofmindex(cstruct, i));

    char *json = ccunparseto(&cctypeofmeta(cstruct), &c);
    cc_free(json)

### 封装的api: iccalloc, iccrelease, iccfree, iccparse, iccunparse 的用法
    // 申请对象
    cstruct *c = iccalloc(cstruct);
    // 从JSON 初始化对象
    iccparse(c, "{"\"i\":123}");
    // 释放对象
    iccfree(c);

# 不支持的特性
    不支持异构类型的数组，数组的成员类型必须一致{ "a":[1, "string", false]}
    不支持数组的数组 {"a"=[1, 2, [1, 3]]}
    不支持基础对象的Null (bool, number, string 不支持 null)
