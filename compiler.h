#ifndef clox_compiler_h
#define clox_compiler_h

#include "common.h"
#include "object.h"

ObjFunction* compile(const char* source);
 //ch26 q3
void setCompilerRootMarks(bool value);

#endif
