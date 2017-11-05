/*
Compile as
    gcc -g -O2 -std=gnu99 -lsqlite3 -o kladr-query kladr-query.c

Use as
    kladr-query CODE TYPE [NAME_PATTERN]

    This perform search for objects in the hierarchy level TYPE in CODE where name like to NAME_PATTERN

As example
    kladr-query 77000000000021200 postalcode
    77000000000021200   109382
    77000000000021200   109387

    kladr-query 77000000000021200 postalcode 387
    77000000000021200   109387

See also
ПРИКАЗ от 17 ноября 2005 г. N САЭ-3-13/594@
ОБ ИЗМЕНЕНИИ СТРУКТУРЫ ВЕДОМСТВЕННОГО КЛАССИФИКАТОРА АДРЕСОВ РОССИЙСКОЙ ФЕДЕРАЦИИ (КЛАДР)
http://www.kladr.org/2009-10-26-12-22-12/2009-10-27-07-56-46
*/
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>

#define STREQ(a, b) (*(a) == *(b) && strcmp((a), (b)) == 0)

#define DB "/usr/share/sqlite3-kladr/kladr.db"
#define DEBUG 0

// определяем индекс
// поиск начинается с самого детального адреса и далее по убыванию детализации до района
#define SQL_POSTALCODE_DOMA "select distinct substr(%Q,1,17), \"index\" from doma where code>substr(%Q,1,17) and code<substr(%Q,1,17) + 1 and \"index\" like '%%' || trim(%Q) || '%%'"
#define SQL_POSTALCODE_STREET "select distinct substr(%Q,1,15), \"index\" from street where code>substr(%Q,1,15) and code<substr(%Q,1,15) + 1 and \"index\" like '%%' || trim(%Q) || '%%'"
#define SQL_POSTALCODE_POINT "select substr(%Q,1,11), \"index\" from kladr where substr(code,9,3)!='000' and code>substr(%Q,1,11) and code<substr(%Q,1,11)+1 and \"index\" like '%%' || trim(%Q) || '%%'"
#define SQL_POSTALCODE_TOWN "select substr(%Q,1,8), \"index\" from kladr where substr(code,6,3)!='000' and code>substr(%Q,1,8) and code<substr(%Q,1,8)+1 and \"index\" like '%%' || trim(%Q) || '%%'"
#define SQL_POSTALCODE_DISTRICT "select substr(%Q,1,5), \"index\" from kladr where substr(code,3,3)!='000' and code>substr(%Q,1,5) and code<substr(%Q,1,5)+1 and \"index\" like '%%' || trim(%Q) || '%%'"

//select name || ' ' || socr as name, code from street where code > substr($kladr_code,1,11) and code < substr($kladr_code,1,11) + 1. and substr(code,16,2)='00' and name like $q order by name asc limit 10

// код региона состоит из 2-х значащих цифр и 9-ти нулей (плюс 2-х цифр признака актуальности - нас интересуют действующие, т.е. 00)
// внимание: ищем по коду или по имени, предусмотрен вывод полного списка всех регионов, т.к. именно с региона пользователь начинает заполнять адрес
#define SQL_REGION "select code, name || ' ' || socr from region where (%Q = '' or code = substr(%Q,1,2) || '00000000000') and name || ' ' || socr like '%%' || trim(%Q) || '%%' order by name asc"

// код района состоит из 2-х цифр кода региона, 3-х цифр кода района и 6-ти нулей (плюс 2-х цифр признака актуальности)
#define SQL_DISTRICT "select code, name || ' ' || socr from kladr \
where code >= substr(%Q,1,2) || '00100000000' and code <= substr(%Q,1,2) || '99900000000' and code like '_____00000000' \
and name || ' ' || socr like '%%' || trim(%Q) || '%%' order by name asc"

// код города состоит из 2-х цифр кода региона, 3-х цифр кода района, 3-х цифр кода города и 3-х нулей (плюс 2-х цифр признака актуальности)
// substr(code,9,3)='000' - пункт не определен (пункты нас здесь не интересуют)
// substr(code,3,9)!='000000000' - регионы не отображаем
#define SQL_TOWN "select code, name || ' ' || socr from kladr \
where code >= substr(%Q,1,5) || '00100000' and code <= substr(%Q,1,5) || '99900000' and code like '________00000' \
and substr(code,6,3)!='000' and substr(code,3,9)!='000000000' \
and name || ' ' || socr like '%%' || trim(%Q) || '%%' order by name asc"

