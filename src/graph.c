#include <Rinternals.h>
#include <Rdefines.h>
#include <Rmath.h>
#include <R_ext/RConverters.h>
#include <R_ext/Rdynload.h>
#include <Rversion.h>

SEXP R_scalarString(const char *);
SEXP intersectStrings(SEXP, SEXP);
SEXP graphIntersection(SEXP, SEXP, SEXP, SEXP, SEXP);
SEXP checkEdgeList(SEXP, SEXP);
SEXP listLen(SEXP);
SEXP graph_attrData_lookup(SEXP attrObj, SEXP keys, SEXP attr);
SEXP graph_sublist_assign(SEXP x, SEXP subs, SEXP sublist, SEXP values);
SEXP graph_is_adjacent(SEXP fromEdges, SEXP to);


#if defined(R_VERSION) && R_VERSION >= R_Version(2, 6, 0)
# define graph_duplicated(x) Rf_duplicated(x, FALSE)
#else
# define graph_duplicated(x) Rf_duplicated(x)
#endif

static const R_CallMethodDef R_CallDef[] = {
    {"intersectStrings", (DL_FUNC)&intersectStrings, 2},
    {"graphIntersection", (DL_FUNC)&graphIntersection, 5},
    {"listLen", (DL_FUNC)&listLen, 1},
    {"graph_attrData_lookup", (DL_FUNC)&graph_attrData_lookup, 3},
    {"graph_sublist_assign", (DL_FUNC)&graph_sublist_assign, 4},
    {"graph_is_adjacent", (DL_FUNC)&graph_is_adjacent, 2},
    {NULL, NULL, 0},
};

void R_init_graph(DllInfo *info) {
    R_registerRoutines(info, NULL, R_CallDef, NULL, NULL);
}


SEXP
R_scalarString(const char *v)
{
  SEXP ans = allocVector(STRSXP, 1);
  PROTECT(ans);
  if(v)
    SET_STRING_ELT(ans, 0, mkChar(v));
  UNPROTECT(1);
  return(ans);
}

SEXP intersectStrings(SEXP x, SEXP y) {
    SEXP ans, matchRes, matched, dup;
    int i, j, k, n, numZero=0, size;
    int curEntry=0;

    PROTECT(matchRes = Rf_match(y, x, 0));

    for (i = 0; i < length(matchRes); i++) {
	if (INTEGER(matchRes)[i] == 0)
	    numZero++;
    }

    size = length(matchRes) - numZero;
    PROTECT(matched = allocVector(STRSXP, size));

    for (i = 0; i < length(matchRes); i++) {
	if (INTEGER(matchRes)[i] != 0) {
	    SET_STRING_ELT(matched, curEntry++,
			   STRING_ELT(y, INTEGER(matchRes)[i]-1));
	}
    }

    PROTECT(dup = graph_duplicated(matched));
    n = length(matched);

    k = 0;
    for (j = 0; j < n; j++)
	if (LOGICAL(dup)[j] == 0)
	    k++;

    PROTECT(ans = allocVector(STRSXP, k));
    k = 0;
    for (j = 0; j < n; j++) {
	if (LOGICAL(dup)[j] == 0) {
	    SET_STRING_ELT(ans, k++, STRING_ELT(matched, j));
	}
    }

    UNPROTECT(4);
    return(ans);
}


