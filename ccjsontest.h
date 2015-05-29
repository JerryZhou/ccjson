//
//  ccjsontest.h
//  cjson
//
//  Created by JerryZhou on 15/5/29.
//  Copyright (c) 2015年 JerryZhou. All rights reserved.
//

#ifndef cjson_ccjsontest_h
#define cjson_ccjsontest_h

#include "simpletest.h"
#include "cjsonstruct.h"
#include "ccjson.h"

// 文本缓冲区
typedef struct __cc_content {
    size_t size;
    char content[];
}__cc_content;

//  申请文本缓存区域
char *__cc_alloc(size_t size) {
    __cc_content *content = (__cc_content*)malloc(size + sizeof(__cc_content) + 1);
    content->size = size;
    content->content[size] = 0;
    return content->content;
}

// 释放文本缓冲区
void __cc_free(char *c) {
    __cc_content *content = (__cc_content*)(c - sizeof(__cc_content));
    free(content);
}

// 生成一个权限的字符缓冲区
char *__cc_copy(char* src, size_t len) {
    char* content = __cc_alloc(len);
    memcpy(content, src, len);
    return content;
}

// 文本缓冲区的长度
size_t __cc_len(char *c) {
    __cc_content *content = (__cc_content*)(c - sizeof(__cc_content));
    return content->size;
}

// 读取文本文件 
char * __cc_read_file(const char* fn) {
    char *content = NULL;
    size_t filesize = 0;
    FILE *file = 0;

    file = fopen(fn, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        filesize = ftell(file);
        rewind(file);

        content = __cc_alloc(filesize);
        fread(content, 1, filesize, file);
        fclose(file);
    }
    return content;
}

// 写文件
size_t __cc_write_file(char* content, const char* fn) {
    size_t write = 0;
    FILE *file = fopen(fn, "w");
    if (file) {
        write = fwrite(content, 1, __cc_len(content), file);
        fclose(file);
    }
    return write;
} 

SP_SUIT(ccjson);

SP_CASE(ccjson, ccarraymalloc) {
    SP_TRUE(1);
}

#define print printf 

SP_CASE(ccjson, eg0) {
    config_app app = {0};

    char *json = __cc_read_file("app.json");

    ccparsefrom(&cctypeofmeta(config_app), &app, json); 

    // print("json: %s\n", json);
    
    __cc_free(json);

    char *unjson = ccunparseto(&cctypeofmeta(config_app), &app);

    print("unjson: %s\n", unjson);

    free(unjson);

    ccobjrelease(&cctypeofmeta(config_app), &app);

    SP_TRUE(1);
}

SP_CASE(ccjson, eg1) {
    config_date date = {0};
    date.invaliddate = (char*)("2015-05-11");
    date.validdate = (char*)("2015-05-31");
    ccobjset(&date, cctypeofmindex(config_date, invaliddate));
    ccobjset(&date, cctypeofmindex(config_date, validdate));

    char *unjson = ccunparseto(&cctypeofmeta(config_date), &date);

    print("unjson: %s\n", unjson);

    char *content = __cc_copy(unjson, strlen(unjson));
    __cc_write_file(content, "date.json");
    __cc_free(content);

    free(unjson);
}

SP_CASE(ccjson, eg2) {
    SP_TRUE(1);
}


#endif
