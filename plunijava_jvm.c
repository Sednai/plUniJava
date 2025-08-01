#include "pg_config.h"
#include "pg_config_manual.h"

#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "catalog/pg_type.h"
#include <dlfcn.h>
#include "plunijava_jvm.h"
#include "utils/guc.h"

#include "utils/tuplestore.h"
#include "utils/builtins.h"

JNIEnv *jenv;
JavaVM *jvm;

ArrayType* createArray(jsize nElems, size_t elemSize, Oid elemType, bool withNulls);
ArrayType* create2dArray(jsize dim1, jsize dim2, size_t elemSize, Oid elemType, bool withNulls);

Datum build_datum_from_return_field(bool* primitive, jobject data, jclass cls, const char* fieldname, const char* sig, char* error_msg);

JavaVMOption* setJVMoptions(int* numOptions);
char** readOptions(char* filename, int* N);

const char* convert_name_to_JNI_signature(const char* name, char* error_msg) {
    // Arrays
    if(name[0] == '[') {
        if(name[1] != '[') {
            switch(name[1]) {
                case 'B':
                case 'I':
                case 'J':
                case 'S':
                case 'F':
                case 'D':
                case 'L':
                    return name;
            }
        } else {
            switch(name[2]) {
                case 'I':
                case 'J': 
                case 'S':
                case 'F':
                case 'D':
                case 'L':
                    return name;
            }
        }
    } else {
        // Natives
        if (strcmp(name, "int") == 0) {
            return "I";
        } else if (strcmp(name, "double") == 0) {
            return "D";
        } else if (strcmp(name, "float") == 0) {
            return "F";
        } else if (strcmp(name, "long") == 0) {
            return "J";
        // Objects
        } else if (strcmp(name, "java.lang.String") == 0 ) {
            return "Ljava/lang/String;";
        }
    }
    
    snprintf(error_msg, 128, "Unsupported Java type: %s",name);
    return NULL;
}

ArrayType* createArray(jsize nElems, size_t elemSize, Oid elemType, bool withNulls)
{
	ArrayType* v;
	Size nBytes = elemSize * nElems;
	//MemoryContext currCtx = Invocation_switchToUpperContext();

	Size dataoffset;
	if(withNulls)
	{
		dataoffset = ARR_OVERHEAD_WITHNULLS(1, nElems);
		nBytes += dataoffset;
	}
	else
	{
		dataoffset = 0;			/* marker for no null bitmap */
		nBytes += ARR_OVERHEAD_NONULLS(1);
	}

	v = (ArrayType*)palloc0(nBytes);
	AssertVariableIsOfType(v->dataoffset, int32);
	v->dataoffset = (int32)dataoffset;
	//MemoryContextSwitchTo(currCtx);

	SET_VARSIZE(v, nBytes);
	ARR_NDIM(v) = 1;
	ARR_ELEMTYPE(v) = elemType;
	*((int*)ARR_DIMS(v)) = nElems;
	*((int*)ARR_LBOUND(v)) = 1;

	return v;
}

ArrayType* create2dArray(jsize dim1, jsize dim2, size_t elemSize, Oid elemType, bool withNulls)
{
	ArrayType* v;
	jsize nElems = dim1*dim2;
	Size nBytes = nElems * elemSize;
	
	Size dataoffset;
	if(withNulls)
	{
		dataoffset = ARR_OVERHEAD_WITHNULLS(dim1, nElems);
		nBytes += dataoffset;
	}
	else
	{
		dataoffset = 0;			/* marker for no null bitmap */
		nBytes += ARR_OVERHEAD_NONULLS(dim1);
	}
	v = (ArrayType*)palloc0(nBytes);
	AssertVariableIsOfType(v->dataoffset, int32);
	v->dataoffset = (int32)dataoffset;
	
	SET_VARSIZE(v, nBytes);

	ARR_NDIM(v) = 2;
	ARR_ELEMTYPE(v) = elemType;
	ARR_DIMS(v)[0] = dim1;
	ARR_DIMS(v)[1] = dim2;
	ARR_LBOUND(v)[0] = 1;
	ARR_LBOUND(v)[1] = 1;
	
	return v;
}

