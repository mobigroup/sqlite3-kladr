# sqlite3-kladr
Utilities for SQLite3 KLADR (КЛАДР) address database for Russian Federation

# About

Provide utilities to search addresses and codes in SQLite3 KLADR (КЛАДР) address database.

# Compile

Build bedian package using debian/rules or compile as

```
gcc -g -O2 -std=gnu99 -lsqlite3 -o kladr-query kladr-query.c
gcc -g -O2 -std=gnu99 -lsqlite3 -o kladr-info kladr-info.c
```

# Usage

Address search:

```
$ kladr-query "" region вла
3300000000000 Владимирская обл

$ kladr-query 3300000000000 district ковров
3300800000000 Ковровский р-н

$ kladr-query 3300800000000 point ковров
3300800017800 Ковров-35 городок

$ kladr-query 3300800000000 town ковров
3300800100000 Ковров г
```

KLADR code search:

```
$ kladr-info 3300800000000
postalcode
postaltype district
region Владимирская
region_socr обл
district Ковровский
district_socr р-н
town
town_socr
point Ковровский
point_socr р-н
street
street_socr
doma
doma_socr
korp
korp_socr
```

Typical execution times:

```
$ time kladr-query 3300000000000 district ковров
3300800000000 Ковровский р-н
real 0m0.007s
user 0m0.004s
sys 0m0.004s

$ time kladr-query 3300000000000 district ковров
3300800000000 Ковровский р-н
real 0m0.007s
user 0m0.000s
sys 0m0.004s

$ time kladr-query 3300800000000 point ковров
3300800017800 Ковров-35 городок
real 0m0.005s
user 0m0.000s
sys 0m0.004s

$ time kladr-query 3300800000000 town ковров
3300800100000 Ковров г
real 0m0.004s
user 0m0.000s
sys 0m0.004s

$ time kladr-info 3300800000000
postalcode 
postaltype district
region Владимирская
region_socr обл
district Ковровский
district_socr р-н
town 
town_socr 
point Ковровский
point_socr р-н
street 
street_socr 
doma 
doma_socr 
korp 
korp_socr 

real 0m0.005s
user 0m0.004s
sys 0m0.000s
```

# History

The project moved from my own fossil repository.

