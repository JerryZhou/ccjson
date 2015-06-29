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
#include <time.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

// int64
typedef signed long long ccint64;
typedef unsigned long long ccuint64;
typedef signed int ccint32;
typedef unsigned int ccuint32;

// ccibool
typedef int ccibool;

#define ccitrue 1
#define ccifalse 0
#define cciyes 1
#define ccino 0
    
// the default member count of complex type
#define CCMaxMemberCount 64 
// system support max type count
#define CCMaxTypeCount 1000
// used macro
#define cc_unused(x) (void)x 

// ******************************************************************************
// get current system time in nanos 
ccint64 ccgetcurnano();
// get current system time in millseconds
ccint64 ccgetcurtick();
// alawys get the next nano of current
ccint64 ccgetnextnano();

// ******************************************************************************
// disable and enable the memory cache
ccibool cc_enablememorycache(ccibool enable); 
// get current memory useage total size
size_t cc_mem_size();
// memory alloc, all memory will end with 0,  call cc_free to free
char *cc_alloc(size_t size);
// free memory from cc_alloc
void cc_free(char *c);
// get the memory size
size_t cc_len(char *c); 
// make copy of string, and free memory with cc_free
char *cc_dup(const char* src);
// print the memory states
size_t cc_mem_state();
// print the memory states, and return the memory size hold by caches
size_t cc_mem_cache_state();
// get the memory cache unit size by index 
size_t cc_mem_cache_current(int index);
// get the memory cache capacity by index
size_t cc_mem_cache_capacity(int index);
// set the memory capacity, set the capacity to 0 will disable the cache
void cc_mem_cache_setcapacity(int index, size_t capacity); 
// clear the memory cache of index
void cc_mem_cache_clearof(int index);
// clear all memory cache
void cc_mem_cache_clear(); 


// ******************************************************************************
// return the all content of file, need free with cc_free
char * cc_read_file(const char* fn);
// write content to file, content must returned from cc_alloc, cc_dup, cc_read_file
size_t cc_write_file(char* content, const char* fn);

// ******************************************************************************
// type meta infromation
struct cctypemeta;
// type member meta information 
struct ccmembermeta;

// meta init entry
typedef int (*cctypemeta_init)( struct cctypemeta *meta);

// type meta infromation
typedef struct cctypemeta {
    const char *type;
    size_t size;
    int index;
    
    int membercount;
    void *members;
    struct ccmembermeta **indexmembers;
    cctypemeta_init init;
}cctypemeta;

// member compose way: array, pointer
typedef enum enumflagcompose {
    enumflagcompose_array = 1,
    enumflagcompose_point = 2,
}enumflagcompose;

// type member meta information
typedef struct ccmembermeta {
    const char *name;
    int compose;    // 1 array
    int idx;        // index of member
    int offset;
    cctypemeta *type;
}ccmembermeta;

// basic json object flag 
typedef enum enumflagccjsonobj {
    enumflagccjsonobj_null = 1,
}enumflagccjsonobj;

// basic json object
#define _ccjson_obj(n) int __index; int __flag; char __has[2*((n+7)/8)]

// basic cjson_obj
struct ccjson_obj;

// get the basic information about ccjson_obj
char ccjsonobjhas(void *obj, int index);
int ccjsonobjindex(void *obj);
int ccjsonobjflag(void *obj);

// array operator
// n    : array length
// size : every element size
// index: type meta index 
void * ccarraymalloc(size_t n, size_t size, int index);
void * ccarraymallocof(size_t n, cctypemeta* meta);
struct cctypemeta* ccarraymeta(void *array);
struct ccjson_obj* ccarrayobj(void *array);
void ccarrayfree(void *array);
size_t ccarraylen(void *array);

// get member index 
int ccobjmindex(struct cctypemeta *meta, const char* member);
// get member meta by index 
struct ccmembermeta *ccobjmmetabyindex(struct cctypemeta *meta, int index);
// get member count of type with the meta
int ccobjmcount(struct cctypemeta *meta);

// basic json object member operator
ccibool ccobjhas(void *p, int index);
void ccobjset(void *p, int index);
void ccobjunset(void *p, int index);

// basic json object member operator with null
ccibool ccobjisnull(void *p, int index);
void ccobjsetnull(void *p, int index);
void ccobjunsetnull(void *p, int index);

// array operator
ccibool ccarrayhas(void *p, int index);
void ccarrayset(void *p, int index);
void ccarrayunset(void *p, int index);

