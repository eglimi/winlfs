#!/bin/bash

pushd ../../code
cvs upd -d
popd

doxygen doxy_ExtFsr
doxygen doxy_Ext2Fsd

#find . -iname "*.tex" -exec sed -i s/subsection/section/ {} \;

pushd ExtFsr/pdflatex
make
cvs ci -m ""
popd

pushd Ext2Fsd/pdflatex
make
cvs ci -m ""
popd

