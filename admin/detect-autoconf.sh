#! /bin/sh

# Global variables...
AUTOCONF="autoconf"
AUTOHEADER="autoheader"
AUTOM4TE="autom4te"
AUTOMAKE="automake"
ACLOCAL="aclocal"


# We don't use variable here for remembering the type ... strings.
# local variables are not that portable, but we fear namespace issues with
# our includer.  The repeated type calls are not that expensive.
checkAutoconf()
{
  if test -x "`$WHICH autoconf-2.5x 2>/dev/null`" ; then	
    AUTOCONF="`$WHICH autoconf-2.5x`"
  elif test -x "`$WHICH autoconf-2.57 2>/dev/null`" ; then
    AUTOCONF="`$WHICH autoconf-2.57`"
  elif test -x "`$WHICH autoconf-2.56 2>/dev/null`" ; then
    AUTOCONF="`$WHICH autoconf-2.56`"
  elif test -x "`$WHICH autoconf-2.55 2>/dev/null`" ; then
    AUTOCONF="`$WHICH autoconf-2.55`"
  elif test -x "`$WHICH autoconf-2.54 2>/dev/null`" ; then
    AUTOCONF="`$WHICH autoconf-2.54`"
  elif test -x "`$WHICH autoconf-2.53 2>/dev/null`" ; then
    AUTOCONF="`$WHICH autoconf-2.53`"
  elif test -x "`$WHICH autoconf-2.53a 2>/dev/null`" ; then
    AUTOCONF="`$WHICH autoconf-2.53a`"
  elif test -x "`$WHICH autoconf-2.52 2>/dev/null`" ; then
    AUTOCONF="`$WHICH autoconf-2.52`"
  elif test -x "`$WHICH autoconf2.50 2>/dev/null`" ; then
    AUTOCONF="`$WHICH autoconf2.50`"
  fi
}

checkAutoheader()
{
  if test -x "`$WHICH autoheader-2.5x 2>/dev/null`" ; then
    AUTOHEADER="`$WHICH autoheader-2.5x`"
    AUTOM4TE="`$WHICH autom4te-2.5x`"
  elif test -x "`$WHICH autoheader-2.57 2>/dev/null`" ; then
    AUTOHEADER="`$WHICH autoheader-2.57`"
    AUTOM4TE="`$WHICH autom4te-2.57`"
  elif test -x "`$WHICH autoheader-2.56 2>/dev/null`" ; then
    AUTOHEADER="`$WHICH autoheader-2.56`"
    AUTOM4TE="`$WHICH autom4te-2.56`"
  elif test -x "`$WHICH autoheader-2.55 2>/dev/null`" ; then
    AUTOHEADER="`$WHICH autoheader-2.55`"
    AUTOM4TE="`$WHICH autom4te-2.55`"
  elif test -x "`$WHICH autoheader-2.54 2>/dev/null`" ; then
    AUTOHEADER="`$WHICH autoheader-2.54`"
    AUTOM4TE="`$WHICH autom4te-2.54`"
  elif test -x "`$WHICH autoheader-2.53 2>/dev/null`" ; then
    AUTOHEADER="`$WHICH autoheader-2.53`"
    AUTOM4TE="`$WHICH autom4te-2.53`"
  elif test -x "`$WHICH autoheader-2.53a 2>/dev/null`" ; then
    AUTOHEADER="`$WHICH autoheader-2.53a`"
    AUTOM4TE="`$WHICH autom4te-2.53a`"
  elif test -x "`$WHICH autoheader-2.52 2>/dev/null`" ; then
    AUTOHEADER="`$WHICH autoheader-2.52`"
  elif test -x "`$WHICH autoheader2.50 2>/dev/null`" ; then
    AUTOHEADER="`$WHICH autoheader2.50`"
  fi
}

checkAutomakeAclocal ()
{
 if test -x "`$WHICH automake-1.6 2>/dev/null`" ; then
    AUTOMAKE="`$WHICH automake-1.6`"
    ACLOCAL="`$WHICH aclocal-1.6`"
  elif test -x "`$WHICH automake-1.7 2>/dev/null`" ; then
    AUTOMAKE="`$WHICH automake-1.7`"
    ACLOCAL="`$WHICH aclocal-1.7`"
  fi
  if test -n "$UNSERMAKE"; then 
     AUTOMAKE="$UNSERMAKE"
  fi
}

checkWhich ()
{
  WHICH=""
  for i in "type -p" "which" "type" ; do
    T=`$i sh 2> /dev/null`
    test -x "$T" && WHICH="$i" && break
  done
}

checkWhich
checkAutoconf
checkAutoheader
checkAutomakeAclocal

export WHICH AUTOHEADER AUTOCONF AUTOM4TE AUTOMAKE ACLOCAL