// код населенного пункта состоит из 2-х цифр кода региона, 3-х цифр кода района, 3-х цифр кода города
// и 3-х цифр кода населенного пункта (плюс 2-х цифр признака актуальности)
// substr(code,3,9)!='000000000' - регионы не отображаем
#define SQL_POINT "select code, name || ' ' || socr from kladr \
where code >= substr(%Q,1,8) || '00100' and code <= substr(%Q,1,8) || '99900' and code like '___________00' \
and substr(code,3,9)!='000000000' \
and name || ' ' || socr like '%%' || trim(%Q) || '%%' order by name asc"

// найти улицу в указанном регионе, районе или городе
// код региона всегда известен, а код района и код города могут быть равны 000
// код улицы состоит из 4-х знаков
#define SQL_STREET "select code, name || ' ' || socr from street \
where code >= substr(%Q,1,11) || '000100' and code <= substr(%Q,1,11) || '999900' and code like '_______________00' \
and name || ' ' || socr like '%%' || trim(%Q) || '%%' order by name asc"

int exec_sql (sqlite3 *db, const char *sql, int *counter) {
    sqlite3_stmt *stmt;
    int rc = 0;
    if (DEBUG) fprintf(stderr,"SQL: %s\n", sql);
    // prepare the SQL statement
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if( rc ){
        return rc;
    }else{
        // execute the statement
        do{
            rc = sqlite3_step(stmt);
            switch( rc ){
            case SQLITE_DONE:
                break;
            case SQLITE_ROW:
                // print results for this row
                printf("%s\t%s\n", sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1));
                *counter=*counter+1;
                break;
            default:
                return rc;
                break;
            }
        }while( rc==SQLITE_ROW );
        // finalize the statement to release resources
        rc = sqlite3_finalize(stmt);
    }
    if (DEBUG) fprintf(stderr,"Records counter: %u\n", *counter);
    return rc;
}

int main(int argc, const char *argv[]){
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc = 0;
    int col, cols, count;
    char *sql;
    int counter = 0;
    char *code="", *query="";

    if( argc!=3 && argc!=4 ){
        fprintf(stderr, "Usage: %s CODE TYPE [NAME_PATTERN]\n", argv[0]);
        return 1;
    }
    if( argc==4 ){
        query = (char*)argv[3];
    }
    code = (char*)argv[1];
    // open the database file
    rc = sqlite3_open(DB, &db);
    if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    if ( STREQ(argv[2], "postalcode") ) {
        sql = sqlite3_mprintf(SQL_POSTALCODE_DOMA, code, code, code, query);
        rc = exec_sql (db, sql, &counter);
        sqlite3_free(sql);
        if (rc == SQLITE_OK && counter == 0) {
            sql = sqlite3_mprintf(SQL_POSTALCODE_STREET, code, code, code, query);
            rc = exec_sql (db, sql, &counter);
            sqlite3_free(sql);
        }
        if (rc == SQLITE_OK && counter == 0) {
            sql = sqlite3_mprintf(SQL_POSTALCODE_POINT, code, code, code, query);
            rc = exec_sql (db, sql, &counter);
            sqlite3_free(sql);
        }
        if (rc == SQLITE_OK && counter == 0) {
            sql = sqlite3_mprintf(SQL_POSTALCODE_TOWN, code, code, code, query);
            rc = exec_sql (db, sql, &counter);
            sqlite3_free(sql);
        }
        if (rc == SQLITE_OK && counter == 0) {
            sql = sqlite3_mprintf(SQL_POSTALCODE_DISTRICT, code, code, code, query);
            rc = exec_sql (db, sql, &counter);
            sqlite3_free(sql);
        }
    } else if ( STREQ(argv[2], "region") ) {
        sql = sqlite3_mprintf(SQL_REGION, code, code, query );
        rc = exec_sql (db, sql, &counter);
        sqlite3_free(sql);
    } else if ( STREQ(argv[2], "district") ) {
        sql = sqlite3_mprintf(SQL_DISTRICT, code, code, query );
        rc = exec_sql (db, sql, &counter);
        sqlite3_free(sql);
    } else if ( STREQ(argv[2], "town") ) {
        sql = sqlite3_mprintf(SQL_TOWN, code, code, query );
        rc = exec_sql (db, sql, &counter);
        sqlite3_free(sql);
    } else if ( STREQ(argv[2], "point") ) {
        sql = sqlite3_mprintf(SQL_POINT, code, code, query );
        rc = exec_sql (db, sql, &counter);
        sqlite3_free(sql);
    } else if ( STREQ(argv[2], "street") ) {
        sql = sqlite3_mprintf(SQL_STREET, code, code, query );
        rc = exec_sql (db, sql, &counter);
        sqlite3_free(sql);
    } else {
        fprintf(stderr, "Unknown type : %s\nAllowed types: region, town, point, district, street, postalcode\n", argv[2]);
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %d : %s\n", rc, sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    // close the database file
    sqlite3_close(db);

    return 0;
}
