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
#include "ccjsonstruct.h"
#include "ccjson.h"

#define print printf 

SP_SUIT(ccjson);

SP_CASE(ccjson, ccarraymalloc) {
    SP_TRUE(1);
}

SP_CASE(ccjson, eg0) {
    config_app app = {0};

    char *json = cc_read_file("app.json");

    size_t m1 = cc_mem_state();
    print("parse 1000 times about app json\n");
    for (int i=0; i<1000; ++i) {
        ccparsefrom(&cctypeofmeta(config_app), &app, json); 
    }
    size_t m2 = cc_mem_state();

    print("parse anthor 1000 times about app json\n");
    for (int i=0; i<1000; ++i) {
        ccparsefrom(&cctypeofmeta(config_app), &app, json); 
    }
    size_t m3 = cc_mem_state();
    SP_TRUE(m2 > m1);
    SP_EQUAL(m2, m3);
    
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

    SP_EQUAL(ccobjmcount(&cctypeofmeta(config_date)), 2);
}

SP_CASE(ccjson, eg2) {
    config_date *date = iccalloc(config_date);

    char * json = cc_read_file("date.json");
    iccparse(date, json);

    print("date: %s - %s\n", date->invaliddate, date->validdate);

    cc_free(json);

    iccfree(date);

    SP_TRUE(1);
}

SP_CASE(ccjson, eg3) {
    ccconfig *config = iccalloc(ccconfig);
    const char* json = "{\"ver\":1, \"has\":true, \"noexits\": 1,  \"detail\":\"no details\", \"skips\":[1, 2, 3]}";

    iccparse(config, json);
    print("config->ver: %d\n", config->ver);
    print("config->has: %d\n", config->has);
    SP_EQUAL(config->ver, 1);
    SP_EQUAL(config->has, true);
    SP_EQUAL(strcmp(config->detail, "no details") , 0);

        SP_EQUAL(config->skips[0], 1);
        SP_EQUAL(config->skips[1], 2);
        SP_EQUAL(config->skips[2], 3);

    const char* json1 = "{\"ver\":2, \"has\":\"xxx\", \"noexits\": 1,  \"detail\":\"no details\", \"skips\":[0, 1, 2, 3]}";

    iccparse(config, json1);
    SP_EQUAL(config->ver, 2);
    SP_EQUAL(config->has, true);
    SP_EQUAL(strcmp(config->detail, "no details") , 0);
    SP_EQUAL(config->skips[0], 0);
        SP_EQUAL(config->skips[1], 1);
        SP_EQUAL(config->skips[2], 2);
        SP_EQUAL(config->skips[3], 3);

    const char* json2 = "{\"ver\":3, \"has\":false, \"noexits\": 1,  \"detail\":\"no details\", \"skips\":true}";
    iccparse(config, json2);
    SP_EQUAL(config->ver, 3);
    SP_EQUAL(config->has, false);
    SP_EQUAL(strcmp(config->detail, "no details") , 0);
    SP_EQUAL(config->skips[0], 0);
        SP_EQUAL(config->skips[1], 1);
        SP_EQUAL(config->skips[2], 2);
        SP_EQUAL(config->skips[3], 3);

    iccfree(config);
}

SP_CASE(ccjson, eg4) {
    ccconfig *config = iccalloc(ccconfig);
    const char* json = "{\"ver\":1, \"has\":null, \"noexits\": 1,  \"detail\":\"no details\", \"skips\":[1, null, 2]}";
    print("json %s\n", json);

    iccparse(config, json);
    SP_EQUAL(config->ver, 1);
    SP_TRUE(ccobjhas(config, cctypeofmindex(ccconfig, ver)));
    SP_TRUE(!ccobjhas(config, cctypeofmindex(ccconfig, has)));
    SP_TRUE(ccobjhas(config, cctypeofmindex(ccconfig, detail)));
    SP_TRUE(ccobjhas(config, cctypeofmindex(ccconfig, skips)));

    char* unjson = iccunparse(config);
    print("unjson %s\n", unjson);

    const char* xjson = "{\"ver\":[1, 2, 3], \"has\":null, \"noexits\": 1,  \"detail\":\"no details\", \"skips\":1}";

    iccparse(config, xjson);
    print("unjson %s\n", xjson);
    SP_EQUAL(config->ver, 1);
    SP_TRUE(ccobjhas(config, cctypeofmindex(ccconfig, skips)));
    SP_TRUE(ccobjisnull(config, cctypeofmindex(ccconfig, has)));

    print("i m here skips %d !!\n", ccobjhas(config, cctypeofmindex(ccconfig, skips)));
    print("i m here skips len %zu !!\n", ccarraylen(config->skips));

    SP_EQUAL(config->skips[0], 1);
    SP_EQUAL(config->skips[1], 0);
    SP_EQUAL(config->skips[2], 2);

    iccrelease(config);

    // release all member
    SP_TRUE(!ccobjhas(config, cctypeofmindex(ccconfig, ver)));
    SP_TRUE(!ccobjhas(config, cctypeofmindex(ccconfig, has)));
    SP_TRUE(!ccobjhas(config, cctypeofmindex(ccconfig, detail)));
    SP_TRUE(!ccobjhas(config, cctypeofmindex(ccconfig, skips)));

    print("i m here 002 !!\n");

    iccfree(config);
    iccfree(unjson);


    SP_TRUE(1);
}


#endif
