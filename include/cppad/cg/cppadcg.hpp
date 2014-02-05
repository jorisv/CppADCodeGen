#ifndef CPPADCG_INCLUDED
#define CPPADCG_INCLUDED
/* --------------------------------------------------------------------------
 *  CppADCodeGen: C++ Algorithmic Differentiation with Source Code Generation:
 *    Copyright (C) 2012 Ciengis
 *
 *  CppADCodeGen is distributed under multiple licenses:
 *
 *   - Eclipse Public License Version 1.0 (EPL1), and
 *   - GNU General Public License Version 3 (GPL3).
 *
 *  EPL1 terms and conditions can be found in the file "epl-v10.txt", while
 *  terms and conditions for the GPL3 can be found in the file "gpl3.txt".
 * ----------------------------------------------------------------------------
 * Author: Joao Leal
 */
// all base type requirements
#include <cppad/base_require.hpp> 

// --------------------------------------------------------------------------
// System routines that can be used by rest of CppAD with out including 

#include <algorithm>
#include <assert.h>
#include <cstddef>
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <vector>
#include <forward_list>
#include <set>
#include <stddef.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <string.h>
#include <chrono>

// ---------------------------------------------------------------------------
// definitions needed by rest of includes

// definitions that come from the installation
#include <cppad/configure.hpp>

// definitions that are local to the CppAD include files
#include <cppad/local/define.hpp>

// vectors used with CppAD
#include <cppad/local/testvector.hpp>

// deprecated vectors used with CppAD
#include <cppad/local/test_vector.hpp>

// Declare classes and functions that are used before defined
#include <cppad/local/declare_ad.hpp>

// ---------------------------------------------------------------------------
#include <cppad/cg/cppadcg_assert.hpp>
#include <cppad/cg/exception.hpp>
#include <cppad/cg/declare_cg.hpp>

// ---------------------------------------------------------------------------
// CppAD
#include <cppad/cppad.hpp>
// addons
#include <cppad/extra/extra.hpp>

// ---------------------------------------------------------------------------
// system dependent files
#include <cppad/cg/model/system.hpp>
#include <cppad/cg/model/dynamic_lib/linux/linux_system.hpp>

// ---------------------------------------------------------------------------
// some utilities
#include <cppad/cg/smart_containers.hpp>

// ---------------------------------------------------------------------------
// indexes
#include <cppad/cg/patterns/index/index_pattern.hpp>
#include <cppad/cg/patterns/index/linear_index_pattern.hpp>
#include <cppad/cg/patterns/index/sectioned_index_pattern.hpp>
#include <cppad/cg/patterns/index/plane_2d_index_pattern.hpp>
#include <cppad/cg/patterns/index/random_index_pattern.hpp>
#include <cppad/cg/patterns/index/random_1d_index_pattern.hpp>
#include <cppad/cg/patterns/index/random_2d_index_pattern.hpp>
#include <cppad/cg/patterns/index/index_pattern_impl.hpp>

// ---------------------------------------------------------------------------
// core files
#include <cppad/cg/debug.hpp>
#include <cppad/cg/operation.hpp>
#include <cppad/cg/argument.hpp>
#include <cppad/cg/operation_node.hpp>
#include <cppad/cg/nodes/index_operation_node.hpp>
#include <cppad/cg/nodes/index_dclr_operation_node.hpp>
#include <cppad/cg/nodes/index_assign_operation_node.hpp>
#include <cppad/cg/nodes/loop_start_operation_node.hpp>
#include <cppad/cg/nodes/loop_end_operation_node.hpp>
#include <cppad/cg/nodes/print_operation_node.hpp>
#include <cppad/cg/cg.hpp>
#include <cppad/cg/default.hpp>
#include <cppad/cg/variable.hpp>
#include <cppad/cg/identical.hpp>
#include <cppad/cg/range.hpp>
#include <cppad/cg/atomic_dependency_locator.hpp>
#include <cppad/cg/variable_name_generator.hpp>
#include <cppad/cg/job_timer.hpp>
#include <cppad/cg/language.hpp>
#include <cppad/cg/scope_path_element.hpp>
#include <cppad/cg/array_id_compresser.hpp>
#include <cppad/cg/code_handler.hpp>
#include <cppad/cg/patterns/loop_position.hpp>
#include <cppad/cg/code_handler_loops.hpp>
#include <cppad/cg/arithmetic.hpp>
#include <cppad/cg/arithmetic_assign.hpp>
#include <cppad/cg/math.hpp>
#include <cppad/cg/math_other.hpp>
#include <cppad/cg/nan.hpp>
#include <cppad/cg/cond_exp_op.hpp>
#include <cppad/cg/compare.hpp>
#include <cppad/cg/ordered.hpp>
#include <cppad/cg/unary.hpp>
#include <cppad/cg/base_double.hpp>

// ---------------------------------------------------------------------------
// additional utilities
#include <cppad/cg/util.hpp>
#include <cppad/cg/evaluator.hpp>
#include <cppad/cg/operation_path.hpp>
#include <cppad/cg/solver.hpp>
#include <cppad/cg/graph_mod.hpp>

