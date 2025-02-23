#pragma once
#include "util/types.h"
#include "math.h"

union v2
	{
		struct
		{
			f32 x,y;
		};
		struct
		{
			f32 u,v;
		};
		f32 E[2];
	};
	
	union v2u
	{
		struct
		{
			u32 x,y;
		};
		struct
		{
			u32 u,v;
		};
		u32 E[2];
	};
	
	union v2i
	{
		struct
		{
			i32 x,y;
		};
		struct
		{
			i32 u,v;
		};
		i32 E[2];
	};
	
	union v3
	{
		struct
		{
			f32 x,y,z;
		};
		struct
		{
			f32 r,g,b;
		};
		struct
		{
			f32 u,v,w;
		};
		struct
		{
			v2 xy;
			f32 Ignored0_;
		};
		struct
		{
			f32 Ignored1_;
			v2 yz;
		};
		struct
		{
			v2 uv;
			f32 Ignored2_;
		};
		struct
		{
			f32 Ignored3_;
			v2 yw;
		};
		f32 E[3];
	};
	
	
	union v4
	{
		struct
		{
			f32 x,y,z,w;
		};
		struct
		{
			union
			{
				v3 rgb;
				struct
				{
					f32 r,g,b;
				};
			};
			
			f32 a;
			
		};
		struct
		{
			v2 xy;
			f32 Ignored0_;
			f32 Ignored1_;
		};
		struct
		{
			f32 Ignored2_;
			v2 yz;
			f32 Ignored3_;
		};
		struct
		{
			f32 Ignored4_;
			f32 Ignored5_;
			v2 zw;
		};
		f32 E[4];
	};
	
	struct rectangle2
	{
		v2 Min;
		v2 Max;
	};
	
	struct rectangle3
	{
		v3 Min;
		v3 Max;
	};
	
	
	union mat3
	{
		struct 
		{
			f32 Value1_1;
			f32 Value1_2;
			f32 Value1_3;
			f32 Value2_1;
			f32 Value2_2;
			f32 Value2_3;
			f32 Value3_1;
			f32 Value3_2;
			f32 Value3_3;
		};
		f32 E[9];
	};
	
	union mat4
	{
		struct 
		{
			f32 Value1_1;
			f32 Value1_2;
			f32 Value1_3;
			f32 Value1_4;
			f32 Value2_1;
			f32 Value2_2;
			f32 Value2_3;
			f32 Value2_4;
			f32 Value3_1;
			f32 Value3_2;
			f32 Value3_3;
			f32 Value3_4;
			f32 Value4_1;
			f32 Value4_2;
			f32 Value4_3;
			f32 Value4_4;
		};
		
		f32 E[16];
		
		f32 TE[4][4];
	};

	struct quaternion
	{
		f32 x;
		f32 y;
		f32 z;
		f32 w;
	};

inline i32
SignOf(i32 Value)
{
	i32 Result = (Value >= 0) ? 1 : -1;
	
	return (Result);
}

inline f32
SignOf(f32 Value)
{
	f32 Result = (Value >= 0) ? 1.0f : -1.0f;
	
	return (Result);
}

inline f32
SquareRoot(f32 flt)
{
	f32 Result = sqrtf(flt);
	return (Result);
}

inline f32
AbsoluteValue(f32 flt)
{
	f32 Result = (f32)fabs(flt);
	return(Result);
}

inline u32
RotateLeft(u32 Value, i32 Amount)
{
	u32 Result = _rotl(Value, Amount);
	
	return(Result);
}

inline u32
RotateRight(u32 Value, i32 Amount)
{
	u32 Result = _rotr(Value, Amount);
	
	return(Result);
}

inline i32
Roundf32Toi32(f32 f32)
{
	i32 Result = (i32)roundf(f32);
	return(Result);
}

inline u32
Roundf32Tou32(f32 f32)
{
	u32 Result = (u32)roundf(f32);
	return(Result);
}