int set_jobject_field_from_datum(jobject* obj, jfieldID* fid, Datum* dat, const char* sig) {
    if(sig[0] != '[') {
        
        switch(sig[0]) {
            // Natives
            case 'Z':       
               (*jenv)->SetBooleanField(jenv, *obj , *fid, DatumGetBool( *dat ) );
                return 0;
            case 'I':       
               (*jenv)->SetIntField(jenv, *obj , *fid, DatumGetInt32( *dat ) );
                return 0;
            case 'J':       
                (*jenv)->SetLongField(jenv, *obj , *fid, DatumGetInt64( *dat ) );
                return 0;
            case 'S':       
               (*jenv)->SetShortField(jenv, *obj , *fid, DatumGetInt16( *dat ) );
                return 0;
            case 'D':       
               (*jenv)->SetDoubleField(jenv, *obj , *fid, DatumGetFloat8( *dat ) );
                return 0;
            case 'F':       
               (*jenv)->SetFloatField(jenv, *obj , *fid, DatumGetFloat4( *dat ) );
                return 0;
            // String
            case 'L':
                if(strcmp(sig,"Ljava/lang/String;") == 0) {
                    
                    text* txt = DatumGetTextP( *dat );                            
                    int len = VARSIZE_ANY_EXHDR(txt)+1;
                    char t[len];
                
                    text_to_cstring_buffer(txt, &t[0], len);
                    
                    jstring string = (*jenv)->NewStringUTF(jenv, t);
                    
                    (*jenv)->SetObjectField(jenv, *obj , *fid, string);
                    
                    // Memory leak ?
                    
                    return 0;
                }          
        }
    } else {
        if(sig[1] != '[') {
            // 1D arrays
            ArrayType* v;
            switch(sig[1]) {
                case 'B':
                    bytea* bytes  = DatumGetByteaP( *dat );
                    jsize  nElems = VARSIZE(bytes) - sizeof(int32);
                    jbyteArray byteArray  =(*jenv)->NewByteArray(jenv,nElems);
                    (*jenv)->SetByteArrayRegion(jenv, byteArray, 0, nElems, (jbyte*)VARDATA(bytes));
                    (*jenv)->SetObjectField(jenv, *obj , *fid, byteArray );
                    return 0;                              
                case 'D':
                    v = DatumGetArrayTypeP( *dat );
                    if(!ARR_HASNULL(v)) {
                        jsize      nElems = (jsize)ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v));
                        jdoubleArray doubleArray = (*jenv)->NewDoubleArray(jenv,nElems);
                        (*jenv)->SetDoubleArrayRegion(jenv, doubleArray, 0, nElems, (jdouble *)ARR_DATA_PTR(v));
                        (*jenv)->SetObjectField(jenv, *obj , *fid, doubleArray );
                        return 0;
                    } else {
                        // Copy element by element 
                        //...
                        elog(ERROR,"Array has NULLs");
                    }
                    break;
                case 'F':
                    v = DatumGetArrayTypeP( *dat );
                    if(!ARR_HASNULL(v)) {
                        jsize      nElems = (jsize)ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v));
                        jfloatArray floatArray = (*jenv)->NewFloatArray(jenv,nElems);
                        (*jenv)->SetFloatArrayRegion(jenv, floatArray, 0, nElems, (jfloat *)ARR_DATA_PTR(v));
                        (*jenv)->SetObjectField(jenv, *obj , *fid, floatArray );
                        return 0;
                    } else {
                        // Copy element by element 
                        //...
                        elog(ERROR,"Array has NULLs");
                    }
                    break;
            }
        }
    }

    elog(ERROR,"Datum can not be converted to Java object (%s)",sig);
    return -1;
}

