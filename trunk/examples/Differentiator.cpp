/* Portions copyright (c) 2006 Stanford University and Michael Sherman.
 * Contributors:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS, COPYRIGHT HOLDERS, OR CONTRIBUTORS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**@file
 * This is a test program which uses the Differentiator class in various ways.
 */

//#define SimTK_USE_STATIC_LIBRARIES

#include "SimTKmath.h"

// Just so we can get the version number:
#include "SimTKlapack.h"

#include <cstdio>
#include <iostream>

using namespace SimTK;

// This is a system of functions of a particular set of parameters (state).
// The underlying function wants time also, so we provide that as data in
// the concrete class. Time should be set prior to calculation of the Jacobian.
class MyVectorFunc : public Differentiator::JacobianFunction {
public:
    MyVectorFunc(int nf, int ny) 
        : Differentiator::JacobianFunction(nf,ny), time(0) { }

    void setTime(Real t) {time=t;}
    Real getTime() const {return time;}

    // Must provide this pure virtual function.
    int f(const Vector& y, Vector& fy) const;
private:
    Real time;
};

// This is a single scalar function of a vector of parameters.
class MyObjectiveFunc : public Differentiator::GradientFunction {
public:
    MyObjectiveFunc(int ny) 
        : Differentiator::GradientFunction(ny), time(0) { }

    void setTime(Real t) {time=t;}
    Real getTime() const {return time;}

    // Must provide this pure virtual function.
    int f(const Vector& y, Real& fy) const;
private:
    Real time;
};

// This represents a generic scalar function of a scalar parameter,
// where the actual function has a simple C signature.
class GenericScalarFunc : public Differentiator::ScalarFunction {
    typedef Real (*CFunc)(Real);
public:
    GenericScalarFunc(CFunc cf) 
        : Differentiator::ScalarFunction(), cp(cf) { }

    // Must provide this pure virtual function.
    int f(Real x, Real& fx) const {
        fx = cp(x);
        return 0;
    }
    
    CFunc cp;
};

#define PREC double
class SinOmegaX : public Differentiator::ScalarFunction {
public:
    SinOmegaX(Real omega, Real acc) 
        : ScalarFunction(acc), w(omega) 
    {  
    }

    Real calc(Real x) const {
        return std::sin(w*x);
    }
    Real calcD1(Real x) const {
        return w*std::cos(w*x);
    }
    Real calcD2(Real x) const {
        return -w*w*std::sin(w*x);
    }
    // Must provide this virtual function.
    int f(Real x, Real& fx) const {
        volatile PREC ffx = (PREC)calc(x);
        fx = ffx;
        return 0; // success
    }
private:
    const Real w;
};

class Cubic : public Differentiator::ScalarFunction {
public:
    // ax^3+bx^2+cx+d
    Cubic(Real aa, Real bb, Real cc, Real dd, Real acc) 
        : ScalarFunction(acc), a(aa),b(bb),c(cc),d(dd)
    {  
    }

    Real calc(Real x) const {
        return (a*x*x*x + b*x*x + c*x + d)*std::exp(a*x);
    }

    Real calcD1(Real x) const {
        return (3*a*x*x+2*b*x+c)*std::exp(a*x) + a*calc(x);
    }

    Real calcD2(Real x) const {
        return (6*a*x+2*b)*std::exp(a*x) + (3*a*x*x+2*b*x+c)*a*std::exp(a*x)
                + a*calcD1(x);
    }

    // Must provide this virtual function.
    int f(Real x, Real& fx) const {
        volatile PREC ffx = (PREC)calc(x);
        fx = ffx;
        return 0; // success
    }
private:
    const Real a, b, c, d;
};



static void doSinOmegaExample() {
  for (int digits=0; digits<=41; ++digits) {
    Real acc;
    if (digits < 40) acc = std::pow(10., -(digits/(1.5*sizeof(double)/sizeof(PREC))));
    else if (digits==40) acc=SimTK::NTraits<PREC>::getSignificant();
    else if (digits==41) acc=SimTK::NTraits<PREC>::getEps();

    const Real w = .01;
    SinOmegaX func(w, acc);
    //Cubic func(-1,-2,3,4, acc);
    Differentiator dsin3x(func);

    const int NEntries = 1000;
    const Real offs =0.1;
    const Real increment = (Real)SimTK_PI/NEntries;
    //printf("%8s %12s %12s %12s\n", 
    //    "x", "3cos3x", "err1", "err2");

    Real err1rms=0, err1max=0, err2rms=0, err2max=0;
    for (int i=0; i < NEntries; ++i) {
        const Real x = offs + i*increment;
        if (digits==0) printf("%12g %12g %12g\n", func.calc(x), func.calcD1(x), func.calcD2(x));
        const Real analytic = func.calcD1(x);
        const Real approx1st = dsin3x.calcDerivative(x);
        const Real approx2nd = dsin3x.calcDerivative(x, Differentiator::CentralDifference);
        const Real err1 = std::abs((approx1st-analytic)/analytic);
        const Real err2 = std::abs((approx2nd-analytic)/analytic);
        err1rms += err1*err1; err2rms += err2*err2;
        if (err1 > err1max) err1max=err1;
        if (err2 > err2max) err2max=err2;
        //printf("%8g %12g %12.3e %12.3e\n", 
          //  x, analytic, err1, err2);
    }
    printf("%.3e: err1: max=%.3e, rms=%.3e  err2: max=%.3e, rms=%.3e\n",
        acc, err1max, std::sqrt(err1rms/NEntries), err2max, std::sqrt(err2rms/NEntries));
  }
    exit(0);
};

