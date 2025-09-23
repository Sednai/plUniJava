
package ai.sedn.plunijava;

import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import static java.lang.foreign.ValueLayout.JAVA_DOUBLE;
import static java.lang.foreign.ValueLayout.JAVA_FLOAT;
import static java.lang.foreign.ValueLayout.JAVA_BOOLEAN;
import static java.lang.foreign.ValueLayout.JAVA_BYTE;
import static java.lang.foreign.ValueLayout.JAVA_CHAR;
import static java.lang.foreign.ValueLayout.JAVA_LONG;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SymbolLookup;
import java.lang.invoke.MethodHandle;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.GroupLayout;
import java.lang.invoke.VarHandle;
import java.nio.charset.StandardCharsets;
import java.lang.foreign.SequenceLayout;
import java.lang.foreign.SegmentAllocator;

import java.sql.*;


public class PlUniJava {
	private Arena arena;
	
	private MethodHandle lib_connect;
	private MethodHandle lib_disconnect;
	private MethodHandle lib_execute;
	private MethodHandle lib_fetch_next_double_array;
	private MethodHandle lib_fetch_next;
	private MethodHandle lib_getdouble;
	private MethodHandle lib_getfloat;
	private MethodHandle lib_getint;
	private MethodHandle lib_getboolean;
	private MethodHandle lib_getlong;
	private MethodHandle lib_getstring;
	private MethodHandle lib_getintarray;
	private MethodHandle lib_getdoublearray;
	private MethodHandle lib_getfloatarray;
	private MethodHandle lib_getvector;
	private MethodHandle lib_getfloatmultiarray;

	private GroupLayout arrayLayout = MemoryLayout.structLayout(
			ADDRESS.withName("arr"),
			JAVA_INT.withName("size")
	);
	
	private GroupLayout multiarrayLayout = MemoryLayout.structLayout(
			ADDRESS.withName("arr"),
			JAVA_INT.withName("size"),
			JAVA_INT.withName("Nd"),
			ADDRESS.withName("dims")
	);
	
	private VarHandle resultSize = arrayLayout.varHandle(MemoryLayout.PathElement.groupElement("size"));
	private VarHandle resultArr = arrayLayout.varHandle(MemoryLayout.PathElement.groupElement("arr"));
	
	private VarHandle mresultSize = multiarrayLayout.varHandle(MemoryLayout.PathElement.groupElement("size"));
	private VarHandle mresultArr = multiarrayLayout.varHandle(MemoryLayout.PathElement.groupElement("arr"));
	private VarHandle mresultNd = multiarrayLayout.varHandle(MemoryLayout.PathElement.groupElement("Nd"));
	private VarHandle mresultDims = multiarrayLayout.varHandle(MemoryLayout.PathElement.groupElement("dims"));
	