// array element operator 
ccibool ccarrayisnull(void *p, int index);
void ccarraysetnull(void *p, int index);
void ccarrayunsetnull(void *p, int index);

// set the basic json object to null 
void ccobjnullset(void *p, ccibool isnull);
// is the basic json object null
ccibool ccobjnullis(void *p);

/**
 * make a member meta object
 * name : member name
 * type : member type
 * offset : member offset in type
 * index : member index in type, index will be unique in one type
 * compose : if the member is array or pointer, we only support array and pointer
 * TODO: may be add support for: list, map, ...
 */
ccmembermeta *ccmakemember(const char* name,
                           const char* type, int offset,
                           int index,
                           int compose);
ccmembermeta *ccmakememberwithmeta(const char* name,
                           cctypemeta* type, int offset,
                           int index,
                           int compose);

// add a member to type(meta)
void ccaddmember(cctypemeta *meta, ccmembermeta *member);

// make a type meta object
cctypemeta *ccmaketypemeta(const char* type, size_t size);
    
// registe the type meta to system, after this we can be use the json system
// return the type index in system
int ccaddtypemeta(cctypemeta *meta);

// init a type meta object, will call the init function in meta 
int ccinittypemeta(cctypemeta *meta);

// find type meta by type name
cctypemeta *ccgettypemeta(const char* type);
// find type meta by type index
cctypemeta *ccgettypemetaof(int index);

// release the memory hold by p with type meta
// the json object that have been called from ccparsefrom need call this to free memory 
void ccobjrelease(cctypemeta *meta, void *p);
// release the memory hold by value with array type meta 
void ccobjreleasearray(cctypemeta* meta, void *value);

// serial the infromation from json , will fill all the data to value
ccibool ccparsefrom(cctypemeta *meta, void *value, const char *json);

// unserial the json object to a json string, returned string neededcall cc_free to free the memory
char *ccunparseto(cctypemeta *meta, void *value);


// ******************************************************************************
// helper: malloc a basic json object with type meta, we can call the ccjsonobjfree to free memories
void *ccjsonobjalloc(cctypemeta* meta);

// helper: free the memories holded by p, but not free p self
void ccjsonobjrelease(void *p);

// helper: free the memories holded by p, and then free p self
void ccjsonobjfree(void *p);

// helper: serial from json 
ccibool ccjsonobjparsefrom(void *p, const char* json);

// helper: unserial to json
char* ccjsonobjunparseto(void *p);

// helper macro: make a json object 
#define iccalloc(type) ((type*)ccjsonobjalloc(cctypeofmeta(type)))
// helper macro: free memories holded by p members
#define iccrelease(p) ccjsonobjrelease(p)
// helper macro: free all memories taken by p
#define iccfree(p) ccjsonobjfree(p)
// helper macro: serial from json
#define iccparse(p, json) ccjsonobjparsefrom(p, json)
// helper macro: unserial to json
#define iccunparse(p) ccjsonobjunparseto(p) 

// ******************************************************************************
// helper macro: to create the type meta
#define cctypeofname(type) __cc_name_##type
#define cctypeofmeta(type) __cc_meta_get_point_##type##_()
#define cctypeofmindex(type, member) __cc_index_m_##type##_mmm_##member
#define cctypeofmcount(type) __cc_index_max_##type

#define cctypeofmetavar(type) __cc_meta_var_point_##type
#define cctypeofmetaget(type) cctypeofmeta(type)

// helper macro: delcare the member index 
#define __ccdeclareindexbegin(type) typedef enum __cc_index_##type {
#define __ccdeclareindexmember(type, mtype, member) cctypeofmindex(type, member),
#define __ccdeclareindexmember_array(type, mtype, member) cctypeofmindex(type, member),
#define __ccdeclareindexmember_point(type, mtype, member) cctypeofmindex(type, member),
#define __ccdeclareindexend(type) cctypeofmcount(type) } __cc_index_##type ;

// helper macro: declare the type meta information
#define __ccdeclaretype(mtype) \
    extern const int cctypeofmcount(mtype);\
    extern const char* cctypeofname(mtype); \
    extern struct cctypemeta *cctypeofmetaget(mtype);

