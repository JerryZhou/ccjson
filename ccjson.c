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

#include <stdio.h>
#include <memory.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ccjson.h"

#include "dict.h"
#include "cJSON.h"

#define cccheck(exp) do { if(!(exp)) { return ; } } while(0)
#define cccheckret(exp, ret) do { if(!(exp)) { return ret; }} while(0)
#define __ccmalloc(type) (type*)calloc(1, sizeof(type));

// ******************************************************************************
// 文本缓冲区
typedef struct __cc_content {
    size_t size;
    char content[];
}__cc_content;

//  申请文本缓存区域
char *cc_alloc(size_t size) {
    __cc_content *content = (__cc_content*)malloc(size + sizeof(__cc_content) + 1);
    content->size = size;
    content->content[size] = 0;
    return content->content;
}

// 释放文本缓冲区
void cc_free(char *c) {
    cccheck(c);
    __cc_content *content = (__cc_content*)(c - sizeof(__cc_content));
    free(content);
}

// 文本长度
size_t cc_len(char *c) {
    cccheckret(c, 0);
    __cc_content *content = (__cc_content*)(c - sizeof(__cc_content));
    return content->size;
}

// 生成一个填满字符缓冲区
char *cc_dup(const char* src) {
    cccheckret(src, NULL);
    size_t len = strlen(src);
    char* content = cc_alloc(len);
    memcpy(content, src, len);
    return content;
}

// ******************************************************************************
// 读取文本文件 
char * cc_read_file(const char* fn) {
    char *content = NULL;
    size_t filesize = 0;
    FILE *file = 0;

    file = fopen(fn, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        filesize = ftell(file);
        rewind(file);

        content = cc_alloc(filesize);
        fread(content, 1, filesize, file);
        fclose(file);
    }
    return content;
}

// 写文件
size_t cc_write_file(char* content, const char* fn) {
    size_t write = 0;
    FILE *file = fopen(fn, "w");
    if (file) {
        write = fwrite(content, 1, cc_len(content), file);
        fclose(file);
    }
    return write;
}

// ******************************************************************************
// 自定义字典
/* ----------------------- StringCopy Hash Table Type ------------------------*/

static unsigned int _dictStringCopyHTHashFunction(const void *key)
{
    return dictGenHashFunction(key, (int)strlen(key));
}

static void *_dictStringDup(void *privdata, const void *key)
{
    size_t len = strlen(key);
    char *copy = malloc(len+1);
    DICT_NOTUSED(privdata);
    
    memcpy(copy, key, len);
    copy[len] = '\0';
    return copy;
}

static int _dictStringCopyHTKeyCompare(void *privdata, const void *key1,
                                       const void *key2)
{
    DICT_NOTUSED(privdata);
    const char* key1char = (const char*)key1;
    const char* key2char = (const char*)key2;
    
    return strcmp(key1char, key2char) == 0;
}

static void _dictStringDestructor(void *privdata, void *key)
{
    DICT_NOTUSED(privdata);
    
    free(key);
}

static dictType xdictTypeHeapStringCopyKey = {
    _dictStringCopyHTHashFunction, /* hash function */
    _dictStringDup,                /* key dup */
    NULL,                          /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    NULL                           /* val destructor */
};

// ******************************************************************************
// 数组 (可以加一点salt 在结构里面做校验)
typedef struct ccjsonarray {
    size_t n;
    size_t size;
    char addr[];
}ccjsonarray;

/**
 */
void * ccarraymalloc(size_t n, size_t size) {
    ccjsonarray * p = (ccjsonarray*)malloc(n * size + sizeof(ccjsonarray));
    p->n = n;
    p->size = size;
    return p->addr;
}

/**
 */
void ccarrayfree(void *array) {
    cccheck(array);
    ccjsonarray * p = (ccjsonarray*)((char*)array - sizeof(ccjsonarray));
    free(p);
}

/**
 */
size_t ccarraylen(void *array) {
    cccheckret(array, 0);
    ccjsonarray *p = (ccjsonarray*)((char*)array - sizeof(ccjsonarray));
    return p->n;
}

// ******************************************************************************
// 根据名字找索引 {name : parse_value};
static dict *gparses = NULL;
static struct cctypemeta *gtypemetas[CCMaxTypeCount] = {};
static int gtypemetascnt = 0;