	public PlUniJava() {
		Linker linker = Linker.nativeLinker();		
		
		arena = Arena.ofConfined();
		
		SymbolLookup lib = SymbolLookup.libraryLookup("plunijava.so", arena);
	
		MemorySegment lib_connect_addr = lib.find("connect_SPI").get();
		FunctionDescriptor lib_connect_sig = FunctionDescriptor.of(JAVA_INT);
		
		lib_connect = linker.downcallHandle(lib_connect_addr, lib_connect_sig); 
			
		MemorySegment lib_disconnect_addr = lib.find("disconnect_SPI").get();
		FunctionDescriptor lib_disconnect_sig = FunctionDescriptor.ofVoid();
		lib_disconnect = linker.downcallHandle(lib_disconnect_addr, lib_disconnect_sig); 
		
		MemorySegment lib_execute_addr = lib.find("execute").get();
		FunctionDescriptor lib_execute_sig = FunctionDescriptor.of(JAVA_INT,ADDRESS,JAVA_BOOLEAN);
		lib_execute = linker.downcallHandle(lib_execute_addr, lib_execute_sig); 
		
		MemorySegment lib_fetch_next_double_array_addr = lib.find("fetch_next_double_array").get();
		FunctionDescriptor lib_fetch_next_double_array_sig = FunctionDescriptor.of(ADDRESS.withTargetLayout(arrayLayout),JAVA_INT);
		lib_fetch_next_double_array = linker.downcallHandle(lib_fetch_next_double_array_addr, lib_fetch_next_double_array_sig); 
		
		MemorySegment lib_fetch_next_addr = lib.find("fetch_next").get();
		FunctionDescriptor lib_fetch_next_sig = FunctionDescriptor.of(JAVA_BOOLEAN);
		lib_fetch_next = linker.downcallHandle(lib_fetch_next_addr, lib_fetch_next_sig); 
	
		MemorySegment lib_getdouble_addr = lib.find("getdouble").get();
		FunctionDescriptor lib_getdouble_sig = FunctionDescriptor.of(JAVA_DOUBLE,JAVA_INT);
		lib_getdouble = linker.downcallHandle(lib_getdouble_addr, lib_getdouble_sig); 
	
		MemorySegment lib_getfloat_addr = lib.find("getfloat").get();
		FunctionDescriptor lib_getfloat_sig = FunctionDescriptor.of(JAVA_FLOAT,JAVA_INT);
		lib_getfloat = linker.downcallHandle(lib_getfloat_addr, lib_getfloat_sig); 
	
		MemorySegment lib_getint_addr = lib.find("getint").get();
		FunctionDescriptor lib_getint_sig = FunctionDescriptor.of(JAVA_INT,JAVA_INT);
		lib_getint = linker.downcallHandle(lib_getint_addr, lib_getint_sig); 
	
		MemorySegment lib_getlong_addr = lib.find("getlong").get();
		FunctionDescriptor lib_getlong_sig = FunctionDescriptor.of(JAVA_LONG,JAVA_INT);
		lib_getlong = linker.downcallHandle(lib_getlong_addr, lib_getlong_sig); 
		
		MemorySegment lib_getboolean_addr = lib.find("getboolean").get();
		FunctionDescriptor lib_getboolean_sig = FunctionDescriptor.of(JAVA_BOOLEAN,JAVA_INT);
		lib_getboolean = linker.downcallHandle(lib_getboolean_addr, lib_getboolean_sig); 
		
		MemorySegment lib_getstring_addr = lib.find("getstring").get();
		FunctionDescriptor lib_getstring_sig = FunctionDescriptor.of(ADDRESS.withTargetLayout(arrayLayout),JAVA_INT);
		lib_getstring = linker.downcallHandle(lib_getstring_addr, lib_getstring_sig); 
	
		MemorySegment lib_getintarray_addr = lib.find("getintarray").get();
		FunctionDescriptor lib_getintarray_sig = FunctionDescriptor.of(ADDRESS.withTargetLayout(arrayLayout),JAVA_INT);
		lib_getintarray = linker.downcallHandle(lib_getintarray_addr, lib_getintarray_sig); 
	
		MemorySegment lib_getdoublearray_addr = lib.find("getdoublearray").get();
		FunctionDescriptor lib_getdoublearray_sig = FunctionDescriptor.of(ADDRESS.withTargetLayout(arrayLayout),JAVA_INT);
		lib_getdoublearray = linker.downcallHandle(lib_getdoublearray_addr, lib_getdoublearray_sig); 

		MemorySegment lib_getfloatarray_addr = lib.find("getfloatarray").get();
		FunctionDescriptor lib_getfloatarray_sig = FunctionDescriptor.of(ADDRESS.withTargetLayout(arrayLayout),JAVA_INT);
		lib_getfloatarray = linker.downcallHandle(lib_getfloatarray_addr, lib_getfloatarray_sig); 

		MemorySegment lib_getfloatmultiarray_addr = lib.find("getfloatmultiarray").get();
		FunctionDescriptor lib_getfloatmultiarray_sig = FunctionDescriptor.of(ADDRESS.withTargetLayout(multiarrayLayout),JAVA_INT);
		lib_getfloatmultiarray = linker.downcallHandle(lib_getfloatmultiarray_addr, lib_getfloatmultiarray_sig); 

		MemorySegment lib_getvector_addr = lib.find("getvector").get();
		FunctionDescriptor lib_getvector_sig = FunctionDescriptor.of(ADDRESS.withTargetLayout(arrayLayout),JAVA_INT);
		lib_getvector = linker.downcallHandle(lib_getvector_addr, lib_getvector_sig); 	
	}
	