SEXP graphIntersection(SEXP xN, SEXP yN, SEXP xE, SEXP yE,
		       SEXP edgeMode) {
    /* edgeMode == 0 implies "undirected" */
    SEXP bN, newXE, newYE;
    SEXP klass, outGraph;
    SEXP rval, ans, curRval, curWeights, curEdges, newNames, matches;
    int i, j, curEle=0;

    klass = MAKE_CLASS("graphNEL");
    PROTECT(outGraph = NEW_OBJECT(klass));
    if (INTEGER(edgeMode)[0])
	SET_SLOT(outGraph, Rf_install("edgemode"),
		 R_scalarString("directed"));
    else
	SET_SLOT(outGraph, Rf_install("edgemode"),
		 R_scalarString("undirected"));
    PROTECT(bN = intersectStrings(xN, yN));
    if (length(bN) == 0) {
	SET_SLOT(outGraph, Rf_install("nodes"), allocVector(STRSXP, 0));
	SET_SLOT(outGraph, Rf_install("edgeL"), allocVector(VECSXP, 0));
	UNPROTECT(1);
	return(outGraph);
    }
    PROTECT(newXE = checkEdgeList(xE, bN));
    PROTECT(newYE = checkEdgeList(yE, bN));
    PROTECT(newNames = allocVector(STRSXP, 2));
    SET_STRING_ELT(newNames, 0, mkChar("edges"));
    SET_STRING_ELT(newNames, 1, mkChar("weights"));
    PROTECT(rval = allocVector(VECSXP, length(newXE)));
    for (i = 0; i < length(newXE); i++) {
	PROTECT(curRval = allocVector(VECSXP, 2));
	setAttrib(curRval, R_NamesSymbol, newNames);

	PROTECT(ans = intersectStrings(VECTOR_ELT(newXE, i), VECTOR_ELT(newYE,
									i)));
	if (length(ans) == 0) {
	    SET_VECTOR_ELT(curRval, 0, allocVector(INTSXP, 0));
	    SET_VECTOR_ELT(curRval, 1, allocVector(INTSXP, 0));
	}
	else {
	    PROTECT(curEdges = allocVector(INTSXP, length(ans)));
	    PROTECT(matches = Rf_match(bN, ans, 0));
	    curEle = 0;
	    for (j = 0; j < length(matches); j++) {
		if (INTEGER(matches)[j] != 0)
		    INTEGER(curEdges)[curEle++] = INTEGER(matches)[j];
	    }
	    SET_VECTOR_ELT(curRval, 0, curEdges);
	    PROTECT(curWeights = allocVector(INTSXP, length(ans)));
	    for (j = 0; j < length(ans); j++)
		INTEGER(curWeights)[j] = 1;
	    SET_VECTOR_ELT(curRval, 1, curWeights);
	    UNPROTECT(3);
	}
	SET_VECTOR_ELT(rval, i, curRval);
	UNPROTECT(2);
    }
    setAttrib(rval, R_NamesSymbol, bN);
    SET_SLOT(outGraph, Rf_install("nodes"), bN);
    SET_SLOT(outGraph, Rf_install("edgeL"), rval);

    UNPROTECT(6);
    return(outGraph);
}

SEXP checkEdgeList(SEXP eL, SEXP bN) {
    SEXP newEL, curVec, curMatches, newVec, eleNames;
    int i, j, k, size, curEle;


    PROTECT(newEL = allocVector(VECSXP, length(bN)));
    PROTECT(eleNames = getAttrib(eL, R_NamesSymbol));
    for (i = 0; i < length(bN); i++) {
	for (k = 0; k < length(eL); k++) {
	    if (strcmp(CHAR(STRING_ELT(eleNames, k)),
		       CHAR(STRING_ELT(bN, i))) == 0)
		break;
	}
	if (k < length(eL)) {
	    curVec = VECTOR_ELT(eL, k);
	    PROTECT(curMatches = Rf_match(curVec, bN, 0));
	    size = length(curMatches);
	    for (j = 0; j < length(curMatches); j++) {
		if (INTEGER(curMatches)[j] == 0)
		    size--;
	    }
	    PROTECT(newVec = allocVector(STRSXP, size));
	    curEle = 0;
	    for (j = 0; j < length(curMatches); j++) {
		if (INTEGER(curMatches)[j] != 0) {
		    SET_STRING_ELT(newVec, curEle++,
				   STRING_ELT(curVec,
					      INTEGER(curMatches)[j]-1));
		}
	    }
	    SET_VECTOR_ELT(newEL, i, newVec);
	    UNPROTECT(2);
	}
    }
    setAttrib(newEL, R_NamesSymbol, bN);
    UNPROTECT(2);
    return(newEL);
}

/* Taken from Biobase to avoid depending on it */
SEXP listLen(SEXP x)
{
  SEXP ans;
  int i;

  if( !Rf_isNewList(x) )
    error("require a list");

  PROTECT(ans = allocVector(REALSXP, length(x)));

  for(i=0; i<length(x); i++)
    REAL(ans)[i] = length(VECTOR_ELT(x, i));
  UNPROTECT(1);
  return(ans);
}

