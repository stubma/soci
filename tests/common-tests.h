//
// Copyright (C) 2004-2008 Maciej Sobczak, Stephen Hutton
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SOCI_COMMON_TESTS_H_INCLUDED
#define SOCI_COMMON_TESTS_H_INCLUDED

#include "soci/soci.h"

#ifdef SOCI_HAVE_BOOST
// explicitly pull conversions for Boost's optional, tuple and fusion:
#include <boost/version.hpp>
#include "soci/boost-optional.h"
#include "soci/boost-tuple.h"
#include "soci/boost-gregorian-date.h"
#if defined(BOOST_VERSION) && BOOST_VERSION >= 103500
#include "soci/boost-fusion.h"
#endif // BOOST_VERSION
#endif // SOCI_HAVE_BOOST
#include "soci/mysql/soci-mysql.h"
#include "soci-compiler.h"

#define CATCH_CONFIG_RUNNER
#include <catch.hpp>

#if defined(_MSC_VER) && (_MSC_VER < 1500)
#undef SECTION
#define SECTION(name) INTERNAL_CATCH_SECTION(name, "dummy-for-vc8")
#endif

#include <algorithm>
#include <cassert>
#include <clocale>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <typeinfo>

// Although SQL standard mandates right padding CHAR(N) values to their length
// with spaces, some backends don't confirm to it:
//
//  - Firebird does pad the string but to the byte-size (not character size) of
//  the column (i.e. CHAR(10) NONE is padded to 10 bytes but CHAR(10) UTF8 --
//  to 40).
//  - For MySql PAD_CHAR_TO_FULL_LENGTH option must be set, otherwise the value
//  is trimmed.
//  - SQLite never behaves correctly at all.
//
// This method will check result string from column defined as fixed char It
// will check only bytes up to the original string size. If padded string is
// bigger than expected string then all remaining chars must be spaces so if
// any non-space character is found it will fail.
void
checkEqualPadded(const std::string& padded_str, const std::string& expected_str)
{
    size_t const len = expected_str.length();
    std::string const start_str(padded_str, 0, len);

    if (start_str != expected_str)
    {
        throw soci::soci_error(
                "Expected string \"" + expected_str + "\" "
                "is different from the padded string \"" + padded_str + "\""
              );
    }

    if (padded_str.length() > len)
    {
        std::string const end_str(padded_str, len);
        if (end_str != std::string(padded_str.length() - len, ' '))
        {
            throw soci::soci_error(
                  "\"" + padded_str + "\" starts with \"" + padded_str +
                  "\" but non-space characater(s) are found aftewards"
                );
        }
    }
}

#define CHECK_EQUAL_PADDED(padded_str, expected_str) \
    CHECK_NOTHROW(checkEqualPadded(padded_str, expected_str));

// Objects used later in tests 14,15
struct PhonebookEntry
{
    std::string name;
    std::string phone;
};

struct PhonebookEntry2 : public PhonebookEntry
{
};

class PhonebookEntry3
{
public:
    void setName(std::string const & n) { name_ = n; }
    std::string getName() const { return name_; }

    void setPhone(std::string const & p) { phone_ = p; }
    std::string getPhone() const { return phone_; }

public:
    std::string name_;
    std::string phone_;
};

// user-defined object for test26 and test28
class MyInt
{
public:
    MyInt() : i_() {}
    MyInt(int i) : i_(i) {}
    void set(int i) { i_ = i; }
    int get() const { return i_; }
private:
    int i_;
};

