package ai.sedn.plunijava;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Iterator;

public class Tests {

	/*
	 * Integer
	 */
	
	public static int test_int1(int in) throws SQLException  {
        return in;
    }
	
	public static int test_int2(int in1, int in2) throws SQLException {
        return in1+in2;
    }

	public static int test_int3(int[] in) throws SQLException {
        int ret = 0;
		for(int i = 0; i < in.length; i++) {
			ret += in[i];
        }
		return ret;
    }
	
	public static int test_int4(int[][] in) throws SQLException {
		int ret = 0;
		for(int i = 0; i < in.length; i++) {
			for(int j = 0; j < in[i].length; j++) {
				ret += in[i][j];
			}
        }
		return ret;
	}
	
	/*
	 * Float
	 */
	
	public static float test_float1(float in) throws SQLException  {
        return in;
    }
	
	public static float test_float2(float in1, float in2) throws SQLException {
        return in1+in2;
    }

	public static float test_float3(float[] in) throws SQLException {
        float ret = 0;
		for(int i = 0; i < in.length; i++) {
			ret += in[i];
        }
		return ret;
    }
	
	public static float test_float4(float[][] in) throws SQLException {
		float ret = 0;
		for(int i = 0; i < in.length; i++) {
			for(int j = 0; j < in[i].length; j++) {
				ret += in[i][j];
			}
        }
		return ret;
	}
	
	
	/*
	 * Double
	 */
	
	public static double test_double1(double in) throws SQLException  {
        return in;
    }
	
	public static double test_double2(double in1, double in2) throws SQLException {
        return in1+in2;
    }

	public static double test_double3(double[] in) throws SQLException {
        double ret = 0;
		for(int i = 0; i < in.length; i++) {
			ret += in[i];
        }
		return ret;
    }
	
	public static double test_double4(double[][] in) throws SQLException {
		double ret = 0;
		for(int i = 0; i < in.length; i++) {
			for(int j = 0; j < in[i].length; j++) {
				ret += in[i][j];
			}
        }
		return ret;
	}
	
	/*
	 * String
	 */
	public static String test_string1(String in) throws SQLException {
		return in;
	}
	
	public static String test_string2(String[] in) throws SQLException {
		return in[0];
	}
	
	/*
	 * Complex types
	 */
	public static TestType1 test_complextype1(int in1, double in2) {
		TestType1 R = new TestType1();
		
		R.A = in1;
		R.B = in2;
		
		return R;
	}
	
	public static TestType1 test_complextype2(TestType1[] in) {
		TestType1 R = new TestType1();
		
		R.A = in[0].A;
		R.B = in[0].B;
		
		return R;
	}
	
	public static TestType2 test_complextype3(TestType2 in) {
		TestType2 R = new TestType2();
		R.A = in.A;

		return R;
	}

	/*
	 * Setof return
	 */
	public static Iterator<TestType1> test_setof1(TestType1[] in) {
		ArrayList<TestType1> L = new ArrayList<TestType1>();
	
		for(int i = 0; i < in.length; i++) {
			TestType1 R = new TestType1();
			R.A = in[i].A;
			R.B = in[i].B;
			L.add(R);
		}
	
		return L.listIterator();
	}
	
	/*
	 * Non-JDBC
	 */
	public static Iterator test_njdbc1() throws Throwable {
		
		ArrayList<TestType1> L = new ArrayList<TestType1>();
		
		PlUniJava unij = new PlUniJava();
			
	    unij.connect();
	    
	    unij.execute("select id,data from test_table1");
	    
	    while(unij.fetch_next()) {
			TestType1 R = new TestType1();
	    	R.A = unij.getint(1);
			R.B = unij.getdoublearray(2)[0];
	    	
	    	L.add(R);
	    }
	    
	    unij.disconnect();
	
		return L.listIterator();
		
	}
	
}