inline i32
Floorf32Toi32(f32 f32)
{
	i32 Result = (i32)floorf(f32);
	return(Result);
}

inline i32
Ceilf32Toi32(f32 f32)
{
	i32 Result = (i32)ceilf(f32);
	return(Result);
}

inline u32
Truncatef32Toi32(f32 f32)
{
	i32 Result = (i32)(f32);
	return(Result);
}

inline f32
Sin(f32 Angle)
{
	f32 Result = sinf(Angle);
	return(Result);
}

inline f32
Cos(f32 Angle)
{
	f32 Result = cosf(Angle);
	return (Result);
}

inline f32
ATan2(f32 Y, f32 X)
{
	f32 Result = atan2f(Y, X);
	return(Result);
}

inline f32
Tan(f32 Angle)
{
	f32 Result = tanf(Angle);
	return(Result);
}

inline v2i
V2i(i32 x, i32 y)
{
	v2i Result = { x, y};
	
	return(Result);
}

inline v2
V2i(u32 x, u32 y)
{
	v2 Result = {(f32)x, (f32)y};
	
	return(Result);
}

inline v2 
V2(f32 x, f32 y)
{
	v2 Result;
	
	Result.x = x;
	Result.y = y;
	
	return(Result);
}

inline v2 
V2(f32 value)
{
	v2 Result;
	
	Result.x = value;
	Result.y = value;
	
	return(Result);
}

inline v3
V3(f32 x, f32 y, f32 z)
{
	v3 Result;
	
	Result.x = x;
	Result.y = y;
	Result.z = z;
	
	return(Result);
}

inline v3
V3(v2 xy, f32 z)
{
	v3 Result;
	
	Result.x = xy.x;
	Result.y = xy.y;
	Result.z = z;
	
	return(Result);
}

inline v4 
V4(f32 x, f32 y, f32 z, f32 w)
{
	v4 Result;
	
	Result.x = x;
	Result.y = y;
	Result.z = z;
	Result.w = w;
	
	return(Result);
}

inline v3
V4(v2 XY, f32 Z)
{
	v3 Result;
	
	Result.xy = XY;
	Result.z = Z;
	
	return(Result);
}

inline v4
V4(v3 XYZ, f32 W)
{
	v4 Result;
	
	Result.rgb = XYZ;
	Result.w = W;
	
	return(Result);
}

//
// NOTE(Denis): Scalar operations
//

inline f32
Square(f32 A)
{
	f32 Result = A*A;
	
	return(Result);
}

inline f32
Lerp(f32 A, f32 t, f32 B)
{
	f32 Result = (1.0f-t)*A + B*t;
	
	return(Result);
}

inline f32
Clamp(f32 Min, f32 Value, f32 Max)
{
	f32 Result = Value;
	
	if(Result < Min)
	{
		Result = Min;
	}
	else if(Result > Max)
	{
		Result = Max;
	}
	
	return(Result);
}

inline f32
Clamp01(f32 Value)
{
	f32 Result = Clamp(0.0f,Value,1.0f);
	
	return(Result);
}

inline f32
Clamp01MapToRange(f32 Min, f32 t, f32 Max)
{
	f32 Result = 0.0f;
	f32 Range = Max - Min;
	if(Range != 0)
	{
		Result = Clamp01((t - Min) / Range);
	}
	
	return(Result);
}

inline f32
SafeRatioN(f32 Numerator, f32 Divisor, f32 N)
{
	f32 epsilon = 1e-6;
	f32 Result = N;
	
	if(Divisor != 0.0f)
	{
		Result = Numerator / Divisor;
	}
	
	return(Result);
}


inline f32
SafeRatio0(f32 Numerator, f32 Divisor)
{
	f32 Result = SafeRatioN(Numerator, Divisor, 0.0f);
	
	return(Result);
}


inline f32
SafeRatio1(f32 Numerator, f32 Divisor)
{
	f32 Result = SafeRatioN(Numerator, Divisor, 1.0f);
	
	return(Result);
}


