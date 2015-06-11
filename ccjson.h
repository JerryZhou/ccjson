/*
  Copyright (c) 2015 JerryZhou (JerryZhou@outlook.com)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef cjson_ccjson_h
#define cjson_ccjson_h

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif
    
// 一个类型最多成员个数
#define CCMaxMemberCount 64 
// 结构体类型的最多格式
#define CCMaxTypeCount 1000

// ******************************************************************************
// 获取当前系统的纳秒数
int64_t ccgetcurnano();
// 获取当前系统的毫秒数
int64_t ccgetcurtick();
// 获取系统的下一个唯一的事件纳秒数
int64_t ccgetnextnano();

// ******************************************************************************
// 设置内存缓冲区，并返回设置前的状态
bool cc_enablememorycache(bool enable); 
// 返回当前使用的内存总数
size_t cc_mem_size();
//  申请文本缓存区域, 以0结尾, 返回的字符串需要调用 cc_free
char *cc_alloc(size_t size);
// 释放文本缓冲区
void cc_free(char *c);
// 文本缓冲区的长度, 不包括结尾的0
size_t cc_len(char *c); 
// 复制一份字符串, 返回的字符串需要调用 cc_free
char *cc_dup(const char* src);
// 打印内存当前状况, 并返回当前持有内存的总数
size_t cc_mem_state();
// 打印内存缓冲区当前状况，并返回当前缓冲区持有的内存总数
size_t cc_mem_cache_state();
// 索引所在缓冲区域的大小 
size_t cc_mem_cache_current(int index);
// 索引所在缓冲区域的容量
size_t cc_mem_cache_capacity(int index);
// 设置缓冲区的容量
void cc_mem_cache_setcapacity(int index, size_t capacity); 
// 清理指定大小的缓冲区
void cc_mem_cache_clearof(int index);
// 清理缓冲区
void cc_mem_cache_clear(); 



// ******************************************************************************
// 读取文本文件 , 返回的字符串需要调用 cc_free
char * cc_read_file(const char* fn);
// 写文件, content 必须是 cc_alloc, cc_dup, cc_read_file 返回的缓冲区
size_t cc_write_file(char* content, const char* fn);

// ******************************************************************************
// 类型的元信息
struct cctypemeta;
// 成员的元信息
struct ccmembermeta;

// 初始化函数
typedef int (*cctypemeta_init)( struct cctypemeta *meta);

// 类型编码
typedef struct cctypemeta {
    const char *type;
    size_t size;
    int index;
    
    int membercount;
    void *members;
    struct ccmembermeta **indexmembers;
    cctypemeta_init init;
}cctypemeta;

// 组合标志
typedef enum enumflagcompose {
    enumflagcompose_array = 1,
    enumflagcompose_point = 2,
}enumflagcompose;

// 成员的编码
typedef struct ccmembermeta {
    const char *name;
    int compose;    // 1 array
    int idx;        // index of member
    int offset;
    cctypemeta *type;
}ccmembermeta;

// 基础对象的标志位
typedef enum enumflagccjsonobj {
    enumflagccjsonobj_null = 1,
}enumflagccjsonobj;

#define _ccjson_obj(n) int __index; int __flag; char __has[2*((n+7)/8)]

// 基础的 cjson_obj
typedef struct ccjson_obj {
    int __index;
    int __flag;
    char __has[];
}ccjson_obj;

// 数组操作
// n : 数组个数
// size : 每个元素的大小
// index : meta 的索引
void * ccarraymalloc(size_t n, size_t size, int index);
void * ccarraymallocof(size_t n, cctypemeta* meta);
cctypemeta* ccarraymeta(void *array);
ccjson_obj* ccarrayobj(void *array);
void ccarrayfree(void *array);
size_t ccarraylen(void *array);

// 获取一个成员的索引
int ccobjmindex(struct cctypemeta *meta, const char* member);
// 获取一个类型给定索引的成员元数据
struct ccmembermeta *ccobjmmetabyindex(struct cctypemeta *meta, int index);
// 获取成员个数
int ccobjmcount(struct cctypemeta *meta);

// 结构体成员操作
bool ccobjhas(void *p, int index);
void ccobjset(void *p, int index);
void ccobjunset(void *p, int index);

// 结构体成员为空的操作
bool ccobjisnull(void *p, int index);
void ccobjsetnull(void *p, int index);
void ccobjunsetnull(void *p, int index);

// 结构体成员操作
bool ccarrayhas(void *p, int index);
void ccarrayset(void *p, int index);
void ccarrayunset(void *p, int index);

// 结构体成员为空的操作
bool ccarrayisnull(void *p, int index);
void ccarraysetnull(void *p, int index);
void ccarrayunsetnull(void *p, int index);

// 设置对象是否是Null
void ccobjnullset(void *p, bool isnull);
// 判断对象是否是Null
bool ccobjnullis(void *p);

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

// 释放结构体相关资源: 有调用过ccparsefrom 的都需要调用这个函数来释放资源
void ccobjrelease(cctypemeta *meta, void *p);
// 释放数组相关资源
void ccobjreleasearray(cctypemeta* meta, void *value);

// 从字符串解析
bool ccparsefrom(cctypemeta *meta, void *value, const char *json);

// 把对象解析到字符串： 输出的字符串自己负责释放 free()
char *ccunparseto(cctypemeta *meta, void *value);


// ******************************************************************************
// 辅助函数: 生成一个可以用来进行序列化的对象, 然后可以调用ccobjfree 释放内存
// 生成对象
void *ccjsonobjalloc(cctypemeta* meta);

// 释放对象拥有的资源
void ccjsonobjrelease(void *p);

// 释放对象
void ccjsonobjfree(void *p);

// 从json序列化对象
bool ccjsonobjparsefrom(void *p, const char* json);

// 把对象序列化到json
char* ccjsonobjunparseto(void *p);

// 对象申请宏
#define iccalloc(type) ((type*)ccjsonobjalloc(&cctypeofmeta(type)))
// 对象资源释放
#define iccrelease(p) ccjsonobjrelease(p)
// 对象释放宏
#define iccfree(p) ccjsonobjfree(p)
// json转对象
#define iccparse(p, json) ccjsonobjparsefrom(p, json)
// 对象转json
#define iccunparse(p) ccjsonobjunparseto(p) 

// ******************************************************************************
// 辅助宏：用来创建各种元信息
#define cctypeofname(type) __cc_name_##type
#define cctypeofmeta(type) __cc_meta_##type
#define cctypeofmindex(type, member) __cc_index_m_##type##_mmm_##member
#define cctypeofmcount(type) __cc_index_max_##type

// 声明成员索引
#define __ccdeclareindexbegin(type) typedef enum __cc_index_##type {
#define __ccdeclareindexmember(type, mtype, member) cctypeofmindex(type, member),
#define __ccdeclareindexmember_array(type, mtype, member) cctypeofmindex(type, member),
#define __ccdeclareindexmember_point(type, mtype, member) cctypeofmindex(type, member),
#define __ccdeclareindexend(type) cctypeofmcount(type) } __cc_index_##type ;

// 声明元信息
#define __ccdeclaretype(mtype) \
    extern const int cctypeofmcount(mtype);\
    extern const char* cctypeofname(mtype); \
    extern struct cctypemeta cctypeofmeta(mtype);

// 实现元信息
#define __ccimplementtype(mtype) \
    const int cctypeofmcount(mtype) = 0;\
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
        .membercount = cctypeofmcount(mtype), \
        .indexmembers= NULL,\
        .index=0, \
        .init=__cc_init_##mtype\
    };

// 复杂类型
#define __ccdeclaretypebegin(mtype) \
    typedef struct mtype { _ccjson_obj(cctypeofmcount(mtype));

// 声明成员
#define __ccdeclaremember(mtype, htype, m) \
    htype m;

// 声明数组成员
#define __ccdeclaremember_array(mtype, htype, m) \
    htype* m;

// 声明指针成员
#define __ccdeclaremember_point(mtype, htype, m) \
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

// 结束有成员变量的类型
#define __ccimplementtypeend(mtype) \
        return meta->index;\
    }\
    struct cctypemeta cctypeofmeta(mtype) =  { \
        .type=#mtype, \
        .size=sizeof(mtype), \
        .members=NULL, \
        .membercount = cctypeofmcount(mtype), \
        .indexmembers= NULL,\
        .index=0, \
        .init=__cc_init_##mtype\
    };

// 实现成员类型
#define __ccimplementmember(mtype, ntype, member)  do {\
    ccmembermeta *_member = ccmakememberwithmeta(#member, &cctypeofmeta(ntype), \
                        offsetof(mtype, member), cctypeofmindex(mtype, member), 0); \
    ccaddmember(meta, _member); } while(0);

// 实现成员数组类型
#define __ccimplementmember_array(mtype,  ntype, member)  do {\
    ccmembermeta *_member = ccmakememberwithmeta(#member, &cctypeofmeta(ntype), \
                        offsetof(mtype, member), cctypeofmindex(mtype, member), enumflagcompose_array); \
    ccaddmember(meta, _member); } while(0);

// 实现成员数组类型
#define __ccimplementmember_point(mtype,  ntype, member)  do {\
    ccmembermeta *_member = ccmakememberwithmeta(#member, &cctypeofmeta(ntype), \
                        offsetof(mtype, member), cctypeofmindex(mtype, member), enumflagcompose_point); \
    ccaddmember(meta, _member); } while(0);
    
// 实现基础对象
// ******************************************************************************
typedef char* ccstring;
typedef bool ccbool;
typedef double ccnumber;
typedef int ccint;
typedef int64_t ccint64;

// 声明基础类型
__ccdeclaretype(ccint)
__ccdeclaretype(ccint64)
__ccdeclaretype(ccnumber)
__ccdeclaretype(ccstring)
__ccdeclaretype(ccbool)

// 声明类型索引
__ccdeclareindexbegin(ccconfig)
__ccdeclareindexmember(ccconfig, ccint, ver)
__ccdeclareindexmember(ccconfig, ccint64, ver64)
__ccdeclareindexmember(ccconfig, ccbool, has)
__ccdeclareindexmember(ccconfig, ccstring, detail)
__ccdeclareindexmember_array(ccconfig, ccint, skips)
__ccdeclareindexend(ccconfig)

// 声明复杂类型
__ccdeclaretypebegin(ccconfig)
__ccdeclaremember(ccconfig, ccint, ver)
__ccdeclaremember(ccconfig, ccint64, ver64)
__ccdeclaremember(ccconfig, ccbool, has)
__ccdeclaremember(ccconfig, ccstring, detail)
__ccdeclaremember_array(ccconfig, ccint, skips)
__ccdeclaretypeend(ccconfig)
    
/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif


#endif