static SEXP graph_getListElement(SEXP list, char *str, SEXP defaultVal)
{
    SEXP elmt = defaultVal;
    SEXP names = getAttrib(list, R_NamesSymbol);
    int i;

    for (i = 0; i < length(list); i++)
        if (strcmp(CHAR(STRING_ELT(names, i)), str) == 0) {
            elmt = VECTOR_ELT(list, i);
            break;
        }
    return elmt;
}

static int graph_getListIndex(SEXP list, SEXP name)
{
    SEXP names = GET_NAMES(list);
    int i;
    char* str = CHAR(STRING_ELT(name, 0));

    for (i = 0; i < length(list); i++)
        if (strcmp(CHAR(STRING_ELT(names, i)), str) == 0)
            return i;
    return -1;
}

static SEXP graph_sublist_lookup(SEXP x, SEXP subs, SEXP sublist,
                                 SEXP defaultVal)
{
    SEXP ans, idx, names, el;
    int ns, i, j;
    sublist = STRING_ELT(sublist, 0);
    ns = length(subs);
    names = GET_NAMES(x);
    PROTECT(idx = match(names, subs, -1));
    PROTECT(ans = allocVector(VECSXP, ns));
    for (i = 0; i < ns; i++) {
        j = INTEGER(idx)[i];
        if (j < 0)
            SET_VECTOR_ELT(ans, i, defaultVal);
        else {
            el = graph_getListElement(VECTOR_ELT(x, j-1), CHAR(sublist),
                                      defaultVal);
            SET_VECTOR_ELT(ans, i, el);
        }
    }
    SET_NAMES(ans, subs);
    UNPROTECT(2);
    return ans;
}

SEXP graph_attrData_lookup(SEXP attrObj, SEXP keys, SEXP attr)
{
    SEXP data, defaults, defaultVal;
    char* attribute;

    data = GET_SLOT(attrObj, install("data"));
    defaults = GET_SLOT(attrObj, install("defaults"));
    attribute = CHAR(STRING_ELT(attr, 0));
    /* We might want this to error out instead of grabbing a default.
       The default value should exist.
    */
    defaultVal = graph_getListElement(defaults, attribute, R_NilValue);
    return graph_sublist_lookup(data, keys, attr, defaultVal);
}

#ifdef __NOT_USED__
static SEXP graph_list_lookup(SEXP x, SEXP subs, SEXP defaultVal)
{
    SEXP ans, idx, names;
    int ns, i, j;
    ns = length(subs);
    names = GET_NAMES(x);
    PROTECT(idx = match(names, subs, -1));
    PROTECT(ans = allocVector(VECSXP, ns));
    for (i = 0; i < ns; i++) {
        j = INTEGER(idx)[i];
        if (j < 0)
            SET_VECTOR_ELT(ans, i, defaultVal); /* need to duplicate? */
        else {
            SET_VECTOR_ELT(ans, i, VECTOR_ELT(x, j-1));
        }
    }
    SET_NAMES(ans, subs);
    UNPROTECT(2);
    return ans;
}
#endif

static SEXP graph_makeItem(SEXP s, int i)
{
    if (s == R_NilValue)
	return s;
    else {
	SEXP item = R_NilValue;/* -Wall */
	switch (TYPEOF(s)) {
	case STRSXP:
            item = ScalarString(STRING_ELT(s, i));
            break;
	case EXPRSXP:
	case VECSXP:
	    item = duplicate(VECTOR_ELT(s, i));
	    break;
	case LGLSXP:
	    item = ScalarLogical(LOGICAL(s)[i]);
	    break;
	case INTSXP:
	    item = ScalarInteger(INTEGER(s)[i]);
	    break;
	case REALSXP:
	    item = ScalarReal(REAL(s)[i]);
	    break;
	case CPLXSXP:
	    item = ScalarComplex(COMPLEX(s)[i]);
	    break;
	case RAWSXP:
	    item = ScalarRaw(RAW(s)[i]);
	    break;
	default:
	    error("unknown type");
	}
	return item;
    }
}