//
// NOTE(Denis): v2 operations
//

inline v2
Perp(v2 A)
{
	v2 Result = {-A.y,A.x};
	return(Result);
}

inline v2 
operator*(f32 A, v2 B)
{
	v2 Result;
	
	Result.x = A*B.x;
	Result.y = A*B.y;
	
	return(Result);
}

inline v2 
operator*(v2 B, f32 A)
{
	v2 Result = A * B;
	
	return(Result);
}

inline v2 
operator/(v2 A, f32 B)
{
	v2 Result = V2(A.x / B, A.y / B);
	
	return(Result);
}

inline v2 
operator*(v2 B, v2 A)
{
	v2 Result = V2(A.x * B.x, A.y * B.y);
	
	return(Result);
}

inline v2 &
operator*=(v2 &B, f32 A)
{
	B = A * B;
	
	return(B);
}

inline v2 
operator-(v2 A)
{
	v2 Result;
	
	Result.x = -A.x;
	Result.y = -A.y;
	
	return(Result);
}

inline v2 
operator+(v2 A, v2 B)
{
	v2 Result;
	
	Result.x = A.x + B.x;
	Result.y = A.y + B.y;
	
	return(Result);
}

inline v2 &
operator+=(v2 &A, v2 B)
{
	A = A + B;
	
	return(A);
}

inline v2 
operator-(v2 A, v2 B)
{
	v2 Result;
	
	Result.x = A.x - B.x;
	Result.y = A.y - B.y;
	
	return(Result);
}

inline v2 &
operator-=(v2 &A, v2 B)
{
	A = A - B;
	
	return(A);
}

inline bool
operator==(v2 A, v2 B)
{
	bool Result = ((A.x == B.x) && (A.y == B.y));
	return Result;
}

inline v2
Hadamard(v2 A, v2 B)
{
	v2 Result = { A.x*B.x,A.y*B.y};
	
	return(Result);
}

inline f32
Inner(v2 A, v2 B)
{
	f32 Result = A.x*B.x + A.y*B.y;
	return(Result);
}

inline f32
LengthSq(v2 A)
{
	f32 Result = Inner(A,A);
	
	return(Result);
}

inline f32
Length(v2 A)
{
	f32 Result = SquareRoot(LengthSq(A));
	
	return(Result);
}

inline v2
Clamp01(v2  Value)
{
	v2 Result;
	
	Result.x = Clamp01(Value.x);
	Result.y = Clamp01(Value.y);
	
	return(Result);
}

inline v2
Arm2(f32 Angle)
{
	v2 Result = {Cos(Angle), Sin(Angle)};
	return Result;
}

inline v2
Lerp(v2 A, f32 t, v2 B)
{
	v2 Result = (1.0f - t)*A + t*B;
	
	return(Result);
}

inline v2
normalize(v2 A)
{
	v2 Result = A * (SafeRatio0(1.0f, Length(A)));
	
	return(Result);
}

//
// NOTE(Denis): v3 operations
//

inline v3 
operator*(f32 A, v3 B)
{
	v3 Result;
	
	Result.x = A*B.x;
	Result.y = A*B.y;
	Result.z = A*B.z;
	
	return(Result);
}

inline v3 
operator*(v3 B, f32 A)
{
	v3 Result = A * B;
	
	return(Result);
}

inline v3 &
operator*=(v3 &B, f32 A)
{
	B = A * B;
	
	return(B);
}

inline v3 
operator-(v3 A)
{
	v3 Result;
	
	Result.x = -A.x;
	Result.y = -A.y;
	Result.z = -A.z;
	
	return(Result);
}

inline v3 
operator+(v3 A, v3 B)
{
	v3 Result;
	
	Result.x = A.x + B.x;
	Result.y = A.y + B.y;
	Result.z = A.z + B.z;
	
	return(Result);
}

inline v3 &
operator+=(v3 &A, v3 B)
{
	A = A + B;
	
	return(A);
}