	public void connect() throws Throwable {
		int ret = (int) lib_connect.invokeExact();
		
		if(ret != 0) {
			throw new Exception("Connection to db failed!"); 
		}
	}
	
	public void disconnect() throws Throwable {
		lib_disconnect.invokeExact();
	}
	
	public void execute(String query) throws Throwable {
		
		var cString = arena.allocateUtf8String(query);
		
		int ret = (int) lib_execute.invokeExact(cString,true);
		
		if(ret != 0) {
			throw new SQLException("Execution failed! ("+query+")"); 
		}
	}
	
	public void execute_nc(String query) throws Throwable {
		
		var cString = arena.allocateUtf8String(query);
		
		int ret = (int) lib_execute.invokeExact(cString,false);
		
		if(ret != 0) {
			throw new SQLException("Execution failed! ("+query+")"); 
		}
	}
	
	
	public double[] fetch_next_double_array(int column) throws Throwable{
		
		MemorySegment next = (MemorySegment) lib_fetch_next_double_array.invokeExact(column);  
	
		int size = (int) resultSize.get(next);
		
		if(size > 0) {
			MemorySegment ARR = (MemorySegment) resultArr.get(next);
			
			SequenceLayout L = MemoryLayout.sequenceLayout(size,JAVA_DOUBLE);
			ARR = ARR.reinterpret(L.byteSize());
			
			double[] ret = ARR.toArray(JAVA_DOUBLE);
			
			return ret;
		}
		
		return null;
	}
	
	public MemorySegment fetch_next_double_array_ms(int column) throws Throwable {
		
		MemorySegment next = (MemorySegment) lib_fetch_next_double_array.invokeExact(column);  
	
		int size = (int) resultSize.get(next);
		
		if(size > 0) {
			MemorySegment ARR = (MemorySegment) resultArr.get(next);
			
			SequenceLayout L = MemoryLayout.sequenceLayout(size,JAVA_DOUBLE);
			ARR = ARR.reinterpret(L.byteSize()).asReadOnly();
			
			return ARR;
		}
			
		return null;
	}
	
	
	public boolean fetch_next() throws Throwable {
		return (boolean) lib_fetch_next.invokeExact();		
	}

	public double getdouble(int column) throws Throwable {
		return (double) lib_getdouble.invokeExact(column);		
	}

	public float getfloat(int column) throws Throwable {
		return (float) lib_getfloat.invokeExact(column);		
	}
	
	public int getint(int column) throws Throwable {
		return (int) lib_getint.invokeExact(column);		
	}
	
	public long getlong(int column) throws Throwable {
		return (long) lib_getlong.invokeExact(column);		
	}
	
	public boolean getboolean(int column) throws Throwable {
		return (boolean) lib_getboolean.invokeExact(column);		
	}
	
	public String getstring(int column)  throws Throwable {
		MemorySegment next = (MemorySegment) lib_getstring.invokeExact(column);  
	
		int size = (int) resultSize.get(next);
		
		if(size > 0) {
			MemorySegment ARR = (MemorySegment) resultArr.get(next);
			
			SequenceLayout L = MemoryLayout.sequenceLayout(size,JAVA_BYTE);
			ARR = ARR.reinterpret(L.byteSize());
			
			byte[] ret = ARR.toArray(JAVA_BYTE);
		
			return new String(ret, StandardCharsets.UTF_8);
		}
		
		return null;	
	}

