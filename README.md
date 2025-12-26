# KANEK FOUNDATION LIB (KFL)
Foundations library for Kanek Storage

This is a group of foundations functions maked for Kanek File system.
The next functionality would be covered. 

- Tracing and logging macros and libraries.
- Hashing
- Memory management/Garbage collection
- Fast 64bit random generator
- Configuration files parser
- Hexadecimal dumps
- Stack dump display for debugging


#### 1     Trace and Logging macros 
Trace facilities are just trace messages functionality, for debugging
and error analysis. Logging just registers in logs important, relevant 
notification messages. 
    
#### 2     Hashing for faster word searches and ordering. 
In order to make faster string searchs, a hash function which returns
an unsigned 64-bit integer has been implemented. The 64-bit value
matches string order, so the hash created by the word "cat" will be
always lesser than "dog", and both hashes will be lesser than "duck"
and "pig" respectively. 

#### 3     Garbage Collecting
This library allows to keep track of garbage collecting, to avoid
memory leaks. Possibly it can use the cache framework facilities.

#### 4     Fast random 64bit unsigned integers generator
Code for this does exist already
    

#### 5     A configuration files parser.
The parser should support BASH style comments. Also variables, assignations of 
Python style arrays, strings, and operators like '=', '+' and '+=".

Support for reserved words like "include" should be added.
When the parser finds this word, it should proceed to parse the
specified file, and once it completes that file parsing, continue with
the current file parsing. 

Also support for "display" reserved word should be added, and it
should display strings, variables and all. 

The dictionaries facilities descripted in 2.7 should be used for the 
parser. It should create a whole new dictionary with the key-values 
parsed from the file. 

Example of configuration file:
```
# this is a conf file

########
##
# some comments


VARIABLE=123
MY_SRING="hello world "
MY_ARRAY=[ 0, 1, 2, 3, "four", "five" ]

MY_VAR0 = MY_ARRAY[0] # should assign 0
MY_VARS = MY_ARRAY[4] # should assign "four"

############ notice the next operators should be supported
MY_NEW_STRING = MY_STRING + MY_VARS # it should be a concatenation, the value
                                    # "hello world four" is expected

MY_NEW_STRING += " six" # it should be a concatenation, and expected
                        # value is "hello world four six"

MY_NEW_STRING_AGAIN=MY_STRING+" "+"mars"+" "+VARIABLE # should be 
                                                      # "hello world mars 123"

BANNER2="GOFS FILE SYSTEM"

VERSION="1"


# MOST EXAMPLE LINES BELOW
# default configurations for logging
DATE_FORMAT="+%m%d%Y_%H%M%S"

# directories of product
PRODUCT_DIR="/opt/gofs/"
LOG_DIR=PRODUCT_DIR + "log/"
CONF_DIR = PRODUCT_DIR + "etc/"
```


#### 6     Hexadecimal dump facilities
Functions and code already exist for hexadecimal dumps of memory. 

#### 7    Stackdump functionalities
Functions and code for get the stackdump for debugging purposes is on plan. 


#### 8    Cache Framework
A simple cache framework is needed to support all the other caches will be 
added.

The simple cache framework design will provide generic flags for
operation, status and a simple posix thread, which will process the 
cache elements, running operations according with cache elements
flags. The cache data structure consists on:
- One list of elements 
- One list of dirty elements. 
- Cache operation and status Flags
- A thread ID
- A thread mutex
- Callbacks for the on_map(), on_evict(), on_flush().

Each element in the cache will have the next fields:
- 64bit ID
- thread mutex
- flags
- access count
- a pointer to the parent cache data structure.

The framework make a distintion between the cache data structure and
the cache element data structure. Both are two separated entities. 
The cache structure interface:
- Alloc a cache
- Init cache ( populate cache with default values)
- Cache disable (flush and evict cache elements, stop thread)
- Cache enable  ( start thread)
- Cache sync ( evict all the elements, except pinned elements)
- Cache pause (pauses thread)
- Cache unpause
- Cache wait for flags ( wait for an specific flag)
- Cache lookup ( look for an element)

Cache elements:
- Element map ( map an element into a cache)
- Mark for eviction
- Mark dirty
- element pin
- element unpin
- element wait for flags
- evict 

So, most of those functions will be exported, some of them may 
be reimplemented by other caches built above this library. 
