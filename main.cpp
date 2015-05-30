#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
// C99
#include <stdbool.h>

// ccjson
#include "ccjson.h"
// unit test
#include "ccjsontest.h"

#define cc_unused(v) (void)v

int main(int argc, const char** argv){
    cc_unused(argc);
    cc_unused(argv);
    
    ccconfig config = {0};
    ccparsefrom(&cctypeofmeta(ccconfig), &config, "{\"ver\":2, \"skips\":[1,2,3], \"has\":true, \"detail\":\"abcdefg\"}");
    
    {
        char * xxjson = ccunparseto(&cctypeofmeta(ccconfig), &config);
        printf("unparse : %s \n", xxjson);
        cc_free(xxjson);
    }
    
    ccobjrelease(&cctypeofmeta(ccconfig), &config);
    
    runAllTest();
    
    return 0;
}


