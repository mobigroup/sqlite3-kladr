/*
Compile as
    gcc -g -O2 -std=gnu99 -lsqlite3 -o kladr-info kladr-info.c

Use as
    kladr-info CODE

As example
    kladr-info 7700000001
    postalcode
    postaltype
    region  Москва
    region_socr г
    district
    district_socr
    town
    town_socr
    point
    point_socr
    street
    street_socr
    doma
    doma_socr
    korp
    korp_socr

    kladr-info 7700000001000
    postalcode  111123
    postaltype  point
    region  Москва
    region_socr г
    district
    district_socr
    town
    town_socr
    point   Измайловская Пасека
    point_socr  п
    street
    street_socr
    doma
    doma_socr
    korp
    korp_socr

    kladr-info 7700000001000000000
    postalcode  111123
    postaltype  doma
    region  Москва
    region_socr г
    district
    district_socr
    town
    town_socr
    point   Измайловская Пасека
    point_socr  п
    street
    street_socr
    doma    7стр5
    doma_socr   ДОМ
    korp
    korp_socr


    kladr-info 0200000500000410003
    postalcode  453259
    postaltype  doma
    region  Башкортостан
    region_socr Респ
    district
    district_socr
    town    Салават
    town_socr   г
    point
    point_socr
    street  Матросова
    street_socr ул
    doma    Ч(24-36),Ч(40-50),3,4,7,10-13,16-23,26,28,Н(31-37),2,6,8,25,25,29,44,14,3
    doma_socr   ДОМ
    korp    34
    korp_socr


    kladr-info 77000000000045900
    postalcode  111141
    postaltype  doma
    region  Москва
    region_socr г
    district
    district_socr
    town
    town_socr
    point
    point_socr
    street  Электродная
    street_socr ул
    doma
    doma_socr
    korp
    korp_socr

*/

#include <stdio.h>
#include <sqlite3.h>
#include <string.h>

#define DB "/usr/share/sqlite3-kladr/kladr.db"
#define DEBUG 0

// определяем индекс
// ищется начиная с самого детального адреса и далее по убыванию детализации до района
#define SQL_POSTALCODE_DOMA "select \"index\" as postalcode, 'doma' as postaltype from doma where length(%Q)>=17 and code>=substr(%Q,1,17) || '00' and code<=substr(%Q,1,17) || '99'"
#define SQL_POSTALCODE_STREET "select \"index\" as postalcode, 'street' as postaltype from street where length(%Q)>=15 and code>=substr(%Q,1,15) || '00' and code<=substr(%Q,1,15) || '99'"
#define SQL_POSTALCODE_POINT "select \"index\" as postalcode, 'point' as postaltype from kladr where length(%Q)>=11 and substr(code,9,3)!='000' and code>substr(%Q,1,11) and code<substr(%Q,1,11)+1"
#define SQL_POSTALCODE_TOWN "select \"index\" as postalcode, 'town' as postaltype from kladr where length(%Q)>=8 and substr(code,6,3)!='000' and code>substr(%Q,1,8) and code<substr(%Q,1,8)+1"
#define SQL_POSTALCODE_DISTRICT "select \"index\" as postalcode, 'district' as postaltype from kladr where length(%Q)>=5 and substr(code,3,3)!='000' and code>substr(%Q,1,5) and code<substr(%Q,1,5)+1"

// 19 знаков
#define SQL_KORP "select coalesce(group_concat(korp,','),'') as korp, '' as korp_socr from doma where length(%Q)>=19 and code = substr(%Q,1,19)"
// 19 знаков
// socr в КЛАДР сейчас тождественно равно ДОМ
#define SQL_DOMA "select coalesce(group_concat(name,','),'') as doma, coalesce(min(socr),'') as doma_socr from doma where length(%Q)>=17 and code >= substr(%Q,1,17) || '00' and code <= substr(%Q,1,17) || '99'"
// найти улицу в указанном регионе, районе или городе
// код региона всегда известен, а код района и код города могут быть равны 000
// код улицы состоит из 4-х знаков
#define SQL_STREET "select name as street, socr as street_socr from street where length(%Q)>=15 and code = substr(%Q,1,15) || '00'"
// код населенного пункта состоит из 2-х цифр кода региона, 3-х цифр кода района, 3-х цифр кода города
// и 3-х цифр кода населенного пункта (плюс 2-х цифр признака актуальности)
// регионы не отображаем
#define SQL_POINT "select name as point, socr as point_socr from kladr where length(%Q)>=11 and code = substr(%Q,1,11) || '00' and substr(code,3,9)!='000000000' and substr(code,6,3)='000'"
// код города состоит из 2-х цифр кода региона, 3-х цифр кода района, 3-х цифр кода города и 3-х нулей (плюс 2-х цифр признака актуальности)
// пункты и регионы не отображаем
#define SQL_TOWN "select name as town, socr as town_socr from kladr where length(%Q)>=8 and code = substr(%Q,1,8) || '00000' and substr(code,6,3)!='000' and substr(code,9,3)='000'"
// код района состоит из 2-х цифр кода региона, 3-х цифр кода района и 6-ти нулей (плюс 2-х цифр признака актуальности)
#define SQL_DISTRICT "select name as district, socr as district_socr from kladr where length(%Q)>=5 and code = substr(%Q,1,5) || '00000000' and substr(code,3,9)!='000000000'"
// код региона состоит из 2-х значащих цифр и 9-ти нулей (плюс 2-х цифр признака актуальности - нас интересуют действующие, т.е. 00)
#define SQL_REGION "select name as region, socr as region_socr from region where length(%Q)>=2 and code = substr(%Q,1,2) || '00000000000'"

