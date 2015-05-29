//
//  ccjson.h
//  cjson
//
//  Created by JerryZhou on 15/5/28.
//  Copyright (c) 2015年 JerryZhou. All rights reserved.
//

#ifndef cjson_ccjson_h
#define cjson_ccjson_h

#include <stdlib.h>
#include <stddef.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif
    

// 结构体类型的最多格式
#define CCMaxTypeCount 1000

// ******************************************************************************
// 类型的元信息
struct cctypemeta;
// 成员的元信息
struct ccmembermeta;
// 字典
struct dict;
// json 中间键
struct cJSON;

// 初始化函数
typedef int (*cctypemeta_init)( struct cctypemeta *meta);

// 类型编码
typedef struct cctypemeta {
    const char *type;
    size_t size;
    int index;
    
    struct dict *members;
    cctypemeta_init init;
}cctypemeta;

// 成员的编码
typedef struct ccmembermeta {
    const char *name;
    int compose;    // 1 array
    int idx;        // index of member
    int offset;
    cctypemeta *type;
}ccmembermeta;

#define _ccjson_obj(n) int __index; char __has[(n+7)/8]

// 基础的 cjson_obj
typedef struct ccjson_obj {
    int __index;
    char __has[];
}ccjson_obj;

// 数组操作
void * ccarraymalloc(size_t n, size_t size);
void ccarrayfree(void *array);
size_t ccarraylen(void *array);

// 获取一个成员的索引
int ccobjmindex(struct cctypemeta *meta, const char* member);

// 结构体成员操作
bool ccobjhas(void *p, int index);
void ccobjset(void *p, int index);
void ccobjunset(void *p, int index);

/**
 * 造一个成员
 * name : 成员名字
 * type : 成员类型
 * offset : 成员在类型中的偏移
 * index : 成员的索引
 * compose : 是否是数组
 * TODO: 我们可以增加集中组合类型， list, map 等
 */
ccmembermeta *ccmakemember(const char* name,
                           const char* type, int offset,
                           int index,
                           int compose);
ccmembermeta *ccmakememberwithmeta(const char* name,
                           cctypemeta* type, int offset,
                           int index,
                           int compose);

// 增加一项成员
void ccaddmember(cctypemeta *meta, ccmembermeta *member);

// 创建一个类型元信息
cctypemeta *ccmaketypemeta(const char* type, size_t size);
    
// 注册一个元信息
int ccaddtypemeta(cctypemeta *meta);

// 初始化一个元信息: 注册进去，调用设置好的成员初始化, 返回meta 的索引
int ccinittypemeta(cctypemeta *meta);

// 查找元信息
cctypemeta *ccgettypemeta(const char* type);
cctypemeta *ccgettypemetaof(int index);

// 解析
bool ccparse(cctypemeta *meta, void *value, struct cJSON *json);
// 反解析
struct cJSON *ccunparse(cctypemeta *meta, void *value);

// 释放结构体相关资源
void ccobjrelease(cctypemeta *meta, void *p);

// 从字符串解析
bool ccparsefrom(cctypemeta *meta, void *value, const char *json);

// 把对象解析到字符串： 输出的字符串自己负责释放 free()
char *ccunparseto(cctypemeta *meta, void *value);

// ******************************************************************************
// 辅助宏：用来创建各种元信息
#define cctypeofname(type) __cc_name_##type
#define cctypeofmeta(type) __cc_meta_##type
#define cctypeofmindex(type, mtype) __cc_index_##type##_##mtype

// 声明元信息
#define __ccdeclaretype(mtype) \
    extern const char* cctypeofname(mtype); \
    extern struct cctypemeta cctypeofmeta(mtype); \

// 实现元信息
#define __ccimplementtype(mtype) \
    const char* cctypeofname(mtype) = #mtype; \
    static int __cc_init_##mtype(cctypemeta * meta) {\
        if(meta->index) { \
            return meta->index; \
        }\
        meta->index = ccaddtypemeta(meta); \
        return meta->index;\
    }\
    struct cctypemeta cctypeofmeta(mtype) =  { \
        .type=#mtype, \
        .size=sizeof(mtype), \
        .members=NULL, \
        .index=0, \
        .init=__cc_init_##mtype\
    }

// 复杂类型
#define __ccdeclaretypebegin(mtype) \
    typedef struct mtype { _ccjson_obj(64);

// 声明成员
#define __ccdeclaremember(mtype, htype, m) \
    htype m;

// 声明数组成员
#define __ccdeclaremember_array(mtype, htype, m) \
    htype* m;

#define __ccdeclaretypeend(mtype) \
    } mtype; \
    extern const char* cctypeofname(mtype); \
    extern struct cctypemeta cctypeofmeta(mtype); \

// 实现有成员变量的类型
#define __ccimplementtypebegin(mtype) \
    const char* cctypeofname(mtype) = #mtype; \
    static int __cc_init_##mtype(cctypemeta * meta) {\
        if(meta->index) { \
            return meta->index; \
        }\
        meta->index = ccaddtypemeta(meta); \
        int _midx = 0; \

// 结束有成员变量的类型
#define __ccimplementtypeend(mtype) \
        return meta->index;\
    }\
    struct cctypemeta cctypeofmeta(mtype) =  { \
        .type=#mtype, \
        .size=sizeof(mtype), \
        .members=NULL, \
        .index=0, \
        .init=__cc_init_##mtype\
    }

// 实现成员类型
#define __ccimplementmember(mtype, ntype, member)  do {\
    ccmembermeta *_member = ccmakememberwithmeta(#member, &cctypeofmeta(ntype), \
                        offsetof(mtype, member), _midx++, 0); \
    ccaddmember(meta, _member); } while(0)

// 实现成员数组类型
#define __ccimplementmember_array(mtype,  ntype, member)  do {\
    ccmembermeta *_member = ccmakememberwithmeta(#member, &cctypeofmeta(ntype), \
                        offsetof(mtype, member), _midx++, 1); \
    ccaddmember(meta, _member); } while(0)

    
// 实现基础对象
// ******************************************************************************
typedef char* ccstring;
typedef bool ccbool;
typedef double ccnumber;
typedef int ccint;

// 声明基础类型
__ccdeclaretype(ccint);
__ccdeclaretype(ccnumber);
__ccdeclaretype(ccstring);
__ccdeclaretype(ccbool);

// 声明复杂类型
__ccdeclaretypebegin(ccconfig);
__ccdeclaremember(ccconfig, ccint, ver);
__ccdeclaremember(ccconfig, ccbool, has);
__ccdeclaremember(ccconfig, ccstring, detail);
__ccdeclaremember_array(ccconfig, ccint, skips);
__ccdeclaretypeend(ccconfig);
    
/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif


#endif
