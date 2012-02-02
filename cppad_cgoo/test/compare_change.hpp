#ifndef CPPADCGOO_COMPARE_CHANGE_INCLUDED
#define	CPPADCGOO_COMPARE_CHANGE_INCLUDED
/* --------------------------------------------------------------------------
CppAD: C++ Algorithmic Differentiation: Copyright (C) 2012 Ciengis

CppAD is distributed under multiple licenses. This distribution is under
the terms of the 
                    Common Public License Version 1.0.

A copy of this license is included in the COPYING file of this distribution.
Please visit http://www.coin-or.org/CppAD/ for information on other licenses.
-------------------------------------------------------------------------- */

#include <assert.h>

// ------------------------------- < ----------------------------

template<class T>
CppAD::ADFun<T>* CompareChangeFunc1(const std::vector<CppAD::AD<T> >& X) {
    using namespace CppAD;
    assert(X.size() == 2);
    std::vector< AD<T> > Y(6); // create dependent variables

    // CondExp would never require retaping 
    if (X[0] < X[1]) // True variable < variable
        Y[0] = X[0];
    else Y[0] = X[1];
    if (X[1] < X[0]) // False variable < variable
        Y[1] = X[0];
    else Y[1] = X[1];
    if (3.5 < X[1]) // True parameter < variable
        Y[2] = X[0];
    else Y[2] = X[1];
    if (3.5 < X[0]) // False parameter < variable
        Y[3] = X[0];
    else Y[3] = X[1];
    if (X[0] < 4.) // True variable < parameter
        Y[4] = X[0];
    else Y[4] = X[1];
    if (X[1] < 4.) // False variable < parameter
        Y[5] = X[0];
    else Y[5] = X[1];

    // f : X -> Y
    return new ADFun<T > (X, Y);
}

// ------------------------------- > ----------------------------

template<class T>
CppAD::ADFun<T>* CompareChangeFunc2(const std::vector<CppAD::AD<T> >& X) {
    using namespace CppAD;
    assert(X.size() == 2);
    std::vector< AD<T> > Y(6); // create dependent variables

    if (X[0] > X[1]) // False variable > variable
        Y[0] = X[0];
    else Y[0] = X[1];
    if (X[1] > X[0]) // True variable > variable
        Y[1] = X[0];
    else Y[1] = X[1];
    if (3.5 > X[1]) // False parameter > variable
        Y[2] = X[0];
    else Y[2] = X[1];
    if (3.5 > X[0]) // True parameter > variable
        Y[3] = X[0];
    else Y[3] = X[1];
    if (X[0] > 3.) // False variable > parameter
        Y[4] = X[0];
    else Y[4] = X[1];
    if (X[1] > 3.) // True variable > parameter
        Y[5] = X[0];
    else Y[5] = X[1];

    // f : X -> Y
    return new ADFun<T > (X, Y);

}

// ------------------------------- <= ----------------------------

template<class T>
CppAD::ADFun<T>* CompareChangeFunc3(const std::vector<CppAD::AD<T> >& X) {
    using namespace CppAD;
    assert(X.size() == 2);
    std::vector< AD<T> > Y(6); // create dependent variables

    if (X[0] <= X[1]) // True variable <= variable
        Y[0] = X[0];
    else Y[0] = X[1];
    if (X[1] <= X[0]) // False variable <= variable
        Y[1] = X[0];
    else Y[1] = X[1];
    if (4. <= X[1]) // True parameter <= variable
        Y[2] = X[0];
    else Y[2] = X[1];
    if (4. <= X[0]) // False parameter <= variable
        Y[3] = X[0];
    else Y[3] = X[1];
    if (X[0] <= 3.5) // True variable <= parameter
        Y[4] = X[0];
    else Y[4] = X[1];
    if (X[1] <= 3.5) // False variable <= parameter
        Y[5] = X[0];
    else Y[5] = X[1];

    // f : X -> Y
    return new ADFun<T > (X, Y);

}

// ------------------------------- >= ----------------------------

template<class T>
CppAD::ADFun<T>* CompareChangeFunc4(const std::vector<CppAD::AD<T> >& X) {
    using namespace CppAD;
    assert(X.size() == 2);
    std::vector< AD<T> > Y(6); // create dependent variables


    if (X[0] >= X[1]) // False variable >= variable
        Y[0] = X[0];
    else Y[0] = X[1];
    if (X[1] >= X[0]) // True variable >= variable
        Y[1] = X[0];
    else Y[1] = X[1];
    if (3.5 >= X[1]) // False parameter >= variable
        Y[2] = X[0];
    else Y[2] = X[1];
    if (3.5 >= X[0]) // True parameter >= variable
        Y[3] = X[0];
    else Y[3] = X[1];
    if (X[0] >= 4.) // False variable >= parameter
        Y[4] = X[0];
    else Y[4] = X[1];
    if (X[1] >= 4.) // True variable >= parameter
        Y[5] = X[0];
    else Y[5] = X[1];

    // f : X -> Y
    return new ADFun<T > (X, Y);   
}

// ------------------------------- == ----------------------------

template<class T>
CppAD::ADFun<T>* CompareChangeFunc5(const std::vector<CppAD::AD<T> >& X) {
    using namespace CppAD;
    assert(X.size() == 2);
    std::vector< AD<T> > Y(6); // create dependent variables


    if (X[0] == X[1]) // False variable == variable
        Y[0] = X[0];
    else Y[0] = X[1];
    if (X[0] == X[0]) // True variable == variable
        Y[1] = X[0];
    else Y[1] = X[1];
    if (3. == X[1]) // False parameter == variable
        Y[2] = X[0];
    else Y[2] = X[1];
    if (3. == X[0]) // True parameter == variable
        Y[3] = X[0];
    else Y[3] = X[1];
    if (X[0] == 4.) // False variable == parameter
        Y[4] = X[0];
    else Y[4] = X[1];
    if (X[1] == 4.) // True variable == parameter
        Y[5] = X[0];
    else Y[5] = X[1];

    // f : X -> Y
    return new ADFun<T > (X, Y);
}

// ------------------------------- != ----------------------------

template<class T>
CppAD::ADFun<T>* CompareChangeFunc6(const std::vector<CppAD::AD<T> >& X) {
    using namespace CppAD;
    assert(X.size() == 2);
    std::vector< AD<T> > Y(6); // create dependent variables


    if (X[0] != X[1]) // True variable != variable
        Y[0] = X[0];
    else Y[0] = X[1];
    if (X[0] != X[0]) // False variable != variable
        Y[1] = X[0];
    else Y[1] = X[1];
    if (3. != X[1]) // True parameter != variable
        Y[2] = X[0];
    else Y[2] = X[1];
    if (3. != X[0]) // False parameter != variable
        Y[3] = X[0];
    else Y[3] = X[1];
    if (X[0] != 4.) // True variable != parameter
        Y[4] = X[0];
    else Y[4] = X[1];
    if (X[1] != 4.) // False variable != parameter
        Y[5] = X[0];
    else Y[5] = X[1];

    // f : X -> Y
    return new ADFun<T> (X, Y);
}

#endif