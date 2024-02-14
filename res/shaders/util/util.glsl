#ifndef UTIL_GLSL
#define UTIL_GLSL

float linearDepth(float z, float near) {
    return /*-*/near / z;
}

#endif //UTIL_GLSL