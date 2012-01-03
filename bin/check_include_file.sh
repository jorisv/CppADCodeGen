#! /bin/bash -e
# $Id: check_include_file.sh 2082 2011-08-31 17:50:58Z bradbell $
# -----------------------------------------------------------------------------
# CppAD: C++ Algorithmic Differentiation: Copyright (C) 2003-11 Bradley M. Bell
#
# CppAD is distributed under multiple licenses. This distribution is under
# the terms of the
#                     Common Public License Version 1.0.
#
# A copy of this license is included in the COPYING file of this distribution.
# Please visit http://www.coin-or.org/CppAD/ for information on other licenses.
# -----------------------------------------------------------------------------
if [ ! -e "bin/check_include_file.sh" ]
then
	msg="must be executed from its parent directory"
	echo "bin/check_include_file.sh: $msg"
	exit 1
fi
# -----------------------------------------------------------------------------
#
echo "Checking difference between C++ include directives and file names."
echo "-------------------------------------------------------------------"
if [ -e check_include_file.1.$$ ]
then
	echo "bin/check_include_file.sh: unexpected bin/check_include_file.1.$$"
	exit 1
fi
#
declare -A files;
declare -A root;
declare -A rootInclude;
#
files[cppad]="cppad/*.hpp\
	cppad/local/*.hpp\
	cppad/speed/*.hpp"
root[cppad]="cppad"
rootInclude[cppad]=""
#
files[cppad_codegen]="cppad_codegen/src/cppad_codegen/*.hpp \
	cppad_codegen/src/cppad_codegen/local/*.hpp"
root[cppad_codegen]="cppad_codegen/src/"
rootInclude[cppad_codegen]="cppad_codegen/src/"
#
different="no"
for ff in cppad_codegen cppad
do
	r=${root[$ff]}
	for ext in .cpp .hpp
	do

		dir_list=`find "$r" -name "*$ext" | \
			sed -e 's|^\./||' -e '/^work/d' -e 's|/[^/]*$||' | sort -u`  
		for dir in $dir_list 
		do
			list=`ls $dir/*$ext`
			for file in $list
			do
				sed -n -e '/^# *include *<'$ff'\//p' $file \
					>> bin/check_include_file.1.$$
			done
		done
	done
#
	cat bin/check_include_file.1.$$ | \
		sed -e 's%[^<]*<%%'  -e 's%>.*$%%' -e 's|^${root[$ff]}||' | \
		sort -u > bin/check_include_file.2.$$
# The file cppad/local/prototype_op.hpp should never be included. 
# All other files should.
	
	if [ "${rootInclude[$ff]}" = "" ]
 	then
 	 	ls	${files[$ff]} | \
			sed -e '/cppad\/local\/prototype_op.hpp/d' | \
			sort > bin/check_include_file.3.$$ 
 	else
 	 	ls	${files[$ff]} | \
			sed -e '/cppad\/local\/prototype_op.hpp/d' | \
 	 	 	sed -e "s|${rootInclude[$ff]}||" | \
			sort > bin/check_include_file.3.$$ 
        fi
	if !(diff bin/check_include_file.2.$$ bin/check_include_file.3.$$)
	then
		different="yes"
	fi
	for index in 1 2 3
	do
		rm bin/check_include_file.$index.$$
	done

done
#
echo "-------------------------------------------------------------------"
if [ $different = "yes" ]
then
	echo "Error: nothing should be between the two dashed lines above"
	exit 1
else
	echo "Ok: nothing is between the two dashed lines above"
	exit 0
fi