static void showSimTKAboutInfo() {

    int major,minor,build;
    char out[100];
    const char* keylist[] = { "version", "library", "type", "debug", "authors", "copyright", "svn_revision", 0 };

    SimTK_version_SimTKlapack(&major,&minor,&build);
    std::printf("==> SimTKlapack library version: %d.%d.%d\n", major, minor, build);
    std::printf("    SimTK_about_SimTKlapack():\n");
    for (const char** p = keylist; *p; ++p) {
        SimTK_about_SimTKlapack(*p, 100, out);
        std::printf("      about(%s)='%s'\n", *p, out);
    }

    SimTK_version_SimTKcommon(&major,&minor,&build);
    std::printf("==> SimTKcommon library version: %d.%d.%d\n", major, minor, build);
    std::printf("    SimTK_about_SimTKcommon():\n");
    for (const char** p = keylist; *p; ++p) {
        SimTK_about_SimTKcommon(*p, 100, out);
        std::printf("      about(%s)='%s'\n", *p, out);
    }

    SimTK_version_simmath(&major,&minor,&build);
    std::printf("==> SimTKmath library version: %d.%d.%d\n", major, minor, build);
    std::printf("    SimTK_about_simmath():\n");
    for (const char** p = keylist; *p; ++p) {
        SimTK_about_simmath(*p, 100, out);
        std::printf("      about(%s)='%s'\n", *p, out);
    }
    std::printf("\n");

}

static Real mysin(Real x) {
    return std::sin(x);
}
static Real mycos(Real x) {
    return std::cos(x);
}

