#ifndef CPPAD_CG_C_LANG_COMPILE_MODEL_HELPER_LOOPS_JAC_INCLUDED
#define CPPAD_CG_C_LANG_COMPILE_MODEL_HELPER_LOOPS_JAC_INCLUDED
/* --------------------------------------------------------------------------
 *  CppADCodeGen: C++ Algorithmic Differentiation with Source Code Generation:
 *    Copyright (C) 2013 Ciengis
 *
 *  CppADCodeGen is distributed under multiple licenses:
 *
 *   - Common Public License Version 1.0 (CPL1), and
 *   - GNU General Public License Version 2 (GPL2).
 *
 * CPL1 terms and conditions can be found in the file "epl-v10.txt", while
 * terms and conditions for the GPL2 can be found in the file "gpl2.txt".
 * ----------------------------------------------------------------------------
 * Author: Joao Leal
 */

namespace CppAD {

    class JacobianWithLoopsRowInfo {
    public:
        // tape J index -> {locationIt0, locationIt1, ...}
        std::map<size_t, std::vector<size_t> > indexedPositions;

        // original J index -> {locationIt0, locationIt1, ...}
        std::map<size_t, std::vector<size_t> > nonIndexedPositions;

        // original J index 
        std::set<size_t> nonIndexedEvals;

        // original J index -> k index 
        std::map<size_t, std::set<size_t> > tmpEvals;
    };

    /***************************************************************************
     *  Methods related with loop insertion into the operation graph
     **************************************************************************/

