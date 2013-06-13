#ifndef CPPAD_CG_CODE_HANDLER_INCLUDED
#define CPPAD_CG_CODE_HANDLER_INCLUDED
/* --------------------------------------------------------------------------
 *  CppADCodeGen: C++ Algorithmic Differentiation with Source Code Generation:
 *    Copyright (C) 2012 Ciengis
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

    template<class Base>
    class CG;

    /**
     * Helper class to analyze the operation graph and generate source code
     * for several languages
     * 
     * @author Joao Leal
     */
    template<class Base>
    class CodeHandler {
    public:
        typedef std::vector<SourceCodePathNode<Base> > SourceCodePath;
    protected:
        // counter used to generate variable IDs
        size_t _idCount;
        // counter used to generate array variable IDs
        size_t _idArrayCount;
        // counter used to generate IDs for atomic functions
        size_t _idAtomicCount;
        // the independent variables
        std::vector<SourceCodeFragment<Base> *> _independentVariables;
        // all the source code blocks created with the CG<Base> objects (does not include independent variables)
        std::vector<SourceCodeFragment<Base> *> _codeBlocks;
        // the order for the variable creation in the source code
        std::vector<SourceCodeFragment<Base> *> _variableOrder;
        // maps the ids of the atomic functions to their names (used by this handler only)
        std::map<size_t, std::string> _atomicFunctions;
        /**
         * already used atomic function names (may contain names which were 
         * used by previous calls to this/other CondeHandlers)
         */
        std::set<std::string> _atomicFunctionsSet;
        /**
         * the order of the atomic functions(may contain names which were 
         * used by previous calls to this/other CondeHandlers)
         */
        std::vector<std::string>* _atomicFunctionsOrder;
        // a flag indicating if this handler was previously used to generate code
        bool _used;
        // a flag indicating whether or not to reuse the IDs of destroyed variables
        bool _reuseIDs;
        // the language used for source code generation
        Language<Base>* _lang;
        // the lowest ID used for temporary variables
        size_t _minTemporaryVarID;
        //
        bool _verbose;
    public:

        CodeHandler(size_t varCount = 50) :
            _idCount(1),
            _idArrayCount(1),
            _idAtomicCount(1),
            _atomicFunctionsOrder(NULL),
            _used(false),
            _reuseIDs(true),
            _lang(NULL),
            _minTemporaryVarID(0),
            _verbose(false) {
            _codeBlocks.reserve(varCount);
            _variableOrder.reserve(1 + varCount / 3);
        }

        inline void setReuseVariableIDs(bool reuse) {
            _reuseIDs = reuse;
        }

        inline bool isReuseVariableIDs() const {
            return _reuseIDs;
        }

        inline void makeVariables(std::vector<CG<Base> >& variables) {
            for (typename std::vector<CG<Base> >::iterator it = variables.begin(); it != variables.end(); ++it) {
                makeVariable(*it);
            }
        }

        inline void makeVariables(std::vector<AD<CG<Base> > >& variables) {
            for (typename std::vector<AD<CG<Base> > >::iterator it = variables.begin(); it != variables.end(); ++it) {
                CG<Base> v;
                makeVariable(v); // make it a codegen variable
                *it = v; // variable[i] id now the same as v
            }
        }

        inline void makeVariable(CG<Base>& variable) {
            _independentVariables.push_back(new SourceCodeFragment<Base > (CGInvOp));
            variable.makeVariable(*this, _independentVariables.back());
        }

        size_t getIndependentVariableSize() const {
            return _independentVariables.size();
        }

        size_t getIndependentVariableIndex(const SourceCodeFragment<Base>& var) const throw (CGException) {
            assert(var.operation_ == CGInvOp);

            typename std::vector<SourceCodeFragment<Base> *>::const_iterator it =
                    std::find(_independentVariables.begin(), _independentVariables.end(), &var);
            if (it == _independentVariables.end()) {
                throw CGException("Variable not found in the independent variable vector");
            }

            return it - _independentVariables.begin();
        }

        inline size_t getMaximumVariableID() const {
            return _idCount;
        }

        inline bool isVerbose() const {
            return _verbose;
        }

        inline void setVerbose(bool verbose) {
            _verbose = verbose;
        }

        /***********************************************************************
         *                   Graph management functions
         **********************************************************************/
        /**
         * Finds occurences of a source code fragment in an operation graph.
         * 
         * @param root the operation graph where to search
         * @param code the source code fragment to find in root
         * @param max the maximum number of occurences of code to find in root
         * @return the paths from root to code
         */
        inline std::vector<SourceCodePath> findPaths(SourceCodeFragment<Base>& root,
                                                     SourceCodeFragment<Base>& code,
                                                     size_t max);

        inline bool isSolvable(const SourceCodePath& path) throw (CGException);

        /***********************************************************************
         *                   Source code generation
         **********************************************************************/

        /**
         * Creates the source code from the operations registered so far.
         * 
         * @param out The output stream where the source code is to be printed.
         * @param lang The targeted language.
         * @param dependent The dependent variables for which the source code
         *                  should be generated. By defining this vector the 
         *                  number of operations in the source code can be 
         *                  reduced and thus providing a more optimized code.
         * @param nameGen Provides the rules for variable name creation.
         */
        virtual void generateCode(std::ostream& out,
                                  CppAD::Language<Base>& lang,
                                  std::vector<CG<Base> >& dependent,
                                  VariableNameGenerator<Base>& nameGen,
                                  const std::string& jobName = "source") {
            std::vector<std::string> atomicFunctions;
            generateCode(out, lang, dependent, nameGen, atomicFunctions, jobName);
        }

        /**
         * Creates the source code from the operations registered so far.
         * 
         * @param out The output stream where the source code is to be printed.
         * @param lang The targeted language.
         * @param dependent The dependent variables for which the source code
         *                  should be generated. By defining this vector the 
         *                  number of operations in the source code can be 
         *                  reduced and thus providing a more optimized code.
         * @param nameGen Provides the rules for variable name creation.
         * @param atomicFunctions The order of the atomic functions.
         */
        virtual void generateCode(std::ostream& out,
                                  CppAD::Language<Base>& lang,
                                  std::vector<CG<Base> >& dependent,
                                  VariableNameGenerator<Base>& nameGen,
                                  std::vector<std::string>& atomicFunctions,
                                  const std::string& jobName = "source") {
            double beginTime;
            if (_verbose) {
                std::cout << "generating source for '" << jobName << "' ... ";
                std::cout.flush();
                beginTime = system::currentTime();
            }

            _lang = &lang;
            _idCount = 1;
            _idArrayCount = 1;
            _idAtomicCount = 1;
            _atomicFunctionsOrder = &atomicFunctions;
            _atomicFunctionsSet.clear();
            for (size_t i = 0; i < atomicFunctions.size(); i++) {
                _atomicFunctionsSet.insert(atomicFunctions[i]);
            }

            if (_used) {
                resetCounters();
            }
            _used = true;

            /**
             * the first variable IDs are for the independent variables
             */
            for (typename std::vector<SourceCodeFragment<Base> *>::iterator it = _independentVariables.begin(); it != _independentVariables.end(); ++it) {
                (*it)->setVariableID(_idCount++);
            }

            for (typename std::vector<CG<Base> >::iterator it = dependent.begin(); it != dependent.end(); ++it) {
                if (it->getSourceCodeFragment() != NULL && it->getSourceCodeFragment()->variableID() == 0) {
                    it->getSourceCodeFragment()->setVariableID(_idCount++);
                }
            }

            _minTemporaryVarID = _idCount;

            // determine the number of times each variable is used
            for (typename std::vector<CG<Base> >::iterator it = dependent.begin(); it != dependent.end(); ++it) {
                CG<Base>& var = *it;
                if (var.getSourceCodeFragment() != NULL) {
                    SourceCodeFragment<Base>& code = *var.getSourceCodeFragment();
                    markCodeBlockUsed(code);
                }
            }

            // determine the variable creation order
            for (typename std::vector<CG<Base> >::iterator it = dependent.begin(); it != dependent.end(); ++it) {
                CG<Base>& var = *it;
                if (var.getSourceCodeFragment() != NULL) {
                    SourceCodeFragment<Base>& code = *var.getSourceCodeFragment();
                    if (code.usageCount() == 0) {
                        // dependencies not visited yet
                        checkVariableCreation(code);

                        // make sure new temporary variables are NOT created for
                        // the independent variables and that a dependency did
                        // not use it first
                        if ((code.variableID() == 0 || !isIndependent(code)) && code.usageCount() == 0) {
                            addToEvaluationQueue(code);
                        }
                    }
                    code.increaseUsageCount();
                }
            }

            //assert(_idCount - 1 + _idArrayCount == _variableOrder.size() + _independentVariables.size());

            if (_reuseIDs) {
                reduceTemporaryVariables(dependent);
            }

            nameGen.setTemporaryVariableID(_minTemporaryVarID, _idCount - 1, _idArrayCount - 1);

            std::map<std::string, size_t> atomicFunctionName2Id;
            std::map<size_t, std::string>::const_iterator itA;
            for (itA = _atomicFunctions.begin(); itA != _atomicFunctions.end(); ++itA) {
                atomicFunctionName2Id[itA->second] = itA->first;
            }

            std::map<size_t, size_t> atomicFunctionId2Index;
            for (size_t i = 0; i < _atomicFunctionsOrder->size(); i++) {
                const std::string& atomicName = (*_atomicFunctionsOrder)[i];
                std::map<std::string, size_t>::const_iterator it = atomicFunctionName2Id.find(atomicName);
                if (it != atomicFunctionName2Id.end()) {
                    atomicFunctionId2Index[it->second] = i;
                }
            }

            /**
             * Creates the source code for a specific language
             */
            LanguageGenerationData<Base> info(_independentVariables, dependent,
                                              _minTemporaryVarID, _variableOrder,
                                              nameGen, atomicFunctionId2Index,
                                              _reuseIDs);
            lang.generateSourceCode(out, info);

            _atomicFunctionsSet.clear();

            if (_verbose) {
                double endTime = system::currentTime();
                std::cout << "done [" << std::fixed << std::setprecision(3)
                        << (endTime - beginTime) << "]" << std::endl;
            }
        }

        size_t getTemporaryVariableCount() const {
            if (_idCount == 1)
                return 0; // no code generated
            else
                return _idCount - _minTemporaryVarID;
        }

        size_t getTemporaryArraySize() const {
            return _idArrayCount - 1;
        }

        virtual void reset() {
            typename std::vector<SourceCodeFragment<Base> *>::iterator itc;
            for (itc = _codeBlocks.begin(); itc != _codeBlocks.end(); ++itc) {
                delete *itc;
            }
            _codeBlocks.clear();
            _independentVariables.clear();
            _idCount = 1;
            _idArrayCount = 1;
            _idAtomicCount = 1;
            _used = false;
        }

        /***********************************************************************
         *                   Operation graph manipulation
         **********************************************************************/

        /**
         * Solves an expression (e.g. f(x, y) == 0) for a given variable (e.g. x)
         * The variable can appear only once in the expression.
         * 
         * @param expression  The original expression (f(x, y))
         * @param code  The variable to solve for
         * @return  The expression for variable
         */
        inline CG<Base> solveFor(SourceCodeFragment<Base>& expression,
                                 SourceCodeFragment<Base>& code) throw (CGException);

        inline CG<Base> solveFor(const std::vector<SourceCodePathNode<Base> >& path) throw (CGException);

        /**
         * Eliminates an independent variable by substitution using the provided
         * dependent variable which is assumed to be a residual of an equation.
         * If successful the model will contain one less independent variable.
         * 
         * @param indep The independent variable to eliminate.
         * @param dep The dependent variable representing a residual
         * @param removeFromIndeps Whether or not to immediatelly remove the
         *                         independent variable from the list of
         *                         independents in the model. The subtitution
         *                         operation can only be reversed if the 
         *                         variable is not removed.
         */
        inline void substituteIndependent(const CG<Base>& indep,
                                          const CG<Base>& dep,
                                          bool removeFromIndeps = true) throw (CGException);

        inline void substituteIndependent(SourceCodeFragment<Base>& indep,
                                          SourceCodeFragment<Base>& dep,
                                          bool removeFromIndeps = true) throw (CGException);

        /**
         * Reverts a subtitution of an independent variable that has not been 
         * removed from the list of independents yet.
         * Warning: it does not recover any custom name assigned to the variable.
         * 
         * @param indep The independent variable
         */
        inline void undoSubstituteIndependent(SourceCodeFragment<Base>& indep) throw (CGException);

        /**
         * Finallizes the subtitution of an independent variable by eliminating
         * it from the list of independents. After this operation the variable
         * subtitution cannot be undone.
         * 
         * @param indep The independent variable
         */
        inline void removeIndependent(SourceCodeFragment<Base>& indep) throw (CGException);

        inline virtual ~CodeHandler() {
            reset();
        }

    protected:

        virtual void manageSourceCodeBlock(SourceCodeFragment<Base>* code) {
            //assert(std::find(_codeBlocks.begin(), _codeBlocks.end(), code) == _codeBlocks.end()); // <<< too great of an impact in performance
            if (_codeBlocks.capacity() == _codeBlocks.size()) {
                _codeBlocks.reserve((_codeBlocks.size()*3) / 2 + 1);
            }

            _codeBlocks.push_back(code);
        }

        virtual void markCodeBlockUsed(SourceCodeFragment<Base>& code) {
            code.total_use_count_++;

            if (code.total_use_count_ == 1) {
                // first time this operation is visited

                const std::vector<Argument<Base> >& args = code.arguments_;

                typename std::vector<Argument<Base> >::const_iterator it;
                for (it = args.begin(); it != args.end(); ++it) {
                    if (it->operation() != NULL) {
                        SourceCodeFragment<Base>& arg = *it->operation();
                        markCodeBlockUsed(arg);
                    }
                }
            }
        }

        virtual void registerAtomicFunction(size_t id, const std::string& name) {
            _atomicFunctions[id] = name;
        }

        virtual void checkVariableCreation(SourceCodeFragment<Base>& code) {
            const std::vector<Argument<Base> >& args = code.arguments_;

            typename std::vector<Argument<Base> >::const_iterator it;

            for (it = args.begin(); it != args.end(); ++it) {
                if (it->operation() != NULL) {
                    SourceCodeFragment<Base>& arg = *it->operation();

                    if (arg.usageCount() == 0) {
                        // dependencies not visited yet
                        checkVariableCreation(arg);

                        /**
                         * Save atomic function related information
                         */
                        if (arg.operation() == CGAtomicForwardOp || arg.operation() == CGAtomicReverseOp) {
                            assert(arg.arguments().size() > 1);
                            assert(arg.info().size() > 1);
                            size_t id = arg.info()[0];
                            const std::string& atomicName = _atomicFunctions.at(id);
                            if (_atomicFunctionsSet.find(atomicName) == _atomicFunctionsSet.end()) {
                                _atomicFunctionsSet.insert(atomicName);
                                _atomicFunctionsOrder->push_back(atomicName);
                            }
                        }

                    }
                }
            }

            for (it = args.begin(); it != args.end(); ++it) {
                if (it->operation() != NULL) {
                    SourceCodeFragment<Base>& arg = *it->operation();
                    // make sure new temporary variables are NOT created for
                    // the independent variables and that a dependency did
                    // not use it first
                    if ((arg.variableID() == 0 || !isIndependent(arg)) && arg.usageCount() == 0) {

                        size_t argIndex = it - args.begin();
                        if (_lang->createsNewVariable(arg) ||
                                _lang->requiresVariableArgument(code.operation(), argIndex)) {
                            addToEvaluationQueue(arg);
                            if (arg.variableID() == 0) {
                                if (arg.operation() == CGAtomicForwardOp || arg.operation() == CGAtomicReverseOp) {
                                    arg.setVariableID(_idAtomicCount);
                                    _idAtomicCount++;
                                } else if (arg.operation() != CGArrayCreationOp) {
                                    // a single temporary variable
                                    arg.setVariableID(_idCount);
                                    _idCount++;
                                } else {
                                    // a temporary array
                                    size_t arraySize = arg.arguments().size();
                                    arg.setVariableID(_idArrayCount);
                                    _idArrayCount += arraySize;
                                }
                            }
                        }
                    }

                    arg.increaseUsageCount();
                }
            }

        }

        inline void addToEvaluationQueue(SourceCodeFragment<Base>& arg) {
            if (_variableOrder.size() == _variableOrder.capacity()) {
                _variableOrder.reserve((_variableOrder.size()*3) / 2 + 1);
            }

            _variableOrder.push_back(&arg);
            arg.setEvaluationOrder(_variableOrder.size());

            dependentAdded2EvaluationQueue(arg);
        }

        inline void reduceTemporaryVariables(std::vector<CG<Base> >& dependent) {

            /**
             * determine the last line where each temporary variable is used
             */
            resetUsageCount();

            for (typename std::vector<CG<Base> >::iterator it = dependent.begin(); it != dependent.end(); ++it) {
                CG<Base>& var = *it;
                if (var.getSourceCodeFragment() != NULL) {
                    SourceCodeFragment<Base>& code = *var.getSourceCodeFragment();
                    if (code.use_count_ == 0) {
                        // dependencies not visited yet
                        determineLastTempVarUsage(code);
                    }
                    code.increaseUsageCount();
                }
            }

            // where temporary variables can be released
            std::vector<std::vector<SourceCodeFragment<Base>* > > tempVarRelease(_variableOrder.size());
            for (size_t i = 0; i < _variableOrder.size(); i++) {
                SourceCodeFragment<Base>* var = _variableOrder[i];
                if (isTemporary(*var) || isTemporaryArray(*var)) {
                    size_t releaseLocation = var->getLastUsageEvaluationOrder() - 1;
                    tempVarRelease[releaseLocation].push_back(var);
                }
            }


            /**
             * Redefine temporary variable IDs
             */
            std::vector<size_t> freedVariables; // variable IDs no longer in use
            _idCount = _minTemporaryVarID;
            _idArrayCount = 1;
            std::map<size_t, size_t> freeArrayStartSpace; // [start] = end
            std::map<size_t, size_t> freeArrayEndSpace; // [end] = start

            for (size_t i = 0; i < _variableOrder.size(); i++) {
                SourceCodeFragment<Base>& var = *_variableOrder[i];

                const std::vector<SourceCodeFragment<Base>* >& released = tempVarRelease[i];
                for (size_t r = 0; r < released.size(); r++) {
                    if (isTemporary(*released[r])) {
                        freedVariables.push_back(released[r]->variableID());
                    } else if (isTemporaryArray(*released[r])) {
                        addFreeArraySpace(*released[r], freeArrayStartSpace, freeArrayEndSpace);
                        assert(freeArrayStartSpace.size() == freeArrayEndSpace.size());
                    }
                }

                if (isTemporary(var)) {
                    // a single temporary variable
                    if (freedVariables.empty()) {
                        var.setVariableID(_idCount);
                        _idCount++;
                    } else {
                        size_t id = freedVariables.back();
                        freedVariables.pop_back();
                        var.setVariableID(id);
                    }
                } else if (isTemporaryArray(var)) {
                    // a temporary array
                    size_t arrayStart = reserveArraySpace(var, freeArrayStartSpace, freeArrayEndSpace);
                    assert(freeArrayStartSpace.size() == freeArrayEndSpace.size());
                    var.setVariableID(arrayStart + 1);
                }

            }
        }

        inline static void addFreeArraySpace(const SourceCodeFragment<Base>& released,
                                             std::map<size_t, size_t>& freeArrayStartSpace,
                                             std::map<size_t, size_t>& freeArrayEndSpace) {
            size_t arrayStart = released.variableID() - 1;
            const size_t arraySize = released.arguments().size();
            size_t arrayEnd = arrayStart + arraySize - 1;

            std::map<size_t, size_t>::iterator it;
            if (arrayStart > 0) {
                it = freeArrayEndSpace.find(arrayStart - 1); // previous
                if (it != freeArrayEndSpace.end()) {
                    arrayStart = it->second; // merge space
                    freeArrayEndSpace.erase(it);
                    freeArrayStartSpace.erase(arrayStart);
                }
            }
            it = freeArrayStartSpace.find(arrayEnd + 1); // next
            if (it != freeArrayStartSpace.end()) {
                arrayEnd = it->second; // merge space 
                freeArrayStartSpace.erase(it);
                freeArrayEndSpace.erase(arrayEnd);
            }

            freeArrayStartSpace[arrayStart] = arrayEnd;
            freeArrayEndSpace[arrayEnd] = arrayStart;
        }

        inline size_t reserveArraySpace(const SourceCodeFragment<Base>& newArray,
                                        std::map<size_t, size_t>& freeArrayStartSpace,
                                        std::map<size_t, size_t>& freeArrayEndSpace) {
            size_t arraySize = newArray.arguments().size();

            std::set<size_t> blackList;
            const std::vector<Argument<Base> >& args = newArray.arguments();
            for (size_t i = 0; i < args.size(); i++) {
                const SourceCodeFragment<Base>* argOp = args[i].operation();
                if (argOp != NULL && argOp->operation() == CGArrayElementOp) {
                    const SourceCodeFragment<Base>& otherArray = *argOp->arguments()[0].operation();
                    assert(otherArray.variableID() > 0); // make sure it had already been assigned space
                    size_t otherArrayStart = otherArray.variableID() - 1;
                    size_t index = argOp->info()[0];
                    blackList.insert(otherArrayStart + index);
                }
            }

            /**
             * Find the best location for the new array
             */
            std::map<size_t, size_t>::reverse_iterator it;
            std::map<size_t, size_t>::reverse_iterator itBestFit = freeArrayStartSpace.rend();
            for (it = freeArrayStartSpace.rbegin(); it != freeArrayStartSpace.rend(); ++it) {
                size_t start = it->first;
                size_t end = it->second;
                size_t space = end - start + 1;
                if (space < arraySize) {
                    continue;
                }

                std::set<size_t>::const_iterator itBlack = blackList.lower_bound(start);
                if (itBlack != blackList.end() && *itBlack <= end) {
                    continue; // cannot use this space
                }

                if (space == arraySize) {
                    // jackpot
                    itBestFit = it;
                    break;
                } else {
                    //possible candidate
                    if (itBestFit == freeArrayStartSpace.rend()) {
                        itBestFit = it;
                    } else {
                        size_t bestSpace = itBestFit->second - itBestFit->first + 1;
                        if (space < bestSpace) {
                            // better fit
                            itBestFit = it;
                        }
                    }
                }
            }

            if (itBestFit != freeArrayStartSpace.rend()) {
                /**
                 * Use available space
                 */
                size_t bestStart = itBestFit->first;
                size_t bestEnd = itBestFit->second;
                size_t bestSpace = bestEnd - bestStart + 1;
                freeArrayStartSpace.erase(bestStart);
                if (bestSpace == arraySize) {
                    // entire space 
                    freeArrayEndSpace.erase(bestEnd);
                } else {
                    // some space left
                    size_t newFreeStart = bestStart + arraySize;
                    freeArrayStartSpace[newFreeStart] = bestEnd;
                    freeArrayEndSpace.at(bestEnd) = newFreeStart;
                }
                return bestStart;
            } else {
                /**
                 * no space available, need more
                 */
                // check if there is some free space at the end
                std::map<size_t, size_t>::iterator itEnd;
                itEnd = freeArrayEndSpace.find(_idArrayCount - 1);
                if (itEnd != freeArrayEndSpace.end()) {
                    // check if it can be used
                    size_t lastSpotStart = itEnd->second;
                    size_t lastSpotEnd = itEnd->first;
                    size_t lastSpotSize = lastSpotEnd - lastSpotStart + 1;
                    std::set<size_t>::const_iterator itBlack = blackList.lower_bound(lastSpotStart);
                    if (itBlack == blackList.end()) {
                        // can use this space
                        size_t newEnd = lastSpotStart + arraySize - 1;

                        freeArrayEndSpace.erase(itEnd);
                        freeArrayEndSpace[newEnd] = lastSpotStart;
                        freeArrayStartSpace[lastSpotStart] = newEnd;

                        _idArrayCount += arraySize - lastSpotSize;
                        return lastSpotStart;
                    }
                }

                // brand new space
                size_t id = _idArrayCount;
                _idArrayCount += arraySize;
                return id - 1;
            }
        }

        inline void determineLastTempVarUsage(SourceCodeFragment<Base>& code) {
            const std::vector<Argument<Base> >& args = code.arguments_;

            typename std::vector<Argument<Base> >::const_iterator it;

            /**
             * count variable usage
             */
            for (it = args.begin(); it != args.end(); ++it) {
                if (it->operation() != NULL) {
                    SourceCodeFragment<Base>& arg = *it->operation();

                    if (arg.use_count_ == 0) {
                        // dependencies not visited yet
                        determineLastTempVarUsage(arg);
                    }

                    arg.increaseUsageCount();

                    if (arg.getLastUsageEvaluationOrder() < code.getEvaluationOrder()) {
                        arg.setLastUsageEvaluationOrder(code.getEvaluationOrder());
                    }
                }
            }
        }

        inline void resetUsageCount() {
            typename std::vector<SourceCodeFragment<Base> *>::const_iterator it;
            for (it = _codeBlocks.begin(); it != _codeBlocks.end(); ++it) {
                SourceCodeFragment<Base>* block = *it;
                block->use_count_ = 0;
            }
        }

        /**
         * Defines the evaluation order for the code fragments that do not
         * create variables
         * @param code The operation just added to the evaluation order
         */
        inline void dependentAdded2EvaluationQueue(SourceCodeFragment<Base>& code) {
            const std::vector<Argument<Base> >& args = code.arguments_;

            typename std::vector<Argument<Base> >::const_iterator it;

            for (it = args.begin(); it != args.end(); ++it) {
                if (it->operation() != NULL) {
                    SourceCodeFragment<Base>& arg = *it->operation();
                    if (arg.getEvaluationOrder() == 0) {
                        arg.setEvaluationOrder(code.getEvaluationOrder());
                        dependentAdded2EvaluationQueue(arg);
                    }
                }
            }
        }

        inline bool isIndependent(const SourceCodeFragment<Base>& arg) const {
            if (arg.operation() == CGArrayCreationOp ||
                    arg.operation() == CGAtomicForwardOp ||
                    arg.operation() == CGAtomicReverseOp)
                return false;

            size_t id = arg.variableID();
            return id > 0 && id <= _independentVariables.size();
        }

        inline bool isTemporary(const SourceCodeFragment<Base>& arg) const {
            return arg.operation() != CGArrayCreationOp &&
                    arg.operation() != CGAtomicForwardOp &&
                    arg.operation() != CGAtomicReverseOp &&
                    arg.variableID() >= _minTemporaryVarID;
        }

        inline bool isTemporaryArray(const SourceCodeFragment<Base>& arg) const {
            return arg.operation() == CGArrayCreationOp;
        }

        virtual void resetCounters() {
            _variableOrder.clear();

            for (typename std::vector<SourceCodeFragment<Base> *>::const_iterator it = _codeBlocks.begin(); it != _codeBlocks.end(); ++it) {
                SourceCodeFragment<Base>* block = *it;
                block->resetHandlerCounters();
            }
        }

        /***********************************************************************
         *                   Graph management functions
         **********************************************************************/

        inline void findPaths(SourceCodePath& path2node,
                              SourceCodeFragment<Base>& code,
                              std::vector<SourceCodePath>& found,
                              size_t max);

        static inline std::vector<SourceCodePath> findPathsFromNode(const std::vector<SourceCodePath> nodePaths,
                                                                    SourceCodeFragment<Base>& node);

    private:

        CodeHandler(const CodeHandler&); // not implemented

        CodeHandler& operator=(const CodeHandler&); // not implemented

        friend class CG<Base>;
        friend class CGAbstractAtomicFun<Base>;

    };

}
#endif

