/*********************************************
 * Copyright: XGrids Corporation
 * Author   : Feng HaiLiang
 * ******************************************/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLCC, Log, All);

#define LCC_LOG(Format, ...) UE_LOG(LogLCC, Log, TEXT(Format), ##__VA_ARGS__)
#define LCC_WARNING(Format, ...) UE_LOG(LogLCC, Warning, TEXT(Format), ##__VA_ARGS__)
#define LCC_ERROR(Format, ...) UE_LOG(LogLCC, Error, TEXT(Format), ##__VA_ARGS__)
#define LCC_DISPLAY(Format, ...) UE_LOG(LogLCC, Display, TEXT(Format), ##__VA_ARGS__)
#define LCC_FATAL(Format, ...) UE_LOG(LogLCC, Fatal, TEXT(Format), ##__VA_ARGS__)

//similar to UE_SMALL_NUMBER,but bigger
#define LCC_SMALL_NUMBER (1.e-4f)
//represents any value
#define LCC_ANY_VALUE -1
//meter to centimeters
#define METER_TO_CENTIMETERS 100
//clip volume max num
#define LCC_MAX_CLIPPING_VOLUME 50
//section plane max num
#define LCC_MAX_SECTION_PLANE 50
//use quad for lcc splat,higher performance
#define LCC_USE_QUAD 1
//use ellipsoid for splat,low performance
#define LCC_USE_ELLIPSOID 0
//Lcc Render max distance
#define LCC_MAX_DISTANCE 300
//LCC max splat num
#define LCC_MAX_SPLAT 3000
//LCC max load collision data distance
#define LCC_MAX_COLLISION_DISTANCE 50
//default add extra preload
#define LCC_ADD_EXTRA_PRELOAD 1
//full load splat num
#define LCC_FULL_LOAD_Num 1500

//Foreign website
#define LCC_AUTH_URL_FOREIGN TEXT("https://api-gw.xgrids.com/lixel-front-api/dev/sdk/authentication")
#define LCC_REGISTER_URL_FOREIGN TEXT("https://developer.xgrids.com/#/authentication")
#define LCC_DOC_URL_FOREIGN TEXT("https://developer.xgrids.com/#/document?titleId=en-1720509312452")
#define LCC_TUTORIAL_URL_FOREIGN TEXT("https://developer.xgrids.com/#/tutorial?page=UE_SDK")
#define LCC_SUPPORT_URL_FOREIGN TEXT("https://developer.xgrids.com/#/forum")
#define LCC_CHANGELOG_URL_FOREIGN TEXT("https://developer.xgrids.com/#/document?titleId=en-1720509717058")
#define LCC_HOME_URL_FOREIGN TEXT("https://developer.xgrids.com/#/")

//Domestic website
#define LCC_AUTH_URL_DOMESTIC TEXT("https://api-gw.xgrids.cloud/lixel-front-api/dev/sdk/authentication")
#define LCC_REGISTER_URL_DOMESTIC TEXT("https://developer.xgrids.cn/#/authentication")
#define LCC_DOC_URL_DOMESTIC TEXT("https://developer.xgrids.cn/#/document?titleId=cn-1720170162723")
#define LCC_TUTORIAL_URL_DOMESTIC TEXT("https://developer.xgrids.cn/#/tutorial?page=UE_SDK")
#define LCC_SUPPORT_URL_DOMESTIC TEXT("https://developer.xgrids.cn/#/forum")
#define LCC_CHANGELOG_URL_DOMESTIC TEXT("https://developer.xgrids.cn/#/document?titleId=cn-1720170723795")
#define LCC_HOME_URL_DOMESTIC TEXT("https://developer.xgrids.cn/#/")

//Stg website
#define LCC_AUTH_URL_STG TEXT("https://stg-api-gw.xgrids.cloud/lixel-front-api/dev/sdk/authentication")