// ---------------------------------------------------------------------------
// atomic function utilities
#include <cppad/cg/custom_position.hpp>
#include <cppad/cg/base_abstract_atomic_fun.hpp>
#include <cppad/cg/abstract_atomic_fun.hpp>
#include <cppad/cg/atomic_fun.hpp>
#include <cppad/cg/atomic_fun_bridge.hpp>
#include <cppad/cg/model/atomic_generic_model.hpp>

// ---------------------------------------------------------------------------
// loop/pattern detection
#include <cppad/cg/patterns/independent_node_sorter.hpp>
#include <cppad/cg/patterns/equation_group.hpp>
#include <cppad/cg/patterns/iter_equation_group.hpp>
#include <cppad/cg/patterns/loop_model.hpp>
#include <cppad/cg/patterns/loop_free_model.hpp>
#include <cppad/cg/patterns/equation_pattern.hpp>
#include <cppad/cg/patterns/loop.hpp>
#include <cppad/cg/patterns/dependent_pattern_matcher.hpp>

// ---------------------------------------------------------------------------
// C source code generation
#include <cppad/cg/c/lang_c_atomic_fun.hpp>
#include <cppad/cg/c/language_c.hpp>
#include <cppad/cg/c/language_c_arrays.hpp>
#include <cppad/cg/c/language_c_index_patterns.hpp>
#include <cppad/cg/c/language_c_double.hpp>
#include <cppad/cg/c/lang_c_default_var_name_gen.hpp>
#include <cppad/cg/c/lang_c_default_hessian_var_name_gen.hpp>
#include <cppad/cg/c/lang_c_default_reverse2_var_name_gen.hpp>
#include <cppad/cg/c/lang_c_custom_var_name_gen.hpp>

//
#include <cppad/cg/model/external_function_wrapper.hpp>
#include <cppad/cg/model/atomic_external_function_wrapper.hpp>
#include <cppad/cg/model/generic_model_external_function_wrapper.hpp>
#include <cppad/cg/model/model_library_processor.hpp>
#include <cppad/cg/model/model_library.hpp>
#include <cppad/cg/model/generic_model.hpp>
#include <cppad/cg/model/functor_generic_model.hpp>
#include <cppad/cg/model/functor_model_library.hpp>
#include <cppad/cg/model/save_files_model_library_processor.hpp>

// automated static library creation
#include <cppad/cg/model/dynamic_lib/archiver.hpp>
#include <cppad/cg/model/dynamic_lib/ar_archiver.hpp>

// compiler
#include <cppad/cg/model/compiler/c_compiler.hpp>
#include <cppad/cg/model/compiler/abstract_c_compiler.hpp>
#include <cppad/cg/model/compiler/gcc_compiler.hpp>
#include <cppad/cg/model/compiler/clang_compiler.hpp>

// model source code generation helpers
#include <cppad/cg/model/model_c_source_gen.hpp>
#include <cppad/cg/model/model_c_source_gen_impl.hpp>
#include <cppad/cg/model/model_library_c_source_gen.hpp>
#include <cppad/cg/model/model_library_c_source_gen_impl.hpp>

#include <cppad/cg/model/model_c_source_gen_for0.hpp>
#include <cppad/cg/model/model_c_source_gen_for1.hpp>
#include <cppad/cg/model/model_c_source_gen_rev1.hpp>
#include <cppad/cg/model/model_c_source_gen_rev2.hpp>
#include <cppad/cg/model/model_c_source_gen_jac.hpp>
#include <cppad/cg/model/model_c_source_gen_hes.hpp>
#include <cppad/cg/model/patterns/model_c_source_gen_loops.hpp>
#include <cppad/cg/model/patterns/model_c_source_gen_loops_for0.hpp>
#include <cppad/cg/model/patterns/model_c_source_gen_loops_for1.hpp>
#include <cppad/cg/model/patterns/model_c_source_gen_loops_jac.hpp>
#include <cppad/cg/model/patterns/model_c_source_gen_loops_jac_fr1.hpp>
#include <cppad/cg/model/patterns/model_c_source_gen_loops_hess.hpp>
#include <cppad/cg/model/patterns/model_c_source_gen_loops_rev1.hpp>
#include <cppad/cg/model/patterns/model_c_source_gen_loops_rev2.hpp>
#include <cppad/cg/model/patterns/model_c_source_gen_loops_hess_r2.hpp>
#include <cppad/cg/model/patterns/hessian_with_loops_info.hpp>

// automated dynamic library creation
#include <cppad/cg/model/dynamic_lib/dynamiclib.hpp>
#include <cppad/cg/model/dynamic_lib/dynamic_library_processor.hpp>

// ---------------------------------------------------------------------------
// automated dynamic library creation for Linux
#include <cppad/cg/model/dynamic_lib/linux/linux_dynamiclib_model.hpp>
#include <cppad/cg/model/dynamic_lib/linux/linux_dynamiclib.hpp>
#include <cppad/cg/model/dynamic_lib/linux/linux_dynamic_model_library_processor.hpp>

// ---------------------------------------------------------------------------
// automated DAE differential index reduction
//#define CPPAD_CG_DAE_VERBOSE

#endif