Datum build_datum_from_return_field(bool* primitive, jobject data, jclass cls, const char* fieldname, const char* sig, char* error_msg) {
    jfieldID fid = (*jenv)->GetFieldID(jenv,cls,fieldname,sig);       
    if(sig[0] != '[') {
        // Natives
        *primitive = true;
        switch(sig[0]) {
            case 'I':
                return Int32GetDatum(  (*jenv)->GetIntField(jenv,data,fid) );
            case 'J':
                return Int64GetDatum(  (*jenv)->GetLongField(jenv,data,fid) );
            case 'F':
                return Float4GetDatum(  (*jenv)->GetFloatField(jenv,data,fid) );
            case 'D':
                return Float8GetDatum(  (*jenv)->GetDoubleField(jenv,data,fid) );
            case 'Z':
                return BoolGetDatum( (*jenv)->GetBooleanField(jenv,data,fid) );
            case 'L':
                *primitive = false;
                jstring string = (*jenv)->GetObjectField(jenv,data,fid);
                const char *nativeString = (*jenv)->GetStringUTFChars(jenv, string, 0);
                
                int len = strlen(nativeString);
                text       *result = (text *) palloc(len + VARHDRSZ);
                SET_VARSIZE(result, len + VARHDRSZ);
                memcpy(VARDATA(result), nativeString, len);

                (*jenv)->ReleaseStringUTFChars(jenv, string, nativeString);

                return PointerGetDatum( result );
        }
    } else {
        // Native arrays
        *primitive = false;
        if(sig[1] != '[') {
            jarray arr;
            int nElems;
            ArrayType* v;
            arr = (jarray) (*jenv)->GetObjectField(jenv,data,fid);
            if(arr != 0) {
                nElems = (*jenv)->GetArrayLength(jenv, arr) ; 
            } else {
                nElems = 0;
            }

            switch(sig[1]) {
                case 'B':
                    bytea* b = (bytea*)palloc(nElems + sizeof(int32));
                    SET_VARSIZE(b, nElems + sizeof(int32));
                    if(nElems > 0) {
                        (*jenv)->GetByteArrayRegion(jenv,arr, 0, nElems, (jbyte*)VARDATA(b));
                        (*jenv)->DeleteLocalRef(jenv,arr);
                    }
                    return PointerGetDatum(b); 
                case 'I':
                    v = createArray(nElems, sizeof(jint), INT4OID, false);
                    if(nElems > 0) {
                        (*jenv)->GetIntArrayRegion(jenv,arr, 0, nElems, (jint*)ARR_DATA_PTR(v));
                        (*jenv)->DeleteLocalRef(jenv,arr);
                    }
                    return PointerGetDatum(v);
                case 'J':
                    v = createArray(nElems, sizeof(jlong), INT8OID, false);
                    if(nElems > 0) {
                        (*jenv)->GetLongArrayRegion(jenv,arr, 0, nElems, (jlong*)ARR_DATA_PTR(v));
                        (*jenv)->DeleteLocalRef(jenv,arr);
                    }
                    return PointerGetDatum(v);
                case 'S':
                    v = createArray(nElems, sizeof(jshort), INT2OID, false);
                    if(nElems > 0) {
                        (*jenv)->GetShortArrayRegion(jenv,arr, 0, nElems, (jshort*)ARR_DATA_PTR(v));
                        (*jenv)->DeleteLocalRef(jenv,arr);
                    }
                    return PointerGetDatum(v);
                case 'F':
                    v = createArray(nElems, sizeof(jfloat), FLOAT4OID, false);
                    if(nElems > 0) {
                        (*jenv)->GetFloatArrayRegion(jenv,arr, 0, nElems, (jfloat*)ARR_DATA_PTR(v));
                        (*jenv)->DeleteLocalRef(jenv,arr);
                    }
                    return PointerGetDatum(v); 
                case 'D': 
                    v = createArray(nElems, sizeof(jdouble), FLOAT8OID, false);
                    if(nElems > 0) {
                        (*jenv)->GetDoubleArrayRegion(jenv,arr, 0, nElems, (jdouble*)ARR_DATA_PTR(v));
                        (*jenv)->DeleteLocalRef(jenv,arr);
                    }
                    return PointerGetDatum(v); 
            }
        } else {
            jarray arr;
            int nElems;
            jarray arr0;
            jsize dim2;
            ArrayType* v;
           
            arr = (jarray) (*jenv)->GetObjectField(jenv,data,fid);
            if(arr != 0) { 
                nElems = (*jenv)->GetArrayLength(jenv, arr); 
                arr0 = (jarray) (*jenv)->GetObjectArrayElement(jenv,arr,0); 
            } else {
                nElems = 1;
                arr0 = 0;
            }

            if(arr0 == 0) {
                dim2 = 0;
                nElems = 1;
            } else
                dim2 =  (*jenv)->GetArrayLength(jenv, arr0); 
            
            switch(sig[2]) {
                case 'F':
            
                    v = create2dArray(nElems, dim2, sizeof(jfloat), FLOAT4OID, false);

                    if(dim2 > 0) {
                        // Copy first dim
                        (*jenv)->GetFloatArrayRegion(jenv, arr0, 0, dim2, (jfloat*)ARR_DATA_PTR(v));
                        (*jenv)->DeleteLocalRef(jenv,arr0);
                        
                        // Copy remaining
                        for(int i = 1; i < nElems; i++) {
                            jfloatArray els =  (jfloatArray) (*jenv)->GetObjectArrayElement(jenv,arr,i); 
                            (*jenv)->GetFloatArrayRegion(jenv, els, 0, dim2,  (jfloat*) (ARR_DATA_PTR(v)+i*dim2*sizeof(jfloat)) );
                            (*jenv)->DeleteLocalRef(jenv,els);                  
                        }
                       
                    }
                    (*jenv)->DeleteLocalRef(jenv,arr);
                    return PointerGetDatum(v);
                case 'D':
            
                    v = create2dArray(nElems, dim2, sizeof(jdouble), FLOAT8OID, false);

                    if(dim2 > 0) {
                        // Copy first dim
                        (*jenv)->GetDoubleArrayRegion(jenv, arr0, 0, dim2, (jdouble*)ARR_DATA_PTR(v));
                        (*jenv)->DeleteLocalRef(jenv,arr0);
                        // Copy remaining
                        for(int i = 1; i < nElems; i++) {
                            jdoubleArray els =  (jdoubleArray) (*jenv)->GetObjectArrayElement(jenv,arr,i); 
                            (*jenv)->GetDoubleArrayRegion(jenv, els, 0, dim2,  (jdouble*) (ARR_DATA_PTR(v)+i*dim2*sizeof(jdouble)) );
                            (*jenv)->DeleteLocalRef(jenv,els);
                        }
                    }
                    (*jenv)->DeleteLocalRef(jenv,arr);
                    return PointerGetDatum(v);

                case 'I':
                 
                    v = create2dArray(nElems, dim2, sizeof(jint), INT4OID, false);

                    if(dim2 > 0) {
                        // Copy first dim
                        (*jenv)->GetIntArrayRegion(jenv, arr0, 0, dim2, (jint*)ARR_DATA_PTR(v));
                        (*jenv)->DeleteLocalRef(jenv,arr0);
                        // Copy remaining
                        for(int i = 1; i < nElems; i++) {
                            jintArray els =  (jintArray) (*jenv)->GetObjectArrayElement(jenv,arr,i); 
                            (*jenv)->GetIntArrayRegion(jenv, els, 0, dim2,  (jint*) (ARR_DATA_PTR(v)+i*dim2*sizeof(jint)) );
                            (*jenv)->DeleteLocalRef(jenv,els);
                        }
                    }
                    (*jenv)->DeleteLocalRef(jenv,arr);
                    return PointerGetDatum(v);

                case 'J':
              
                    v = create2dArray(nElems, dim2, sizeof(jlong), INT8OID, false);

                    if(dim2 > 0) {
                        // Copy first dim
                        (*jenv)->GetLongArrayRegion(jenv, arr0, 0, dim2, (jlong*)ARR_DATA_PTR(v));
                        (*jenv)->DeleteLocalRef(jenv,arr0);
                        // Copy remaining
                        for(int i = 1; i < nElems; i++) {
                            jlongArray els =  (jlongArray) (*jenv)->GetObjectArrayElement(jenv,arr,i); 
                            (*jenv)->GetLongArrayRegion(jenv, els, 0, dim2,  (jlong*) (ARR_DATA_PTR(v)+i*dim2*sizeof(jlong)) );
                            (*jenv)->DeleteLocalRef(jenv,els);
                        }
                    }
                    (*jenv)->DeleteLocalRef(jenv,arr);
                    return PointerGetDatum(v);

                case 'S':
                    
                    v = create2dArray(nElems, dim2, sizeof(jshort), INT2OID, false);

                    if(dim2 > 0) {
                        // Copy first dim
                        (*jenv)->GetShortArrayRegion(jenv, arr0, 0, dim2, (jshort*)ARR_DATA_PTR(v));
                        (*jenv)->DeleteLocalRef(jenv,arr0);
                        // Copy remaining
                        for(int i = 1; i < nElems; i++) {
                            jshortArray els =  (jshortArray) (*jenv)->GetObjectArrayElement(jenv,arr,i); 
                            (*jenv)->GetShortArrayRegion(jenv, els, 0, dim2,  (jshort*) (ARR_DATA_PTR(v)+i*dim2*sizeof(jshort)) );
                            (*jenv)->DeleteLocalRef(jenv,els);
                        }
                    }
                    (*jenv)->DeleteLocalRef(jenv,arr);
                    return PointerGetDatum(v);
            }
        }

    }

    snprintf(error_msg, 256, "Unsupported Java signature %s in composite return",sig);
    return (Datum) 0;
}