namespace soci
{

// basic type conversion for user-defined type with single base value
template<> struct type_conversion<MyInt>
{
    typedef int base_type;

    static void from_base(int i, indicator ind, MyInt &mi)
    {
        if (ind == i_ok)
        {
            mi.set(i);
        }
    }

    static void to_base(MyInt const &mi, int &i, indicator &ind)
    {
        i = mi.get();
        ind = i_ok;
    }
};

// basic type conversion on many values (ORM)
template<> struct type_conversion<PhonebookEntry>
{
    typedef soci::values base_type;

    static void from_base(values const &v, indicator /* ind */, PhonebookEntry &pe)
    {
        // here we ignore the possibility the the whole object might be NULL
        pe.name = v.get<std::string>("NAME");
        pe.phone = v.get<std::string>("PHONE", "<NULL>");
    }

    static void to_base(PhonebookEntry const &pe, values &v, indicator &ind)
    {
        v.set("NAME", pe.name);
        v.set("PHONE", pe.phone, pe.phone.empty() ? i_null : i_ok);
        ind = i_ok;
    }
};

// type conversion which directly calls values::get_indicator()
template<> struct type_conversion<PhonebookEntry2>
{
    typedef soci::values base_type;

    static void from_base(values const &v, indicator /* ind */, PhonebookEntry2 &pe)
    {
        // here we ignore the possibility the the whole object might be NULL

        pe.name = v.get<std::string>("NAME");
        indicator ind = v.get_indicator("PHONE"); //another way to test for null
        pe.phone = ind == i_null ? "<NULL>" : v.get<std::string>("PHONE");
    }

    static void to_base(PhonebookEntry2 const &pe, values &v, indicator &ind)
    {
        v.set("NAME", pe.name);
        v.set("PHONE", pe.phone, pe.phone.empty() ? i_null : i_ok);
        ind = i_ok;
    }
};

template<> struct type_conversion<PhonebookEntry3>
{
    typedef soci::values base_type;

    static void from_base(values const &v, indicator /* ind */, PhonebookEntry3 &pe)
    {
        // here we ignore the possibility the the whole object might be NULL

        pe.setName(v.get<std::string>("NAME"));
        pe.setPhone(v.get<std::string>("PHONE", "<NULL>"));
    }

    static void to_base(PhonebookEntry3 const &pe, values &v, indicator &ind)
    {
        v.set("NAME", pe.getName());
        v.set("PHONE", pe.getPhone(), pe.getPhone().empty() ? i_null : i_ok);
        ind = i_ok;
    }
};

} // namespace soci

namespace soci
{
namespace tests
{

// TODO: improve cleanup capabilities by subtypes, soci_test name may be omitted --mloskot
//       i.e. optional ctor param accepting custom table name
class table_creator_base
{
public:
    table_creator_base(session& sql)
        : msession(sql) { drop(); }

    virtual ~table_creator_base() { drop();}
private:
    void drop()
    {
        try
        {
            msession << "drop table soci_test";
        }
        catch (soci_error const& e)
        {
            //std::cerr << e.what() << std::endl;
            e.what();
        }
    }
    session& msession;

    SOCI_NOT_COPYABLE(table_creator_base)
};

class procedure_creator_base
{
public:
    procedure_creator_base(session& sql)
        : msession(sql) { drop(); }

    virtual ~procedure_creator_base() { drop();}
private:
    void drop()
    {
        try { msession << "drop procedure soci_test"; } catch (soci_error&) {}
    }
    session& msession;

    SOCI_NOT_COPYABLE(procedure_creator_base)
};

class function_creator_base
{
public:
    function_creator_base(session& sql)
        : msession(sql) { drop(); }

    virtual ~function_creator_base() { drop();}

protected:
    virtual std::string dropstatement()
    {
        return "drop function soci_test";
    }

private:
    void drop()
    {
        try { msession << dropstatement(); } catch (soci_error&) {}
    }
    session& msession;

    SOCI_NOT_COPYABLE(function_creator_base)
};

// This is a singleton class, at any given time there is at most one test
// context alive and common_tests fixture class uses it.
class test_context_base
{
public:
    test_context_base(backend_factory const &backEnd,
                    std::string const &connectString)
        : backEndFactory_(backEnd),
          connectString_(connectString)
    {
        // This can't be a CHECK() because the test context is constructed
        // outside of any test.
        assert(!the_test_context_);

        the_test_context_ = this;

        // To allow running tests in non-default ("C") locale, the following
        // environment variable can be set and then the current default locale
        // (which can itself be changed by setting LC_ALL environment variable)
        // will then be used.
        if (std::getenv("SOCI_TEST_USE_LC_ALL"))
            std::setlocale(LC_ALL, "");
    }

    static test_context_base const& get_instance()
    {
        REQUIRE(the_test_context_);

        return *the_test_context_;
    }

