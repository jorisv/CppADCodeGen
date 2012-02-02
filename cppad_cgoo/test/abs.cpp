/* --------------------------------------------------------------------------
CppAD: C++ Algorithmic Differentiation: Copyright (C) 2012 Ciengis

CppAD is distributed under multiple licenses. This distribution is under
the terms of the 
                    Common Public License Version 1.0.

A copy of this license is included in the COPYING file of this distribution.
Please visit http://www.coin-or.org/CppAD/ for information on other licenses.
-------------------------------------------------------------------------- */

#include <cppad_cgoo/cg.hpp>

#include "gcc_load_dynamic.hpp"
#include "abs.hpp"

bool Abs() {
    using namespace CppAD;
    using namespace std;

    std::vector<std::vector<double> > uV;
    std::vector<double> u(1);
    u[0] = 0;
    uV.push_back(u);
    u[0] = 1;
    uV.push_back(u);
    u[0] = -1;
    uV.push_back(u);

    return test0nJac("abs", &AbsFunc<double >, &AbsFunc<CG<double> >, uV);
}