    template<class Base>
    vector<CG<Base> > CLangCompileModelHelper<Base>::prepareSparseJacobianWithLoops(CodeHandler<Base>& handler,
                                                                                    const vector<CGBase>& x,
                                                                                    bool forward) {
        using namespace std;
        using CppAD::vector;
        //printSparsityPattern(_jacSparsity.rows, _jacSparsity.cols, "jacobian", _fun->Range());

        handler.setZeroDependents(true);

        /**
         * determine sparsities
         */
        typename std::set<LoopModel<Base>*>::const_iterator itloop;
        for (itloop = _loopTapes.begin(); itloop != _loopTapes.end(); ++itloop) {
            LoopModel<Base>* l = *itloop;
            l->evalJacobianSparsity();
        }

        if (_funNoLoops != NULL)
            _funNoLoops->evalJacobianSparsity();

        /**
         * Generate index patterns for the jacobian elements resulting from loops
         */
        size_t nonIndexdedEqSize = _funNoLoops != NULL ? _funNoLoops->getOrigDependentIndexes().size() : 0;
        vector<set<size_t> > noLoopEvalSparsity(_funNoLoops != NULL ? _funNoLoops->getTapeDependentCount() : 0);

        // tape equation -> original J -> locations
        vector<map<size_t, set<size_t> > > noLoopEvalLocations(noLoopEvalSparsity.size());
        map<LoopModel<Base>*, vector<set<size_t> > > loopsEvalSparsities;

        // loop -> equation -> row info
        map<LoopModel<Base>*, std::vector<JacobianWithLoopsRowInfo> > loopEqInfo;
        for (itloop = _loopTapes.begin(); itloop != _loopTapes.end(); ++itloop) {
            LoopModel<Base>* loop = *itloop;
            loopEqInfo[loop].resize(loop->getTapeDependentCount());
            loopsEvalSparsities[loop].resize(loop->getTapeDependentCount());
        }

        size_t nnz = _jacSparsity.rows.size();
        vector<CGBase> jac(nnz);

        /** 
         * Load locations in the compressed jacobian
         */
        for (size_t e = 0; e < nnz; e++) {
            size_t i = _jacSparsity.rows[e];
            size_t j = _jacSparsity.cols[e];

            // find LOOP + get loop results
            LoopModel<Base>* loop = NULL;

            size_t tapeI;
            size_t iteration;

            for (itloop = _loopTapes.begin(); itloop != _loopTapes.end(); ++itloop) {
                LoopModel<Base>* l = *itloop;
                const std::map<size_t, LoopIndexedPosition>& depIndexes = l->getOriginalDependentIndexes();
                std::map<size_t, LoopIndexedPosition>::const_iterator iti = depIndexes.find(i);
                if (iti != depIndexes.end()) {
                    loop = l;
                    tapeI = iti->second.tape;
                    iteration = iti->second.iteration;
                    break;
                }
            }

            if (loop == NULL) {
                /**
                 * Equation present in the model without loops
                 */
                assert(_funNoLoops != NULL);
                size_t il = _funNoLoops->getLocalDependentIndex(i);

                noLoopEvalSparsity[il].insert(j);
                noLoopEvalLocations[il][j].insert(e);

            } else {
                /**
                 * Equation belongs to a loop
                 */
                size_t iterations = loop->getIterationCount();

                const std::vector<std::vector<LoopPosition> >& indexedIndepIndexes = loop->getIndexedIndepIndexes();
                const std::vector<LoopPosition>& nonIndexedIndepIndexes = loop->getNonIndexedIndepIndexes();
                const std::vector<LoopPosition>& temporaryIndependents = loop->getTemporaryIndependents();

                size_t nIndexed = indexedIndepIndexes.size();
                size_t nNonIndexed = nonIndexedIndepIndexes.size();
                //size_t nTape = nIndexed + nNonIndexed + temporaryIndependents.size();

                const vector<std::set<size_t> >& loopSparsity = loop->getJacobianSparsity();
                const std::set<size_t>& loopRow = loopSparsity[tapeI];

                JacobianWithLoopsRowInfo& rowInfo = loopEqInfo[loop][tapeI];

                std::set<size_t>& loopEvalRow = loopsEvalSparsities[loop][tapeI];

                /**
                 * find if there are indexed variables in this equation pattern
                 * and iteration which use j
                 */
                const std::set<size_t>& tapeJs = loop->getIndexedTapeIndexes(iteration, j);
                std::set<size_t>::const_iterator itTJ;
                for (itTJ = tapeJs.begin(); itTJ != tapeJs.end(); ++itTJ) {
                    size_t tapeJ = *itTJ;
                    if (loopRow.find(tapeJ) != loopRow.end()) {
                        loopEvalRow.insert(tapeJ);

                        //this indexed variable must be request for all iterations 
                        std::vector<size_t>& positions = rowInfo.indexedPositions[tapeJ];
                        positions.resize(iterations, nnz);
                        if (positions[iteration] != nnz) {
                            std::ostringstream ss;
                            ss << "Repeated jacobian elements requested (equation " << i << ", variable " << j << ")";
                            throw CGException(ss.str());
                        }
                        positions[iteration] = e;
                    }
                }

                /**
                 * find if there is a non indexed variable in this equation pattern for j
                 */
                const LoopPosition* pos = loop->getNonIndexedIndepIndexes(j);
                bool jInNonIndexed = false;
                if (pos != NULL && loopRow.find(pos->tape) != loopRow.end()) {
                    loopEvalRow.insert(pos->tape);

                    //this non-indexed element must be request for all iterations 
                    std::vector<size_t>& positions = rowInfo.nonIndexedPositions[j];
                    positions.resize(iterations, nnz);
                    if (positions[iteration] != nnz) {
                        std::ostringstream ss;
                        ss << "Repeated jacobian elements requested (equation " << i << ", variable " << j << ")";
                        throw CGException(ss.str());
                    }
                    positions[iteration] = e;

                    rowInfo.nonIndexedEvals.insert(j);

                    jInNonIndexed = true;
                }

                /**
                 * find temporary variables used by this equation pattern
                 */
                if (_funNoLoops != NULL) {
                    set<size_t>::const_iterator itz = loopRow.lower_bound(nIndexed + nNonIndexed);

                    // loop temporary variables
                    for (; itz != loopRow.end(); ++itz) {
                        size_t k = temporaryIndependents[*itz - nIndexed - nNonIndexed].original;

                        /**
                         * check if this temporary depends on j
                         */
                        bool used = false;
                        const set<size_t>& sparsity = _funNoLoops->getJacobianSparsity()[nonIndexdedEqSize + k];
                        if (sparsity.find(j) != sparsity.end()) {
                            noLoopEvalSparsity[nonIndexdedEqSize + k].insert(j); // element required
                            if (!jInNonIndexed) {
                                std::vector<size_t>& positions = rowInfo.nonIndexedPositions[j];
                                positions.resize(iterations, nnz);
                                if (positions[iteration] != nnz) {
                                    std::ostringstream ss;
                                    ss << "Repeated jacobian elements requested (equation " << i << ", variable " << j << ")";
                                    throw CGException(ss.str());
                                }
                                positions[iteration] = e;
                                jInNonIndexed = true;
                            }
                            rowInfo.tmpEvals[j].insert(k);
                            used = true;
                        }

                        if (used) {
                            // this temporary variable should be evaluated
                            loopEvalRow.insert(*itz);
                        }
                    }
                }

            }
        }

        /**
         * Check that the jacobian elements are requested for all iterations
         */
        typename map<LoopModel<Base>*, std::vector<JacobianWithLoopsRowInfo> >::iterator itl2Eq;
        for (itl2Eq = loopEqInfo.begin(); itl2Eq != loopEqInfo.end(); ++itl2Eq) {
            LoopModel<Base>* loop = itl2Eq->first;
            size_t iterations = loop->getIterationCount();
            std::vector<JacobianWithLoopsRowInfo>& eqs = itl2Eq->second;

            for (size_t tapeI = 0; tapeI < eqs.size(); tapeI++) {
                JacobianWithLoopsRowInfo& rowInfo = eqs[tapeI];

                // tape J index -> {locationIt0, locationIt1, ...}
                map<size_t, std::vector<size_t> >::iterator itJ2Pos;
                for (itJ2Pos = rowInfo.indexedPositions.begin(); itJ2Pos != rowInfo.indexedPositions.end(); ++itJ2Pos) {
                    size_t tapeJ = itJ2Pos->first;
                    const std::vector<size_t>& positions = itJ2Pos->second;
                    for (size_t it = 0; it < iterations; it++) {
                        if (positions[it] == nnz) {
                            const LoopPosition& eqPos = loop->getDependentIndexes()[tapeI][it];
                            const std::vector<LoopPosition>& indexPos = loop->getIndexedIndepIndexes()[tapeJ];
                            std::ostringstream ss;
                            ss << "Jacobian elements for an equation pattern (equation in a loop) must be requested for all iterations.\n"
                                    "Element for the indexed variable (";
                            for (size_t it2 = 0; it2 < indexPos.size(); it2++) {
                                if (it2 > 0) ss << ", ";
                                ss << indexPos[it2].original;
                            }
                            ss << ") was NOT requested for equation " << eqPos.original << " (iteration " << it << ").";
                            throw CGException(ss.str());
                        }
                    }
                }

                // original J index -> {locationIt0, locationIt1, ...}
                for (itJ2Pos = rowInfo.nonIndexedPositions.begin(); itJ2Pos != rowInfo.nonIndexedPositions.end(); ++itJ2Pos) {
                    size_t j = itJ2Pos->first;
                    const std::vector<size_t>& positions = itJ2Pos->second;
                    for (size_t it = 0; it < iterations; it++) {
                        if (positions[it] == nnz) {
                            const LoopPosition& eqPos = loop->getDependentIndexes()[tapeI][it];
                            std::ostringstream ss;
                            ss << "Jacobian elements for an equation pattern (equation in a loop) must be requested for all iterations.\n"
                                    "Element for the non-indexed variable (" << j << ") was NOT requested for equation " << eqPos.original << " (iteration " << it << ").";
                            throw CGException(ss.str());
                        }
                    }
                }
            }

        }

        /***********************************************************************
         *        generate the operation graph
         **********************************************************************/

        /**
         * original equations outside the loops 
         */
        // temporaries (zero orders)
        vector<CGBase> tmps;

        // jacobian for temporaries
        std::map<size_t, std::map<size_t, CGBase> > dzDx;

        // jacobian for equations outside loops
        vector<CGBase> jacNoLoop;
        if (_funNoLoops != NULL) {
            ADFun<CGBase>& fun = _funNoLoops->getTape();

            /**
             * zero order
             */
            vector<CGBase> depNL = _funNoLoops->getTape().Forward(0, x);

            tmps.resize(depNL.size() - nonIndexdedEqSize);
            for (size_t i = 0; i < tmps.size(); i++)
                tmps[i] = depNL[nonIndexdedEqSize + i];

            /**
             * jacobian
             */
            vector<size_t> row, col;
            generateSparsityIndexes(noLoopEvalSparsity, row, col);
            jacNoLoop.resize(row.size());

            CppAD::sparse_jacobian_work work; // temporary structure for CPPAD
            if (forward) {
                fun.SparseJacobianForward(x, _funNoLoops->getJacobianSparsity(), row, col, jacNoLoop, work);
            } else {
                fun.SparseJacobianReverse(x, _funNoLoops->getJacobianSparsity(), row, col, jacNoLoop, work);
            }

            for (size_t el = 0; el < row.size(); el++) {
                size_t il = row[el];
                size_t j = col[el];
                if (il < nonIndexdedEqSize) {
                    // (dy_i/dx_v) elements from equations outside loops
                    const std::set<size_t>& locations = noLoopEvalLocations[il][j];
                    for (std::set<size_t>::const_iterator itE = locations.begin(); itE != locations.end(); ++itE)
                        jac[*itE] = jacNoLoop[el];
                } else {
                    // dz_k/dx_v (for temporary variable)
                    size_t k = il - nonIndexdedEqSize;
                    dzDx[k][j] = jacNoLoop[el];
                }
            }
        }

        /***********************************************************************
         * Generate loop body
         **********************************************************************/
        IndexDclrOperationNode<Base>* iterationIndexDcl = new IndexDclrOperationNode<Base>(LoopModel<Base>::ITERATION_INDEX_NAME);
        handler.manageOperationNodeMemory(iterationIndexDcl);

        vector<CGBase> jacLoop;

        // loop loops :)
        for (itl2Eq = loopEqInfo.begin(); itl2Eq != loopEqInfo.end(); ++itl2Eq) {
            LoopModel<Base>& lModel = *itl2Eq->first;
            std::vector<JacobianWithLoopsRowInfo>& eqs = itl2Eq->second;
            ADFun<CGBase>& fun = lModel.getTape();

            size_t nIndexed = lModel.getIndexedIndepIndexes().size();
            size_t nNonIndexed = lModel.getNonIndexedIndepIndexes().size();

            /**
             * make the loop start
             */
            LoopStartOperationNode<Base>* loopStart = new LoopStartOperationNode<Base>(*iterationIndexDcl, lModel.getIterationCount());
            handler.manageOperationNodeMemory(loopStart);

            IndexOperationNode<Base>* iterationIndexOp = new IndexOperationNode<Base>(*loopStart);
            handler.manageOperationNodeMemory(iterationIndexOp);
            std::set<IndexOperationNode<Base>*> indexesOps;
            indexesOps.insert(iterationIndexOp);

            /**
             * evaluate loop model jacobian
             */
            vector<CGBase> indexedIndeps = createIndexedIndependents(handler, lModel, *iterationIndexOp);
            vector<CGBase> xl = createLoopIndependentVector(handler, lModel, indexedIndeps, x, tmps);

            vector<size_t> row, col;
            generateSparsityIndexes(loopsEvalSparsities[&lModel], row, col);
            jacLoop.resize(row.size());

            CppAD::sparse_jacobian_work work; // temporary structure for CppAD
            if (forward) {
                fun.SparseJacobianForward(xl, lModel.getJacobianSparsity(), row, col, jacLoop, work);
            } else {
                fun.SparseJacobianReverse(xl, lModel.getJacobianSparsity(), row, col, jacLoop, work);
            }

            // organize results
            std::vector<std::map<size_t, CGBase> > dyiDxtape(lModel.getTapeDependentCount());
            for (size_t el = 0; el < jacLoop.size(); el++) {
                size_t tapeI = row[el];
                size_t tapeJ = col[el];
                dyiDxtape[tapeI][tapeJ] = jacLoop[el];
            }

            // all assigned elements in the compressed jacobian by this loop
            std::set<size_t> allLocations;

            // store results in indexedLoopResults
            size_t jacElSize = 0;
            for (size_t tapeI = 0; tapeI < eqs.size(); tapeI++) {
                JacobianWithLoopsRowInfo& rowInfo = eqs[tapeI];
                jacElSize += rowInfo.indexedPositions.size();
                jacElSize += rowInfo.nonIndexedPositions.size();
            }
            vector<std::pair<CGBase, IndexPattern*> > indexedLoopResults(jacElSize);
            size_t jacLE = 0;

            // create the dependents (jac elements) for indexed and constant 
            for (size_t tapeI = 0; tapeI < eqs.size(); tapeI++) {
                JacobianWithLoopsRowInfo& rowInfo = eqs[tapeI];

                /**
                 * indexed variable contributions
                 */
                // tape J index -> {locationIt0, locationIt1, ...}
                std::map<size_t, std::vector<size_t> >::iterator itJ2Pos;
                for (itJ2Pos = rowInfo.indexedPositions.begin(); itJ2Pos != rowInfo.indexedPositions.end(); ++itJ2Pos) {
                    size_t tapeJ = itJ2Pos->first;
                    const std::vector<size_t>& positions = itJ2Pos->second;

                    allLocations.insert(positions.begin(), positions.end());

                    // generate the index pattern for the jacobian compressed element
                    IndexPattern* pattern = IndexPattern::detect(positions);
                    handler.manageLoopDependentIndexPattern(pattern);

                    indexedLoopResults[jacLE++] = std::make_pair(dyiDxtape[tapeI][tapeJ], pattern);
                }

                /**
                 * non-indexed variable contributions
                 */
                // original J index -> {locationIt0, locationIt1, ...}
                for (itJ2Pos = rowInfo.nonIndexedPositions.begin(); itJ2Pos != rowInfo.nonIndexedPositions.end(); ++itJ2Pos) {
                    size_t j = itJ2Pos->first;
                    const std::vector<size_t>& positions = itJ2Pos->second;

                    allLocations.insert(positions.begin(), positions.end());

                    // generate the index pattern for the jacobian compressed element
                    IndexPattern* pattern = IndexPattern::detect(positions);
                    handler.manageLoopDependentIndexPattern(pattern);

                    CGBase jacVal = Base(0);

                    // non-indexed variables used directly
                    const LoopPosition* pos = lModel.getNonIndexedIndepIndexes(j);
                    if (pos != NULL) {
                        size_t tapeJ = pos->tape;
                        typename std::map<size_t, CGBase>::const_iterator itVal = dyiDxtape[tapeI].find(tapeJ);
                        if (itVal != dyiDxtape[tapeI].end()) {
                            jacVal += itVal->second;
                        }
                    }

                    // non-indexed variables used through temporary variables
                    std::map<size_t, std::set<size_t> >::const_iterator itks = rowInfo.tmpEvals.find(j);
                    if (itks != rowInfo.tmpEvals.end()) {
                        const std::set<size_t>& ks = itks->second;
                        std::set<size_t>::const_iterator itk;
                        for (itk = ks.begin(); itk != ks.end(); ++itk) {
                            size_t k = *itk;
                            size_t tapeJ = nIndexed + nNonIndexed + k;

                            jacVal += dyiDxtape[tapeI][tapeJ] * dzDx[k][j];
                        }
                    }

                    indexedLoopResults[jacLE++] = std::make_pair(jacVal, pattern);
                }
            }
            assert(jacLE == indexedLoopResults.size());

            /**
             * make the loop end
             */
            size_t assignOrAdd = 1;
            LoopEndOperationNode<Base>* loopEnd = createLoopEnd(handler, *loopStart, indexedLoopResults, indexesOps, assignOrAdd);

            std::vector<size_t> info(1);
            std::vector<Argument<Base> > args(1);
            std::set<size_t>::const_iterator itE;
            for (itE = allLocations.begin(); itE != allLocations.end(); ++itE) {
                // an additional alias variable is required so that each dependent variable can have its own ID
                size_t e = *itE;
                info[0] = e;
                args[0] = Argument<Base>(*loopEnd);
                jac[e] = handler.createCG(new OperationNode<Base> (CGDependentRefRhsOp, info, args));
            }

            /**
             * move no-nindexed expressions outside loop
             */
            moveNonIndexedOutsideLoop(*loopStart, *loopEnd);
        }

        return jac;
    }

}

#endif