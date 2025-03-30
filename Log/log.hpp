#ifndef _XD_LOGE_H_
#define _XD_LOGE_H_

#include <iostream>
#include <string>
#include <sstream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include <deque>
#include <time.h>
#include <fstream>
#include <algorithm>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

//! LOGE Level
enum ENUM_LOGE_LEVEL
{
    LOGE_LEVEL_TRACE = 0,
    LOGE_LEVEL_DEBUG,
    LOGE_LEVEL_INFO,
    LOGE_LEVEL_WARN,
    LOGE_LEVEL_ERROR,
    LOGE_LEVEL_ALARM,
    LOGE_LEVEL_FATAL,
};


void LogeInit(ENUM_LOGE_LEVEL logelevel, const char *savepath);

/** 
LOGEFMTT("format input *** %s *** %d ***", "LOGEFMTT", 123456);
LOGEFMTD("format input *** %s *** %d ***", "LOGEFMTD", 123456);
LOGEFMTI("format input *** %s *** %d ***", "LOGEFMTI", 123456);
LOGEFMTW("format input *** %s *** %d ***", "LOGEFMTW", 123456);
LOGEFMTE("format input *** %s *** %d ***", "LOGEFMTE", 123456);
LOGEFMTA("format input *** %s *** %d ***", "LOGEFMTA", 123456);
LOGEFMTF("format input *** %s *** %d ***", "LOGEFMTF", 123456);

int value = 10;
string str = "test";
LOGET("stream input *** " << "LOGET LOGET LOGET LOGET" << " *** " << value);
LOGED("stream input *** " << "LOGED LOGED LOGED LOGED" << " *** " << str);
LOGEI("stream input *** " << "LOGEI LOGEI LOGEI LOGEI" << " *** ");
LOGEW("stream input *** " << "LOGEW LOGEW LOGEW LOGEW" << " *** ");
LOGEE("stream input *** " << "LOGEE LOGEE LOGEE LOGEE" << " *** ");
LOGEA("stream input *** " << "LOGEA LOGEA LOGEA LOGEA" << " *** ");
LOGEF("stream input *** " << "LOGEF LOGEF LOGEF LOGEF" << " *** ");
**/

//! logeger ID type. DO NOT TOUCH
typedef int LogegerId;

//! the invalid logeger id. DO NOT TOUCH
const int LOGE4Z_INVALID_LOGEGER_ID = -1;

//! the main logeger id. DO NOT TOUCH
//! can use this id to set the main logeger's attribute.
//! example:
//! ILoge4zManager::getPtr()->setLogegerLevel(LOGE4Z_MAIN_LOGEGER_ID, LOGE_LEVEL_WARN);
//! ILoge4zManager::getPtr()->setLogegerDisplay(LOGE4Z_MAIN_LOGEGER_ID, false);
const int LOGE4Z_MAIN_LOGEGER_ID = 0;

//! the main logeger name. DO NOT TOUCH
const char *const LOGE4Z_MAIN_LOGEGER_KEY = "Main";

//! check VC VERSION. DO NOT TOUCH
//! format micro cannot support VC6 or VS2003, please use stream input loge, like LOGEI, LOGED, LOGE_DEBUG, LOGE_STREAM ...
#if _MSC_VER >= 1400 // MSVC >= VS2005
#define LOGE4Z_FORMAT_INPUT_ENABLE
#endif

#ifndef WIN32
#define LOGE4Z_FORMAT_INPUT_ENABLE
#endif

//////////////////////////////////////////////////////////////////////////
//! -----------------default logeger config, can change on this.-----------
//////////////////////////////////////////////////////////////////////////
//! the max logeger count.
const int LOGE4Z_LOGEGER_MAX = 20;
//! the max loge content length.
const int LOGE4Z_LOGE_BUF_SIZE = 1024 * 8;
//! the max stl container depth.
const int LOGE4Z_LOGE_CONTAINER_DEPTH = 5;