// prints only first record
int exec_sql_1row (sqlite3 *db, const char *sql, int *counter, int notfound) {
    sqlite3_stmt *stmt;
    int rc = 0;
    if (DEBUG) fprintf(stderr,"SQL: %s\n", sql);
    *counter = 0;
    // prepare the SQL statement
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if( rc ){
        return rc;
    }else{
        // execute the statement
        rc = sqlite3_step(stmt);
        // select only first record if exists
        switch( rc ){
        case SQLITE_DONE:
            if (notfound) {
                printf("%s\t\n", sqlite3_column_name(stmt, 0));
                printf("%s\t\n", sqlite3_column_name(stmt, 1));
            }
            break;
        case SQLITE_ROW:
            // print results for this row
            printf("%s\t%s\n", sqlite3_column_name(stmt, 0), sqlite3_column_text(stmt, 0));
            printf("%s\t%s\n", sqlite3_column_name(stmt, 1), sqlite3_column_text(stmt, 1));
            *counter = 1;
            break;
        default:
            return rc;
            break;
        }
        // finalize the statement to release resources
        rc = sqlite3_finalize(stmt);
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %d : %s\n", rc, sqlite3_errmsg(db));
    }
    return rc;
}


int main(int argc, const char *argv[]){
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc = 0;
    int col, cols, count;
    char *sql;
    int counter = 0;
    char *code="";

    if( argc!=2 ){
        fprintf(stderr, "Usage: %s CODE\nCODE may has 5 symbols or more.\n", argv[0]);
        return 1;
    }
    code = (char*)argv[1];
    if( strlen(code)<5 ){
        fprintf(stderr, "Code is too short: %s\n", code);
        return 1;
    }
    // open the database file
    rc = sqlite3_open(DB, &db);
    if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // postalcode
    sql = sqlite3_mprintf(SQL_POSTALCODE_DOMA, code, code, code);
    rc = exec_sql_1row (db, sql, &counter, 0);
    sqlite3_free(sql);
    if (rc == SQLITE_OK && counter == 0) {
        sql = sqlite3_mprintf(SQL_POSTALCODE_STREET, code, code, code);
        rc = exec_sql_1row (db, sql, &counter, 0);
        sqlite3_free(sql);
    }
    if (rc == SQLITE_OK && counter == 0) {
        sql = sqlite3_mprintf(SQL_POSTALCODE_POINT, code, code, code);
        rc = exec_sql_1row (db, sql, &counter, 0);
        sqlite3_free(sql);
    }
    if (rc == SQLITE_OK && counter == 0) {
        sql = sqlite3_mprintf(SQL_POSTALCODE_TOWN, code, code, code);
        rc = exec_sql_1row (db, sql, &counter, 0);
        sqlite3_free(sql);
    }
    if (rc == SQLITE_OK && counter == 0) {
        sql = sqlite3_mprintf(SQL_POSTALCODE_DISTRICT, code, code, code);
        rc = exec_sql_1row (db, sql, &counter, 1);
        sqlite3_free(sql);
    }
    // region
    if (rc == SQLITE_OK) {
        sql = sqlite3_mprintf(SQL_REGION, code, code );
        rc = exec_sql_1row (db, sql, &counter, 1);
        sqlite3_free(sql);
    }
    // district
    if (rc == SQLITE_OK) {
        sql = sqlite3_mprintf(SQL_DISTRICT, code, code );
        rc = exec_sql_1row (db, sql, &counter, 1);
        sqlite3_free(sql);
    }
    // town
    if (rc == SQLITE_OK) {
        sql = sqlite3_mprintf(SQL_TOWN, code, code );
        rc = exec_sql_1row (db, sql, &counter, 1);
        sqlite3_free(sql);
    }
    // point
    if (rc == SQLITE_OK) {
        sql = sqlite3_mprintf(SQL_POINT, code, code );
        rc = exec_sql_1row (db, sql, &counter, 1);
        sqlite3_free(sql);
    }
    // street
    if (rc == SQLITE_OK) {
        sql = sqlite3_mprintf(SQL_STREET, code, code );
        rc = exec_sql_1row (db, sql, &counter, 1);
        sqlite3_free(sql);
    }
    // doma
    if (rc == SQLITE_OK) {
        sql = sqlite3_mprintf(SQL_DOMA, code, code, code );
        rc = exec_sql_1row (db, sql, &counter, 1);
        sqlite3_free(sql);
    }
    // korp
    if (rc == SQLITE_OK) {
        sql = sqlite3_mprintf(SQL_KORP, code, code );
        rc = exec_sql_1row (db, sql, &counter, 1);
        sqlite3_free(sql);
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