// ToDo: Iterator / setof multi-row return
int call_java_function(Datum* values, bool* primitive, char* class_name, char* method_name, char* signature, char* return_type, jvalue* args, char* error_msg) {
    jmethodID methodID;

    // Prep and call function
    jclass clazz = (*jenv)->FindClass(jenv, class_name);

    if(clazz == NULL) {
        elog(WARNING,"Java class %s not found !",class_name);
        snprintf(error_msg, 128, "Java class %s not found", class_name);
        return -1;
    }

    methodID = (*jenv)->GetStaticMethodID(jenv, clazz, method_name, signature);

    if(methodID == NULL) {
        elog(WARNING,"Java method %s with signature %s not found",method_name, signature);
        snprintf(error_msg, 128, "Java method %s with signature %s not found",method_name, signature);
        return -2;
    }
       
    // Note: Keep non-switch for now for future extension to arrays

    if(strcmp(return_type, "J") == 0) {
    
        jlong ret = (*jenv)->CallStaticLongMethodA(jenv, clazz, methodID, args);
        
        // Catch exception
        if( (*jenv)->ExceptionCheck(jenv) ) {
            return 1;
        }
        
        primitive[0] = true;
        values[0] = Int64GetDatum( ret );
    
        return 0;

    } else if(strcmp(return_type, "I") == 0) {
        jint ret = (*jenv)->CallStaticIntMethodA(jenv, clazz, methodID, args);
      
        // Catch exception
        if( (*jenv)->ExceptionCheck(jenv) ) {
            return 1;
        }
        
        primitive[0] = true;
        values[0] = Int32GetDatum( ret );
    
        return 0;
    
    } else if(strcmp(return_type, "D") == 0) {
    
        jdouble ret = (*jenv)->CallStaticDoubleMethodA(jenv, clazz, methodID, args);
        
        // Catch exception
        if( (*jenv)->ExceptionCheck(jenv) ) {
            return 1;
        }
        
        primitive[0] = true;
        values[0] = Float8GetDatum( ret );
    
        return 0;
    
    } else if(strcmp(return_type, "F") == 0) {
    
        jfloat ret = (*jenv)->CallStaticFloatMethodA(jenv, clazz, methodID, args);
        
        // Catch exception
        if( (*jenv)->ExceptionCheck(jenv) ) {
            return 1;
        }
        
        primitive[0] = true;
        values[0] = Float4GetDatum( ret );
    
        return 0;
    
    } else if(strcmp(return_type, "Z") == 0) {
    
        jboolean ret = (*jenv)->CallStaticBooleanMethodA(jenv, clazz, methodID, args);
        
        // Catch exception
        if( (*jenv)->ExceptionCheck(jenv) ) {
            return 1;
        }
        
        primitive[0] = true;
        values[0] = BoolGetDatum( ret );
    
        return 0;

    } else if(strcmp(return_type, "V") == 0) {
        (*jenv)->CallStaticVoidMethodA(jenv, clazz, methodID, args);
        
        // Catch exception
        if( (*jenv)->ExceptionCheck(jenv) ) {
            return 1;
        }

        return 0;

    } else if(strcmp(return_type, "Ljava/lang/String;") == 0) {
        const char* str;
        int len;
        text *result;

        jobject ret = (*jenv)->CallStaticObjectMethodA(jenv, clazz, methodID, args);
     
        // Catch exception
        if( (*jenv)->ExceptionCheck(jenv) ) {
            return 1;
        }
        
        if(ret == NULL) {
            strcpy(error_msg,"Null pointer returned from java function call");
            return -3;
        }
        
        str =  (*jenv)->GetStringUTFChars(jenv, ret, false);
        
        len = strlen(str);
        result = (text *) palloc(len + VARHDRSZ);
        SET_VARSIZE(result, len + VARHDRSZ);
        memcpy(VARDATA(result), str, len);

        (*jenv)->ReleaseStringUTFChars(jenv, ret, str);

        values[0] = (Datum) result; 
        
        return 0;

    /*} 
        // TO BE DONE ...
        // Problem for BG worker: limited space in queue package -> Distribute over several

        else if(strcmp(return_type,"ITER") == 0) {
        
        jobject ret = (*jenv)->CallStaticObjectMethodA(jenv, clazz, methodID, args);

        // Catch exception
        if( (*jenv)->ExceptionCheck(jenv) ) {
            return 1;
        }

        // Analyis return
        jclass cls = (*jenv)->GetObjectClass(jenv, ret);
        
        // Iterator
        jmethodID hasNext = (*jenv)->GetMethodID(jenv, cls, "hasNext", "()Z");
	    jmethodID next = (*jenv)->GetMethodID(jenv, cls, "next", "()Ljava/lang/Object;");

        if(hasNext == NULL || next == NULL) {
            // ToDo: CLEAN RETURN WITH ERROR MSG !
            elog(ERROR,"Object returned is not iterator");
        }

        bool hasNext = (bool) (*jenv)->CallBooleanMethod(jenv, cls, hasNext);
 
        if(hasNext) {
            // ToDo: Set function to return one item per call ?        
        
        }
    */    
    }  else {
        jclass cls;
        jmethodID getFields;
        jobjectArray fieldsList;
        jsize len;

        jobject ret = (*jenv)->CallStaticObjectMethodA(jenv, clazz, methodID, args);
     
        // Catch exception
        if( (*jenv)->ExceptionCheck(jenv) ) {
            return 1;
        }
        
        if(ret == NULL) {
            strcpy(error_msg,"Null pointer returned from java function call");
            return -3;
        }

        // Analyis return
        // ToDo: Move into helper function
        cls = (*jenv)->GetObjectClass(jenv, ret);

        getFields = (*jenv)->GetMethodID(jenv, (*jenv)->GetObjectClass(jenv,cls), "getFields", "()[Ljava/lang/reflect/Field;");

        fieldsList = (jobjectArray)  (*jenv)->CallObjectMethod(jenv, cls, getFields); 
        
        len =  (*jenv)->GetArrayLength(jenv,fieldsList);

        if(len == 0) {
                strcpy(error_msg,"Empty composite return");
                return -4;
        } else {
            // Composite return
            for(int i = 0; i < len; i++) {
                
                // ToDo: Move into helper function ?

                // Detect field
                jobject field = (*jenv)->GetObjectArrayElement(jenv, fieldsList, i);
                jclass fieldClass = (*jenv)->GetObjectClass(jenv, field);
            
                // Obtain signature
                jmethodID m =  (*jenv)->GetMethodID(jenv, fieldClass, "getName", "()Ljava/lang/String;");   
                jstring jstr = (jstring)(*jenv)->CallObjectMethod(jenv, field, m);
            
                const char* fieldname =  (*jenv)->GetStringUTFChars(jenv, jstr, false);
            
                m =  (*jenv)->GetMethodID(jenv, fieldClass, "getType", "()Ljava/lang/Class;");   
                jobject value = (*jenv)->CallObjectMethod(jenv, field, m);
                jclass  valueClass = (*jenv)->GetObjectClass(jenv, value);

                m =  (*jenv)->GetMethodID(jenv, valueClass, "getName", "()Ljava/lang/String;");   
                jstring jstr2 = (jstring)(*jenv)->CallObjectMethod(jenv, value, m);
                const char* typename =  (*jenv)->GetStringUTFChars(jenv, jstr2, false);
            
                const char* sig = convert_name_to_JNI_signature(typename, error_msg);
                if(sig == NULL) {
                    // Cleanup
                    (*jenv)->ReleaseStringUTFChars(jenv, jstr, fieldname);
                    (*jenv)->ReleaseStringUTFChars(jenv, jstr2, typename);
                    return -4;
                }

                values[i] = build_datum_from_return_field(&primitive[i], ret, cls, fieldname, sig, error_msg);
                if(values == NULL) {
                    // Cleanup
                    (*jenv)->ReleaseStringUTFChars(jenv, jstr, fieldname);
                    (*jenv)->ReleaseStringUTFChars(jenv, jstr2, typename);
                    return -5;
                }

                // Cleanup
                (*jenv)->ReleaseStringUTFChars(jenv, jstr, fieldname);
                (*jenv)->ReleaseStringUTFChars(jenv, jstr2, typename);
            }
        }
    }

    return 0;
}