static SEXP graph_addItemToList(SEXP list, SEXP item, SEXP name)
{
    SEXP ans, ansnames, listnames;
    int len = length(list);
    int i;

    PROTECT(ans = allocVector(VECSXP, len + 1));
    PROTECT(ansnames = allocVector(STRSXP, len + 1));
    listnames = GET_NAMES(list);
    for (i = 0; i < len; i++) {
        SET_STRING_ELT(ansnames, i, STRING_ELT(listnames, i));
        SET_VECTOR_ELT(ans, i, VECTOR_ELT(list, i));
    }
    SET_STRING_ELT(ansnames, len, STRING_ELT(name, 0));
    SET_VECTOR_ELT(ans, len, item);
    SET_NAMES(ans, ansnames);
    UNPROTECT(2);
    return ans;
}

SEXP graph_sublist_assign(SEXP x, SEXP subs, SEXP sublist, SEXP values)
{
    SEXP idx, names, tmpItem, newsubs, ans, ansnames, val;
    int ns, i, j, nnew, nextempty, origlen, numVals, tmpIdx;

    ns = length(subs);
    origlen = length(x);
    numVals = length(values);
    if (numVals > 1 && ns != numVals)
        error("invalid args: subs and values must be the same length");
    names = GET_NAMES(x);
    PROTECT(idx = match(names, subs, -1));
    PROTECT(newsubs = allocVector(STRSXP, ns));
    nnew = 0;
    for (i = 0; i < ns; i++) {
        if (INTEGER(idx)[i] == -1)
            SET_STRING_ELT(newsubs, nnew++, STRING_ELT(subs, i));
    }
    PROTECT(ans = allocVector(VECSXP, origlen + nnew));
    PROTECT(ansnames = allocVector(STRSXP, length(ans)));
    for (i = 0; i < origlen; i++) {
        SET_VECTOR_ELT(ans, i, duplicate(VECTOR_ELT(x, i)));
        SET_STRING_ELT(ansnames, i, duplicate(STRING_ELT(names, i)));
    }
    j = origlen;
    for (i = 0; i < nnew; i++)
        SET_STRING_ELT(ansnames, j++, STRING_ELT(newsubs, i));
    SET_NAMES(ans, ansnames);
    UNPROTECT(1);

    nextempty = origlen; /* index of next unfilled element of ans */
    for (i = 0; i < ns; i++) {
        if (numVals > 1)
            PROTECT(val = graph_makeItem(values, i));
        else if (numVals == 1 && isVectorList(values))
            PROTECT(val = duplicate(VECTOR_ELT(values, 0)));
        else
            PROTECT(val = duplicate(values));
        j = INTEGER(idx)[i];
        if (j < 0) {
            tmpItem = graph_addItemToList(R_NilValue, val, sublist);
            SET_VECTOR_ELT(ans, nextempty, tmpItem);
            nextempty++;
        } else {
            tmpItem = VECTOR_ELT(ans, j-1);
            tmpIdx = graph_getListIndex(tmpItem, sublist);
            if (tmpIdx == -1) {
                tmpItem = graph_addItemToList(tmpItem, val, sublist);
                SET_VECTOR_ELT(ans, j-1, tmpItem);
            } else
                SET_VECTOR_ELT(tmpItem, tmpIdx, val);
        }
        UNPROTECT(1);
    }
    UNPROTECT(3);
    return ans;
}

SEXP graph_is_adjacent(SEXP fromEdges, SEXP to)
{
    SEXP ans, frEdges, toEdge, idx;
    int i, j, lenTo;
    int found = 0;

    lenTo = length(to);
    PROTECT(ans = allocVector(LGLSXP, lenTo));
    for (i = 0; i < lenTo; i++) {
        PROTECT(toEdge = ScalarString(STRING_ELT(to, i)));
        frEdges = VECTOR_ELT(fromEdges, i);
        idx = match(toEdge, frEdges, 0);
        for (j = 0; j < length(idx); j++)
            if ((found = (INTEGER(idx)[j] > 0)))
                break;
        LOGICAL(ans)[i] = found;
        UNPROTECT(1);
    }
    UNPROTECT(1);
    return ans;
}