#define __ccobj(p) (ccjson_obj*)(p)

/**
 * 获取成员的索引
 */
int ccobjmindex(struct cctypemeta *meta, const char* member) {
    int idx = -1;
    if (meta->members) {
        dictEntry *entry = dictFind(meta->members, member);
        if (entry) {
            ccmembermeta *m = (ccmembermeta*)entry->v.val;
            idx = m->idx;
        }
    }
    return idx;
}

/**
 */
bool ccobjhas(void *p, int index) {
    ccjson_obj *obj = __ccobj(p);
    return obj->__has[index/8] & (1 << (index%8));
}

/**
 */
void ccobjset(void *p, int index) {
    ccjson_obj *obj = __ccobj(p);
    obj->__has[index/8] |= (1<<(index%8));
}

/**
 */
void ccobjunset(void *p, int index) {
   ccjson_obj *obj = __ccobj(p);
   obj->__has[index/8] &= ~(1<<(index%8));
}

// 初始化字典
dict *ccgetparsedict() {
    if (gparses == NULL ) {
        gparses = dictCreate(&xdictTypeHeapStringCopyKey, NULL);
    }
    return gparses;
}

// 加入一个类型
int ccaddtypemeta(cctypemeta *meta) {
    if (meta->index > 0) {
        return meta->index;
    }
    meta->index = ++gtypemetascnt;
    
    dictAdd(ccgetparsedict(), (void*)meta->type, (void*)meta);
    gtypemetas[meta->index] = meta;
    return meta->index;
}

int ccinittypemeta(cctypemeta *meta) {
    if (meta->init) {
        return meta->init(meta);
    }
    
    return ccaddtypemeta(meta);
}

// 创建一个字典
dict *ccmakedict(cctypemeta *meta) {
   return dictCreate(&xdictTypeHeapStringCopyKey, meta);
}

/**
 * 造一个成员
 * name : 成员名字
 * type : 成员类型
 * offset : 成员在类型中的偏移
 * index : 成员的索引
 * compose : 是否是数组
 * TODO: 我们可以增加集中组合类型， list, map 等
 */
ccmembermeta *ccmakemember(const char* name, const char* type, int offset, int index, int compose) {
    return ccmakememberwithmeta(name, ccgettypemeta(type), offset, index, compose);
}

ccmembermeta *ccmakememberwithmeta(const char* name,
                           cctypemeta* type, int offset,
                           int index,
                           int compose) {
    ccmembermeta *_member = __ccmalloc(ccmembermeta);
    _member->name = name;
    _member->type = type;
    _member->offset = offset;
    _member->idx = index;
    _member->compose = compose;
    return _member;
    
}

// 向类型里面增加一项成员
void ccaddmember(cctypemeta *meta, ccmembermeta *member) {
    if (meta->members == NULL) {
        meta->members = ccmakedict(meta);
    }
    // auto member index
    unsigned long size = dictSize(meta->members);
    if (member->idx < (int)size) {
        member->idx = (int)size + 1;
    }
    
    // 加入字典
    dictAdd(meta->members, (void*)member->name, member);

    // 顺序查找
    meta->indexmembers[member->idx] = member;
}

// 创建一个类型meta
cctypemeta *ccmaketypemeta(const char* type, size_t size) {
    cctypemeta* meta = (cctypemeta*)calloc(1, sizeof(cctypemeta));
    meta->size = size;
    meta->type = type;
    meta->members = NULL;
    return meta;
}

// 查找某一个类型的元数据
cctypemeta *ccgettypemeta(const char* type) {
    dictEntry *entry = dictFind(ccgetparsedict(), (void*)type);
    if (entry) {
        return (cctypemeta*)entry->v.val;
    }
    return NULL;
}

// 查找某个类型的元信息
cctypemeta *ccgettypemetaof(int index) {
    return gtypemetas[index];
}