inline v3 
operator-(v3 A, v3 B)
{
	v3 Result;
	
	Result.x = A.x - B.x;
	Result.y = A.y - B.y;
	Result.z = A.z - B.z;
	
	return(Result);
}

inline v3 &
operator-=(v3 &A, v3 B)
{
	A = A - B;
	
	return(A);
}

inline bool
operator==(v3 A, v3 B)
{
	bool Result = ((A.x == B.x) && (A.y == B.y) && (A.z == B.z));
	return Result;
}

inline v3
Hadamard(v3 A, v3 B)
{
	v3 Result = { A.x*B.x, A.y*B.y, A.z*B.z};
	
	return(Result);
}

inline f32
Inner(v3 A, v3 B)
{
	f32 Result = A.x*B.x + A.y*B.y + A.z*B.z;
	return(Result);
}

inline f32
LengthSq(v3 A)
{
	f32 Result = Inner(A,A);
	
	return(Result);
}

inline f32
length(v3 A)
{
	f32 Result = SquareRoot(LengthSq(A));
	
	return(Result);
}

inline v3
normalize(v3 A)
{
	v3 Result = A * (SafeRatio0(1.0f, length(A)));
	
	return(Result);
}

inline v3
clamp01(v3  Value)
{
	v3 Result;
	
	Result.x = Clamp01(Value.x);
	Result.y = Clamp01(Value.y);
	Result.z = Clamp01(Value.z);
	
	return(Result);
}

inline v3
cross(v3 A, v3 B)
{
	v3 Result = {};
	
	Result.x = A.y*B.z - A.z*B.y;
	Result.y = A.z*B.x - A.x*B.z;
	Result.z = A.x*B.y - A.y*B.x;
	
	return Result;
}

inline v3
lerp(v3 A, f32 t, v3 B)
{
	v3 Result = (1.0f - t)*A + t*B;
	
	return(Result);
}


//
// NOTE(Denis): v4 operations
//


inline v4
operator*(f32 A, v4 B)
{
	v4 Result;
	
	Result.x = A*B.x;
	Result.y = A*B.y;
	Result.z = A*B.z;
	Result.w = A*B.w;
	
	return(Result);
}

inline v4 
operator*(v4 B, f32 A)
{
	v4 Result = A * B;
	
	return(Result);
}

inline v4 &
operator*=(v4 &B, f32 A)
{
	B = A * B;
	
	return(B);
}

inline v4 
operator-(v4 A)
{
	v4 Result;
	
	Result.x = -A.x;
	Result.y = -A.y;
	Result.z = -A.z;
	Result.w = -A.w;
	
	return(Result);
}

inline v4
operator+(v4 A, v4 B)
{
	v4 Result;
	
	Result.x = A.x + B.x;
	Result.y = A.y + B.y;
	Result.z = A.z + B.z;
	Result.w = A.w + B.w;
	
	return(Result);
}

inline v4 &
operator+=(v4 &A, v4 B)
{
	A = B + A;
	
	return(A);
}

inline v4 
operator-(v4 A, v4 B)
{
	v4 Result;
	
	Result.x = A.x - B.x;
	Result.y = A.y - B.y;
	Result.z = A.z - B.z;
	Result.w = A.w - B.w;
	
	return(Result);
}

inline v4 &
operator-=(v4 &A, v4 B)
{
	A = A - B;
	
	return(A);
}

inline bool
operator==(v4 A, v4 B)
{
	bool Result = ((A.x == B.x) && (A.y == B.y) && (A.z == B.z) && (A.w == B.w));
	return Result;
}

inline v4
Hadamard(v4 A, v4 B)
{
	v4 Result = { A.x*B.x, A.y*B.y, A.z*B.z, A.w*B.w};
	
	return(Result);
}

inline f32
Inner(v4 A, v4 B)
{
	f32 Result = A.x*B.x + A.y*B.y + A.z*B.z + A.w*B.w;
	return(Result);
}

