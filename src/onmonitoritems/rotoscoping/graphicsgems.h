/*
  Copyright : (C) 1990 Philip J. Schneider (pjs@apple.com)
 * GraphicsGems.h
 * Version 1.0 - Andrew Glassner
 * from "Graphics Gems", Academic Press, 1990
 *
 * This code repository predates the concept of Open Source, and predates most licenses along such lines.
 * As such, the official license truly is:
 *
 * EULA: The Graphics Gems code is copyright-protected.
 * In other words, you cannot claim the text of the code as your own and resell it.
 * Using the code is permitted in any program, product, or library, non-commercial or commercial.
 * Giving credit is not required, though is a nice gesture.
 * The code comes as-is, and if there are any flaws or problems with any Gems code,
 * nobody involved with Gems - authors, editors, publishers, or webmasters - are to be held responsible.
 * Basically, don't be a jerk, and remember that anything free comes with no guarantee.
 */

#ifndef GRAPHICSGEMS_H
#define GRAPHICSGEMS_H

/*********************/
/* 2d geometry types */
/*********************/

typedef struct Point2Struct {   /* 2d point */
    double x, y;
} Point2;
typedef Point2 Vector2;

typedef struct IntPoint2Struct {        /* 2d integer point */
    int x, y;
} IntPoint2;

typedef struct Matrix3Struct {  /* 3-by-3 matrix */
    double element[3][3];
} Matrix3;

typedef struct Box2dStruct {            /* 2d box */
    Point2 min, max;
} Box2;

/*********************/
/* 3d geometry types */
/*********************/

typedef struct Point3Struct {   /* 3d point */
    double x, y, z;
} Point3;
typedef Point3 Vector3;

typedef struct IntPoint3Struct {        /* 3d integer point */
    int x, y, z;
} IntPoint3;

typedef struct Matrix4Struct {  /* 4-by-4 matrix */
    double element[4][4];
} Matrix4;

typedef struct Box3dStruct {            /* 3d box */
    Point3 min, max;
} Box3;

/***********************/
/* one-argument macros */
/***********************/

/* absolute value of a */
#define ABS(a)          (((a)<0) ? -(a) : (a))

/* round a to nearest int */
#define ROUND(a)        ((a)>0 ? (int)((a)+0.5) : -(int)(0.5-(a)))

/* take sign of a, either -1, 0, or 1 */
#define ZSGN(a)         (((a)<0) ? -1 : (a)>0 ? 1 : 0)

/* take binary sign of a, either -1, or 1 if >= 0 */
#define SGN(a)          (((a)<0) ? -1 : 1)

/* shout if something that should be true isn't */
#define ASSERT(x) \
    if (!(x)) fprintf(stderr," Assert failed: x\n");

/* square a */
#define SQR(a)          ((a)*(a))

/***********************/
/* two-argument macros */
/***********************/

/* find minimum of a and b */
#define MIN(a,b)        (((a)<(b))?(a):(b))

/* find maximum of a and b */
#define MAX(a,b)        (((a)>(b))?(a):(b))

/* swap a and b (see Gem by Wyvill) */
#define SWAP(a,b)       { a^=b; b^=a; a^=b; }

/* linear interpolation from l (when a=0) to h (when a=1)*/
/* (equal to (a*h)+((1-a)*l) */
#define LERP(a,l,h)     ((l)+(((h)-(l))*(a)))

/* clamp the input to the specified range */
#define CLAMP(v,l,h)    ((v)<(l) ? (l) : (v) > (h) ? (h) : v)

/****************************/
/* memory allocation macros */
/****************************/

/* create a new instance of a structure (see Gem by Hultquist) */
#define NEWSTRUCT(x)    (struct x *)(malloc((unsigned)sizeof(struct x)))

/* create a new instance of a type */
#define NEWTYPE(x)      (x *)(malloc((unsigned)sizeof(x)))

/********************/
/* useful constants */
/********************/

#ifndef PI
#define PI              3.141592        /* the venerable pi */
#endif
#define PITIMES2        6.283185        /* 2 * pi */
#define PIOVER2         1.570796        /* pi / 2 */
#define E               2.718282        /* the venerable e */
#define SQRT2           1.414214        /* sqrt(2) */
#define SQRT3           1.732051        /* sqrt(3) */
#define GOLDEN          1.618034        /* the golden ratio */
#define DTOR            0.017453        /* convert degrees to radians */
#define RTOD            57.29578        /* convert radians to degrees */

/************/
/* booleans */
/************/

#define ON              1
#define OFF             0
typedef int boolean;                    /* boolean data type */
typedef boolean flag;                   /* flag data type */

/*extern double V2SquaredLength(Vector2 *a), V2Length();
extern double V2Dot(Vector2 *a, Vector2 *b), V2DistanceBetween2Points();
extern Vector2 *V2Negate(), *V2Normalize(), *V2Scale(), *V2Add(), *V2Sub(Vector2 *a, Vector2 *b, Vector2 *c);
extern Vector2 *V2Lerp(), *V2Combine(), *V2Mul(), *V2MakePerpendicular();
extern Vector2 *V2New(), *V2Duplicate();
extern Point2 *V2MulPointByMatrix();
extern Matrix3 *V2MatMul();

extern double V3SquaredLength(), V3Length();
extern double V3Dot(), V3DistanceBetween2Points();
extern Vector3 *V3Normalize(), *V3Scale(), *V3Add(), *V3Sub();
extern Vector3 *V3Lerp(), *V3Combine(), *V3Mul(), *V3Cross();
extern Vector3 *V3New(), *V3Duplicate();
extern Point3 *V3MulPointByMatrix();
extern Matrix4 *V3MatMul();

extern double RegulaFalsi(), NewtonRaphson(), findroot();*/

#endif
