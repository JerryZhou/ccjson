#ifndef __cstructjson_h_
#define __cstructjson_h_
#include "ccjson.h"

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

// 声明成员索引
#define __cc_type_begin __ccdeclareindexbegin
#define __cc_type_member __ccdeclareindexmember
#define __cc_type_member_array __ccdeclareindexmember_array
#define __cc_type_end __ccdeclareindexend

#include "ccjsonstruct.inl"

#undef __cc_type_begin
#undef __cc_type_member
#undef __cc_type_member_array
#undef __cc_type_end

// 声明类型
#define __cc_type_begin __ccdeclaretypebegin
#define __cc_type_member __ccdeclaremember
#define __cc_type_member_array __ccdeclaremember_array 
#define __cc_type_end __ccdeclaretypeend

#include "ccjsonstruct.inl"

#undef __cc_type_begin
#undef __cc_type_member
#undef __cc_type_member_array
#undef __cc_type_end
 
    
/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif // __cstructjson_h_


