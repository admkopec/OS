//
//  OSRuntime.hpp
//  Libc++
//
//  Created by Adam Kopeć on 6/7/17.
//  Copyright © 2017 Adam Kopeć. All rights reserved.
//

#ifndef OSRuntime_hpp
#define OSRuntime_hpp

#include <stddef.h>

class OSRuntime {
public:
    static void* OSMalloc(size_t size);
    static void  OSFree(void * addr);
};

typedef int OSReturn;

#define kOSReturnSuccess true
#define kOSReturnTimeout false
#define kOSReturnError   -3

#endif /* OSRuntime_hpp */