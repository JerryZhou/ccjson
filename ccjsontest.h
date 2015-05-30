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

#define print printf 

SP_SUIT(ccjson);

SP_CASE(ccjson, ccarraymalloc) {
    SP_TRUE(1);
}

SP_CASE(ccjson, eg0) {
    config_app app = {0};

    char *json = cc_read_file("app.json");

    ccparsefrom(&cctypeofmeta(config_app), &app, json); 
    
    cc_free(json);

    char *unjson = ccunparseto(&cctypeofmeta(config_app), &app);

    print("unjson: %s\n", unjson);

    cc_free(unjson);

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

    cc_write_file(unjson, "date.json");
    cc_free(unjson);
}

SP_CASE(ccjson, eg2) {
    SP_TRUE(1);
}


#endif