	public int[] getintarray(int column) throws Throwable{
		
		MemorySegment next = (MemorySegment) lib_getintarray.invokeExact(column);  
	
		int size = (int) resultSize.get(next);
		
		if(size > 0) {
			MemorySegment ARR = (MemorySegment) resultArr.get(next);
			
			SequenceLayout L = MemoryLayout.sequenceLayout(size,JAVA_INT);
			ARR = ARR.reinterpret(L.byteSize());
			
			int[] ret = ARR.toArray(JAVA_INT);
		
			return ret;
		}
		
		return null;
	}

	public double[] getdoublearray(int column) throws Throwable{
		
		MemorySegment next = (MemorySegment) lib_getdoublearray.invokeExact(column);  
	
		int size = (int) resultSize.get(next);
		
		if(size > 0) {
			MemorySegment ARR = (MemorySegment) resultArr.get(next);
			
			SequenceLayout L = MemoryLayout.sequenceLayout(size,JAVA_DOUBLE);
			ARR = ARR.reinterpret(L.byteSize());
			
			double[] ret = ARR.toArray(JAVA_DOUBLE);
		
			return ret;
		}
		
		return null;
	}

	public float[] getfloatarray(int column) throws Throwable{
		
		MemorySegment next = (MemorySegment) lib_getfloatmultiarray.invokeExact(column);  
	
		int size = (int) resultSize.get(next);
		
		if(size > 0) {
			MemorySegment ARR = (MemorySegment) resultArr.get(next);
			
			SequenceLayout L = MemoryLayout.sequenceLayout(size,JAVA_FLOAT);
			ARR = ARR.reinterpret(L.byteSize());
			
			float[] ret = ARR.toArray(JAVA_FLOAT);
		
			return ret;
		}
		
		return null;
	}
	
	public float[][] getfloat2darray(int column) throws Throwable {
		
		MemorySegment next = (MemorySegment) lib_getfloatmultiarray.invokeExact(column);  
	
		int size = (int) mresultSize.get(next);
		int Nd = (int) mresultNd.get(next);
		
		if(size > 0 && Nd == 2) {
			MemorySegment ARR = (MemorySegment) mresultArr.get(next);
			MemorySegment DIMS = (MemorySegment) mresultDims.get(next);
			ARR = ARR.reinterpret(size*4);

			SequenceLayout LD = MemoryLayout.sequenceLayout(Nd,JAVA_INT);
			DIMS = DIMS.reinterpret(LD.byteSize());
			
			int[] dims = DIMS.toArray(JAVA_INT);
			float[][] RET = new float[dims[0]][];
		
			SequenceLayout LA = MemoryLayout.sequenceLayout(dims[1],JAVA_FLOAT);
			
			for(int i = 0; i < dims[0]; i++) {
				MemorySegment R = ARR.asSlice(i*dims[1]*4, dims[1]*4);
				R = R.reinterpret(LA.byteSize());
				RET[i] = R.toArray(JAVA_FLOAT);
			}
	
			return RET;
		} else {
			if(Nd != 2) {
				throw new Exception("Array needs to be 2 dimensional, not "+Nd+"d."); 
			}
		}
		
		return null;
	}

	
	public float[] getvector(int column) throws Throwable{
		
		MemorySegment next = (MemorySegment) lib_getvector.invokeExact(column);  
	
		int size = (int) resultSize.get(next);
		
		if(size > 0) {
			MemorySegment ARR = (MemorySegment) resultArr.get(next);
			
			SequenceLayout L = MemoryLayout.sequenceLayout(size,JAVA_FLOAT);
			ARR = ARR.reinterpret(L.byteSize());
			
			float[] ret = ARR.toArray(JAVA_FLOAT);
		
			return ret;
		}
		
		return null;
	}
}