inline f32
LengthSq(v4 A)
{
	f32 Result = Inner(A,A);
	
	return(Result);
}

inline f32
Length(v4 A)
{
	f32 Result = SquareRoot(LengthSq(A));
	
	return(Result);
}

inline v4
Clamp01(v4  Value)
{
	v4 Result;
	
	Result.x = Clamp01(Value.x);
	Result.y = Clamp01(Value.y);
	Result.z = Clamp01(Value.z);
	Result.w = Clamp01(Value.w);
	
	return(Result);
}

inline v4
Lerp(v4 A, f32 t, v4 B)
{
	v4 Result = (1.0f - t)*A + t*B;
	
	return(Result);
}

//
// NOTE(Denis): Rectangle2 
//

inline rectangle2
Union(rectangle2 A, rectangle2 B)
{
	rectangle2 Result;
	
	Result.Min.x = (A.Min.x < B.Min.x) ? A.Min.x : B.Min.x;
	Result.Min.y = (A.Min.y < B.Min.y) ? A.Min.y : B.Min.y;
	Result.Max.x = (A.Max.x > B.Max.x) ? A.Max.x : B.Max.x;
	Result.Max.y = (A.Max.y > B.Max.y) ? A.Max.y : B.Max.y;
	
	return(Result);
}

inline v2
GetMinCorner(rectangle2 Rect)
{
	v2 Result = Rect.Min;
	return(Result);
}

inline v2
GetMaxCorner(rectangle2 Rect)
{
	v2 Result = Rect.Max;
	return(Result);
}

inline v2
GetDim(rectangle2 Rect)
{
	v2 Result = Rect.Max - Rect.Min;
	return(Result);
}

inline v3
GetDim(rectangle3 Rect)
{
	v3 Result = Rect.Max - Rect.Min;
	return(Result);
}


inline v2
GetCenter(rectangle2 Rect)
{
	v2 Result = 0.5f*(Rect.Min + Rect.Max);
	return(Result);
}


inline rectangle2
RectMinMax(v2 Min, v2 Max)
{
	rectangle2 Result;
	
	Result.Min = Min;
	Result.Max = Max;
	
	return(Result);
}

inline rectangle2
AddRadiusTo(rectangle2 A, v2 Radius)
{
	rectangle2 Result;
	
	Result.Min = A.Min - Radius;
	Result.Max = A.Max + Radius;
	
	return(Result);
}

inline rectangle2
Offset(rectangle2 A, v2 Offset)
{
	rectangle2 Result;
	
	Result.Min = A.Min + Offset;
	Result.Max = A.Max + Offset;
	
	return(Result);
}

inline rectangle2
RectMinDim(v2 Min, v2 Dim)
{
	rectangle2 Result;
	
	Result.Min = Min;
	Result.Max = Min + Dim;
	
	return(Result);
}

inline rectangle2
RectCenterHalfDim(v2 Center, v2 HalfDim)
{
	rectangle2 Result;
	
	Result.Min = Center - HalfDim;
	Result.Max = Center + HalfDim;
	
	return(Result);
}

inline rectangle2
RectCenterDim(v2 Center, v2 Dim)
{
	rectangle2 Result = RectCenterHalfDim(Center, 0.5f*Dim);
	
	return(Result);
}

inline bool
IsInRectangle(rectangle2 Rectangle, v2 Test)
{
	bool Result = ((Test.x >= Rectangle.Min.x) &&
					 (Test.y >= Rectangle.Min.y) &&
					 (Test.x < Rectangle.Max.x) &&
					 (Test.y < Rectangle.Max.y));
	
	return(Result);
}

inline v2
GetBarycentric(rectangle2 A, v2 P)
{
	v2 Result;
	
	Result.x = SafeRatio0(P.x - A.Min.x, A.Max.x - A.Min.x);
	Result.y = SafeRatio0(P.y - A.Min.y, A.Max.y - A.Min.y);
	
	return(Result);
}