// helper macro: implement the type meta information 
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
    struct cctypemeta * cctypeofmetaget(mtype) {\
        static int init = 0;\
        static struct cctypemeta cctypeofmetavar(mtype);\
        if (init == 0) {\
            init = 1;\
            cctypeofmetavar(mtype).type=#mtype, \
            cctypeofmetavar(mtype).size=sizeof(mtype), \
            cctypeofmetavar(mtype).members=NULL, \
            cctypeofmetavar(mtype).membercount = cctypeofmcount(mtype), \
            cctypeofmetavar(mtype).indexmembers= NULL,\
            cctypeofmetavar(mtype).index=0, \
            cctypeofmetavar(mtype).init=__cc_init_##mtype;\
        }\
        return &cctypeofmetavar(mtype);\
    }

// helper macro: declare the complex type meta information 
#define __ccdeclaretypebegin(mtype) \
    typedef struct mtype { _ccjson_obj(cctypeofmcount(mtype));

// helper macro: declare the complex type member
#define __ccdeclaremember(mtype, htype, m) \
    htype m;

// helper macro: declare the complex type array member
#define __ccdeclaremember_array(mtype, htype, m) \
    htype* m;

// heler macro: declare the complex type member pointer
#define __ccdeclaremember_point(mtype, htype, m) \
    htype* m;

// helper macro: end of complex type meta declare
#define __ccdeclaretypeend(mtype) \
    } mtype; \
    extern const char* cctypeofname(mtype); \
    extern struct cctypemeta *cctypeofmetaget(mtype); \

// helper macro: implenet the member type 
#define __ccimplementtypebegin(mtype) \
    const char* cctypeofname(mtype) = #mtype; \
    static int __cc_init_##mtype(cctypemeta * meta) {\
        if(meta->index) { \
            return meta->index; \
        }\
        meta->index = ccaddtypemeta(meta); \

// helper macro: end of implememt complex type 
#define __ccimplementtypeend(mtype) \
        return meta->index;\
    }\
    struct cctypemeta * cctypeofmetaget(mtype) {\
        static int init = 0;\
        static struct cctypemeta cctypeofmetavar(mtype);\
        if (init == 0) {\
            init = 1;\
            cctypeofmetavar(mtype).type=#mtype, \
            cctypeofmetavar(mtype).size=sizeof(mtype), \
            cctypeofmetavar(mtype).members=NULL, \
            cctypeofmetavar(mtype).membercount = cctypeofmcount(mtype), \
            cctypeofmetavar(mtype).indexmembers= NULL,\
            cctypeofmetavar(mtype).index=0, \
            cctypeofmetavar(mtype).init=__cc_init_##mtype;\
        }\
        return &cctypeofmetavar(mtype);\
    }


// helper macro: implement the member type 
#define __ccimplementmember(mtype, ntype, member)  do {\
    ccmembermeta *_member = ccmakememberwithmeta(#member, cctypeofmetaget(ntype), \
                        offsetof(mtype, member), cctypeofmindex(mtype, member), 0); \
    ccaddmember(meta, _member); } while(0);

// helper macro: implement the array member type 
#define __ccimplementmember_array(mtype,  ntype, member)  do {\
    ccmembermeta *_member = ccmakememberwithmeta(#member, cctypeofmetaget(ntype), \
                        offsetof(mtype, member), cctypeofmindex(mtype, member), enumflagcompose_array); \
    ccaddmember(meta, _member); } while(0);

// helper macro: implement the pointer member type 
#define __ccimplementmember_point(mtype,  ntype, member)  do {\
    ccmembermeta *_member = ccmakememberwithmeta(#member, cctypeofmetaget(ntype), \
                        offsetof(mtype, member), cctypeofmindex(mtype, member), enumflagcompose_point); \
    ccaddmember(meta, _member); } while(0);
    
// declare all the basic type: bool , int, number, string, int64
// ******************************************************************************
typedef char* ccstring;
typedef int ccbool;
typedef double ccnumber;
typedef int ccint;

// all basic type
__ccdeclaretype(ccint)
__ccdeclaretype(ccint64)
__ccdeclaretype(ccnumber)
__ccdeclaretype(ccstring)
__ccdeclaretype(ccbool)

// make a example of complex type (index) 
__ccdeclareindexbegin(ccconfig)
__ccdeclareindexmember(ccconfig, ccint, ver)
__ccdeclareindexmember(ccconfig, ccint64, ver64)
__ccdeclareindexmember(ccconfig, ccbool, has)
__ccdeclareindexmember(ccconfig, ccstring, detail)
__ccdeclareindexmember_array(ccconfig, ccint, skips)
__ccdeclareindexend(ccconfig)

// make a example of complex type  (meta information)
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