    backend_factory const & get_backend_factory() const
    {
        return backEndFactory_;
    }

    std::string get_connect_string() const
    {
        return connectString_;
    }

    virtual std::string to_date_time(std::string const &dateTime) const = 0;

    virtual table_creator_base* table_creator_1(session&) const = 0;
    virtual table_creator_base* table_creator_2(session&) const = 0;
    virtual table_creator_base* table_creator_3(session&) const = 0;
    virtual table_creator_base* table_creator_4(session&) const = 0;

    // Override this to return the table creator for a simple table containing
    // an integer "id" column and CLOB "s" one.
    //
    // Returns null by default to indicate that CLOB is not supported.
    virtual table_creator_base* table_creator_clob(session&) const { return NULL; }

    // Override this to return the table creator for a simple table containing
    // an integer "id" column and XML "x" one.
    //
    // Returns null by default to indicate that XML is not supported.
    virtual table_creator_base* table_creator_xml(session&) const { return NULL; }

    // Return the casts that must be used to convert the between the database
    // XML type and the query parameters.
    //
    // By default no special casts are done.
    virtual std::string to_xml(std::string const& x) const { return x; }
    virtual std::string from_xml(std::string const& x) const { return x; }

    // Override this if the backend not only supports working with XML values
    // (and so returns a non-null value from table_creator_xml()), but the
    // database itself has real XML support instead of just allowing to store
    // and retrieve XML as text. "Real" support means at least preventing the
    // application from storing malformed XML in the database.
    virtual bool has_real_xml_support() const { return false; }

    // Override this if the backend doesn't handle floating point values
    // correctly, i.e. writing a value and reading it back doesn't return
    // *exactly* the same value.
    virtual bool has_fp_bug() const { return false; }

    // Override this if the backend doesn't handle multiple active select
    // statements at the same time, i.e. a result set must be entirely consumed
    // before creating a new one (this is the case of MS SQL without MARS).
    virtual bool has_multiple_select_bug() const { return false; }

    // Override this if the backend may not have transactions support.
    virtual bool has_transactions_support(session&) const { return true; }

    // Override this if the backend silently truncates string values too long
    // to fit by default.
    virtual bool has_silent_truncate_bug(session&) const { return false; }

    // Override this to call commit() if it's necessary for the DDL statements
    // to be taken into account (currently this is only the case for Firebird).
    virtual void on_after_ddl(session&) const { }

    // Put the database in SQL-complient mode for CHAR(N) values, return false
    // if it's impossible, i.e. if the database doesn't behave correctly
    // whatever we do.
    virtual bool enable_std_char_padding(session&) const { return true; }

    // Return the SQL expression giving the length of the specified string,
    // i.e. "char_length(s)" in standard SQL but often "len(s)" or "length(s)"
    // in practice and sometimes even worse (thanks Oracle).
    virtual std::string sql_length(std::string const& s) const = 0;

    virtual ~test_context_base()
    {
        the_test_context_ = NULL;
    }

private:
    backend_factory const &backEndFactory_;
    std::string const connectString_;

    static test_context_base* the_test_context_;