//
// NOTE(Denis): Rectangle3
//

inline v3
GetMinCorner(rectangle3 Rect)
{
	v3 Result = Rect.Min;
	return(Result);
}

inline v3
GetMaxCorner(rectangle3 Rect)
{
	v3 Result = Rect.Max;
	return(Result);
}

inline v3
GetCenter(rectangle3 Rect)
{
	v3 Result = 0.5f*(Rect.Min + Rect.Max);
	return(Result);
}


inline rectangle3
RectMinMax(v3 Min, v3 Max)
{
	rectangle3 Result;
	
	Result.Min = Min;
	Result.Max = Max;
	
	return(Result);
}

inline rectangle3
AddRadiusTo(rectangle3 A, v3 Radius)
{
	rectangle3 Result;
	
	Result.Min = A.Min - Radius;
	Result.Max = A.Max + Radius;
	
	return(Result);
}

inline rectangle3
Offset(rectangle3 A, v3 Offset)
{
	rectangle3 Result;
	
	Result.Min = A.Min + Offset;
	Result.Max = A.Max + Offset;
	
	return(Result);
}

inline rectangle3
RectMinDim(v3 Min, v3 Dim)
{
	rectangle3 Result;
	
	Result.Min = Min;
	Result.Max = Min + Dim;
	
	return(Result);
}

inline rectangle3
RectCenterHalfDim(v3 Center, v3 HalfDim)
{
	rectangle3 Result;
	
	Result.Min = Center - HalfDim;
	Result.Max = Center + HalfDim;
	
	return(Result);
}

inline rectangle3
RectCenterDim(v3 Center, v3 Dim)
{
	rectangle3 Result = RectCenterHalfDim(Center, 0.5f*Dim);
	
	return(Result);
}

inline bool
IsInRectangle(rectangle3 Rectangle, v3 Test)
{
	bool Result = ((Test.x >= Rectangle.Min.x) &&
					 (Test.y >= Rectangle.Min.y) &&
					 (Test.z >= Rectangle.Min.z) &&
					 (Test.x < Rectangle.Max.x) &&
					 (Test.y < Rectangle.Max.y) &&
					 (Test.z < Rectangle.Max.z));
	
	return(Result);
}

inline bool
RectanglesIntersect(rectangle3 A, rectangle3 B)
{
	bool Result = !((B.Max.x <= A.Min.x) ||
					  (B.Min.x >= A.Max.x) ||
					  (B.Max.y <= A.Min.y) ||
					  (B.Min.y >= A.Max.y) ||
					  (B.Max.z <= A.Min.z) ||
					  (B.Min.z >= A.Max.z));
	return(Result);
}

inline v3
GetBarycentric(rectangle3 A, v3 P)
{
	v3 Result;
	
	Result.x = SafeRatio0(P.x - A.Min.x, A.Max.x - A.Min.x);
	Result.y = SafeRatio0(P.y - A.Min.y, A.Max.y - A.Min.y);
	Result.z = SafeRatio0(P.z - A.Min.z, A.Max.z - A.Min.z);
	
	return(Result);
}

inline rectangle2
ToRectanglexy(rectangle3 A)
{
	rectangle2 Result;
	
	Result.Min = A.Min.xy;
	Result.Max = A.Max.xy;
	
	return(Result);
}

//
//
//

struct rectangle2i
{
	i32 MinX, MinY;
	i32 MaxX, MaxY;
};

inline rectangle2i 
Intersect(rectangle2i A, rectangle2i B)
{
	rectangle2i Result;
	
	Result.MinX = (A.MinX < B.MinX) ? B.MinX : A.MinX;
	Result.MinY = (A.MinY < B.MinY) ? B.MinY : A.MinY;
	Result.MaxX = (A.MaxX > B.MaxX) ? B.MaxX : A.MaxX;
	Result.MaxY = (A.MaxY > B.MaxY) ? B.MaxY : A.MaxY;
	
	return(Result);
}