// 从json 解析到结构体
bool ccparse(cctypemeta *meta, void *value, cJSON *json) {
    cccheckret(meta, false);
    cccheckret(json, false);
    // be sure all the meta will be init before use
    ccinittypemeta(meta);
    // 解析
    bool has = false;
    switch(json->type) {
        case cJSON_False : {
            cccheckret(meta->type == cctypeofname(ccbool), has);
            bool *b = (bool*) value;
            *b = false;
            has = true;
            break; }
        case cJSON_True : {
            cccheckret(meta->type == cctypeofname(ccbool), has);
            bool *b = (bool*) value;
            *b = true;
            has = true;
            break; }
        case cJSON_NULL : {
            // should canside the meta type
            if (meta->type == cctypeofname(ccint)) {
                int *i = (int*)value;
                *i = 0;
                has = true;
            } else if(meta->type == cctypeofname(ccstring)) {
               char **cc = (char**)value;
               *cc = NULL;
                has = true;
            } else if(meta->type == cctypeofname(ccbool)) {
                bool *b = (bool*) value;
                *b = false;
                has = true;
            } else if(meta->type == cctypeofname(ccnumber)) {
                ccnumber *n = (ccnumber*)value;
                *n = 0;
                has = true;
            }
            break; }
        case cJSON_Number : {
            cccheckret(meta->type == cctypeofname(ccint) ||
                       meta->type == cctypeofname(ccnumber) , has);
            if (meta->type == cctypeofname(ccint)) {
                ccint *i = (ccint*)value;
                *i = json->valueint;
            } else if(meta->type == cctypeofname(ccnumber)) {
                ccnumber *n = (ccnumber*)value;
                *n = json->valuedouble;
            }
            has = true;
            break; }
        case cJSON_String : {
            cccheckret(meta->type == cctypeofname(ccstring), has);
            ccstring *cc = (ccstring*)value;
            if (json->valuestring) {
                *cc = cc_dup(json->valuestring);
            } else {
                *cc = NULL;
            }
            has = true;
            break; }
        case cJSON_Array : {
            int arraysize = cJSON_GetArraySize(json);
            void **vv = (void**)value;
            if (arraysize) {
                void *v = ccarraymalloc(arraysize, meta->size);
                *vv = v;
                for (int i=0; i<arraysize; ++i) {
                    ccparse(meta, (char*)v + i * meta->size, cJSON_GetArrayItem(json, i));
                }
            } else {
                *vv = NULL;
            }
            
            has = true;
            break; }
        case cJSON_Object : {
            if(meta->members == NULL) {
                break;
            }
            cJSON *child = json->child;
            while(child) {
                dictEntry *entry= dictFind(meta->members, child->string);
                if (entry) {
                    ccmembermeta *membermeta = (ccmembermeta*)entry->v.val;
                    if (ccparse(membermeta->type, (char*)value + membermeta->offset, child)) {
                        ccobjset(value, membermeta->idx);
                    }
                }
                child = child->next;
            }
            has = true;
            break; }
    }
    
    return has;
}

// 前置声明
bool ccunparsemember(ccmembermeta *mmeta, void *value, struct cJSON *json);

// 反向解析
cJSON *ccunparse(cctypemeta *meta, void *value) {
    // be sure all the meta will be init before use
    ccinittypemeta(meta);
    // 解析
    cJSON *obj = NULL;
    if (meta->members) {
        obj = cJSON_CreateObject();
        dictIterator *ite = dictGetIterator(meta->members);
        dictEntry *entry = dictNext(ite);
        while (entry) {
            ccmembermeta* member = (ccmembermeta*)entry->v.val;
            if (ccobjhas(value, member->idx)) {
                ccunparsemember(member, (char*)value + member->offset, obj);
            }
            entry = dictNext(ite);
        }
        dictReleaseIterator(ite);
    } else if (meta->type == cctypeofname(ccbool)) {
        bool *b = (bool*)value;
        obj = cJSON_CreateBool(*b);
    } else if(meta->type == cctypeofname(ccint)) {
        ccint *i = (ccint*)value;
        obj = cJSON_CreateNumber(*i);
    } else if(meta->type == cctypeofname(ccnumber)) {
        ccnumber *n = (ccnumber*)value;
        obj = cJSON_CreateNumber(*n);
    } else if(meta->type == cctypeofname(ccstring)) {
        ccstring *s = (ccstring *)value;
        if(*s) {
            obj = cJSON_CreateString(*s);
        }else {
            obj = cJSON_CreateNull();
        }
    } else {
      // todo: @@
    }
    return obj;
}