/*
    Call java function with iterator return (ONLY FOR FG WORKER !)
*/
int call_iter_java_function(Tuplestorestate* tupstore, TupleDesc tupdesc, char* class_name, char* method_name, char* signature, jvalue* args, char* error_msg) {
    jclass clazz;
    jmethodID methodID;
    jobject ret;
    jclass cls;
    
    jmethodID hasNextF;
    jmethodID nextF;
    
    bool hasNext;

    // Prep and call function
    clazz = (*jenv)->FindClass(jenv, class_name);

    if(clazz == NULL) {
        elog(WARNING,"Java class %s not found !",class_name);
        snprintf(error_msg, 128, "Java class %s not found", class_name);
        return -1;
    }

    methodID = (*jenv)->GetStaticMethodID(jenv, clazz, method_name, signature);

    if(methodID == NULL) {
        elog(WARNING,"Java method %s with signature %s not found !",method_name, signature);
        snprintf(error_msg, 128, "Java method %s with signature %s not found",method_name, signature);
        return -2;
    }

    ret = (*jenv)->CallStaticObjectMethodA(jenv, clazz, methodID, args);

    // Catch exception
    if( (*jenv)->ExceptionCheck(jenv) ) {
        return 1;
    }

    // Analyis return
    cls = (*jenv)->GetObjectClass(jenv, ret);
    
    // Iterator
    hasNextF = (*jenv)->GetMethodID(jenv, cls, "hasNext", "()Z");
    nextF = (*jenv)->GetMethodID(jenv, cls, "next", "()Ljava/lang/Object;");

    if(hasNextF == NULL || nextF == NULL) {
        strcpy(error_msg,"Java object returned is not an iterator");
        return -6;
    }
    
    hasNext = (bool) (*jenv)->CallBooleanMethod(jenv, ret, hasNextF);

    while(hasNext) {
        // Get row object        
        jobject row = (*jenv)->CallObjectMethod(jenv, ret, nextF);
        jclass rcls = (*jenv)->GetObjectClass(jenv, row);
        
        // Prepare row
        jmethodID getFields = (*jenv)->GetMethodID(jenv, (*jenv)->GetObjectClass(jenv, rcls), "getFields", "()[Ljava/lang/reflect/Field;");

        jobjectArray fieldsList = (jobjectArray)  (*jenv)->CallObjectMethod(jenv, rcls, getFields); 
  
        jsize len =  (*jenv)->GetArrayLength(jenv,fieldsList);
        
        if(len == 0) {
            elog(ERROR,"Empty composite return for iterator");
        } else {
            // Composite return
            
            Datum values[len];
            bool*  nulls = palloc0( len * sizeof( bool ) );
            bool* primitive = palloc0( len * sizeof( bool ) );
            
            // Composite return
            for(int i = 0; i < len; i++) {
                
                // Detect field
                jobject field = (*jenv)->GetObjectArrayElement(jenv, fieldsList, i);
                jclass fieldClass = (*jenv)->GetObjectClass(jenv, field);
              
                // Obtain signature
                jmethodID m =  (*jenv)->GetMethodID(jenv, fieldClass, "getName", "()Ljava/lang/String;");   
                jstring jstr = (jstring)(*jenv)->CallObjectMethod(jenv, field, m);
            
                const char* fieldname =  (*jenv)->GetStringUTFChars(jenv, jstr, false);
            
                m =  (*jenv)->GetMethodID(jenv, fieldClass, "getType", "()Ljava/lang/Class;");   
                jobject value = (*jenv)->CallObjectMethod(jenv, field, m);
                jclass  valueClass = (*jenv)->GetObjectClass(jenv, value);

                m =  (*jenv)->GetMethodID(jenv, valueClass, "getName", "()Ljava/lang/String;");   
                jstring jstr2 = (jstring)(*jenv)->CallObjectMethod(jenv, value, m);
                const char* typename =  (*jenv)->GetStringUTFChars(jenv, jstr2, false);
            
                const char* sig = convert_name_to_JNI_signature(typename, error_msg);
                if(sig == NULL) {
                    // Cleanup
                    (*jenv)->ReleaseStringUTFChars(jenv, jstr, fieldname);
                    (*jenv)->ReleaseStringUTFChars(jenv, jstr2, typename);
                
                    return -4;
                }

                values[i] = build_datum_from_return_field(&primitive[i], row, rcls, fieldname, sig, error_msg);
                if(values == NULL) {
                    // Cleanup
                    (*jenv)->ReleaseStringUTFChars(jenv, jstr, fieldname);
                    (*jenv)->ReleaseStringUTFChars(jenv, jstr2, typename);
                    return -5;
                }

                // Cleanup
                (*jenv)->ReleaseStringUTFChars(jenv, jstr, fieldname);
                (*jenv)->ReleaseStringUTFChars(jenv, jstr2, typename);
            }
            tuplestore_putvalues(tupstore, tupdesc, values, nulls);

            pfree(primitive);
        }

        // Next
        hasNext = (bool) (*jenv)->CallBooleanMethod(jenv, ret, hasNextF);
    }

    return 0;
}