    SOCI_NOT_COPYABLE(test_context_base)
};

// Currently all tests consist of just a single source file, so we can define
// this member here because this header is included exactly once.
tests::test_context_base* tests::test_context_base::the_test_context_ = NULL;


// Compare doubles for approximate equality. This has to be used everywhere
// where we write "3.14" (or "6.28") to the database as a string and then
// compare the value read back with the literal 3.14 floating point constant
// because they are not the same.
//
// It is also used for the backends which currently don't handle doubles
// correctly.
//
// Notice that this function is normally not used directly but rather from the
// macro below.
inline bool are_doubles_approx_equal(double const a, double const b)
{
    // The formula taken from CATCH test framework
    // https://github.com/philsquared/Catch/
    // Thanks to Richard Harris for his help refining this formula
    double const epsilon(std::numeric_limits<float>::epsilon() * 100);
    double const scale(1.0);
    return std::fabs(a - b) < epsilon * (scale + (std::max)(std::fabs(a), std::fabs(b)));
}

// This is a macro to ensure we use the correct line numbers. The weird
// do/while construction is used to make this a statement and the even weirder
// condition in while ensures that the loop is executed exactly once without
// triggering warnings from MSVC about the condition being always false.
#define ASSERT_EQUAL_APPROX(a, b) \
    do { \
      if (!are_doubles_approx_equal((a), (b))) { \
        FAIL( "Approximate equality check failed: " \
                  << std::fixed \
                  << std::setprecision(std::numeric_limits<double>::digits10 + 1) \
                  << (a) << " != " << (b) ); \
      } \
    } while ( (void)0, 0 )


// Exact double comparison function. We need one, instead of writing "a == b",
// only in order to have some place to put the pragmas disabling gcc warnings.
inline bool
are_doubles_exactly_equal(double a, double b)
{
    // Avoid g++ warnings: we do really want the exact equality here.
    GCC_WARNING_SUPPRESS(float-equal)

    return a == b;

    GCC_WARNING_RESTORE(float-equal)
}

#define ASSERT_EQUAL_EXACT(a, b) \
    do { \
      if (!are_doubles_exactly_equal((a), (b))) { \
        FAIL( "Exact equality check failed: " \
                  << std::fixed \
                  << std::setprecision(std::numeric_limits<double>::digits10 + 1) \
                  << (a) << " != " << (b) ); \
      } \
    } while ( (void)0, 0 )


// Compare two floating point numbers either exactly or approximately depending
// on test_context::has_fp_bug() return value.
inline bool
are_doubles_equal(test_context_base const& tc, double a, double b)
{
    return tc.has_fp_bug()
                ? are_doubles_approx_equal(a, b)
                : are_doubles_exactly_equal(a, b);
}

// This macro should be used when where we don't have any problems with string
// literals vs floating point literals mismatches described above and would
// ideally compare the numbers exactly but, unfortunately, currently can't do
// this unconditionally because at least some backends are currently buggy and
// don't handle the floating point values correctly.
//
// This can be only used from inside the common_tests class as it relies on
// having an accessible "tc_" variable to determine whether exact or
// approximate comparison should be used.
#define ASSERT_EQUAL(a, b) \
    do { \
      if (!are_doubles_equal(tc_, (a), (b))) { \
        FAIL( "Equality check failed: " \
                  << std::fixed \
                  << std::setprecision(std::numeric_limits<double>::digits10 + 1) \
                  << (a) << " != " << (b) ); \
      } \
    } while ( (void)0, 0 )


class common_tests
{
public:
    common_tests()
    : tc_(test_context_base::get_instance()),
      backEndFactory_(tc_.get_backend_factory()),
      connectString_(tc_.get_connect_string())
    {}

protected:
    test_context_base const & tc_;
    backend_factory const &backEndFactory_;
    std::string const connectString_;

