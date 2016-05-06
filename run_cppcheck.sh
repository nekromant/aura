#!/bin/bash
if [ "$CC" == "" ]; then
    CC=gcc
fi

paths_list() {
    $CC -E -Wp,-v - < /dev/null 2>&1|grep -E '^ '
}

paths() {
    paths_list | while read line; do
        echo -n "-I $line "
    done
    cat .clang_complete | while read line; do
        echo -n "$line "
    done
}

cppcheck `paths` --enable=all --suppressions-list=cppcheck.suppress  --platform=unix64 `cat .core_sources` --xml 2>cppcheck.xml