/*
    Helper function to release jvalues
*/
void freejvalues(jvalue* jvals, short* argprim, int N) {
    for(int i = 0; i < N; i++) {
        if(argprim[i] == 0) 
            continue;
        else {
            if(argprim[i] == 1) 
                (*jenv)->DeleteLocalRef(jenv, jvals[i].l);
            else
                (*jenv)->ReleaseStringUTFChars(jenv, jvals[i].l, (*jenv)->GetStringUTFChars(jenv,jvals[i].l,false) );
        }
    }
}

/*
    Read JVM options from file
*/

char** readOptions(char* filename, int* N) {
    FILE *file;
    char **lines = NULL;
    char *line = NULL;
    char* newLine = NULL;
    char *p;
    size_t len = 0;
    size_t read;

    *N = -1;

    file = fopen(filename,"r");
    if(file == NULL) {
        elog(ERROR,"File %s not found",filename);
    }
   
    while((read = getline(&line,&len,file)) != -1) {
        if ((line[0] != '#') && (line[0] != '\n')) {
            (*N)++;
            
            // Insert = for add-exports
            p = strstr(line, "--add-exports ");
            if (p != NULL) {
                memcpy(p, "--add-exports=",14);
            }

            // Insert = for add-opens
            p = strstr(line, "--add-opens ");
            if (p != NULL) {
                memcpy(p, "--add-opens=",12);
            }

            // Alloc mem
            lines = (char**)realloc(lines, ( (*N)+1) * sizeof(char*));
            
            line[read-1] = '\0';
            newLine = (char*)malloc((read) * sizeof(char));
            strncpy(newLine,line,read);

            lines[*N] = newLine;
        }
    } 

    (*N)++;

    return lines;
}



