#include "GLApi.h"

#if !defined(__ANDROID__)
#define ZK_GL_DEFINE(type, name) type zk_##name = nullptr;
ZK_GL_FUNCS(ZK_GL_DEFINE)
#undef ZK_GL_DEFINE

bool zkLoadGL(void* (*getProc)(const char*)) {
    bool ok = true;
#define ZK_GL_LOAD(type, name) zk_##name = reinterpret_cast<type>(getProc(#name)); ok = ok && zk_##name;
    ZK_GL_FUNCS(ZK_GL_LOAD)
#undef ZK_GL_LOAD
    return ok;
}
#endif