// 反解析
bool ccunparsemember(ccmembermeta *mmeta, void *value, struct cJSON *json) {
    cccheckret(mmeta, false);
    cccheckret(json, false);
    cctypemeta *meta = mmeta->type;
    cccheckret(meta, false);
    // unparse
    if (mmeta->compose) {
        void **arrayvalue = (void**)value;
        int len = (int)ccarraylen(*arrayvalue);
        cJSON *array = cJSON_CreateArray();
        if (len) {
            char *v = (char*)(*arrayvalue);
            for (int i=0; i < len; ++i) {
                cJSON * obj = ccunparse(meta, v + i * meta->size);
                if (obj) {
                    cJSON_AddItemToArray(array, obj);
                }
            }
        }
        
        cJSON_AddItemToObject(json, mmeta->name, array);
    }else {
        cJSON *obj = ccunparse(meta, value);
        if (obj) {
            cJSON_AddItemToObject(json, mmeta->name, obj);
        }
    }
    return true;
}

// 释放成员
bool ccobjreleasemember(ccmembermeta *mmeta, void *value);

// 释放结构体相关资源
void ccobjrelease(cctypemeta *meta, void *value) {
    // 解析
    if (meta->members) {
        dictIterator *ite = dictGetIterator(meta->members);
        dictEntry *entry = dictNext(ite);
        while (entry) {
            ccmembermeta* member = (ccmembermeta*)entry->v.val;
            if (ccobjhas(value, member->idx)) {
                ccobjreleasemember(member, (char*)value + member->offset);
                ccobjunset(value, member->idx);
            }
            entry = dictNext(ite);
        }
        dictReleaseIterator(ite);
    } else if (meta->type == cctypeofname(ccbool)) {
        //bool *b = (bool*)value;
    } else if(meta->type == cctypeofname(ccint)) {
        //ccint *i = (ccint*)value;
    } else if(meta->type == cctypeofname(ccnumber)) {
        //ccnumber *n = (ccnumber*)value;
    } else if(meta->type == cctypeofname(ccstring)) {
        ccstring * s = (ccstring *)value;
        if (*s) {
            cc_free(*s);
            *s = NULL;
        }
    } else {
        // todo: @@
    }
}

// 反解析
bool ccobjreleasemember(ccmembermeta *mmeta, void *value) {
    cccheckret(mmeta, false);
    cctypemeta *meta = mmeta->type;
    cccheckret(meta, false);
    // release
    if (mmeta->compose) {
        void **arrayvalue = (void**)value;
        int len = (int)ccarraylen(*arrayvalue);
        if (len) {
            char *v = (char*)(*arrayvalue);
            for (int i=0; i < len; ++i) {
                ccobjrelease(meta, v + i * meta->size);
            }
            ccarrayfree(v);
            *arrayvalue = NULL;
        }
        *arrayvalue = NULL;
    }else {
        ccobjrelease(meta, value);
    }
    return true;
}

// 从字符串解析
bool ccparsefrom(cctypemeta *meta, void *value, const char *json) {
    bool ok = false;
    cJSON* cjson = cJSON_Parse(json);
    ok = ccparse(meta, value, cjson);
    cJSON_Delete(cjson);
    return ok;
}

// 解析到字符串
char *ccunparseto(cctypemeta *meta, void *value) {
    char *str = NULL;
    char *content = NULL;
    cJSON *json = ccunparse(meta, value);
    str = cJSON_Print(json);
    cJSON_Delete(json);
    content = cc_dup(str);
    free(str);
    return content;
}

// 实现基础对象
// ******************************************************************************
__ccimplementtype(ccint)
__ccimplementtype(ccnumber)
__ccimplementtype(ccstring)
__ccimplementtype(ccbool)


// 声明复杂类型
__ccimplementtypebegin(ccconfig)
__ccimplementmember(ccconfig, ccint, ver)
__ccimplementmember(ccconfig, ccbool, has)
__ccimplementmember(ccconfig, ccstring, detail)
__ccimplementmember_array(ccconfig, ccint, skips)
__ccimplementtypeend(ccconfig)