inline rectangle2i 
Union(rectangle2i A, rectangle2i B)
{
	rectangle2i Result;
	
	Result.MinX = (A.MinX < B.MinX) ? A.MinX : B.MinX;
	Result.MinY = (A.MinY < B.MinY) ? A.MinY : B.MinY;
	Result.MaxX = (A.MaxX > B.MaxX) ? A.MaxX : B.MaxX;
	Result.MaxY = (A.MaxY > B.MaxY) ? A.MaxY : B.MaxY;
	
	return(Result);
}

inline i32
GetClampedRectArea(rectangle2i A)
{
	i32 Width = (A.MaxX - A.MinX);
	i32 Height =  (A.MaxY - A.MinY);
	i32 Result = 0;
	
	if((Width > 0 && (Height > 0)))
	{
		Result = Width * Height;
	}
	
	return(Result);
	
}

inline bool
HasArea(rectangle2i A)
{
	bool Result = ((A.MinX < A.MaxX) && (A.MinY < A.MaxY));
	
	return(Result);
}

inline rectangle2i 
InvertedInfinityRectangle2i()
{
	rectangle2i Result;
	
	Result.MinX = Result.MinY = INT_MAX;
	Result.MaxX = Result.MaxY = -INT_MAX;
	
	return(Result);
}

inline v4
SRGB255ToLinear1(v4 C)
{
	v4 Result;
	
	f32 Inv255 = 1.0f / 255.0f;
	
	Result.r = Square(Inv255*C.r);
	Result.g = Square(Inv255*C.g);
	Result.b = Square(Inv255*C.b);
	Result.a = Inv255*C.a;
	
	return(Result);
}

inline v4
Linear1ToSRGB255(v4 C)
{
	v4 Result;
	
	f32 One255 = 255.0f;
	
	Result.r = One255*SquareRoot(C.r);
	Result.g = One255*SquareRoot(C.g);
	Result.b = One255*SquareRoot(C.b);
	Result.a = One255*C.a;
	
	return(Result);
}

//
//
//

// mat4

// Matrix 4x4

inline mat4
Identity()
{
	mat4 Result = {};
	
	Result.Value1_1 = 1.0f;
	Result.Value2_2 = 1.0f;
	Result.Value3_3 = 1.0f;
	Result.Value4_4 = 1.0f;
	
	return Result;
}

inline mat4
TransposeMatrix(mat4 Value)
{
	mat4 Result = Identity();
	
	for(u32 Column = 0;
		Column < 4;
		++Column)
	{
		for(u32 Row = 0;
			Row< 4;
			++Row)
		{
			Result.TE[Row][Column] = Value.TE[Column][Row];
		}
	}
	
	return Result;
}

inline v4
operator*(mat4 A, v4 B)
{
	v4 Result;
	
	Result.x = (B.x * A.Value1_1) + (B.y * A.Value1_2) + (B.z * A.Value1_3) + (B.w * A.Value1_4);
	Result.y = (B.x * A.Value2_1) + (B.y * A.Value2_2) + (B.z * A.Value2_3) + (B.w * A.Value2_4);
	Result.z = (B.x * A.Value3_1) + (B.y * A.Value3_2) + (B.z * A.Value3_3) + (B.w * A.Value3_4);
	Result.w = (B.x * A.Value4_1) + (B.y * A.Value4_2) + (B.z * A.Value4_3) + (B.w * A.Value4_4);
	
	return Result;
}

inline v4
operator*(v4 A, mat4 B)
{
	v4 Result;
	
	Result.x = (A.x * B.Value1_1) + (A.y * B.Value2_1) + (A.z * B.Value3_1) + (A.w * B.Value4_1);
	Result.y = (A.x * B.Value1_2) + (A.y * B.Value2_2) + (A.z * B.Value3_2) + (A.w * B.Value4_2);
	Result.z = (A.x * B.Value1_3) + (A.y * B.Value2_3) + (A.z * B.Value3_3) + (A.w * B.Value4_3);
	Result.w = (A.x * B.Value1_4) + (A.y * B.Value2_4) + (A.z * B.Value3_4) + (A.w * B.Value4_4);
	
	return Result;
}