/*
    Read JVM options from GUC
*/
JavaVMOption* setJVMoptions(int* numOptions) {
    JavaVMOption* opts;
    int No = 0;
    bool active = false;
    int spos = 0;
  
    // Read option string from GUC
    const char* OPTIONS = GetConfigOption("pluj.jvmoptions",true,true);

    if(OPTIONS == NULL) {
        elog(ERROR,"pluj.jvmoptions GUC not set");
    } 

    // Parse options
    opts = malloc( 1*sizeof(JavaVMOption) );
    
    for(int i=0; i < strlen(OPTIONS); i++) {
        if( (OPTIONS[i] == '-' || OPTIONS[i] == '@') && !active) { 
            active = true;
            spos = i;
            continue;
        }

        if( (OPTIONS[i] == ' ' || i == strlen(OPTIONS)-1) && active) {
            active = false;
            int len;
            if( i < strlen(OPTIONS)-1) {
                len = i-spos+1;
            } else {
                len = i-spos+2;
            }

            // Create option
            if(OPTIONS[spos] != '@') {
                
                char* buf = malloc(len); 
                strncpy(buf,&OPTIONS[spos],len);
                buf[len-1] = '\0';
            
                opts[No].optionString = buf;
                No++;

                opts = realloc(opts,No*sizeof(JavaVMOption));

            } else {
                // Read from file
                int N;
                char buf[len]; 
                strncpy(buf,&OPTIONS[spos],len);
                buf[len-1] = '\0';
                char **lines =  readOptions(&buf[1],&N);
                for(int l = 0; l < N; l++ ) {
              
                    opts[No].optionString = lines[l];
                    No++;

                    opts = realloc(opts,No*sizeof(JavaVMOption));
                }
            }

            continue;
        }

    }
    
    *numOptions = No;
    return opts;
} 

