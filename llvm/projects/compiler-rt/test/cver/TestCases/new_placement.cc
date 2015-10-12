// This test should not generate any reports from

// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: CVER_OPTIONS=die_on_error=1 %run %t 2>&1

#include <iostream>
#include <map>
#include <stdio.h>

bool fncomp (char lhs, char rhs) {return lhs<rhs;}

struct classcomp {
  bool operator() (const char& lhs, const char& rhs) const
  {return lhs<rhs;}
};

int main ()
{

  std::map<char,int> first;
  printf("first : %p\n", &first);
  first['a']=10;

  std::map<char,int> second (first.begin(),first.end());
  std::map<char,int> third (second);
  std::map<char,int,classcomp> fourth;
  bool(*fn_pt)(char,char) = fncomp;
  std::map<char,int,bool(*)(char,char)> fifth (fn_pt);

  return 0;

}