int main() {
    showSimTKAboutInfo();
    doSinOmegaExample();

    Vector yy, yp;

    yy.resize(4);
    yp.resize(4);

    /* Initialize y */
    yy[0] = 1.0;  /* x */
    yy[1] = 0.0;  /* y */
    yy[2] = 0.0;  /* xd */
    yy[3] = 0.0;  /* yd */

    // Define a scalar, vector, and system of functions.
    GenericScalarFunc gf(mysin);
    MyObjectiveFunc   sf(4);
    MyVectorFunc      vf(4,4); 
    vf.setEstimatedAccuracy(1e-6); // claim reduced accuracy (6 digits)

    // Create differentiators for each of the functions.
    Differentiator dsin(gf,Differentiator::CentralDifference); // use calcDerivative()
    Differentiator gradf(sf);       // use calcGradient()
    Differentiator df(vf);          // use calcJacobian()

    int returnValue = 0; // assume success
  try {
    gradf.setDefaultMethod(Differentiator::ForwardDifference);
    df.setDefaultMethod(Differentiator::UnspecifiedMethod);

    printf("dsin default method: %s\n", Differentiator::getMethodName(dsin.getDefaultMethod()));
    printf("gradf default method: %s\n", Differentiator::getMethodName(gradf.getDefaultMethod()));
    printf("df default method: %s\n", Differentiator::getMethodName(df.getDefaultMethod()));

    const Real ang = SimTK_PI/8;
    std::cout << "sin(x)=" << mysin(ang) << std::endl;
    std::cout << "d sin(x)/dx=" << dsin.calcDerivative(ang) << std::endl;
    std::cout << "cos(x)=" << mycos(ang) << std::endl;

    const Real rp[] = {.01,.02,.03,-.14};
    Vector delta_y(4,rp);
    Vector y0 = yy+delta_y; // don't start right at 0


    printf("Func gf: nf=%d np=%d estacc=%g\n",
        gf.getNFunctions(), gf.getNParameters(), gf.getEstimatedAccuracy());
    printf("Func sf: nf=%d np=%d estacc=%g\n",
        sf.getNFunctions(), sf.getNParameters(), sf.getEstimatedAccuracy());
    printf("Func vf: nf=%d np=%d estacc=%g\n",
        vf.getNFunctions(), vf.getNParameters(), vf.getEstimatedAccuracy());


    Real sfy0, sfyd;
    sf.f(y0, sfy0); // calculate unperturbed value
    Vector grad1;
    gradf.calcGradient(y0, sfy0, grad1);

    Vector grad2;
    gradf.calcGradient(y0, sfy0, grad2, Differentiator::CentralDifference);
    std::cout << "sf(y0)=" << sfy0 << std::endl;
    std::cout << "order 1 grad(sf)=" << grad1 << std::endl;
    std::cout << "order 2 grad(sf)=" << grad2 << std::endl;

    sf.f(y0+delta_y, sfyd);
    std::cout << "sf(y0+dy)=" << sfyd << std::endl;
    std::cout << "sf(y0)+~grad1*dy=" << sfy0 + ~grad1*delta_y << std::endl;
    std::cout << "err @order1=" << std::abs(sfyd-(sfy0 + ~grad1*delta_y)) << std::endl;
    std::cout << "err @order2=" << std::abs(sfyd-(sfy0 + ~grad2*delta_y)) << std::endl;

    vf.f(y0, yp);
    Matrix dfdy;
    df.calcJacobian(y0,yp,dfdy);
    const Matrix dfdy2 = df.calcJacobian(y0, Differentiator::CentralDifference);

    std::cout << "vf(" << y0 << ")=" << yp << std::endl;
    std::cout << "order " << df.getDefaultMethod()
         << " df/dy=" << dfdy;
    std::cout << "2nd order dfdy: " << dfdy2;
    Vector yp2(4);
    vf.f(y0+2*delta_y, yp2);
    std::cout << "f(y0+dy)=" << yp2 << std::endl;
    std::cout << "1 f(y0)+(df/dy)dy=" << yp+dfdy*2*delta_y << std::endl; 
    std::cout << "2 f(y0)+(f/dy)dy=" << yp+dfdy2*2*delta_y << std::endl;
    std::cout << std::setprecision(16);
    std::cout << "1 err=" << (yp2-(yp+dfdy*2*delta_y)).norm() << std::endl;
    std::cout << "2 err=" << (yp2-(yp+dfdy2*2*delta_y)).norm() << std::endl;
  }
  catch (const std::exception& e) {
    std::cout << "Differentiator.cpp  caught exception:";
    std::cout << e.what() << std::endl;
  }

    printf("dsin stats: ndiffs=%ld nfail=%ld nfcalls=%ld\n",
        dsin.getNDifferentiations(), dsin.getNDifferentiationFailures(),
        dsin.getNCallsToUserFunction());
    printf("gradf stats: ndiffs=%ld nfail=%ld nfcalls=%ld\n",
        gradf.getNDifferentiations(), gradf.getNDifferentiationFailures(),
        gradf.getNCallsToUserFunction());
    printf("df stats: ndiffs=%ld nfail=%ld nfcalls=%ld\n",
        df.getNDifferentiations(), df.getNDifferentiationFailures(),
        df.getNCallsToUserFunction());

    printf("gf stats: nCalls=%ld, nFailures=%ld\n",
        gf.getNCalls(), gf.getNFailures());
    printf("sf stats: nCalls=%ld, nFailures=%ld\n",
        sf.getNCalls(), sf.getNFailures());
    printf("vf stats: nCalls=%ld, nFailures=%ld\n",
        vf.getNCalls(), vf.getNFailures());

    vf.resetAllStatistics();
    printf("vf after reset: nCalls=%ld, nFailures=%ld\n",
        vf.getNCalls(), vf.getNFailures());

    return 0;
}

// Here is our system of equations, representing a pendulum. We'll
// use the system as-is for calculating a Jacobian, and use its
// norm for testing gradient calculation.
static int pendODE(Real t, const Vector& yy, Vector& fy)
{
    Real x, y, xd, yd, g, tmp;

    g = 13.7503716373294544;

    x  = yy[0];
    y  = yy[1];
    xd = yy[2];
    yd = yy[3];

    tmp = xd*xd + yd*yd - g*y;

    fy[0] = xd;
    fy[1] = yd;
    fy[2] = -x*tmp;
    fy[3] = -y*tmp - g;

    return 0;
}

// Implement the virtual method for a JacobianFunction.
int MyVectorFunc::f(const Vector& yy, Vector& fy) const
{
    return pendODE(getTime(), yy, fy);
}

// Implement the virtual function for a GradientFunction.
int MyObjectiveFunc::f(const Vector& yy, Real& fy) const
{
    Vector tmp(4);
    const int res = pendODE(getTime(), yy, tmp);
    fy = tmp.norm();
    return res;
}