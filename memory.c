#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "vm.h"

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;

  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif

    if (vm.bytesAllocated > vm.nextGC) {
      collectGarbage();
    }
  }

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);

  return result;
}

//ch26 q3
void addToZCT(Obj* object) { 
    if (object->inZCT) return; 

  if (vm.zctCapacity < vm.zctCount + 1) { 
    vm.zctCapacity = GROW_CAPACITY(vm.zctCapacity); 
    vm.zct = (Obj**)realloc(vm.zct, sizeof(Obj*) * vm.zctCapacity); 
    if (vm.zct == NULL) exit(1); 
  } 

  object->inZCT = true; 
  vm.zct[vm.zctCount++] = object; 
} 


//ch26 q3
void incRef(Obj* object) { 
  if (object == NULL) return; 
  object->refCount++;
} 


//ch26 q3
void decRef(Obj* object) { 
  if (object == NULL || vm.shuttingDown) return; 
  object->refCount--; 
  if (object->refCount == 0) addToZCT(object); 
} 


//ch26 q3
void incRefValue(Value value) { 
  if (IS_OBJ(value)) incRef(AS_OBJ(value)); 
} 


//ch26 q3
void decRefValue(Value value) { 
  if (IS_OBJ(value)) decRef(AS_OBJ(value)); 
} 

//ch26 q3
static void unlinkObject(Obj* object) { 
  if (object->prev != NULL) { 
    object->prev->next = object->next; 
  } else { 
    vm.objects = object->next; 
  } 
  if (object->next != NULL) object->next->prev = object->prev; 
} 



//ch26 q3
static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void*)object, object->type);
#endif

  if (!vm.shuttingDown) unlinkObject(object);

  switch (object->type) {
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      decRef((Obj*)function->name); 
      freeChunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      decRef((Obj*)closure->function); 
      for (int i = 0; i < closure->upvalueCount; i++) { 
        decRef((Obj*)closure->upvalues[i]); 
      } 
      FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
      FREE(ObjClosure, object);
      break;
    }
    case OBJ_UPVALUE:
      decRefValue(((ObjUpvalue*)object)->closed);
      FREE(ObjUpvalue, object);
      break;
    case OBJ_STRING: {
      ObjString* string = (ObjString*)object;
      if (!vm.shuttingDown) tableRemoveString(&vm.strings, string);
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, object);
      break;
    }
  }
}


//ch26 q3
static void setRootMarks(bool value) {
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) { 
    if (IS_OBJ(*slot)) AS_OBJ(*slot)->isMarked = value;
  } 

  for (int i = 0; i < vm.frameCount; i++) { 
    vm.frames[i].closure->obj.isMarked = value; 
  } 

  for (ObjUpvalue* upvalue = vm.openUpvalues; 
       upvalue != NULL; 
       upvalue = upvalue->next) { 
    upvalue->obj.isMarked = value; 
  }

  setCompilerRootMarks(value); 
} 

//ch26 q3
void collectGarbage(void) { 
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm.bytesAllocated;
#endif

  setRootMarks(true); 

  int deferredCount = 0; 
  int deferredCapacity = 0; 
  Obj** deferred = NULL; 

  while (vm.zctCount > 0) {
    Obj* object = vm.zct[--vm.zctCount]; 
    object->inZCT = false; 

    if (object->refCount > 0) continue; 

    if (object->isMarked) { 
      if (deferredCapacity < deferredCount + 1) { 
        deferredCapacity = GROW_CAPACITY(deferredCapacity); 
        deferred = (Obj**)realloc(deferred, sizeof(Obj*) * deferredCapacity); 
        if (deferred == NULL) exit(1);
      } 
      deferred[deferredCount++] = object; 
      continue; 
    } 

    freeObject(object); 
  } 

  for (int i = 0; i < deferredCount; i++) {
    addToZCT(deferred[i]); 
  } 
  free(deferred); 

  setRootMarks(false); 

  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif
}

void freeObjects(void) {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }
}