//! all logeger synchronous output or not
const bool LOGE4Z_ALL_SYNCHRONOUS_OUTPUT = true;
//! all logeger synchronous display to the windows debug output
const bool LOGE4Z_ALL_DEBUGOUTPUT_DISPLAY = true;

//! default logeger output file.
const char *const LOGE4Z_DEFAULT_PATH = "./loge/";
//! default loge filter level
const int LOGE4Z_DEFAULT_LEVEL = LOGE_LEVEL_DEBUG;
//! default logeger display
const bool LOGE4Z_DEFAULT_DISPLAY = true;
//! default logeger output to file
const bool LOGE4Z_DEFAULT_OUTFILE = true;
//! default logeger month dir used status
const bool LOGE4Z_DEFAULT_MONTHDIR = true;
//! default logeger output file limit size, unit M byte.
const int LOGE4Z_DEFAULT_LIMITSIZE = 100;
//! default logeger show suffix (file name and line number)
const bool LOGE4Z_DEFAULT_SHOWSUFFIX = true;
//! support ANSI->OEM console conversion on Windows
#undef LOGE4Z_OEM_CONSOLE
///////////////////////////////////////////////////////////////////////////
//! -----------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////

#ifndef _ZSUMMER_E_BEGIN
#define _ZSUMMER_E_BEGIN \
    namespace zsummer_e  \
    {
#endif
#ifndef _ZSUMMER_E_LOGE4Z_BEGIN
#define _ZSUMMER_E_LOGE4Z_BEGIN \
    namespace loge4z            \
    {
#endif
_ZSUMMER_E_BEGIN
_ZSUMMER_E_LOGE4Z_BEGIN

struct LogeData
{
    LogegerId _id; // dest logeger id
    int _type;     // type.
    int _typeval;
    int _level;            // loge level
    time_t _time;          // create time
    unsigned int _precise; // create time
    int _contentLen;
    char _content[LOGE4Z_LOGE_BUF_SIZE]; // content
};

//! loge4z class
class ILoge4zManager
{
public:
    ILoge4zManager(){};
    virtual ~ILoge4zManager(){};

    //! Loge4z Singleton

    static ILoge4zManager *getInstance();
    inline static ILoge4zManager &getRef() { return *getInstance(); }
    inline static ILoge4zManager *getPtr() { return getInstance(); }

    //! Config or overwrite configure
    //! Needs to be called before ILoge4zManager::Start,, OR Do not call.
    virtual bool config(const char *configPath) = 0;
    virtual bool configFromString(const char *configContent) = 0;

    //! Create or overwrite logeger.
    //! Needs to be called before ILoge4zManager::Start, OR Do not call.
    virtual LogegerId createLogeger(const char *key) = 0;

    //! Start Loge Thread. This method can only be called once by one process.
    virtual bool start() = 0;

    //! Default the method will be calling at process exit auto.
    //! Default no need to call and no recommended.
    virtual bool stop() = 0;

    //! Find logeger. thread safe.
    virtual LogegerId findLogeger(const char *key) = 0;

    // pre-check the loge filter. if filter out return false.
    virtual bool prePushLoge(LogegerId id, int level) = 0;
    //! Push loge, thread safe.
    virtual bool pushLoge(LogeData *pLoge, const char *file = NULL, int line = 0) = 0;

    //! set logeger's attribute, thread safe.
    virtual bool enableLogeger(LogegerId id, bool enable) = 0; // immediately when enable, and queue up when disable.
    virtual bool setLogegerName(LogegerId id, const char *name) = 0;
    virtual bool setLogegerPath(LogegerId id, const char *path) = 0;
    virtual bool setLogegerLevel(LogegerId id, int nLevel) = 0; // immediately when enable, and queue up when disable.
    virtual bool setLogegerFileLine(LogegerId id, bool enable) = 0;
    virtual bool setLogegerDisplay(LogegerId id, bool enable) = 0;
    virtual bool setLogegerOutFile(LogegerId id, bool enable) = 0;
    virtual bool setLogegerLimitsize(LogegerId id, unsigned int limitsize) = 0;
    virtual bool setLogegerMonthdir(LogegerId id, bool enable) = 0;

    //! Update logeger's attribute from config file, thread safe.
    virtual bool setAutoUpdate(int interval /*per second, 0 is disable auto update*/) = 0;
    virtual bool updateConfig() = 0;

    //! Loge4z status statistics, thread safe.
    virtual bool isLogegerEnable(LogegerId id) = 0;
    virtual unsigned long long getStatusTotalWriteCount() = 0;
    virtual unsigned long long getStatusTotalWriteBytes() = 0;
    virtual unsigned long long getStatusWaitingCount() = 0;
    virtual unsigned int getStatusActiveLogegers() = 0;

    virtual LogeData *makeLogeData(LogegerId id, int level) = 0;
    virtual void freeLogeData(LogeData *loge) = 0;
};

class Loge4zStream;
class Loge4zBinary;

#ifndef _ZSUMMER_E_END
#define _ZSUMMER_E_END }
#endif
#ifndef _ZSUMMER_E_LOGE4Z_END
#define _ZSUMMER_E_LOGE4Z_END }
#endif

_ZSUMMER_E_LOGE4Z_END
_ZSUMMER_E_END

//! base macro.
#define LOGE_STREAM(id, level, file, line, loge)                                                                                 \
    do                                                                                                                           \
    {                                                                                                                            \
        if (zsummer_e::loge4z::ILoge4zManager::getPtr()->prePushLoge(id, level))                                                 \
        {                                                                                                                        \
            zsummer_e::loge4z::LogeData *pLoge = zsummer_e::loge4z::ILoge4zManager::getPtr()->makeLogeData(id, level);           \
            zsummer_e::loge4z::Loge4zStream ss(pLoge->_content + pLoge->_contentLen, LOGE4Z_LOGE_BUF_SIZE - pLoge->_contentLen); \
            ss << loge;                                                                                                          \
            pLoge->_contentLen += ss.getCurrentLen();                                                                            \
            zsummer_e::loge4z::ILoge4zManager::getPtr()->pushLoge(pLoge, file, line);                                            \
        }                                                                                                                        \
    } while (0)

//! fast macro
#define LOG_TRACE(id, loge) LOGE_STREAM(id, LOGE_LEVEL_TRACE, __FILE__, __LINE__, loge)
#define LOG_DEBUG(id, loge) LOGE_STREAM(id, LOGE_LEVEL_DEBUG, __FILE__, __LINE__, loge)
#define LOG_INFO(id, loge) LOGE_STREAM(id, LOGE_LEVEL_INFO, __FILE__, __LINE__, loge)
#define LOG_WARN(id, loge) LOGE_STREAM(id, LOGE_LEVEL_WARN, __FILE__, __LINE__, loge)
#define LOG_ERROR(id, loge) LOGE_STREAM(id, LOGE_LEVEL_ERROR, __FILE__, __LINE__, loge)
#define LOG_ALARM(id, loge) LOGE_STREAM(id, LOGE_LEVEL_ALARM, __FILE__, __LINE__, loge)
#define LOG_FATAL(id, loge) LOGE_STREAM(id, LOGE_LEVEL_FATAL, __FILE__, __LINE__, loge)

//! super macro.
#define LOGT(loge) LOG_TRACE(LOGE4Z_MAIN_LOGEGER_ID, loge)
#define LOGD(loge) LOG_DEBUG(LOGE4Z_MAIN_LOGEGER_ID, loge)
#define LOGI(loge) LOG_INFO(LOGE4Z_MAIN_LOGEGER_ID, loge)
#define LOGW(loge) LOG_WARN(LOGE4Z_MAIN_LOGEGER_ID, loge)
#define LOGE(loge) LOG_ERROR(LOGE4Z_MAIN_LOGEGER_ID, loge)
#define LOGA(loge) LOG_ALARM(LOGE4Z_MAIN_LOGEGER_ID, loge)
#define LOGF(loge) LOG_FATAL(LOGE4Z_MAIN_LOGEGER_ID, loge)

//! format input loge.
#ifdef LOGE4Z_FORMAT_INPUT_ENABLE
#ifdef WIN32
#define LOGE_FORMAT(id, level, file, line, logeformat, ...)                                                                                               \
    do                                                                                                                                                    \
    {                                                                                                                                                     \
        if (zsummer_e::loge4z::ILoge4zManager::getPtr()->prePushLoge(id, level))                                                                          \
        {                                                                                                                                                 \
            zsummer_e::loge4z::LogeData *pLoge = zsummer_e::loge4z::ILoge4zManager::getPtr()->makeLogeData(id, level);                                    \
            int len = _snprintf_s(pLoge->_content + pLoge->_contentLen, LOGE4Z_LOGE_BUF_SIZE - pLoge->_contentLen, _TRUNCATE, logeformat, ##__VA_ARGS__); \
            if (len < 0)                                                                                                                                  \
                len = LOGE4Z_LOGE_BUF_SIZE - pLoge->_contentLen;                                                                                          \
            pLoge->_contentLen += len;                                                                                                                    \
            zsummer_e::loge4z::ILoge4zManager::getPtr()->pushLoge(pLoge, file, line);                                                                     \
        }                                                                                                                                                 \
    } while (0)
#else
#define LOGE_FORMAT(id, level, file, line, logeformat, ...)                                                                                 \
    do                                                                                                                                      \
    {                                                                                                                                       \
        if (zsummer_e::loge4z::ILoge4zManager::getPtr()->prePushLoge(id, level))                                                            \
        {                                                                                                                                   \
            zsummer_e::loge4z::LogeData *pLoge = zsummer_e::loge4z::ILoge4zManager::getPtr()->makeLogeData(id, level);                      \
            int len = snprintf(pLoge->_content + pLoge->_contentLen, LOGE4Z_LOGE_BUF_SIZE - pLoge->_contentLen, logeformat, ##__VA_ARGS__); \
            if (len < 0)                                                                                                                    \
                len = 0;                                                                                                                    \
            if (len > LOGE4Z_LOGE_BUF_SIZE - pLoge->_contentLen)                                                                            \
                len = LOGE4Z_LOGE_BUF_SIZE - pLoge->_contentLen;                                                                            \
            pLoge->_contentLen += len;                                                                                                      \
            zsummer_e::loge4z::ILoge4zManager::getPtr()->pushLoge(pLoge, file, line);                                                       \
        }                                                                                                                                   \
    } while (0)
#endif
//! format string
#define LOGEFMT_TRACE(id, fmt, ...) LOGE_FORMAT(id, LOGE_LEVEL_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGEFMT_DEBUG(id, fmt, ...) LOGE_FORMAT(id, LOGE_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGEFMT_INFO(id, fmt, ...) LOGE_FORMAT(id, LOGE_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGEFMT_WARN(id, fmt, ...) LOGE_FORMAT(id, LOGE_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGEFMT_ERROR(id, fmt, ...) LOGE_FORMAT(id, LOGE_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGEFMT_ALARM(id, fmt, ...) LOGE_FORMAT(id, LOGE_LEVEL_ALARM, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGEFMT_FATAL(id, fmt, ...) LOGE_FORMAT(id, LOGE_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGEFMTT(fmt, ...) LOGEFMT_TRACE(LOGE4Z_MAIN_LOGEGER_ID, fmt, ##__VA_ARGS__)
#define LOGEFMTD(fmt, ...) LOGEFMT_DEBUG(LOGE4Z_MAIN_LOGEGER_ID, fmt, ##__VA_ARGS__)
#define LOGEFMTI(fmt, ...) LOGEFMT_INFO(LOGE4Z_MAIN_LOGEGER_ID, fmt, ##__VA_ARGS__)
#define LOGEFMTW(fmt, ...) LOGEFMT_WARN(LOGE4Z_MAIN_LOGEGER_ID, fmt, ##__VA_ARGS__)
#define LOGEFMTE(fmt, ...) LOGEFMT_ERROR(LOGE4Z_MAIN_LOGEGER_ID, fmt, ##__VA_ARGS__)
#define LOGEFMTA(fmt, ...) LOGEFMT_ALARM(LOGE4Z_MAIN_LOGEGER_ID, fmt, ##__VA_ARGS__)
#define LOGEFMTF(fmt, ...) LOGEFMT_FATAL(LOGE4Z_MAIN_LOGEGER_ID, fmt, ##__VA_ARGS__)
#else
inline void empty_loge_format_function1(LogegerId id, const char *, ...)
{
}
inline void empty_loge_format_function2(const char *, ...) {}
#define LOGEFMT_TRACE empty_loge_format_function1
#define LOGEFMT_DEBUG LOGEFMT_TRACE
#define LOGEFMT_INFO LOGEFMT_TRACE
#define LOGEFMT_WARN LOGEFMT_TRACE
#define LOGEFMT_ERROR LOGEFMT_TRACE
#define LOGEFMT_ALARM LOGEFMT_TRACE
#define LOGEFMT_FATAL LOGEFMT_TRACE
#define LOGEFMTT empty_loge_format_function2
#define LOGEFMTD LOGEFMTT
#define LOGEFMTI LOGEFMTT
#define LOGEFMTW LOGEFMTT
#define LOGEFMTE LOGEFMTT
#define LOGEFMTA LOGEFMTT
#define LOGEFMTF LOGEFMTT
#endif

_ZSUMMER_E_BEGIN
_ZSUMMER_E_LOGE4Z_BEGIN

//! optimze from std::stringstream to Loge4zStream
#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
class Loge4zBinary
{
public:
    Loge4zBinary(const char *buf, int len)
    {
        _buf = buf;
        _len = len;
    }
    const char *_buf;
    int _len;
};
class Loge4zStream
{
public:
    inline Loge4zStream(char *buf, int len);
    inline int getCurrentLen() { return (int)(_cur - _begin); }

private:
    template <class T>
    inline Loge4zStream &writeData(const char *ft, T t);
    inline Loge4zStream &writeLongLong(long long t);
    inline Loge4zStream &writeULongLong(unsigned long long t);
    inline Loge4zStream &writePointer(const void *t);
    inline Loge4zStream &writeString(const char *t, size_t len);
    inline Loge4zStream &writeWString(const wchar_t *t);
    inline Loge4zStream &writeBinary(const Loge4zBinary &t);

public:
    inline Loge4zStream &operator<<(const void *t) { return writePointer(t); }

    inline Loge4zStream &operator<<(const char *t) { return writeString(t, strlen(t)); }
#ifdef WIN32
    inline Loge4zStream &operator<<(const wchar_t *t)
    {
        return writeWString(t);
    }
#endif
    inline Loge4zStream &operator<<(bool t)
    {
        return (t ? writeData("%s", "true") : writeData("%s", "false"));
    }

    inline Loge4zStream &operator<<(char t) { return writeData("%c", t); }

    inline Loge4zStream &operator<<(unsigned char t) { return writeData("%u", (unsigned int)t); }

    inline Loge4zStream &operator<<(short t) { return writeData("%d", (int)t); }

    inline Loge4zStream &operator<<(unsigned short t) { return writeData("%u", (unsigned int)t); }

    inline Loge4zStream &operator<<(int t) { return writeData("%d", t); }

    inline Loge4zStream &operator<<(unsigned int t) { return writeData("%u", t); }

    inline Loge4zStream &operator<<(long t) { return writeLongLong(t); }

    inline Loge4zStream &operator<<(unsigned long t) { return writeULongLong(t); }

    inline Loge4zStream &operator<<(long long t) { return writeLongLong(t); }

    inline Loge4zStream &operator<<(unsigned long long t) { return writeULongLong(t); }

    inline Loge4zStream &operator<<(float t) { return writeData("%.4f", t); }

    inline Loge4zStream &operator<<(double t) { return writeData("%.4lf", t); }

    template <class _Elem, class _Traits, class _Alloc> // support std::string, std::wstring
    inline Loge4zStream &operator<<(const std::basic_string<_Elem, _Traits, _Alloc> &t)
    {
        return writeString(t.c_str(), t.length());
    }

    inline Loge4zStream &operator<<(const zsummer_e::loge4z::Loge4zBinary &binary) { return writeBinary(binary); }

    template <class _Ty1, class _Ty2>
    inline Loge4zStream &operator<<(const std::pair<_Ty1, _Ty2> &t) { return *this << "pair(" << t.first << ":" << t.second << ")"; }

    template <class _Elem, class _Alloc>
    inline Loge4zStream &operator<<(const std::vector<_Elem, _Alloc> &t)
    {
        *this << "vector(" << t.size() << ")[";
        int inputCount = 0;
        for (typename std::vector<_Elem, _Alloc>::const_iterator iter = t.begin(); iter != t.end(); iter++)
        {
            inputCount++;
            if (inputCount > LOGE4Z_LOGE_CONTAINER_DEPTH)
            {
                *this << "..., ";
                break;
            }
            *this << *iter << ", ";
        }
        if (!t.empty())
        {
            _cur -= 2;
        }
        return *this << "]";
    }
    template <class _Elem, class _Alloc>
    inline Loge4zStream &operator<<(const std::list<_Elem, _Alloc> &t)
    {
        *this << "list(" << t.size() << ")[";
        int inputCount = 0;
        for (typename std::list<_Elem, _Alloc>::const_iterator iter = t.begin(); iter != t.end(); iter++)
        {
            inputCount++;
            if (inputCount > LOGE4Z_LOGE_CONTAINER_DEPTH)
            {
                *this << "..., ";
                break;
            }
            *this << *iter << ", ";
        }
        if (!t.empty())
        {
            _cur -= 2;
        }
        return *this << "]";
    }
    template <class _Elem, class _Alloc>
    inline Loge4zStream &operator<<(const std::deque<_Elem, _Alloc> &t)
    {
        *this << "deque(" << t.size() << ")[";
        int inputCount = 0;
        for (typename std::deque<_Elem, _Alloc>::const_iterator iter = t.begin(); iter != t.end(); iter++)
        {
            inputCount++;
            if (inputCount > LOGE4Z_LOGE_CONTAINER_DEPTH)
            {
                *this << "..., ";
                break;
            }
            *this << *iter << ", ";
        }
        if (!t.empty())
        {
            _cur -= 2;
        }
        return *this << "]";
    }
    template <class _Elem, class _Alloc>
    inline Loge4zStream &operator<<(const std::queue<_Elem, _Alloc> &t)
    {
        *this << "queue(" << t.size() << ")[";
        int inputCount = 0;
        for (typename std::queue<_Elem, _Alloc>::const_iterator iter = t.begin(); iter != t.end(); iter++)
        {
            inputCount++;
            if (inputCount > LOGE4Z_LOGE_CONTAINER_DEPTH)
            {
                *this << "..., ";
                break;
            }
            *this << *iter << ", ";
        }
        if (!t.empty())
        {
            _cur -= 2;
        }
        return *this << "]";
    }
    template <class _K, class _V, class _Pr, class _Alloc>
    inline Loge4zStream &operator<<(const std::map<_K, _V, _Pr, _Alloc> &t)
    {
        *this << "map(" << t.size() << ")[";
        int inputCount = 0;
        for (typename std::map<_K, _V, _Pr, _Alloc>::const_iterator iter = t.begin(); iter != t.end(); iter++)
        {
            inputCount++;
            if (inputCount > LOGE4Z_LOGE_CONTAINER_DEPTH)
            {
                *this << "..., ";
                break;
            }
            *this << *iter << ", ";
        }
        if (!t.empty())
        {
            _cur -= 2;
        }
        return *this << "]";
    }

private:
    Loge4zStream() {}
    Loge4zStream(Loge4zStream &) {}
    char *_begin;
    char *_end;
    char *_cur;
};

inline Loge4zStream::Loge4zStream(char *buf, int len)
{
    _begin = buf;
    _end = buf + len;
    _cur = _begin;
}

template <class T>
inline Loge4zStream &Loge4zStream::writeData(const char *ft, T t)
{
    if (_cur < _end)
    {
        int len = 0;
        int count = (int)(_end - _cur);
#ifdef WIN32
        len = _snprintf(_cur, count, ft, t);
        if (len == count || len < 0)
        {
            len = count;
            *(_end - 1) = '\0';
        }
#else
        len = snprintf(_cur, count, ft, t);
        if (len < 0)
        {
            *_cur = '\0';
            len = 0;
        }
        else if (len >= count)
        {
            len = count;
            *(_end - 1) = '\0';
        }
#endif
        _cur += len;
    }
    return *this;
}

inline Loge4zStream &Loge4zStream::writeLongLong(long long t)
{
#ifdef WIN32
    writeData("%I64d", t);
#else
    writeData("%lld", t);
#endif
    return *this;
}

inline Loge4zStream &Loge4zStream::writeULongLong(unsigned long long t)
{
#ifdef WIN32
    writeData("%I64u", t);
#else
    writeData("%llu", t);
#endif
    return *this;
}

inline Loge4zStream &Loge4zStream::writePointer(const void *t)
{
#ifdef WIN32
    sizeof(t) == 8 ? writeData("%016I64x", (unsigned long long)t) : writeData("%08I64x", (unsigned long long)t);
#else
    sizeof(t) == 8 ? writeData("%016llx", (unsigned long long)t) : writeData("%08llx", (unsigned long long)t);
#endif
    return *this;
}

inline Loge4zStream &Loge4zStream::writeBinary(const Loge4zBinary &t)
{
    writeData("%s", "\r\n\t[");
    for (int i = 0; i < t._len; i++)
    {
        if (i % 16 == 0)
        {
            writeData("%s", "\r\n\t");
            *this << (void *)(t._buf + i);
            writeData("%s", ": ");
        }
        writeData("%02x ", (unsigned char)t._buf[i]);
    }
    writeData("%s", "\r\n\t]\r\n\t");
    return *this;
}
inline Loge4zStream &zsummer_e::loge4z::Loge4zStream::writeString(const char *t, size_t len)
{
    if (_cur < _end)
    {
        size_t count = (size_t)(_end - _cur);
        if (len > count)
        {
            len = count;
        }
        memcpy(_cur, t, len);
        _cur += len;
        if (_cur >= _end - 1)
        {
            *(_end - 1) = '\0';
        }
        else
        {
            *(_cur + 1) = '\0';
        }
    }
    return *this;
}
inline zsummer_e::loge4z::Loge4zStream &zsummer_e::loge4z::Loge4zStream::writeWString(const wchar_t *t)
{
#ifdef WIN32
    DWORD dwLen = WideCharToMultiByte(CP_ACP, 0, t, -1, NULL, 0, NULL, NULL);
    if (dwLen < LOGE4Z_LOGE_BUF_SIZE)
    {
        std::string str;
        str.resize(dwLen, '\0');
        dwLen = WideCharToMultiByte(CP_ACP, 0, t, -1, &str[0], dwLen, NULL, NULL);
        if (dwLen > 0)
        {
            writeData("%s", str.c_str());
        }
    }
#else
    // not support
    (void)t;
#endif
    return *this;
}

#ifdef WIN32
#pragma warning(pop)
#endif

_ZSUMMER_E_LOGE4Z_END
_ZSUMMER_E_END

#endif