inline mat4
operator*(mat4 A, mat4 B)
{
	mat4 Result = {};
	
	for(u32 Row = 0;
		Row< 4;
		++Row)
	{
		for(u32 Column = 0;
			Column < 4;
			++Column)
		{
			Result.TE[Row][Column] = 0;
			for(u32 k = 0;
				k < 4;
				++k)
			{
				Result.TE[Row][Column] += A.TE[k][Column] * B.TE[Row][k];
			}
		}
	}
	
	return Result;
}


inline func create_ortho_matrix(float size, float aspect_ratio, float left, float right, float bottom, float top, float nearZ, float farZ) -> mat4 {
	left *= size;
	right *= size;
	bottom *= size;
	top *= size;

	left *= aspect_ratio;
	right *= aspect_ratio;
	
	mat4 result = Identity();
	result.Value1_1 = 2.0 / (right - left);
	result.Value2_2 = 2.0 / (top - bottom);
	result.Value3_3 = SafeRatio0(2.0, (farZ - nearZ));
	result.Value1_4 = -((right + left) / (right - left));
	result.Value2_4 = -((top + bottom) / (top - bottom));
	result.Value3_4 = -(SafeRatio0(farZ + nearZ, farZ - nearZ));
	return result;
}

inline func look_at_matrix(v3 eye, v3 pos, v3 up) -> mat4 {
	v3 forward 	= normalize	(pos - eye);
	v3 right 	= normalize	(cross(up, forward));
	v3 new_up 	= cross		(forward, right);

	v3 normalized_dot_right = normalize(Hadamard(right, eye));
	v3 normalized_dot_new_up = normalize(Hadamard(new_up, eye));
	v3 normalized_dot_forward = normalize(Hadamard(forward, eye));
	float x = -(normalized_dot_right.x);
	float y = -(normalized_dot_new_up.x);
	float z = (normalized_dot_forward.x);

	mat4 result = Identity();
	result.Value1_1 = right.x; result.Value1_2 = new_up.x; result.Value1_3 = -forward.x; result.Value1_4 = 0.0f;
	result.Value2_1 = right.y; result.Value2_2 = new_up.y; result.Value2_3 = -forward.y; result.Value2_4 = 0.0f;
	result.Value3_1 = right.z; result.Value3_2 = new_up.z; result.Value3_3 = -forward.z; result.Value3_4 = 0.0f;
	result.Value4_1 = x; result.Value4_2 = y; result.Value4_3 = z;
	
	return result;
}

inline func translation_matrix(v3 target_pos) -> mat4 {
	mat4 result = Identity();
	result.Value1_4 = target_pos.x;
	result.Value2_4 = target_pos.y;
	result.Value3_4 = target_pos.z;
	return result;
}

inline func translation_matrix(v4 target_pos) -> mat4 {
	mat4 result = Identity();
	result.Value1_4 = target_pos.x;
	result.Value2_4 = target_pos.y;
	result.Value3_4 = target_pos.z;
	return result;
}

inline func screen_to_ndc(u32 width, u32 height, f32 scale = 1.0f, f32 aspect = 1.0f) -> mat4 {
	mat4 result = Identity();
	result.Value1_1 = 2.0f / width * scale * aspect; result.Value1_4 = -1.0f;
	result.Value2_2 = 2.0f / height * scale; result.Value2_4 = -1.0f;
	return result;
}

inline func ndc_to_screen(v2 pos, u32 width, u32 height) -> v2 {
	v2 result = {};
	f32 aspect = (f32)width / (f32)height;
	result.x = -((1.0f - pos.x) / 2.0f) * width;
	result.y = -((1.0f - pos.y) / 2.0f) * height * aspect;
	return result;
}