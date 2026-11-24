#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
//ch26 q3
#include "value.h"

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) \
    reallocate(pointer, sizeof(type), 0)

//ch26 q3
void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void freeObjects(void);
void addToZCT(Obj* object); 
void incRef(Obj* object); 
void decRef(Obj* object); 
void incRefValue(Value value);
void decRefValue(Value value);
void collectGarbage(void); 

#endif
