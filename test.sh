#!/bin/bash
assert(){
    expected="$1"
    input="$2"

    ./Mycc "$input" >  tmp.s
    cc -o tmp tmp.s
    ./tmp
    actual="$?"

    if [  "$actual" = "$expected" ]; then 
      echo "$input => $actual"
    else
      echo "$input => $expected expected, but got $actual"
      exit 1
    fi
}

assert 0 0
assert 22 22
assert 21 "5+20-4"
assert 41 " 12 + 34 - 5 "
echo OK