    SOCI_NOT_COPYABLE(common_tests)
};

typedef cxx_details::auto_ptr<table_creator_base> auto_table_creator;

// Define the test cases in their own namespace to avoid clashes with the test
// cases defined in individual backend tests: as only line number is used for
// building the name of the "anonymous" function by the TEST_CASE macro, we
// could have a conflict between a test defined here and in some backend if
// they happened to start on the same line.
namespace test_cases
{


#define SL(s) (s), (unsigned long)strlen((s))

typedef struct {
    char str[64];
    char indicator1;
    unsigned long len;
} blk_row;
blk_row rows[100000];
MYSQL_BIND bind[1];

int check_stmt_rc(int rc, MYSQL_STMT* stmt) {
    if (rc)
    {
      printf("Error: %s", mysql_stmt_error(stmt));
      return 1;
    } else {
        return 0;
    }
}

void* get_backend(soci::session& s) {
    soci::details::session_backend* backend = s.get_backend();
    soci::mysql_session_backend* mysql_backend = (soci::mysql_session_backend*)backend;
    return mysql_backend->conn_;
}

std::string format(const char * const zcFormat, ...) {
    // initialize use of the variable argument array
    va_list vaArgs;
    va_start(vaArgs, zcFormat);

    // reliably acquire the size
    // from a copy of the variable argument array
    // and a functionally reliable call to mock the formatting
    va_list vaArgsCopy;
    va_copy(vaArgsCopy, vaArgs);
    const int iLen = std::vsnprintf(NULL, 0, zcFormat, vaArgsCopy);
    va_end(vaArgsCopy);

    // return a formatted string without risking memory mismanagement
    // and without assuming any compiler or platform specific behavior
    std::vector<char> zc(iLen + 1);
    std::vsnprintf(zc.data(), zc.size(), zcFormat, vaArgs);
    va_end(vaArgs);
    return std::string(zc.data(), iLen);
}

// use vector elements
TEST_CASE_METHOD(common_tests, "Use vector", "[core][use][vector]")
{
    soci::session sql(backEndFactory_, connectString_);
    MYSQL* mysql = (MYSQL*)get_backend(sql);
    mysql_set_server_option(mysql, MYSQL_OPTION_MULTI_STATEMENTS_ON);

    auto_table_creator tableCreator(tc_.table_creator_1(sql));

    int array_size= 100000;
    SECTION("char")
    {
        std::stringstream buf;
        for(int i = 0; i < array_size; i++) {
            buf << format("insert into soci_test(str) values('fdfdsfdfd%d');", i);
        }

        struct timeval tp;
        gettimeofday(&tp, NULL);
        int64_t start = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        sql << buf.str();

        gettimeofday(&tp, NULL);
        int64_t end = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        printf("insert by sql cost %lldms\n", end - start);
    }

    SECTION("char")
    {
        std::stringstream buf;
        buf << "insert into soci_test(str) values ";
        for(int i = 0; i < array_size; i++) {
            buf << format("('fdfdsfdfd%d')", i);
            if(i < array_size - 1) {
                buf << ",";
            } else {
                buf << ";";
            }
        }

        struct timeval tp;
        gettimeofday(&tp, NULL);
        int64_t start = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        sql << buf.str();

        gettimeofday(&tp, NULL);
        int64_t end = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        printf("insert by multi value sql cost %lldms\n", end - start);
    }

    SECTION("std::string")
    {
        for(int i = 0; i < array_size; i++) {
            strcpy(rows[i].str, format("fisdfffff%d", i).c_str());
            rows[i].len = strlen(rows[i].str);
        }

        struct timeval tp;
        gettimeofday(&tp, NULL);
        int64_t start = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        size_t row_size = sizeof(blk_row);
        const char *stmt_str= "insert into soci_test(str) values(?);";
        MYSQL_STMT *stmt = mysql_stmt_init(mysql);
        int rc = mysql_stmt_prepare(stmt, SL(stmt_str));
        rc= mysql_stmt_attr_set(stmt, STMT_ATTR_ARRAY_SIZE, &array_size);
        check_stmt_rc(rc, stmt);
        rc= mysql_stmt_attr_set(stmt, STMT_ATTR_ROW_SIZE, &row_size);
        check_stmt_rc(rc, stmt);

        memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = rows[0].str;
        bind[0].length = &rows[0].len;

        rc= mysql_stmt_bind_param(stmt, bind);
        check_stmt_rc(rc, stmt);

        rc= mysql_stmt_execute(stmt);
        check_stmt_rc(rc, stmt);

        mysql_stmt_close(stmt);

        // std::vector<std::string> v2(4);

        // sql << "select str from soci_test order by str", into(v2);
        // CHECK(v2.size() == 3);
        // CHECK(v2[0] == "ala");
        // CHECK(v2[1] == "kota");
        // CHECK(v2[2] == "ma");

        gettimeofday(&tp, NULL);
        int64_t end = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        printf("insert by bind cost %lldms\n", end - start);
    }

    SECTION("short")
    {
        std::vector<std::string> v;
        for(int i = 0; i < 1000; i++) {
            v.push_back(format("fsdfdfsdf%d", i));
        }

        struct timeval tp;
        gettimeofday(&tp, NULL);
        int64_t start = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        statement st6 = (sql.prepare << "insert into soci_test(str) values(:s)", use(v));
        st6.execute(true);

        gettimeofday(&tp, NULL);
        int64_t end = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        printf("insert by statement cost %lldms\n", end - start);
    }

    SECTION("int")
    {
        std::vector<int> v;
        v.push_back(-2000000000);
        v.push_back(0);
        v.push_back(1);
        v.push_back(2000000000);

        sql << "insert into soci_test(id) values(:i)", use(v);

        std::vector<int> v2(4);

        sql << "select id from soci_test order by id", into(v2);
        CHECK(v2.size() == 4);
        CHECK(v2[0] == -2000000000);
        CHECK(v2[1] == 0);
        CHECK(v2[2] == 1);
        CHECK(v2[3] == 2000000000);
    }

    SECTION("unsigned int")
    {
        std::vector<unsigned int> v;
        v.push_back(0);
        v.push_back(1);
        v.push_back(123);
        v.push_back(1000);

        sql << "insert into soci_test(ul) values(:ul)", use(v);

        std::vector<unsigned int> v2(4);

        sql << "select ul from soci_test order by ul", into(v2);
        CHECK(v2.size() == 4);
        CHECK(v2[0] == 0);
        CHECK(v2[1] == 1);
        CHECK(v2[2] == 123);
        CHECK(v2[3] == 1000);
    }

    SECTION("double")
    {
        std::vector<double> v;
        v.push_back(0);
        v.push_back(-0.0001);
        v.push_back(0.0001);
        v.push_back(3.1415926);

        sql << "insert into soci_test(d) values(:d)", use(v);

        std::vector<double> v2(4);

        sql << "select d from soci_test order by d", into(v2);
        CHECK(v2.size() == 4);
        ASSERT_EQUAL(v2[0],-0.0001);
        ASSERT_EQUAL(v2[1], 0);
        ASSERT_EQUAL(v2[2], 0.0001);
        ASSERT_EQUAL(v2[3], 3.1415926);
    }

    SECTION("std::tm")
    {
        std::vector<std::tm> v;
        std::tm t = std::tm();
        t.tm_year = 105;
        t.tm_mon  = 10;
        t.tm_mday = 26;
        t.tm_hour = 22;
        t.tm_min  = 45;
        t.tm_sec  = 17;

        v.push_back(t);

        t.tm_sec = 37;
        v.push_back(t);

        t.tm_mday = 25;
        v.push_back(t);

        sql << "insert into soci_test(tm) values(:t)", use(v);

        std::vector<std::tm> v2(4);

        sql << "select tm from soci_test order by tm", into(v2);
        CHECK(v2.size() == 3);
        CHECK(v2[0].tm_year == 105);
        CHECK(v2[0].tm_mon  == 10);
        CHECK(v2[0].tm_mday == 25);
        CHECK(v2[0].tm_hour == 22);
        CHECK(v2[0].tm_min  == 45);
        CHECK(v2[0].tm_sec  == 37);
        CHECK(v2[1].tm_year == 105);
        CHECK(v2[1].tm_mon  == 10);
        CHECK(v2[1].tm_mday == 26);
        CHECK(v2[1].tm_hour == 22);
        CHECK(v2[1].tm_min  == 45);
        CHECK(v2[1].tm_sec  == 17);
        CHECK(v2[2].tm_year == 105);
        CHECK(v2[2].tm_mon  == 10);
        CHECK(v2[2].tm_mday == 26);
        CHECK(v2[2].tm_hour == 22);
        CHECK(v2[2].tm_min  == 45);
        CHECK(v2[2].tm_sec  == 37);
    }

    SECTION("const int")
    {
        std::vector<int> v;
        v.push_back(-2000000000);
        v.push_back(0);
        v.push_back(1);
        v.push_back(2000000000);

        std::vector<int> const & cv = v;

        sql << "insert into soci_test(id) values(:i)", use(cv);

        std::vector<int> v2(4);

        sql << "select id from soci_test order by id", into(v2);
        CHECK(v2.size() == 4);
        CHECK(v2[0] == -2000000000);
        CHECK(v2[1] == 0);
        CHECK(v2[2] == 1);
        CHECK(v2[3] == 2000000000);
    }
}

} // namespace test_cases

} // namespace tests

} // namespace soci

#endif // SOCI_COMMON_TESTS_H_INCLUDED