/*
    Java virtual machine startup
*/
int startJVM(char* error_msg) {
   
    int numOptions;
    void *jvmLibrary;
    JavaVMInitArgs vm_args;
    JavaVMOption *options;
    const char* JVM_SO_FILE;
    JNI_CreateJavaVM_func JNI_CreateJavaVM;
    jint result;

    elog(NOTICE,"Starting JVM");
     
    options = setJVMoptions(&numOptions);

    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = numOptions;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_FALSE;

    // Read location of libjvm from GUC
    JVM_SO_FILE = GetConfigOption("pluj.libjvm",true,true);

    if(JVM_SO_FILE == NULL) {
        strcpy(error_msg,"pluj.libjvm GUC pointing to libjvm.so not set");
        return -1;
    }

    jvmLibrary = dlopen(JVM_SO_FILE, RTLD_NOW | RTLD_GLOBAL);

    JNI_CreateJavaVM = (JNI_CreateJavaVM_func) dlsym(jvmLibrary, "JNI_CreateJavaVM");

    result = JNI_CreateJavaVM(&jvm, (void **)&jenv, &vm_args);
    
    if(result < 0) {
        snprintf(error_msg, 14, "JVM error %d",result);
    }

    elog(NOTICE,"JVM startup complete");

    // Free
    for(int i = 0; i < numOptions; i++) {
        free(options[i].optionString);
    }
    free(options);

    return result;
}