//watermark image
#define LCC_WATER_MARK_IMAGE "iVBORw0KGgoAAAANSUhEUgAAAREAAAAZCAMAAAAhbuplAAAALVBMVEVHcEz////////////////////////////////////////////////////////NXt0CAAAAD3RSTlMACy09lpxoWYRMj3ceEwN+b1nbAAAFM0lEQVR42u2X4a7tKAiFq4AKKO//uJOilrZnZ2cymdycH3clO6mKil+X2n38MQEhRKkS5uO3amRXH/aXyBSjS5qC/TEi/C+JpG5HyPrYT6PnrX615of6a4ireaSnJe79hidHKCJEhGy/zCPGUlMUQXSWRm2CW63vVgxFPQtPIA2nRJRHMOcmGFIngrX3DAVJ+q8hEnGBBIQmka5E+INI9crQnMEKFX/R+WomopY384r0wDg9Ag6rEPHyGTBDTv683Gcj525RE1G7NVkHMA84Gzx4Np2lD0SsA0Mec8gdPk7jhka5kBjIWohVwpKXomulBlHLuOZbRCyjwGpSJJlILCMp/Nw1sN4iVYepQkSECuZemwMWxDY8DBFuUcXxVJTMQmqHZcWzYdsdGp4lBnydI1B9pzawI2m8aJSHedKF5AJydKFqn/ykKUpd3h7BawsYNGp9wfVVhYKI4yrOgLCVougvNenskhoto1eSfkU1nGlUwoIo1c7EUUtp5FlbXkVUfHlEV/+TAG97jubjhWwjCSBH9g3xhUgEPT3So3VBNaVyPPT2iDNFTmYJxFn4jvcJkPhwRJoiqp7cPAob9HQuSSCZjeLgks64URHpQYSEx1lPZ8J9nxB5TvLTJQuIC0j6VyLx4j57JLxhJTq9iXiWfERy5ovdOTK12bnLyW14VDi4ktvQw+txba4YbbTn7esgvP5MzZyfzxlp388SDSBBxMZUehJJY4zOQiV98EiYBOHY98oex4KIuSfcDeBLCs+Mdo6YlOpMBBDz5rQsY5tmfGhscrDrjxpE7iUre1Y+9mRvpbJvhyDi1W1K851I19aa7Df02SM+E++xpS0VsEWkFd/RTjXWMM8VK9SGn1NDCPxEH5NWBuZa3AR7b/kDzwakOnF/vn2lB5vr/MiI8Pkjkkp6EvE+SxxEPG0ixBPTF4+kRcRGEbqEbE4EySU1rp5pLB+LSfIB1IYpFUu6KglpqVgQsRINyD44bL6fiXiEV+S9u98CIUSs6UXEMp+q8iDiux1rPh+/eKTLXqZl4KmqhDARaCmlQreVIT+I5LNvPR+Z2hhzfvCbpjID5B5fXJ6O7oZxJwKfiZhHrE02GlX7BKRBoUAC9Mn/cY6cOQTYl0e+nc5dSe+3r+fy3DV1+qsMJZ4X2vlbUVfuQSQe4gTlL/9rokMq1JIP/glIjk81h/uNiCMhHV89MtSD36okj20Sp8djNYU0i3Rnw5VaCv/8IGIBNKiuxJ4eiZNZ+vJQ9jV9BHLckFhHhNetEUQCyRePjMjmC5FALnliXA9AUvZ3mOpcYdpRhw0LIu7+1q+GKGZ5EVn1cLuetS18IdtAAolTbDmZK3WlNoLIE8nbI7PLAKXrhVooZaFyEQkxUuOco1cXWm8akNbrMUCSCgBc9HGO2LlW9obzdvAi5MxN3l/x0visX6B8CJL+2SGBxFYeosWlQsjHm4gFkptHUGeXhoRlrAlKSNdkbyKpIBEiEWnfxvYQZ7OTtnpG+C+ILGuthpbmYeWjNX59swrLWR+nQxb6sWmsBBBH0sb+X3JJeHXie/d0O9nK3sBb2HYfU7ppTQaq+eXU4p8rvMdn1Yk0VdW9gJQ9Sguks4tq37l4d9WazZfB2lqrfZT7NKBlZI+Dy+n6YW/nes9tVFhe71ymKndbrf0ea4ln/rarE5clhhETlJtWvaVkx1OW4pM2IuIporz8arDkDcfWGsw8ImLmABHWmzvgf5f9h9hfoeHXz1/F7lAkHX9BXCpEqP0vh0tWRfl3OOQfwp9DYJa3U9QAAAAASUVORK5CYII="
