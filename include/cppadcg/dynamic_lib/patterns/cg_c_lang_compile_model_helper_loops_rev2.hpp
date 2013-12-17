#ifndef CPPAD_CG_C_LANG_COMPILE_MODEL_HELPER_LOOPS_REV2_INCLUDED
#define CPPAD_CG_C_LANG_COMPILE_MODEL_HELPER_LOOPS_REV2_INCLUDED
/* --------------------------------------------------------------------------
 *  CppADCodeGen: C++ Algorithmic Differentiation with Source Code Generation:
 *    Copyright (C) 2013 Ciengis
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

namespace CppAD {

    /***************************************************************************
     *  Utility classes and functions
     **************************************************************************/
    namespace loops {

        template<class Base>
        class HessianWithLoopsInfo;

        template<class Base>
        class HessianTermContrib;

        template<class Base>
        bool operator<(const HessianTermContrib<Base>& l, const HessianTermContrib<Base>& r);

        template<class Base>
        class HessianRowGroup;

        template<class Base>
        std::vector<std::pair<CG<Base>, IndexPattern*> > generateReverseTwoGroupOps(CodeHandler<Base>& handler,
                                                                                    const LoopModel<Base>& lModel,
                                                                                    const loops::HessianWithLoopsInfo<Base>& info,
                                                                                    HessianRowGroup<Base>& group,
                                                                                    const CG<Base>& tx1,
                                                                                    const std::map<size_t, std::map<size_t, CG<Base> > >& dzDx,
                                                                                    std::map<size_t, std::set<size_t> >& jrow2CompressedLoc);

        template<class Base>
        std::pair<CG<Base>, IndexPattern*> createReverseMode2Contribution(CodeHandler<Base>& handler,
                                                                          HessianRowGroup<Base>& group,
                                                                          const std::vector<HessianElement>& positions,
                                                                          const CG<Base>& ddfdxdx,
                                                                          const CG<Base>& tx1,
                                                                          IndexOperationNode<Base>& iterationIndexOp,
                                                                          std::map<size_t, std::set<size_t> >& jrow2CompressedLoc);
    }

    /***************************************************************************
     *  Methods related with loop insertion into the operation graph
     **************************************************************************/

    template<class Base>
    void CLangCompileModelHelper<Base>::prepareSparseReverseTwoWithLoops(std::map<std::string, std::string>& sources,
                                                                         const std::map<size_t, std::vector<size_t> >& elements) throw (CGException) {
        using namespace std;
        using namespace CppAD::loops;
        using namespace CppAD::extra;
        using CppAD::vector;

        size_t m = _fun.Range();
        size_t n = _fun.Domain();

        IndexDclrOperationNode<Base> indexJrowDcl("jrow");
        IndexDclrOperationNode<Base> indexLocalItDcl("it");
        IndexDclrOperationNode<Base> indexLocalItCountDcl("itCount");
        IndexDclrOperationNode<Base> indexIterationDcl(LoopModel<Base>::ITERATION_INDEX_NAME);
        IndexOperationNode<Base> jrowIndexOp(indexJrowDcl);

        std::vector<OperationNode<Base>* > localNodes(5);
        localNodes[0] = &indexJrowDcl;
        localNodes[1] = &indexLocalItDcl;
        localNodes[2] = &indexLocalItCountDcl;
        localNodes[3] = &indexIterationDcl;
        localNodes[4] = &jrowIndexOp;

        CodeHandler<Base> handler;
        handler.setJobTimer(_jobTimer);
        handler.setZeroDependents(false);

        // independent variables
        vector<CGBase> x(n);
        handler.makeVariables(x);
        if (_x.size() > 0) {
            for (size_t i = 0; i < n; i++) {
                x[i].setValue(_x[i]);
            }
        }

        CGBase tx1;
        handler.makeVariable(tx1);
        if (_x.size() > 0) {
            tx1.setValue(Base(1.0));
        }

        // multipliers
        vector<CGBase> py(m); // (k+1)*m is not used because we are not interested in all values
        handler.makeVariables(py);
        if (_x.size() > 0) {
            for (size_t i = 0; i < m; i++) {
                py[i].setValue(Base(1.0));
            }
        }

        size_t nonIndexdedEqSize = _funNoLoops != NULL ? _funNoLoops->getOrigDependentIndexes().size() : 0;

        size_t nnz = 0;
        std::map<size_t, std::vector<size_t> >::const_iterator itJrow2jcols;
        for (itJrow2jcols = elements.begin(); itJrow2jcols != elements.end(); ++itJrow2jcols) {
            nnz += itJrow2jcols->second.size();
        }

        std::vector<size_t> hessRows(nnz);
        std::vector<size_t> hessCols(nnz);
        std::vector<size_t> hessOrder(nnz);
        size_t ge = 0;
        for (itJrow2jcols = elements.begin(); itJrow2jcols != elements.end(); ++itJrow2jcols) {
            size_t jrow = itJrow2jcols->first;
            const std::vector<size_t>& jcols = itJrow2jcols->second;

            for (size_t e = 0; e < jcols.size(); e++) {
                hessRows[ge] = jrow;
                hessCols[ge] = jcols[e];
                hessOrder[ge] = e;
                ge++;
            }
        }

        vector<set<size_t> > noLoopEvalJacSparsity;
        vector<set<size_t> > noLoopEvalHessSparsity;
        vector<map<size_t, set<size_t> > > noLoopEvalHessLocations;
        map<LoopModel<Base>*, loops::HessianWithLoopsInfo<Base> > loopHessInfo;

        analyseSparseHessianWithLoops(hessRows, hessCols, hessOrder,
                                      noLoopEvalJacSparsity, noLoopEvalHessSparsity,
                                      noLoopEvalHessLocations, loopHessInfo, false);

        /***********************************************************************
         *        generate the operation graph
         **********************************************************************/
        /**
         * Calculate Hessians and Jacobians
         */
        // temporaries (zero orders)
        vector<CGBase> tmpsAlias;
        if (_funNoLoops != NULL) {
            ADFun<CGBase>& fun = _funNoLoops->getTape();

            tmpsAlias.resize(fun.Range() - nonIndexdedEqSize);
            for (size_t k = 0; k < tmpsAlias.size(); k++) {
                // to be defined later
                tmpsAlias[k] = handler.createCG(new OperationNode<Base>(CGAliasOp));
            }
        }

        /**
         * prepare loop independents
         */
        typename map<LoopModel<Base>*, HessianWithLoopsInfo<Base> >::iterator itLoop2Info;
        for (itLoop2Info = loopHessInfo.begin(); itLoop2Info != loopHessInfo.end(); ++itLoop2Info) {
            LoopModel<Base>& lModel = *itLoop2Info->first;
            HessianWithLoopsInfo<Base>& info = itLoop2Info->second;

            info.iterationIndexOp = new IndexOperationNode<Base>(indexIterationDcl);
            handler.manageOperationNodeMemory(info.iterationIndexOp);
            set<IndexOperationNode<Base>*> indexesOps;
            indexesOps.insert(info.iterationIndexOp);

            /**
             * make the loop's indexed variables
             */
            vector<CGBase> indexedIndeps = createIndexedIndependents(handler, lModel, *info.iterationIndexOp);
            info.x = createLoopIndependentVector(handler, lModel, indexedIndeps, x, tmpsAlias);

            info.w = createLoopDependentVector(handler, lModel, *info.iterationIndexOp);
        }

        /**
         * Calculate Hessians and Jacobians
         */
        /**
         * Loops - evaluate Jacobian and Hessian
         */
        bool hasAtomics = isAtomicsUsed(); // TODO: improve this by checking only the current fun
        //const std::map<size_t, std::set<size_t> >& aaa = getAtomicsIndeps();

        for (itLoop2Info = loopHessInfo.begin(); itLoop2Info != loopHessInfo.end(); ++itLoop2Info) {
            LoopModel<Base>& lModel = *itLoop2Info->first;
            HessianWithLoopsInfo<Base>& info = itLoop2Info->second;

            _cache.str("");
            _cache << "model (Jacobian + Hessian, loop " << lModel.getLoopId() << ")";
            std::string jobName = _cache.str();

            startingJob("'" + jobName + "'", JobTimer::GRAPH);

            info.evalLoopModelJacobianHessian(hasAtomics);

            finishedJob();
        }

        /**
         * No loops
         */
        map<size_t, map<size_t, CGBase> > dzDx;
        if (_funNoLoops != NULL) {
            ADFun<CGBase>& fun = _funNoLoops->getTape();
            vector<CGBase> yNL(fun.Range());

            /**
             * Jacobian and Hessian - temporary variables
             */
            startingJob("'model (Jacobian + Hessian, temporaries)'", JobTimer::GRAPH);

            dzDx = _funNoLoops->calculateJacobianHessianUsedByLoops(handler,
                                                                    loopHessInfo, x, yNL,
                                                                    noLoopEvalJacSparsity,
                                                                    hasAtomics);

            finishedJob();

            for (size_t i = 0; i < tmpsAlias.size(); i++)
                tmpsAlias[i].getOperationNode()->getArguments().push_back(asArgument(yNL[nonIndexdedEqSize + i]));

            for (itLoop2Info = loopHessInfo.begin(); itLoop2Info != loopHessInfo.end(); ++itLoop2Info) {
                HessianWithLoopsInfo<Base>& info = itLoop2Info->second;
                // not needed anymore:
                info.dyiDzk.clear();
            }
        }

        /**
         * Loops - Hessian
         */
        for (itLoop2Info = loopHessInfo.begin(); itLoop2Info != loopHessInfo.end(); ++itLoop2Info) {
            LoopModel<Base>& lModel = *itLoop2Info->first;
            HessianWithLoopsInfo<Base>& info = itLoop2Info->second;

            // reset nodes not managed by a handler
            if (itLoop2Info != loopHessInfo.begin()) {
                for (size_t j = 0; j < localNodes.size(); j++) {
                    localNodes[j]->resetHandlerCounters();
                    localNodes[j]->setColor(0);
                }
            }

            /*******************************************************************
             * create Hessian row groups
             * for the contributions from the equations in loops
             ******************************************************************/
            SmartVectorPointer<HessianRowGroup<Base> > loopGroups;

            generateHessianRowGroups(lModel, info, n, loopGroups);

            /*******************************************************************
             * generate the operation graph for each Hessian row subgroup
             ******************************************************************/
            for (size_t g = 0; g < loopGroups.v.size(); g++) {
                HessianRowGroup<Base>& group = *loopGroups.v[g];

                /**
                 * determine if a loop should be created
                 */
                LoopStartOperationNode<Base>* loopStart = NULL;

                map<size_t, set<size_t> > localIterCount2Jrows;

                map<size_t, set<size_t> >::const_iterator itJrow2It;
                for (itJrow2It = group.jRow2Iterations.begin(); itJrow2It != group.jRow2Iterations.end(); ++itJrow2It) {
                    size_t jrow = itJrow2It->first;
                    size_t itCount = itJrow2It->second.size();
                    localIterCount2Jrows[itCount].insert(jrow);
                }

                bool createsLoop = localIterCount2Jrows.size() != 1 || // is there a different number of it
                        localIterCount2Jrows.begin()->first != 1; // is there always just on iteration?

                /**
                 * Model index pattern
                 * 
                 * detect the index pattern for the model iterations
                 * based on jrow and the local loop iteration
                 */
                map<size_t, map<size_t, size_t> > jrow2localIt2ModelIt;

                for (itJrow2It = group.jRow2Iterations.begin(); itJrow2It != group.jRow2Iterations.end(); ++itJrow2It) {
                    size_t jrow = itJrow2It->first;

                    map<size_t, size_t>& localIt2ModelIt = jrow2localIt2ModelIt[jrow];
                    size_t localIt = 0;
                    set<size_t>::const_iterator itIt;
                    for (itIt = itJrow2It->second.begin(); itIt != itJrow2It->second.end(); ++itIt, localIt++) {
                        localIt2ModelIt[localIt] = *itIt;
                    }
                }

                /**
                 * try to fit a combination of two patterns:
                 *  j = fStart(jrow) + flit(lit);
                 */
                std::auto_ptr<IndexPattern> itPattern(Plane2DIndexPattern::detectPlane2D(jrow2localIt2ModelIt));

                if (itPattern.get() == NULL) {
                    // did not match!
                    itPattern.reset(new Random2DIndexPattern(jrow2localIt2ModelIt));
                }

                /**
                 * Local iteration count pattern
                 */
                std::auto_ptr<IndexOperationNode<Base> > localIterIndexOp;
                std::auto_ptr<IndexOperationNode<Base> > localIterCountIndexOp;
                std::auto_ptr<IndexAssignOperationNode<Base> > itCountAssignOp;
                std::auto_ptr<IndexPattern> indexLocalItCountPattern;

                if (createsLoop) {
                    map<size_t, size_t> jrow2litCount;

                    map<size_t, set<size_t> >::const_iterator itJrow2Its;
                    for (itJrow2Its = group.jRow2Iterations.begin(); itJrow2Its != group.jRow2Iterations.end(); ++itJrow2Its) {
                        size_t jrow = itJrow2Its->first;
                        jrow2litCount[jrow] = itJrow2Its->second.size();
                    }

                    indexLocalItCountPattern.reset(IndexPattern::detect(jrow2litCount));

                    if (IndexPattern::isConstant(*indexLocalItCountPattern.get())) {
                        size_t itCount = group.jRow2Iterations.begin()->second.size();
                        loopStart = new LoopStartOperationNode<Base>(indexLocalItDcl, itCount);
                    } else {
                        itCountAssignOp.reset(new IndexAssignOperationNode<Base>(indexLocalItCountDcl, *indexLocalItCountPattern.get(), jrowIndexOp));
                        localIterCountIndexOp.reset(new IndexOperationNode<Base>(*itCountAssignOp.get()));
                        loopStart = new LoopStartOperationNode<Base>(indexLocalItDcl, *localIterCountIndexOp.get());
                    }
                    handler.manageOperationNodeMemory(loopStart);

                    localIterIndexOp.reset(new IndexOperationNode<Base>(*loopStart));
                }


                IndexAssignOperationNode<Base> iterationIndexPatternOp(indexIterationDcl, *itPattern.get(), &jrowIndexOp, localIterIndexOp.get());
                info.iterationIndexOp->makeAssigmentDependent(iterationIndexPatternOp);

                map<size_t, set<size_t> > jrow2CompressedLoc;
                std::vector<pair<CG<Base>, IndexPattern*> > indexedLoopResults;

                indexedLoopResults = generateReverseTwoGroupOps(handler, lModel, info,
                                                                group, tx1,
                                                                dzDx,
                                                                jrow2CompressedLoc);

                _loopRev2Groups[&lModel][g] = jrow2CompressedLoc;

                LoopEndOperationNode<Base>* loopEnd = NULL;
                vector<CGBase> pxCustom;
                if (createsLoop) {
                    /**
                     * make the loop end
                     */
                    size_t assignOrAdd = 1;
                    set<IndexOperationNode<Base>*> indexesOps;
                    indexesOps.insert(info.iterationIndexOp);
                    loopEnd = createLoopEnd(handler, *loopStart, indexedLoopResults, indexesOps, assignOrAdd);

                    /**
                     * move non-indexed expressions outside loop
                     */
                    moveNonIndexedOutsideLoop(handler, *loopStart, *loopEnd);

                    /**
                     * 
                     */
                    pxCustom.resize(1);
                    std::vector<size_t> info(1);
                    info[0] = 0; // must point to itself since there is only one dependent
                    std::vector<Argument<Base> > args(1);
                    args[0] = Argument<Base>(*loopEnd);
                    pxCustom[0] = handler.createCG(new OperationNode<Base> (CGDependentRefRhsOp, info, args));

                } else {
                    /**
                     * No loop required
                     */
                    pxCustom.resize(indexedLoopResults.size());
                    for (size_t i = 0; i < indexedLoopResults.size(); i++) {
                        const CGBase& val = indexedLoopResults[i].first;
                        IndexPattern* ip = indexedLoopResults[i].second;

                        pxCustom[i] = createLoopDependentFunctionResult(handler, i, val, ip, *info.iterationIndexOp);
                    }

                }

                CLanguage<Base> langC(_baseTypeName);
                langC.setFunctionIndexArgument(indexJrowDcl);
                langC.setParameterPrecision(_parameterPrecision);

                std::ostringstream code;
                std::auto_ptr<VariableNameGenerator<Base> > nameGen(createVariableNameGenerator("px"));
                CLangDefaultReverse2VarNameGenerator<Base> nameGenRev2(nameGen.get(), n, 1);

                /**
                 * Generate the source code inside the loop
                 */
                _cache.str("");
                _cache << "model (reverse two, loop " << lModel.getLoopId() << ", group " << g << ")";
                string jobName = _cache.str();
                handler.generateCode(code, langC, pxCustom, nameGenRev2, _atomicFunctions, jobName);

                _cache.str("");
                generateFunctionNameLoopRev2(_cache, lModel, g);
                std::string functionName = _cache.str();

                std::string argsDcl = langC.generateFunctionArgumentsDcl();

                _cache.str("");
                _cache << "#include <stdlib.h>\n"
                        "#include <math.h>\n"
                        "\n"
                        << CLanguage<Base>::ATOMICFUN_STRUCT_DEFINITION << "\n"
                        "\n"
                        "void " << functionName << "(" << argsDcl << ") {\n";
                nameGenRev2.customFunctionVariableDeclarations(_cache);
                _cache << langC.generateIndependentVariableDeclaration() << "\n";
                _cache << langC.generateDependentVariableDeclaration() << "\n";
                _cache << langC.generateTemporaryVariableDeclaration(true) << "\n";
                nameGenRev2.prepareCustomFunctionVariables(_cache);

                // code inside the loop
                _cache << code.str();

                nameGenRev2.finalizeCustomFunctionVariables(_cache);
                _cache << "}\n\n";

                sources[functionName + ".c"] = _cache.str();
                _cache.str("");

                /**
                 * prepare the nodes to be reused!
                 */
                if (g + 1 < loopGroups.v.size()) {
                    handler.resetNodes(); // uncolor nodes
                }
            }

        }

        /*******************************************************************
         * equations NOT in loops
         ******************************************************************/
        if (_funNoLoops != NULL) {
            ADFun<CGBase>& fun = _funNoLoops->getTape();

            /**
             * hessian - original equations
             */
            std::vector<size_t> row, col;
            generateSparsityIndexes(noLoopEvalHessSparsity, row, col);

            if (row.size() > 0) {
                const string jobName = "model (reverse two, no loops)";
                startingJob("'" + jobName + "'", JobTimer::SOURCE_GENERATION);

                // we can use a new handler to reduce memmory usage
                CodeHandler<Base> handlerNL;
                handlerNL.setJobTimer(_jobTimer);

                vector<CGBase> tx0(n);
                handlerNL.makeVariables(tx0);
                if (_x.size() > 0) {
                    for (size_t i = 0; i < n; i++) {
                        tx0[i].setValue(_x[i]);
                    }
                }

                CGBase tx1;
                handlerNL.makeVariable(tx1);
                if (_x.size() > 0) {
                    tx1.setValue(Base(1.0));
                }

                vector<CGBase> py(m); // (k+1)*m is not used because we are not interested in all values
                handlerNL.makeVariables(py);

                vector<CGBase> pyNoLoop(_funNoLoops->getTapeDependentCount());

                const std::vector<size_t>& origIndexes = _funNoLoops->getOrigDependentIndexes();
                for (size_t inl = 0; inl < origIndexes.size(); inl++) {
                    pyNoLoop[inl] = py[origIndexes[inl]];
                    if (_x.size() > 0) {
                        pyNoLoop[inl].setValue(Base(1.0));
                    }
                }

                vector<CGBase> hessNoLoop(row.size());

                CppAD::sparse_hessian_work work; // temporary structure for CPPAD
                fun.SparseHessian(tx0, pyNoLoop, _funNoLoops->getHessianOrigEqsSparsity(), row, col, hessNoLoop, work);

                map<size_t, map<size_t, CGBase> > hess;
                // save non-indexed hessian elements
                for (size_t el = 0; el < row.size(); el++) {
                    size_t j1 = row[el];
                    size_t j2 = col[el];
                    const set<size_t>& locations = noLoopEvalHessLocations[j1][j2];
                    for (set<size_t>::const_iterator itE = locations.begin(); itE != locations.end(); ++itE) {
                        hess[j1][*itE] = hessNoLoop[el];
                        _nonLoopRev2Elements[j1].insert(*itE);
                    }
                }

                /**
                 * Generate one function for each independent variable / hessian row
                 */
                typename map<size_t, map<size_t, CGBase> >::const_iterator it;
                for (it = hess.begin(); it != hess.end(); ++it) {
                    size_t j = it->first;
                    const map<size_t, CGBase>& cols = it->second;

                    _cache.str("");
                    _cache << "model (reverse two, no loops, indep " << j << ")";
                    const string subJobName = _cache.str();

                    vector<CGBase> pxCustom(elements.at(j).size());

                    typename map<size_t, CGBase>::const_iterator it2;
                    for (it2 = cols.begin(); it2 != cols.end(); ++it2) {
                        size_t e = it2->first;
                        pxCustom[e] = it2->second * tx1;
                    }

                    CLanguage<Base> langC(_baseTypeName);
                    langC.setMaxAssigmentsPerFunction(_maxAssignPerFunc, &sources);
                    langC.setParameterPrecision(_parameterPrecision);
                    _cache.str("");
                    _cache << _name << "_" << FUNCTION_SPARSE_REVERSE_TWO << "_noloop_indep" << j;
                    string functionName = _cache.str();
                    langC.setGenerateFunction(functionName);

                    std::ostringstream code;
                    std::auto_ptr<VariableNameGenerator<Base> > nameGen(createVariableNameGenerator("px"));
                    CLangDefaultReverse2VarNameGenerator<Base> nameGenRev2(nameGen.get(), n, 1);

                    handlerNL.generateCode(code, langC, pxCustom, nameGenRev2, _atomicFunctions, subJobName);
                }

                finishedJob();
            }

        }

        /**
         * 
         */
        string functionRev2 = _name + "_" + FUNCTION_SPARSE_REVERSE_TWO;
        sources[functionRev2 + ".c"] = generateGlobalForRevWithLoopsFunctionSource(elements,
                                                                                   _loopRev2Groups, _nonLoopRev2Elements,
                                                                                   functionRev2, _name, _baseTypeName, "indep",
                                                                                   generateFunctionNameLoopRev2);
        /**
         * Sparsity
         */
        _cache.str("");
        generateSparsity1DSource2(_name + "_" + FUNCTION_REVERSE_TWO_SPARSITY, elements);
        sources[_name + "_" + FUNCTION_REVERSE_TWO_SPARSITY + ".c"] = _cache.str();
        _cache.str("");
    }

    namespace loops {

        template<class Base>
        void generateHessianRowGroups(const LoopModel<Base>& lModel,
                                      const HessianWithLoopsInfo<Base>& info,
                                      size_t n,
                                      SmartVectorPointer<HessianRowGroup<Base> >& loopGroups) {
            using namespace std;
            using namespace CppAD::loops;
            using CppAD::vector;

            /**
             * group rows with the same contribution terms
             */
            map<pairss, map<size_t, set<size_t> > > indexedIndexed2jrow2Iter;
            map<pairss, map<size_t, set<size_t> > > indexedNonIndexed2jrow2Iter;
            map<pairss, map<size_t, set<size_t> > > indexedTemp2jrow2Iter;
            map<pairss, map<size_t, set<size_t> > > nonIndexedIndexed2jrow2Iter;
            map<pairss, map<size_t, set<size_t> > > tempIndexed2jrow2Iter;

            map<HessianTermContrib<Base>, set<size_t> > contrib2jrows = groupHessianRowsByContrib(info, n,
                                                                                                  indexedIndexed2jrow2Iter,
                                                                                                  indexedNonIndexed2jrow2Iter,
                                                                                                  indexedTemp2jrow2Iter,
                                                                                                  nonIndexedIndexed2jrow2Iter,
                                                                                                  tempIndexed2jrow2Iter);

            loopGroups.v.reserve(contrib2jrows.size() *2); // TODO: improve this

            typename map<HessianTermContrib<Base>, set<size_t> >::const_iterator itC;
            for (itC = contrib2jrows.begin(); itC != contrib2jrows.end(); ++itC) {
                const HessianTermContrib<Base>& c = itC->first;
                const set<size_t>& jrows = itC->second;

                /**
                 * create subgroups
                 */
                subgroupHessianRowsByContrib(info, c, jrows,
                                             indexedIndexed2jrow2Iter,
                                             indexedNonIndexed2jrow2Iter,
                                             indexedTemp2jrow2Iter,
                                             nonIndexedIndexed2jrow2Iter,
                                             tempIndexed2jrow2Iter,
                                             loopGroups);
            }

        }

        template<class Base>
        std::vector<std::pair<CG<Base>, IndexPattern*> > generateReverseTwoGroupOps(CodeHandler<Base>& handler,
                                                                                    const LoopModel<Base>& lModel,
                                                                                    const HessianWithLoopsInfo<Base>& info,
                                                                                    HessianRowGroup<Base>& group,
                                                                                    const CG<Base>& tx1,
                                                                                    const std::map<size_t, std::map<size_t, CG<Base> > >& dzDx,
                                                                                    std::map<size_t, std::set<size_t> >& jrow2CompressedLoc) {
            using namespace std;
            using namespace CppAD::loops;
            using CppAD::vector;

            typedef CG<Base> CGBase;

            IndexOperationNode<Base>& iterationIndexOp = *info.iterationIndexOp;

            // store results in indexedLoopResults
            size_t hessElSize = group.size();

            std::vector<pair<CGBase, IndexPattern*> > indexedLoopResults(hessElSize);
            size_t hessLE = 0;

            set<pairss>::const_iterator it;

            map<pairss, std::vector<HessianElement> >::const_iterator itPos;

            /**
             * loop the groups of equations present at the same iterations
             */
            for (size_t g = 0; g < info.equationGroups.size(); g++) {

                const HessianWithLoopsEquationGroupInfo<Base>& infog = info.equationGroups[g];

                /***************************************************************
                 * indexed - indexed
                 */
                for (it = group.indexedIndexed.begin(); it != group.indexedIndexed.end(); ++it) {
                    size_t tapeJ1 = it->first;
                    size_t tapeJ2 = it->second;

                    itPos = infog.indexedIndexedPositions.find(*it);
                    if (itPos != infog.indexedIndexedPositions.end()) {
                        const std::vector<HessianElement>& positions = itPos->second;

                        addContribution(indexedLoopResults, hessLE,
                                        createReverseMode2Contribution(handler, group,
                                                                       positions, infog.hess.at(tapeJ1).at(tapeJ2), tx1,
                                                                       iterationIndexOp,
                                                                       jrow2CompressedLoc));
                    }
                }

                /**
                 * indexed - non-indexed
                 */
                for (it = group.indexedNonIndexed.begin(); it != group.indexedNonIndexed.end(); ++it) {
                    size_t tapeJ1 = it->first;
                    size_t tapeJ2 = it->second;

                    itPos = infog.indexedNonIndexedPositions.find(*it);
                    if (itPos != infog.indexedNonIndexedPositions.end()) {
                        const std::vector<HessianElement>& positions = itPos->second;

                        addContribution(indexedLoopResults, hessLE,
                                        createReverseMode2Contribution(handler, group,
                                                                       positions, infog.hess.at(tapeJ1).at(tapeJ2), tx1,
                                                                       iterationIndexOp,
                                                                       jrow2CompressedLoc));
                    }
                }

                /**
                 * indexed - temporary
                 */
                for (it = group.indexedTemp.begin(); it != group.indexedTemp.end(); ++it) {
                    size_t tapeJ1 = it->first;
                    size_t j2 = it->second;

                    itPos = infog.indexedTempPositions.find(*it);
                    if (itPos != infog.indexedTempPositions.end()) {
                        const std::vector<HessianElement>& positions = itPos->second;
                        const set<size_t>& ks = infog.indexedTempEvals.at(*it);

                        CGBase val = Base(0);
                        set<size_t>::const_iterator itz;
                        for (itz = ks.begin(); itz != ks.end(); ++itz) {
                            size_t k = *itz;
                            size_t tapeK = lModel.getTempIndepIndexes(k)->tape;
                            val += infog.hess.at(tapeJ1).at(tapeK) * dzDx.at(k).at(j2);
                        }

                        addContribution(indexedLoopResults, hessLE,
                                        createReverseMode2Contribution(handler, group,
                                                                       positions, val, tx1,
                                                                       iterationIndexOp,
                                                                       jrow2CompressedLoc));
                    }
                }


                /***************************************************************
                 * non-indexed - indexed
                 */
                for (it = group.nonIndexedIndexed.begin(); it != group.nonIndexedIndexed.end(); ++it) {
                    size_t tapeJ1 = it->first;
                    size_t tapeJ2 = it->second;

                    itPos = infog.nonIndexedIndexedPositions.find(*it);
                    if (itPos != infog.nonIndexedIndexedPositions.end()) {
                        const std::vector<HessianElement>& positions = itPos->second;

                        addContribution(indexedLoopResults, hessLE,
                                        createReverseMode2Contribution(handler, group,
                                                                       positions, infog.hess.at(tapeJ1).at(tapeJ2), tx1,
                                                                       iterationIndexOp,
                                                                       jrow2CompressedLoc));
                    }
                }

                /***************************************************************
                 * temporary - indexed
                 * 
                 *      d f_i       .    d x_k1
                 * d x_l2  d z_k1        d x_j1
                 */
                for (it = group.tempIndexed.begin(); it != group.tempIndexed.end(); ++it) {
                    size_t j1 = it->first;
                    size_t tapeJ2 = it->second;

                    itPos = infog.tempIndexedPositions.find(*it);
                    if (itPos != infog.tempIndexedPositions.end()) {
                        const std::vector<HessianElement>& positions = itPos->second;
                        const set<size_t>& ks = infog.indexedTempEvals.at(pairss(tapeJ2, j1));

                        CGBase val = Base(0);
                        set<size_t>::const_iterator itz;
                        for (itz = ks.begin(); itz != ks.end(); ++itz) {
                            size_t k = *itz;
                            size_t tapeK = lModel.getTempIndepIndexes(k)->tape;
                            val += infog.hess.at(tapeK).at(tapeJ2) * dzDx.at(k).at(j1);
                        }

                        addContribution(indexedLoopResults, hessLE,
                                        createReverseMode2Contribution(handler, group,
                                                                       positions, val, tx1,
                                                                       iterationIndexOp,
                                                                       jrow2CompressedLoc));
                    }
                }
            }

            /*******************************************************************
             * contributions to a constant location
             */
            for (it = group.nonIndexedNonIndexed.begin(); it != group.nonIndexedNonIndexed.end(); ++it) {
                const pairss& orig = *it;
                size_t e = info.nonIndexedNonIndexedPosition.at(orig);

                size_t j1 = orig.first;
                size_t j2 = orig.second;
                const LoopPosition* posJ1 = lModel.getNonIndexedIndepIndexes(j1);
                const LoopPosition* posJ2 = lModel.getNonIndexedIndepIndexes(j2);

                // location
                LinearIndexPattern* pattern = new LinearIndexPattern(0, 0, 0, e);
                handler.manageLoopDependentIndexPattern(pattern);

                /**
                 * non-indexed - non-indexed
                 */
                CGBase hessVal = Base(0);

                /**
                 * loop the groups of equations present at the same iterations
                 */
                for (size_t g = 0; g < info.equationGroups.size(); g++) {
                    const IterEquationGroup<Base>& eqGroup = lModel.getEquationsGroups()[g];
                    set<size_t> iterations;
                    set_intersection(group.iterations.begin(), group.iterations.end(),
                                     eqGroup.iterations.begin(), eqGroup.iterations.end(),
                                     std::inserter(iterations, iterations.begin()));

                    CGBase gHessVal = Base(0);
                    const HessianWithLoopsEquationGroupInfo<Base>& infog = info.equationGroups[g];

                    if (infog.nonIndexedNonIndexedEvals.find(orig) != infog.nonIndexedNonIndexedEvals.end()) {
                        gHessVal = infog.hess.at(posJ1->tape).at(posJ2->tape);
                    }

                    /**
                     * non-indexed - temporary
                     */
                    map<pairss, set<size_t> >::const_iterator itNT = infog.nonIndexedTempEvals.find(orig);
                    if (itNT != infog.nonIndexedTempEvals.end()) {
                        const set<size_t>& ks = itNT->second;

                        set<size_t>::const_iterator itz;
                        for (itz = ks.begin(); itz != ks.end(); ++itz) {
                            size_t k = *itz;
                            size_t tapeK = lModel.getTempIndepIndexes(k)->tape;
                            gHessVal += infog.hess.at(posJ1->tape).at(tapeK) * dzDx.at(k).at(j2);
                        }
                    }

                    /**
                     * temporary - non-indexed 
                     * 
                     *      d f_i       .    d x_k1
                     * d x_j2  d z_k1        d x_j1
                     */
                    map<pairss, set<size_t> >::const_iterator itTN = infog.tempNonIndexedEvals.find(orig);
                    if (itTN != infog.tempNonIndexedEvals.end()) {
                        const set<size_t>& ks = itTN->second;

                        set<size_t>::const_iterator itz;
                        for (itz = ks.begin(); itz != ks.end(); ++itz) {
                            size_t k1 = *itz;
                            size_t tapeK = lModel.getTempIndepIndexes(k1)->tape;
                            gHessVal += infog.hess.at(tapeK).at(posJ2->tape) * dzDx.at(k1).at(j1);
                        }
                    }

                    /**
                     * temporary - temporary
                     */
                    map<pairss, map<size_t, set<size_t> > >::const_iterator itTT = infog.tempTempEvals.find(orig);
                    if (itTT != infog.tempTempEvals.end()) {
                        const map<size_t, set<size_t> >& k1k2 = itTT->second;

                        CGBase sum = Base(0);

                        map<size_t, set<size_t> >::const_iterator itzz;
                        for (itzz = k1k2.begin(); itzz != k1k2.end(); ++itzz) {
                            size_t k1 = itzz->first;
                            const set<size_t>& k2s = itzz->second;
                            size_t tapeK1 = lModel.getTempIndepIndexes(k1)->tape;

                            CGBase tmp = Base(0);
                            for (set<size_t>::const_iterator itk2 = k2s.begin(); itk2 != k2s.end(); ++itk2) {
                                size_t k2 = *itk2;
                                size_t tapeK2 = lModel.getTempIndepIndexes(k2)->tape;

                                tmp += infog.hess.at(tapeK1).at(tapeK2) * dzDx.at(k2).at(j2);
                            }

                            sum += tmp * dzDx.at(k1).at(j1);
                        }

                        gHessVal += sum;
                    }

                    if (iterations.size() != group.iterations.size()) {
                        CGBase v = createReverseMode2Contribution(handler, group,
                                                                  *pattern, iterations,
                                                                  gHessVal,
                                                                  *info.iterationIndexOp,
                                                                  group.ifElses);
                        addContribution(indexedLoopResults, hessLE, make_pair(v, (IndexPattern*) NULL));
                        jrow2CompressedLoc[j1].insert(e);
                    } else {
                        hessVal += gHessVal;
                    }
                }

                /**
                 * temporary - temporary
                 */
                map<pairss, set<size_t> >::const_iterator itTT2 = info.nonLoopNonIndexedNonIndexed.find(orig);
                if (itTT2 != info.nonLoopNonIndexedNonIndexed.end()) {
                    hessVal += info.dzDxx.at(j1).at(j2); // it is already the sum of ddz / dx_j1 dx_j2
                }

                hessVal *= tx1;

                // place the result
                if (!hessVal.isParameter() || !hessVal.IdenticalZero()) {
                    addContribution(indexedLoopResults, hessLE, make_pair(hessVal, (IndexPattern*) pattern));

                    jrow2CompressedLoc[j1].insert(e);
                }
            }

            indexedLoopResults.resize(hessLE);

            return indexedLoopResults;
        }

        /**
         * Auxiliary structure
         */
        struct Reverse2Jrow2Iter {
            size_t jrow;
            std::set<size_t> iterations;

            inline Reverse2Jrow2Iter() {
            }

            inline Reverse2Jrow2Iter(size_t row,
                                     const std::set<size_t>& iters) :
                jrow(row),
                iterations(iters) {
            }
        };

        inline bool operator<(const Reverse2Jrow2Iter& l, const Reverse2Jrow2Iter& r) {
            if (l.jrow < r.jrow)
                return true;
            else if (l.jrow > r.jrow)
                return false;

            return compare(l.iterations, r.iterations) == -1;
        }

        /**
         * Create groups with the same contributions at the same Hessian rows
         */
        template<class Base>
        inline std::map<HessianTermContrib<Base>, std::set<size_t> > groupHessianRowsByContrib(const loops::HessianWithLoopsInfo<Base>& info,
                                                                                               size_t n,
                                                                                               std::map<pairss, std::map<size_t, std::set<size_t> > >& indexedIndexed2jrow2Iter,
                                                                                               std::map<pairss, std::map<size_t, std::set<size_t> > >& indexedNonIndexed2jrow2Iter,
                                                                                               std::map<pairss, std::map<size_t, std::set<size_t> > >& indexedTemp2jrow2Iter,
                                                                                               std::map<pairss, std::map<size_t, std::set<size_t> > >& nonIndexedIndexed2jrow2Iter,
                                                                                               std::map<pairss, std::map<size_t, std::set<size_t> > >& tempIndexed2jrow2Iter) {
            using namespace std;

            size_t nIterations = info.model->getIterationCount();

            /**
             * determine the contributions to each Hessian row
             */
            std::vector<HessianTermContrib<Base> > jrows(n);

            for (size_t g = 0; g < info.equationGroups.size(); g++) {
                const HessianWithLoopsEquationGroupInfo<Base>& infog = info.equationGroups[g];

                // indexed-indexed
                map<pairss, std::vector<HessianElement> >::const_iterator it;
                for (it = infog.indexedIndexedPositions.begin(); it != infog.indexedIndexedPositions.end(); ++it) {
                    map<size_t, set<size_t> >& jrow2Iter = indexedIndexed2jrow2Iter[it->first];
                    const std::vector<HessianElement>& positions = it->second;
                    for (size_t iter = 0; iter < nIterations; iter++) {
                        if (positions[iter].count > 0) {
                            jrows[positions[iter].row].indexedIndexed.insert(it->first);
                            jrow2Iter[positions[iter].row].insert(iter);
                        }
                    }
                }

                // indexed - non-indexed
                for (it = infog.indexedNonIndexedPositions.begin(); it != infog.indexedNonIndexedPositions.end(); ++it) {
                    map<size_t, set<size_t> >& jrow2Iter = indexedNonIndexed2jrow2Iter[it->first];
                    const std::vector<HessianElement>& positions = it->second;
                    for (size_t iter = 0; iter < nIterations; iter++) {
                        if (positions[iter].count > 0) {
                            jrows[positions[iter].row].indexedNonIndexed.insert(it->first);
                            jrow2Iter[positions[iter].row].insert(iter);
                        }
                    }
                }

                //  indexed - temporary
                for (it = infog.indexedTempPositions.begin(); it != infog.indexedTempPositions.end(); ++it) {
                    map<size_t, set<size_t> >& jrow2Iter = indexedTemp2jrow2Iter[it->first];
                    const std::vector<HessianElement>& positions = it->second;
                    for (size_t iter = 0; iter < nIterations; iter++) {
                        if (positions[iter].count > 0) {
                            jrows[positions[iter].row].indexedTemp.insert(it->first);
                            jrow2Iter[positions[iter].row].insert(iter);
                        }
                    }
                }

                // non-indexed - indexed
                for (it = infog.nonIndexedIndexedPositions.begin(); it != infog.nonIndexedIndexedPositions.end(); ++it) {
                    map<size_t, set<size_t> >& jrow2Iter = nonIndexedIndexed2jrow2Iter[it->first];
                    const std::vector<HessianElement>& positions = it->second;
                    for (size_t iter = 0; iter < nIterations; iter++) {
                        if (positions[iter].count > 0) {
                            jrows[positions[iter].row].nonIndexedIndexed.insert(it->first);
                            jrow2Iter[positions[iter].row].insert(iter);
                        }
                    }
                }

                //  temporary - indexed
                for (it = infog.tempIndexedPositions.begin(); it != infog.tempIndexedPositions.end(); ++it) {
                    map<size_t, set<size_t> >& jrow2Iter = tempIndexed2jrow2Iter[it->first];
                    const std::vector<HessianElement>& positions = it->second;
                    for (size_t iter = 0; iter < nIterations; iter++) {
                        if (positions[iter].count > 0) {
                            jrows[positions[iter].row].tempIndexed.insert(it->first);
                            jrow2Iter[positions[iter].row].insert(iter);
                        }
                    }
                }
            }

            // non-indexed - non-indexed
            map<pairss, size_t>::const_iterator orig2PosIt;
            for (orig2PosIt = info.nonIndexedNonIndexedPosition.begin(); orig2PosIt != info.nonIndexedNonIndexedPosition.end(); ++orig2PosIt) {
                size_t j1 = orig2PosIt->first.first;
                jrows[j1].nonIndexedNonIndexed.insert(orig2PosIt->first);
            }

            /**
             * group rows with the same contribution terms
             */
            map<HessianTermContrib<Base>, set<size_t> > contrib2jrows;
            for (size_t j = 0; j < n; j++) {
                if (!jrows[j].empty())
                    contrib2jrows[jrows[j]].insert(j);
            }

            return contrib2jrows;
        }

        /**
         * Create subgroups from goups with the same contributions at the 
         * same Hessian rows. Each subgroup has a sub-set of the group's 
         * contributions which have the same relations between Hessian row index
         * and set of iteration indexes.
         */
        template<class Base>
        inline void subgroupHessianRowsByContrib(const HessianWithLoopsInfo<Base>& info,
                                                 const HessianTermContrib<Base>& c,
                                                 const std::set<size_t>& jrows,
                                                 const std::map<pairss, std::map<size_t, std::set<size_t> > >& indexedIndexed2jrow2Iter,
                                                 const std::map<pairss, std::map<size_t, std::set<size_t> > >& indexedNonIndexed2jrow2Iter,
                                                 const std::map<pairss, std::map<size_t, std::set<size_t> > >& indexedTemp2jrow2Iter,
                                                 const std::map<pairss, std::map<size_t, std::set<size_t> > >& nonIndexedIndexed2jrow2Iter,
                                                 const std::map<pairss, std::map<size_t, std::set<size_t> > >& tempIndexed2jrow2Iter,
                                                 SmartVectorPointer<HessianRowGroup<Base> >& subGroups) {
            using namespace std;

            map<Reverse2Jrow2Iter, HessianTermContrib<Base> > contribs;

            set<pairss>::const_iterator it;
            map<size_t, set<size_t> >::const_iterator itJrow2Iter;

            //  indexed - indexed
            for (it = c.indexedIndexed.begin(); it != c.indexedIndexed.end(); ++it) {
                pairss pos = *it;

                map<size_t, set<size_t> > jrow2Iter = filterBykeys(indexedIndexed2jrow2Iter.at(pos), jrows);
                for (itJrow2Iter = jrow2Iter.begin(); itJrow2Iter != jrow2Iter.end(); ++itJrow2Iter) {
                    Reverse2Jrow2Iter k(itJrow2Iter->first, itJrow2Iter->second);
                    contribs[k].indexedIndexed.insert(pos);
                }
            }

            // indexed - non-indexed
            for (it = c.indexedNonIndexed.begin(); it != c.indexedNonIndexed.end(); ++it) {
                pairss pos = *it;

                map<size_t, set<size_t> > jrow2Iter = filterBykeys(indexedNonIndexed2jrow2Iter.at(pos), jrows);
                for (itJrow2Iter = jrow2Iter.begin(); itJrow2Iter != jrow2Iter.end(); ++itJrow2Iter) {
                    Reverse2Jrow2Iter k(itJrow2Iter->first, itJrow2Iter->second);
                    contribs[k].indexedNonIndexed.insert(pos);
                }
            }

            //  indexed - temporary
            for (it = c.indexedTemp.begin(); it != c.indexedTemp.end(); ++it) {
                pairss pos = *it;

                map<size_t, set<size_t> > jrow2Iter = filterBykeys(indexedTemp2jrow2Iter.at(pos), jrows);
                for (itJrow2Iter = jrow2Iter.begin(); itJrow2Iter != jrow2Iter.end(); ++itJrow2Iter) {
                    Reverse2Jrow2Iter k(itJrow2Iter->first, itJrow2Iter->second);
                    contribs[k].indexedTemp.insert(pos);
                }
            }

            // non-indexed - indexed
            for (it = c.nonIndexedIndexed.begin(); it != c.nonIndexedIndexed.end(); ++it) {
                pairss pos = *it;

                map<size_t, set<size_t> > jrow2Iter = filterBykeys(nonIndexedIndexed2jrow2Iter.at(pos), jrows);
                for (itJrow2Iter = jrow2Iter.begin(); itJrow2Iter != jrow2Iter.end(); ++itJrow2Iter) {
                    Reverse2Jrow2Iter k(itJrow2Iter->first, itJrow2Iter->second);
                    contribs[k].nonIndexedIndexed.insert(pos);
                }
            }

            // non-indexed - non-indexed
            if (!c.nonIndexedNonIndexed.empty()) {
                set<size_t> allIters;
                size_t nIterations = info.model->getIterationCount();
                for (size_t iter = 0; iter < nIterations; iter++)
                    allIters.insert(allIters.end(), iter);

                for (it = c.nonIndexedNonIndexed.begin(); it != c.nonIndexedNonIndexed.end(); ++it) {
                    pairss pos = *it;
                    Reverse2Jrow2Iter k(pos.first, allIters);
                    contribs[k].nonIndexedNonIndexed.insert(pos);
                }
            }

            //  temporary - indexed
            for (it = c.tempIndexed.begin(); it != c.tempIndexed.end(); ++it) {
                pairss pos = *it;

                map<size_t, set<size_t> > jrow2Iter = filterBykeys(tempIndexed2jrow2Iter.at(pos), jrows);
                for (itJrow2Iter = jrow2Iter.begin(); itJrow2Iter != jrow2Iter.end(); ++itJrow2Iter) {
                    Reverse2Jrow2Iter k(itJrow2Iter->first, itJrow2Iter->second);
                    contribs[k].tempIndexed.insert(pos);
                }
            }

            /**
             * 
             */
            map<HessianTermContrib<Base>, HessianRowGroup<Base>*> c2subgroups; // add pair<jrow, set<iter> > here

            typename map<Reverse2Jrow2Iter, HessianTermContrib<Base> >::const_iterator itK2C;
            for (itK2C = contribs.begin(); itK2C != contribs.end(); ++itK2C) {
                const Reverse2Jrow2Iter& jrow2Iters = itK2C->first;
                const HessianTermContrib<Base>& hc = itK2C->second;

                typename map<HessianTermContrib<Base>, HessianRowGroup<Base>*>::const_iterator its = c2subgroups.find(hc);
                if (its != c2subgroups.end()) {
                    HessianRowGroup<Base>* sg = its->second;
                    sg->jRow2Iterations[jrow2Iters.jrow] = jrow2Iters.iterations;
                    sg->iterations.insert(jrow2Iters.iterations.begin(), jrow2Iters.iterations.end());
                } else {
                    HessianRowGroup<Base>* sg = new HessianRowGroup<Base>(hc, jrow2Iters);
                    subGroups.v.push_back(sg);
                    c2subgroups[hc] = sg;
                }
            }
        }

        template<class Base>
        std::pair<CG<Base>, IndexPattern*> createReverseMode2Contribution(CodeHandler<Base>& handler,
                                                                          HessianRowGroup<Base>& group,
                                                                          const std::vector<HessianElement>& positions,
                                                                          const CG<Base>& ddfdxdx,
                                                                          const CG<Base>& tx1,
                                                                          IndexOperationNode<Base>& iterationIndexOp,
                                                                          std::map<size_t, std::set<size_t> >& jrow2CompressedLoc) {
            using namespace std;

            if (ddfdxdx.isParameter() && ddfdxdx.IdenticalZero()) {
                return make_pair(ddfdxdx, (IndexPattern*) NULL);
            }

            /**
             * Determine index pattern
             */
            std::map<size_t, size_t> iteration2pos;

            // combine iterations with the same number of additions
            map<size_t, map<size_t, size_t> > locations;
            set<size_t>::const_iterator itIt;
            for (itIt = group.iterations.begin(); itIt != group.iterations.end(); ++itIt) {
                size_t iter = *itIt;
                size_t c = positions[iter].count;
                if (c > 0) {
                    locations[c][iter] = positions[iter].location;
                    iteration2pos[iter] = positions[iter].location;
                    jrow2CompressedLoc[positions[iter].row].insert(positions[iter].location);
                }
            }

            if (locations.empty()) {
                return make_pair(CG<Base>(Base(0)), (IndexPattern*) NULL);
            }

            map<size_t, CG<Base> > results;

            // generate the index pattern for the Hessian compressed element
            map<size_t, map<size_t, size_t> >::const_iterator countIt;
            for (countIt = locations.begin(); countIt != locations.end(); ++countIt) {
                size_t count = countIt->first;

                CG<Base> val = ddfdxdx;
                for (size_t c = 1; c < count; c++)
                    val += ddfdxdx;

                results[count] = val * tx1;
            }

            if (results.size() == 1 && locations.begin()->second.size() == group.iterations.size()) {
                // same expression present in all iterations

                // generate the index pattern for the Hessian compressed element
                IndexPattern* pattern = IndexPattern::detect(locations.begin()->second);
                handler.manageLoopDependentIndexPattern(pattern);

                return make_pair(results.begin()->second, pattern);

            } else {
                /**
                 * must create a conditional element so that this 
                 * contribution to the Hessian is only evaluated at the
                 * relevant iterations
                 */
                map<size_t, IfBranchData<Base> > branches;

                // try to find an existing if-else where these operations can be added
                map<size_t, map<size_t, size_t> >::const_iterator countIt;
                for (countIt = locations.begin(); countIt != locations.end(); ++countIt) {
                    size_t count = countIt->first;
                    IfBranchData<Base> branch(results[count], countIt->second);
                    branches[count] = branch;
                }

                CG<Base> v = createConditionalContribution(handler,
                                                           branches,
                                                           positions.size() - 1,
                                                           group.iterations.size(),
                                                           iterationIndexOp,
                                                           group.ifElses);

                return make_pair(v, (IndexPattern*) NULL);
            }

        }

        /**
         * Hessian contribution to a constant location
         */
        template<class Base>
        CG<Base> createReverseMode2Contribution(CodeHandler<Base>& handler,
                                                HessianRowGroup<Base>& group,
                                                LinearIndexPattern& pattern,
                                                const std::set<size_t>& iterations,
                                                const CG<Base>& ddfdxdx,
                                                IndexOperationNode<Base>& iterationIndexOp,
                                                vector<IfElseInfo<Base> >& ifElses) {
            using namespace std;

            if (ddfdxdx.isParameter() && ddfdxdx.IdenticalZero()) {
                return ddfdxdx;
            }

            CPPADCG_ASSERT_UNKNOWN(pattern.getLinearSlopeDy() == 0); // must be a constant index

            if (iterations.size() == group.iterations.size()) {
                // same expression present in all iterations
                return ddfdxdx;

            } else {
                /**
                 * must create a conditional element so that this 
                 * contribution to the Hessian is only evaluated at the
                 * relevant iterations
                 */
                return createConditionalContribution(handler, pattern,
                                                     iterations, *group.iterations.rbegin(),
                                                     ddfdxdx, iterationIndexOp,
                                                     ifElses);
            }
        }

        /**
         * Group of contributions to an Hessian
         */
        template<class Base>
        class HessianTermContrib {
        public:
            // (tapeJ1, tapeJ2)
            std::set<pairss> indexedIndexed;
            // (tapeJ1, tapeJ2(j2))
            std::set<pairss> indexedNonIndexed;
            // (tapeJ1, j2)
            std::set<pairss> indexedTemp;
            // (tapeJ1(j1), tapeJ2)
            std::set<pairss> nonIndexedIndexed;
            //(j1, j2) 
            std::set<pairss> nonIndexedNonIndexed;
            // (j1, tapeJ2)
            std::set<pairss> tempIndexed;

        public:

            inline bool empty() const {
                return indexedIndexed.empty() && indexedNonIndexed.empty() && indexedTemp.empty() &&
                        nonIndexedIndexed.empty() && nonIndexedNonIndexed.empty() &&
                        tempIndexed.empty();
            }

            inline size_t size() const {
                return indexedIndexed.size() + indexedNonIndexed.size() + indexedTemp.size() +
                        nonIndexedIndexed.size() + nonIndexedNonIndexed.size() +
                        tempIndexed.size();
            }
        };

        template<class Base>
        bool operator<(const HessianTermContrib<Base>& l, const HessianTermContrib<Base>& r) {
            int c = compare(l.indexedIndexed, r.indexedIndexed);
            if (c != 0) return c == -1;
            c = compare(l.indexedNonIndexed, r.indexedNonIndexed);
            if (c != 0) return c == -1;
            c = compare(l.indexedTemp, r.indexedTemp);
            if (c != 0) return c == -1;
            c = compare(l.nonIndexedIndexed, r.nonIndexedIndexed);
            if (c != 0) return c == -1;
            c = compare(l.nonIndexedNonIndexed, r.nonIndexedNonIndexed);
            if (c != 0) return c == -1;
            c = compare(l.tempIndexed, r.tempIndexed);
            if (c != 0) return c == -1;
            return false;
        }

        /**
         * Group of contributions to an Hessian with the same relation between
         * Hessian rows and set of iterations
         */
        template<class Base>
        class HessianRowGroup : public HessianTermContrib<Base> {
        public:
            // all the required iterations for each jrow
            std::map<size_t, std::set<size_t> > jRow2Iterations;
            // all iterations
            std::set<size_t> iterations;
            // if-else branches
            vector<IfElseInfo<Base> > ifElses;
        public:

            inline HessianRowGroup(const HessianTermContrib<Base>& c,
                                   const Reverse2Jrow2Iter& jrow2Iters) :
                HessianTermContrib<Base>(c),
                iterations(jrow2Iters.iterations) {
                jRow2Iterations[jrow2Iters.jrow] = jrow2Iters.iterations;
            }
        };

    } // end loops

    template<class Base>
    void CLangCompileModelHelper<Base>::generateFunctionNameLoopRev2(std::ostringstream& cache,
                                                                     const LoopModel<Base>& loop,
                                                                     size_t g) {
        generateFunctionNameLoopRev2(cache, _name, loop, g);
    }

    template<class Base>
    void CLangCompileModelHelper<Base>::generateFunctionNameLoopRev2(std::ostringstream& cache,
                                                                     const std::string& modelName,
                                                                     const LoopModel<Base>& loop,
                                                                     size_t g) {
        cache << modelName << "_" << FUNCTION_SPARSE_REVERSE_TWO <<
                "_loop" << loop.getLoopId() << "_g" << g;
    }

}

#endif
