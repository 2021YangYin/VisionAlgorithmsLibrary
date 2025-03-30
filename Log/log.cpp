#include "log.hpp"

using namespace zsummer_e::loge4z;

void LogeInit(ENUM_LOGE_LEVEL logelevel, const char *savepath)
{
    ILoge4zManager::getRef().setLogegerDisplay(LOGE4Z_MAIN_LOGEGER_ID, true);
    ILoge4zManager::getRef().setLogegerLevel(LOGE4Z_MAIN_LOGEGER_ID, logelevel);
    ILoge4zManager::getRef().setLogegerFileLine(LOGE4Z_MAIN_LOGEGER_ID, true);
    ILoge4zManager::getRef().setLogegerOutFile(LOGE4Z_MAIN_LOGEGER_ID, true);
    ILoge4zManager::getRef().setLogegerPath(LOGE4Z_MAIN_LOGEGER_ID, savepath);
    ILoge4zManager::getRef().setAutoUpdate(10);
    ILoge4zManager::getRef().start();
    return;
}

#ifdef _WIN32
#include <io.h>
#include <shlwapi.h>
#include <process.h>
#pragma comment(lib, "shlwapi")
#pragma comment(lib, "User32.lib")
#pragma warning(disable : 4996)
#else
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <semaphore.h>
#endif

#ifdef __APPLE__
#include "TargetConditionals.h"
#include <dispatch/dispatch.h>
#if !TARGET_OS_IPHONE
#define LOGE4Z_HAVE_LIBPROC
#include <libproc.h>
#endif
#endif

_ZSUMMER_E_BEGIN
_ZSUMMER_E_LOGE4Z_BEGIN

static const char *const LOGE_STRING[] =
    {
        "LOG_TRACE",
        "LOG_DEBUG",
        "LOG_INFO ",
        "LOG_WARN ",
        "LOG_ERROR",
        "LOG_ALARM",
        "LOG_FATAL",
};

#ifdef WIN32
const static WORD LOGE_COLOR[LOGE_LEVEL_FATAL + 1] = {
    0,
    0,
    FOREGROUND_BLUE | FOREGROUND_GREEN,
    FOREGROUND_GREEN | FOREGROUND_RED,
    FOREGROUND_RED,
    FOREGROUND_GREEN,
    FOREGROUND_RED | FOREGROUND_BLUE};
#else

const static char LOGE_COLOR[LOGE_LEVEL_FATAL + 1][50] = {
    "\033[0m",
    "\033[0m",
    "\033[34m\033[1m", // hight blue
    "\033[33m",      // yellow
    "\033[31m",      // red
    "\033[32m",      // green
    "\033[35m"};
#endif

//////////////////////////////////////////////////////////////////////////
//! Loge4zFileHandler
//////////////////////////////////////////////////////////////////////////
class Loge4zFileHandler
{
public:
    Loge4zFileHandler() { _file = NULL; }
    ~Loge4zFileHandler() { close(); }
    inline bool isOpen() { return _file != NULL; }
    inline bool open(const char *path, const char *mod)
    {
        if (_file != NULL)
        {
            fclose(_file);
            _file = NULL;
        }
        _file = fopen(path, mod);
        return _file != NULL;
    }
    inline void close()
    {
        if (_file != NULL)
        {
            fclose(_file);
            _file = NULL;
        }
    }
    inline void write(const char *data, size_t len)
    {
        if (_file && len > 0)
        {
            if (fwrite(data, 1, len, _file) != len)
            {
                close();
            }
        }
    }
    inline void flush()
    {
        if (_file)
            fflush(_file);
    }

    inline std::string readLine()
    {
        char buf[500] = {0};
        if (_file && fgets(buf, 500, _file) != NULL)
        {
            return std::string(buf);
        }
        return std::string();
    }
    inline const std::string readContent();

public:
    FILE *_file;
};

//////////////////////////////////////////////////////////////////////////
//! UTILITY
//////////////////////////////////////////////////////////////////////////
static void sleepMillisecond(unsigned int ms);
static tm timeToTm(time_t t);
static bool isSameDay(time_t t1, time_t t2);

static void fixPath(std::string &path);
static void trimLogeConfig(std::string &str, std::string extIgnore = std::string());
static std::pair<std::string, std::string> splitPairString(const std::string &str, const std::string &delimiter);

static bool isDirectory(std::string path);
static bool createRecursionDir(std::string path);
static std::string getProcessID();
static std::string getProcessName();

//////////////////////////////////////////////////////////////////////////
//! LockHelper
//////////////////////////////////////////////////////////////////////////
class LockHelper
{
public:
    LockHelper();
    virtual ~LockHelper();

public:
    void lock();
    void unLock();

private:
#ifdef WIN32
    CRITICAL_SECTION _crit;
#else
    pthread_mutex_t _crit;
#endif
};

//////////////////////////////////////////////////////////////////////////
//! AutoLock
//////////////////////////////////////////////////////////////////////////
class AutoLock
{
public:
    explicit AutoLock(LockHelper &lk) : _lock(lk) { _lock.lock(); }
    ~AutoLock() { _lock.unLock(); }

private:
    LockHelper &_lock;
};

//////////////////////////////////////////////////////////////////////////
//! SemHelper
//////////////////////////////////////////////////////////////////////////
class SemHelper
{
public:
    SemHelper();
    virtual ~SemHelper();

public:
    bool create(int initcount);
    bool wait(int timeout = 0);
    bool post();

private:
#ifdef WIN32
    HANDLE _hSem;
#elif defined(__APPLE__)
    dispatch_semaphore_t _semid;
#else
    sem_t _semid;
    bool _isCreate;
#endif
};

//////////////////////////////////////////////////////////////////////////
//! ThreadHelper
//////////////////////////////////////////////////////////////////////////
#ifdef WIN32
static unsigned int WINAPI threadProc(LPVOID lpParam);
#else
static void *threadProc(void *pParam);
#endif

class ThreadHelper
{
public:
    ThreadHelper() { _hThreadID = 0; }
    virtual ~ThreadHelper() {}

public:
    bool start();
    bool wait();
    virtual void run() = 0;

private:
    unsigned long long _hThreadID;
#ifndef WIN32
    pthread_t _phtreadID;
#endif
};

#ifdef WIN32
unsigned int WINAPI threadProc(LPVOID lpParam)
{
    ThreadHelper *p = (ThreadHelper *)lpParam;
    p->run();
    return 0;
}
#else
void *threadProc(void *pParam)
{
    ThreadHelper *p = (ThreadHelper *)pParam;
    p->run();
    return NULL;
}
#endif

//////////////////////////////////////////////////////////////////////////
//! LogeData
//////////////////////////////////////////////////////////////////////////
enum LogeDataType
{
    LDT_GENERAL,
    LDT_ENABLE_LOGEGER,
    LDT_SET_LOGEGER_NAME,
    LDT_SET_LOGEGER_PATH,
    LDT_SET_LOGEGER_LEVEL,
    LDT_SET_LOGEGER_FILELINE,
    LDT_SET_LOGEGER_DISPLAY,
    LDT_SET_LOGEGER_OUTFILE,
    LDT_SET_LOGEGER_LIMITSIZE,
    LDT_SET_LOGEGER_MONTHDIR,
    //    LDT_SET_LOGEGER_,
};

//////////////////////////////////////////////////////////////////////////
//! LogegerInfo
//////////////////////////////////////////////////////////////////////////
struct LogegerInfo
{
    //! attribute
    std::string _key;        // logeger key
    std::string _name;       // one logeger one name.
    std::string _path;       // path for loge file.
    int _level;              // filter level
    bool _display;           // display to screen
    bool _outfile;           // output to file
    bool _monthdir;          // create directory per month
    unsigned int _limitsize; // limit file's size, unit Million byte.
    bool _enable;            // logeger is enable
    bool _fileLine;          // enable/disable the loge's suffix.(file name:line number)

    //! runtime info
    time_t _curFileCreateTime;  // file create time
    unsigned int _curFileIndex; // rolling file index
    unsigned int _curWriteLen;  // current file length
    Loge4zFileHandler _handle;  // file handle.

    LogegerInfo()
    {
        _enable = false;
        _path = LOGE4Z_DEFAULT_PATH;
        _level = LOGE4Z_DEFAULT_LEVEL;
        _display = LOGE4Z_DEFAULT_DISPLAY;
        _outfile = LOGE4Z_DEFAULT_OUTFILE;

        _monthdir = LOGE4Z_DEFAULT_MONTHDIR;
        _limitsize = LOGE4Z_DEFAULT_LIMITSIZE;
        _fileLine = LOGE4Z_DEFAULT_SHOWSUFFIX;

        _curFileCreateTime = 0;
        _curFileIndex = 0;
        _curWriteLen = 0;
    }
};

//////////////////////////////////////////////////////////////////////////
//! LogeerManager
//////////////////////////////////////////////////////////////////////////
class LogeerManager : public ThreadHelper, public ILoge4zManager
{
public:
    LogeerManager();
    virtual ~LogeerManager();

    bool configFromStringImpl(std::string content, bool isUpdate);
    //! ��ȡ�����ļ�����д
    virtual bool config(const char *configPath);
    virtual bool configFromString(const char *configContent);

    //! ��дʽ����
    virtual LogegerId createLogeger(const char *key);
    virtual bool start();
    virtual bool stop();
    virtual bool prePushLoge(LogegerId id, int level);
    virtual bool pushLoge(LogeData *pLoge, const char *file, int line);
    //! ����ID
    virtual LogegerId findLogeger(const char *key);
    bool hotChange(LogegerId id, LogeDataType ldt, int num, const std::string &text);
    virtual bool enableLogeger(LogegerId id, bool enable);
    virtual bool setLogegerName(LogegerId id, const char *name);
    virtual bool setLogegerPath(LogegerId id, const char *path);
    virtual bool setLogegerLevel(LogegerId id, int nLevel);
    virtual bool setLogegerFileLine(LogegerId id, bool enable);
    virtual bool setLogegerDisplay(LogegerId id, bool enable);
    virtual bool setLogegerOutFile(LogegerId id, bool enable);
    virtual bool setLogegerLimitsize(LogegerId id, unsigned int limitsize);
    virtual bool setLogegerMonthdir(LogegerId id, bool enable);

    virtual bool setAutoUpdate(int interval);
    virtual bool updateConfig();
    virtual bool isLogegerEnable(LogegerId id);
    virtual unsigned long long getStatusTotalWriteCount() { return _ullStatusTotalWriteFileCount; }
    virtual unsigned long long getStatusTotalWriteBytes() { return _ullStatusTotalWriteFileBytes; }
    virtual unsigned long long getStatusWaitingCount() { return _ullStatusTotalPushLoge - _ullStatusTotalPopLoge; }
    virtual unsigned int getStatusActiveLogegers();

protected:
    virtual LogeData *makeLogeData(LogegerId id, int level);
    virtual void freeLogeData(LogeData *loge);
    void showColorText(const char *text, int level = LOGE_LEVEL_DEBUG);
    bool onHotChange(LogegerId id, LogeDataType ldt, int num, const std::string &text);
    bool openLogeger(LogeData *loge);
    bool closeLogeger(LogegerId id);
    bool popLoge(LogeData *&loge);
    virtual void run();

private:
    //! thread status.
    bool _runing;
    //! wait thread started.
    SemHelper _semaphore;

    //! hot change name or path for one logeger
    LockHelper _hotLock;
    int _hotUpdateInterval;
    unsigned int _checksum;

    //! the process info.
    std::string _pid;
    std::string _proName;

    //! config file name
    std::string _configFile;

    //! logeger id manager, [logeger name]:[logeger id].
    std::map<std::string, LogegerId> _ids;
    // the last used id of _logegers
    LogegerId _lastId;
    LogegerInfo _logegers[LOGE4Z_LOGEGER_MAX];

    //! loge queue
    LockHelper _logeLock;
    std::list<LogeData *> _loges;
    std::vector<LogeData *> _freeLogeDatas;

    // show color lock
    LockHelper _scLock;
    // status statistics
    // write file
    unsigned long long _ullStatusTotalWriteFileCount;
    unsigned long long _ullStatusTotalWriteFileBytes;

    // Loge queue statistics
    unsigned long long _ullStatusTotalPushLoge;
    unsigned long long _ullStatusTotalPopLoge;
};

//////////////////////////////////////////////////////////////////////////
//! Loge4zFileHandler
//////////////////////////////////////////////////////////////////////////

const std::string Loge4zFileHandler::readContent()
{
    std::string content;

    if (!_file)
    {
        return content;
    }
    char buf[BUFSIZ];
    size_t ret = 0;
    do
    {
        ret = fread(buf, sizeof(char), BUFSIZ, _file);
        content.append(buf, ret);
    } while (ret == BUFSIZ);

    return content;
}

//////////////////////////////////////////////////////////////////////////
//! utility
//////////////////////////////////////////////////////////////////////////

void sleepMillisecond(unsigned int ms)
{
#ifdef WIN32
    ::Sleep(ms);
#else
    usleep(1000 * ms);
#endif
}

struct tm timeToTm(time_t t)
{
#ifdef WIN32
#if _MSC_VER < 1400 // VS2003
    return *localtime(&t);
#else // vs2005->vs2013->
    struct tm tt = {0};
    localtime_s(&tt, &t);
    return tt;
#endif
#else // linux
    struct tm tt = {0,0,0,0,0,0,0,0,0,0,""};
    localtime_r(&t, &tt);
    return tt;
#endif
}

bool isSameDay(time_t t1, time_t t2)
{
    tm tm1 = timeToTm(t1);
    tm tm2 = timeToTm(t2);
    if (tm1.tm_year == tm2.tm_year && tm1.tm_yday == tm2.tm_yday)
    {
        return true;
    }
    return false;
}

void fixPath(std::string &path)
{
    if (path.empty())
    {
        return;
    }
    for (std::string::iterator iter = path.begin(); iter != path.end(); ++iter)
    {
        if (*iter == '\\')
        {
            *iter = '/';
        }
    }
    if (path.at(path.length() - 1) != '/')
    {
        path.append("/");
    }
}

static void trimLogeConfig(std::string &str, std::string extIgnore)
{
    if (str.empty())
    {
        return;
    }
    extIgnore += "\r\n\t ";
    int length = (int)str.length();
    int posBegin = 0;
    int posEnd = 0;

    // trim utf8 file bom
    if (str.length() >= 3 && (unsigned char)str[0] == 0xef && (unsigned char)str[1] == 0xbb && (unsigned char)str[2] == 0xbf)
    {
        posBegin = 3;
    }

    // trim character
    for (int i = posBegin; i < length; i++)
    {
        bool bCheck = false;
        for (int j = 0; j < (int)extIgnore.length(); j++)
        {
            if (str[i] == extIgnore[j])
            {
                bCheck = true;
            }
        }
        if (bCheck)
        {
            if (i == posBegin)
            {
                posBegin++;
            }
        }
        else
        {
            posEnd = i + 1;
        }
    }
    if (posBegin < posEnd)
    {
        str = str.substr(posBegin, posEnd - posBegin);
    }
    else
    {
        str.clear();
    }
}

// split
static std::pair<std::string, std::string> splitPairString(const std::string &str, const std::string &delimiter)
{
    std::string::size_type pos = str.find(delimiter.c_str());
    if (pos == std::string::npos)
    {
        return std::make_pair(str, "");
    }
    return std::make_pair(str.substr(0, pos), str.substr(pos + delimiter.length()));
}

static bool parseConfigLine(const std::string &line, int curLineNum, std::string &key, std::map<std::string, LogegerInfo> &outInfo)
{
    std::pair<std::string, std::string> kv = splitPairString(line, "=");
    if (kv.first.empty())
    {
        return false;
    }

    trimLogeConfig(kv.first);
    trimLogeConfig(kv.second);
    if (kv.first.empty() || kv.first.at(0) == '#')
    {
        return true;
    }

    if (kv.first.at(0) == '[')
    {
        trimLogeConfig(kv.first, "[]");
        key = kv.first;
        {
            std::string tmpstr = kv.first;
            std::transform(tmpstr.begin(), tmpstr.end(), tmpstr.begin(), ::tolower);
            if (tmpstr == "main")
            {
                key = "Main";
            }
        }
        std::map<std::string, LogegerInfo>::iterator iter = outInfo.find(key);
        if (iter == outInfo.end())
        {
            LogegerInfo li;
            li._enable = true;
            li._key = key;
            li._name = key;
            outInfo.insert(std::make_pair(li._key, li));
        }
        else
        {
            std::cout << "loge4z configure warning: duplicate logeger key:[" << key << "] at line:" << curLineNum << std::endl;
        }
        return true;
    }
    trimLogeConfig(kv.first);
    trimLogeConfig(kv.second);
    std::map<std::string, LogegerInfo>::iterator iter = outInfo.find(key);
    if (iter == outInfo.end())
    {
        std::cout << "loge4z configure warning: not found current logeger name:[" << key << "] at line:" << curLineNum
                  << ", key=" << kv.first << ", value=" << kv.second << std::endl;
        return true;
    }
    std::transform(kv.first.begin(), kv.first.end(), kv.first.begin(), ::tolower);
    //! path
    if (kv.first == "path")
    {
        iter->second._path = kv.second;
        return true;
    }
    else if (kv.first == "name")
    {
        iter->second._name = kv.second;
        return true;
    }
    std::transform(kv.second.begin(), kv.second.end(), kv.second.begin(), ::tolower);
    //! level
    if (kv.first == "level")
    {
        if (kv.second == "trace" || kv.second == "all")
        {
            iter->second._level = LOGE_LEVEL_TRACE;
        }
        else if (kv.second == "debug")
        {
            iter->second._level = LOGE_LEVEL_DEBUG;
        }
        else if (kv.second == "info")
        {
            iter->second._level = LOGE_LEVEL_INFO;
        }
        else if (kv.second == "warn" || kv.second == "warning")
        {
            iter->second._level = LOGE_LEVEL_WARN;
        }
        else if (kv.second == "error")
        {
            iter->second._level = LOGE_LEVEL_ERROR;
        }
        else if (kv.second == "alarm")
        {
            iter->second._level = LOGE_LEVEL_ALARM;
        }
        else if (kv.second == "fatal")
        {
            iter->second._level = LOGE_LEVEL_FATAL;
        }
    }
    //! display
    else if (kv.first == "display")
    {
        if (kv.second == "false" || kv.second == "0")
        {
            iter->second._display = false;
        }
        else
        {
            iter->second._display = true;
        }
    }
    //! output to file
    else if (kv.first == "outfile")
    {
        if (kv.second == "false" || kv.second == "0")
        {
            iter->second._outfile = false;
        }
        else
        {
            iter->second._outfile = true;
        }
    }
    //! monthdir
    else if (kv.first == "monthdir")
    {
        if (kv.second == "false" || kv.second == "0")
        {
            iter->second._monthdir = false;
        }
        else
        {
            iter->second._monthdir = true;
        }
    }
    //! limit file size
    else if (kv.first == "limitsize")
    {
        iter->second._limitsize = atoi(kv.second.c_str());
    }
    //! display loge in file line
    else if (kv.first == "fileline")
    {
        if (kv.second == "false" || kv.second == "0")
        {
            iter->second._fileLine = false;
        }
        else
        {
            iter->second._fileLine = true;
        }
    }
    //! enable/disable one logeger
    else if (kv.first == "enable")
    {
        if (kv.second == "false" || kv.second == "0")
        {
            iter->second._enable = false;
        }
        else
        {
            iter->second._enable = true;
        }
    }
    return true;
}

static bool parseConfigFromString(std::string content, std::map<std::string, LogegerInfo> &outInfo)
{

    std::string key;
    int curLine = 1;
    std::string line;
    std::string::size_type curPos = 0;
    if (content.empty())
    {
        return true;
    }
    do
    {
        std::string::size_type pos = std::string::npos;
        for (std::string::size_type i = curPos; i < content.length(); ++i)
        {
            // support linux/unix/windows LRCF
            if (content[i] == '\r' || content[i] == '\n')
            {
                pos = i;
                break;
            }
        }
        line = content.substr(curPos, pos - curPos);
        parseConfigLine(line, curLine, key, outInfo);
        curLine++;

        if (pos == std::string::npos)
        {
            break;
        }
        else
        {
            curPos = pos + 1;
        }
    } while (1);
    return true;
}

bool isDirectory(std::string path)
{
#ifdef WIN32
    return PathIsDirectoryA(path.c_str()) ? true : false;
#else
    DIR *pdir = opendir(path.c_str());
    if (pdir == NULL)
    {
        return false;
    }
    else
    {
        closedir(pdir);
        pdir = NULL;
        return true;
    }
#endif
}

bool createRecursionDir(std::string path)
{
    if (path.length() == 0)
        return true;
    std::string sub;
    fixPath(path);

    std::string::size_type pos = path.find('/');
    while (pos != std::string::npos)
    {
        std::string cur = path.substr(0, pos - 0);
        if (cur.length() > 0 && !isDirectory(cur))
        {
            bool ret = false;
#ifdef WIN32
            ret = CreateDirectoryA(cur.c_str(), NULL) ? true : false;
#else
            ret = (mkdir(cur.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == 0);
#endif
            if (!ret)
            {
                return false;
            }
        }
        pos = path.find('/', pos + 1);
    }

    return true;
}

std::string getProcessID()
{
    std::string pid = "0";
    char buf[260] = {0};
#ifdef WIN32
    DWORD winPID = GetCurrentProcessId();
    sprintf(buf, "%06u", winPID);
    pid = buf;
#else
    sprintf(buf, "%06d", getpid());
    pid = buf;
#endif
    return pid;
}

std::string getProcessName()
{
    std::string name = "process";
    char buf[260] = {0};
#ifdef WIN32
    if (GetModuleFileNameA(NULL, buf, 259) > 0)
    {
        name = buf;
    }
    std::string::size_type pos = name.rfind("\\");
    if (pos != std::string::npos)
    {
        name = name.substr(pos + 1, std::string::npos);
    }
    pos = name.rfind(".");
    if (pos != std::string::npos)
    {
        name = name.substr(0, pos - 0);
    }

#elif defined(LOGE4Z_HAVE_LIBPROC)
    proc_name(getpid(), buf, 260);
    name = buf;
    return name;
    ;
#else
    sprintf(buf, "/proc/%d/cmdline", (int)getpid());
    Loge4zFileHandler i;
    i.open(buf, "rb");
    if (!i.isOpen())
    {
        return name;
    }
    name = i.readLine();
    i.close();

    std::string::size_type pos = name.rfind("/");
    if (pos != std::string::npos)
    {
        name = name.substr(pos + 1, std::string::npos);
    }
#endif

    return name;
}

//////////////////////////////////////////////////////////////////////////
// LockHelper
//////////////////////////////////////////////////////////////////////////
LockHelper::LockHelper()
{
#ifdef WIN32
    InitializeCriticalSection(&_crit);
#else
    //_crit = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_crit, &attr);
    pthread_mutexattr_destroy(&attr);
#endif
}
LockHelper::~LockHelper()
{
#ifdef WIN32
    DeleteCriticalSection(&_crit);
#else
    pthread_mutex_destroy(&_crit);
#endif
}

void LockHelper::lock()
{
#ifdef WIN32
    EnterCriticalSection(&_crit);
#else
    pthread_mutex_lock(&_crit);
#endif
}
void LockHelper::unLock()
{
#ifdef WIN32
    LeaveCriticalSection(&_crit);
#else
    pthread_mutex_unlock(&_crit);
#endif
}
//////////////////////////////////////////////////////////////////////////
// SemHelper
//////////////////////////////////////////////////////////////////////////
SemHelper::SemHelper()
{
#ifdef WIN32
    _hSem = NULL;
#elif defined(__APPLE__)
    _semid = NULL;
#else
    _isCreate = false;
#endif
}
SemHelper::~SemHelper()
{
#ifdef WIN32
    if (_hSem != NULL)
    {
        CloseHandle(_hSem);
        _hSem = NULL;
    }
#elif defined(__APPLE__)
    if (_semid)
    {
        dispatch_release(_semid);
        _semid = NULL;
    }
#else
    if (_isCreate)
    {
        _isCreate = false;
        sem_destroy(&_semid);
    }
#endif
}

bool SemHelper::create(int initcount)
{
    if (initcount < 0)
    {
        initcount = 0;
    }
#ifdef WIN32
    if (initcount > 64)
    {
        return false;
    }
    _hSem = CreateSemaphore(NULL, initcount, 64, NULL);
    if (_hSem == NULL)
    {
        return false;
    }
#elif defined(__APPLE__)
    _semid = dispatch_semaphore_create(initcount);
    if (!_semid)
    {
        return false;
    }
#else
    if (sem_init(&_semid, 0, initcount) != 0)
    {
        return false;
    }
    _isCreate = true;
#endif

    return true;
}
bool SemHelper::wait(int timeout)
{
#ifdef WIN32
    if (timeout <= 0)
    {
        timeout = INFINITE;
    }
    if (WaitForSingleObject(_hSem, timeout) != WAIT_OBJECT_0)
    {
        return false;
    }
#elif defined(__APPLE__)
    if (dispatch_semaphore_wait(_semid, dispatch_time(DISPATCH_TIME_NOW, timeout * 1000)) != 0)
    {
        return false;
    }
#else
    if (timeout <= 0)
    {
        return (sem_wait(&_semid) == 0);
    }
    else
    {
        struct timeval tm;
        gettimeofday(&tm, NULL);
        long long endtime = tm.tv_sec * 1000 + tm.tv_usec / 1000 + timeout;
        do
        {
            sleepMillisecond(50);
            int ret = sem_trywait(&_semid);
            if (ret == 0)
            {
                return true;
            }
            struct timeval tv_cur;
            gettimeofday(&tv_cur, NULL);
            if (tv_cur.tv_sec * 1000 + tv_cur.tv_usec / 1000 > endtime)
            {
                return false;
            }

            if (ret == -1 && errno == EAGAIN)
            {
                continue;
            }
            else
            {
                return false;
            }
        } while (true);
        return false;
    }
#endif
    return true;
}

bool SemHelper::post()
{
#ifdef WIN32
    return ReleaseSemaphore(_hSem, 1, NULL) ? true : false;
#elif defined(__APPLE__)
    return dispatch_semaphore_signal(_semid) == 0;
#else
    return (sem_post(&_semid) == 0);
#endif
}

//////////////////////////////////////////////////////////////////////////
//! ThreadHelper
//////////////////////////////////////////////////////////////////////////
bool ThreadHelper::start()
{
#ifdef WIN32
    unsigned long long ret = _beginthreadex(NULL, 0, threadProc, (void *)this, 0, NULL);

    if (ret == -1 || ret == 0)
    {
        std::cout << "loge4z: create loge4z thread error! \r\n"
                  << std::endl;
        return false;
    }
    _hThreadID = ret;
#else
    int ret = pthread_create(&_phtreadID, NULL, threadProc, (void *)this);
    if (ret != 0)
    {
        std::cout << "loge4z: create loge4z thread error! \r\n"
                  << std::endl;
        return false;
    }
#endif
    return true;
}

bool ThreadHelper::wait()
{
#ifdef WIN32
    if (WaitForSingleObject((HANDLE)_hThreadID, INFINITE) != WAIT_OBJECT_0)
    {
        return false;
    }
#else
    if (pthread_join(_phtreadID, NULL) != 0)
    {
        return false;
    }
#endif
    return true;
}

//////////////////////////////////////////////////////////////////////////
//! LogeerManager
//////////////////////////////////////////////////////////////////////////
LogeerManager::LogeerManager()
{
    _runing = false;
    _lastId = LOGE4Z_MAIN_LOGEGER_ID;
    _hotUpdateInterval = 0;

    _ullStatusTotalPushLoge = 0;
    _ullStatusTotalPopLoge = 0;
    _ullStatusTotalWriteFileCount = 0;
    _ullStatusTotalWriteFileBytes = 0;

    _pid = getProcessID();
    _proName = getProcessName();
    _logegers[LOGE4Z_MAIN_LOGEGER_ID]._enable = true;
    _ids[LOGE4Z_MAIN_LOGEGER_KEY] = LOGE4Z_MAIN_LOGEGER_ID;
    _logegers[LOGE4Z_MAIN_LOGEGER_ID]._key = LOGE4Z_MAIN_LOGEGER_KEY;
    _logegers[LOGE4Z_MAIN_LOGEGER_ID]._name = LOGE4Z_MAIN_LOGEGER_KEY;
}
LogeerManager::~LogeerManager()
{
    stop();
}

LogeData *LogeerManager::makeLogeData(LogegerId id, int level)
{
    LogeData *pLoge = NULL;
    if (true)
    {
        if (!_freeLogeDatas.empty())
        {
            AutoLock l(_logeLock);
            if (!_freeLogeDatas.empty())
            {
                pLoge = _freeLogeDatas.back();
                _freeLogeDatas.pop_back();
            }
        }
        if (pLoge == NULL)
        {
            pLoge = new LogeData();
        }
    }
    // append precise time to loge
    if (true)
    {
        pLoge->_id = id;
        pLoge->_level = level;
        pLoge->_type = LDT_GENERAL;
        pLoge->_typeval = 0;
        pLoge->_contentLen = 0;
#ifdef WIN32
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        unsigned long long now = ft.dwHighDateTime;
        now <<= 32;
        now |= ft.dwLowDateTime;
        now /= 10;
        now -= 11644473600000000ULL;
        now /= 1000;
        pLoge->_time = now / 1000;
        pLoge->_precise = (unsigned int)(now % 1000);
#else
        struct timeval tm;
        gettimeofday(&tm, NULL);
        pLoge->_time = tm.tv_sec;
        pLoge->_precise = tm.tv_usec / 1000;
#endif
    }

    // format loge
    if (true)
    {
        tm tt = timeToTm(pLoge->_time);

        pLoge->_contentLen = sprintf(pLoge->_content, "%d-%02d-%02d %02d:%02d:%02d.%03u %s ",
                                     tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday, tt.tm_hour, tt.tm_min, tt.tm_sec, pLoge->_precise,
                                     LOGE_STRING[pLoge->_level]);
        if (pLoge->_contentLen < 0)
        {
            pLoge->_contentLen = 0;
        }
    }
    return pLoge;
}
void LogeerManager::freeLogeData(LogeData *loge)
{

    if (_freeLogeDatas.size() < 200)
    {
        AutoLock l(_logeLock);
        _freeLogeDatas.push_back(loge);
    }
    else
    {
        delete loge;
    }
}

void LogeerManager::showColorText(const char *text, int level)
{

#if defined(WIN32) && defined(LOGE4Z_OEM_CONSOLE)
    char oem[LOGE4Z_LOGE_BUF_SIZE] = {0};
    CharToOemBuffA(text, oem, LOGE4Z_LOGE_BUF_SIZE);
#endif

    if (level <= LOGE_LEVEL_DEBUG || level > LOGE_LEVEL_FATAL)
    {
#if defined(WIN32) && defined(LOGE4Z_OEM_CONSOLE)
        printf("%s", oem);
#else
        printf("%s", text);
#endif
        return;
    }
#ifndef WIN32
    printf("%s%s\033[0m", LOGE_COLOR[level], text);
#else
    AutoLock l(_scLock);
    HANDLE hStd = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStd == INVALID_HANDLE_VALUE)
        return;
    CONSOLE_SCREEN_BUFFER_INFO oldInfo;
    if (!GetConsoleScreenBufferInfo(hStd, &oldInfo))
    {
        return;
    }
    else
    {
        SetConsoleTextAttribute(hStd, LOGE_COLOR[level]);
#ifdef LOGE4Z_OEM_CONSOLE
        printf("%s", oem);
#else
        printf("%s", text);
#endif
        SetConsoleTextAttribute(hStd, oldInfo.wAttributes);
    }
#endif
    return;
}

bool LogeerManager::configFromStringImpl(std::string content, bool isUpdate)
{
    unsigned int sum = 0;
    for (std::string::iterator iter = content.begin(); iter != content.end(); ++iter)
    {
        sum += (unsigned char)*iter;
    }
    if (sum == _checksum)
    {
        return true;
    }
    _checksum = sum;

    std::map<std::string, LogegerInfo> logegerMap;
    if (!parseConfigFromString(content, logegerMap))
    {
        std::cout << " !!! !!! !!! !!!" << std::endl;
        std::cout << " !!! !!! loge4z load config file error" << std::endl;
        std::cout << " !!! !!! !!! !!!" << std::endl;
        return false;
    }
    for (std::map<std::string, LogegerInfo>::iterator iter = logegerMap.begin(); iter != logegerMap.end(); ++iter)
    {
        LogegerId id = LOGE4Z_INVALID_LOGEGER_ID;
        id = findLogeger(iter->second._key.c_str());
        if (id == LOGE4Z_INVALID_LOGEGER_ID)
        {
            if (isUpdate)
            {
                continue;
            }
            else
            {
                id = createLogeger(iter->second._key.c_str());
                if (id == LOGE4Z_INVALID_LOGEGER_ID)
                {
                    continue;
                }
            }
        }
        enableLogeger(id, iter->second._enable);
        setLogegerName(id, iter->second._name.c_str());
        setLogegerPath(id, iter->second._path.c_str());
        setLogegerLevel(id, iter->second._level);
        setLogegerFileLine(id, iter->second._fileLine);
        setLogegerDisplay(id, iter->second._display);
        setLogegerOutFile(id, iter->second._outfile);
        setLogegerLimitsize(id, iter->second._limitsize);
        setLogegerMonthdir(id, iter->second._monthdir);
    }
    return true;
}

//! read configure and create with overwriting
bool LogeerManager::config(const char *configPath)
{
    if (!_configFile.empty())
    {
        std::cout << " !!! !!! !!! !!!" << std::endl;
        std::cout << " !!! !!! loge4z configure error: too many calls to Config. the old config file=" << _configFile << ", the new config file=" << configPath << " !!! !!! " << std::endl;
        std::cout << " !!! !!! !!! !!!" << std::endl;
        return false;
    }
    _configFile = configPath;

    Loge4zFileHandler f;
    f.open(_configFile.c_str(), "rb");
    if (!f.isOpen())
    {
        std::cout << " !!! !!! !!! !!!" << std::endl;
        std::cout << " !!! !!! loge4z load config file error. filename=" << configPath << " !!! !!! " << std::endl;
        std::cout << " !!! !!! !!! !!!" << std::endl;
        return false;
    }
    return configFromStringImpl(f.readContent().c_str(), false);
}

//! read configure and create with overwriting
bool LogeerManager::configFromString(const char *configContent)
{
    return configFromStringImpl(configContent, false);
}

//! create with overwriting
LogegerId LogeerManager::createLogeger(const char *key)
{
    if (key == NULL)
    {
        return LOGE4Z_INVALID_LOGEGER_ID;
    }

    std::string copyKey = key;
    trimLogeConfig(copyKey);

    LogegerId newID = LOGE4Z_INVALID_LOGEGER_ID;
    {
        std::map<std::string, LogegerId>::iterator iter = _ids.find(copyKey);
        if (iter != _ids.end())
        {
            newID = iter->second;
        }
    }
    if (newID == LOGE4Z_INVALID_LOGEGER_ID)
    {
        if (_lastId + 1 >= LOGE4Z_LOGEGER_MAX)
        {
            showColorText("loge4z: CreateLogeger can not create|writeover, because logegerid need < LOGEGER_MAX! \r\n", LOGE_LEVEL_FATAL);
            return LOGE4Z_INVALID_LOGEGER_ID;
        }
        newID = ++_lastId;
        _ids[copyKey] = newID;
        _logegers[newID]._enable = true;
        _logegers[newID]._key = copyKey;
        _logegers[newID]._name = copyKey;
    }

    return newID;
}

bool LogeerManager::start()
{
    if (_runing)
    {
        showColorText("loge4z already start \r\n", LOGE_LEVEL_FATAL);
        return false;
    }
    _semaphore.create(0);
    bool ret = ThreadHelper::start();
    return ret && _semaphore.wait(3000);
}
bool LogeerManager::stop()
{
    if (_runing)
    {
        showColorText("loge4z stopping \r\n", LOGE_LEVEL_FATAL);
        _runing = false;
        wait();
        return true;
    }
    return false;
}
bool LogeerManager::prePushLoge(LogegerId id, int level)
{
    if (id < 0 || id > _lastId || !_runing || !_logegers[id]._enable)
    {
        return false;
    }
    if (level < _logegers[id]._level)
    {
        return false;
    }
    return true;
}
bool LogeerManager::pushLoge(LogeData *pLoge, const char *file, int line)
{
    // discard loge
    if (pLoge->_id < 0 || pLoge->_id > _lastId || !_runing || !_logegers[pLoge->_id]._enable)
    {
        freeLogeData(pLoge);
        return false;
    }

    // filter loge
    if (pLoge->_level < _logegers[pLoge->_id]._level)
    {
        freeLogeData(pLoge);
        return false;
    }
    if (_logegers[pLoge->_id]._fileLine && file)
    {
        const char *pNameBegin = file + strlen(file);
        do
        {
            if (*pNameBegin == '\\' || *pNameBegin == '/')
            {
                pNameBegin++;
                break;
            }
            if (pNameBegin == file)
            {
                break;
            }
            pNameBegin--;
        } while (true);
        zsummer_e::loge4z::Loge4zStream ss(pLoge->_content + pLoge->_contentLen, LOGE4Z_LOGE_BUF_SIZE - pLoge->_contentLen);
        ss << " " << pNameBegin << ":" << line;
        pLoge->_contentLen += ss.getCurrentLen();
    }
    if (pLoge->_contentLen < 3)
        pLoge->_contentLen = 3;
    if (pLoge->_contentLen + 3 <= LOGE4Z_LOGE_BUF_SIZE)
        pLoge->_contentLen += 3;

    pLoge->_content[pLoge->_contentLen - 1] = '\0';
    pLoge->_content[pLoge->_contentLen - 2] = '\n';
    pLoge->_content[pLoge->_contentLen - 3] = '\r';
    pLoge->_contentLen--; // clean '\0'

    if (_logegers[pLoge->_id]._display && LOGE4Z_ALL_SYNCHRONOUS_OUTPUT)
    {
        showColorText(pLoge->_content, pLoge->_level);
    }

    if (LOGE4Z_ALL_DEBUGOUTPUT_DISPLAY && LOGE4Z_ALL_SYNCHRONOUS_OUTPUT)
    {
#ifdef WIN32
        OutputDebugStringA(pLoge->_content);
#endif
    }

    if (_logegers[pLoge->_id]._outfile && LOGE4Z_ALL_SYNCHRONOUS_OUTPUT)
    {
        AutoLock l(_logeLock);
        if (openLogeger(pLoge))
        {
            _logegers[pLoge->_id]._handle.write(pLoge->_content, pLoge->_contentLen);
            closeLogeger(pLoge->_id);
            _ullStatusTotalWriteFileCount++;
            _ullStatusTotalWriteFileBytes += pLoge->_contentLen;
        }
    }

    if (LOGE4Z_ALL_SYNCHRONOUS_OUTPUT)
    {
        freeLogeData(pLoge);
        return true;
    }

    AutoLock l(_logeLock);
    _loges.push_back(pLoge);
    _ullStatusTotalPushLoge++;
    return true;
}

//! ����ID
LogegerId LogeerManager::findLogeger(const char *key)
{
    std::map<std::string, LogegerId>::iterator iter;
    iter = _ids.find(key);
    if (iter != _ids.end())
    {
        return iter->second;
    }
    return LOGE4Z_INVALID_LOGEGER_ID;
}

bool LogeerManager::hotChange(LogegerId id, LogeDataType ldt, int num, const std::string &text)
{
    if (id < 0 || id > _lastId)
        return false;
    if (text.length() >= LOGE4Z_LOGE_BUF_SIZE)
        return false;
    if (!_runing || LOGE4Z_ALL_SYNCHRONOUS_OUTPUT)
    {
        return onHotChange(id, ldt, num, text);
    }
    LogeData *pLoge = makeLogeData(id, LOGE4Z_DEFAULT_LEVEL);
    pLoge->_id = id;
    pLoge->_type = ldt;
    pLoge->_typeval = num;
    memcpy(pLoge->_content, text.c_str(), text.length());
    pLoge->_contentLen = (int)text.length();
    AutoLock l(_logeLock);
    _loges.push_back(pLoge);
    return true;
}

bool LogeerManager::onHotChange(LogegerId id, LogeDataType ldt, int num, const std::string &text)
{
    if (id < LOGE4Z_MAIN_LOGEGER_ID || id > _lastId)
    {
        return false;
    }
    LogegerInfo &logeger = _logegers[id];
    if (ldt == LDT_ENABLE_LOGEGER)
        logeger._enable = num != 0;
    else if (ldt == LDT_SET_LOGEGER_NAME)
        logeger._name = text;
    else if (ldt == LDT_SET_LOGEGER_PATH)
        logeger._path = text;
    else if (ldt == LDT_SET_LOGEGER_LEVEL)
        logeger._level = num;
    else if (ldt == LDT_SET_LOGEGER_FILELINE)
        logeger._fileLine = num != 0;
    else if (ldt == LDT_SET_LOGEGER_DISPLAY)
        logeger._display = num != 0;
    else if (ldt == LDT_SET_LOGEGER_OUTFILE)
        logeger._outfile = num != 0;
    else if (ldt == LDT_SET_LOGEGER_LIMITSIZE)
        logeger._limitsize = num;
    else if (ldt == LDT_SET_LOGEGER_MONTHDIR)
        logeger._monthdir = num != 0;
    return true;
}

bool LogeerManager::enableLogeger(LogegerId id, bool enable)
{
    if (id < 0 || id > _lastId)
        return false;
    if (enable)
    {
        _logegers[id]._enable = true;
        return true;
    }
    return hotChange(id, LDT_ENABLE_LOGEGER, false, "");
}
bool LogeerManager::setLogegerLevel(LogegerId id, int level)
{
    if (id < 0 || id > _lastId)
        return false;
    if (level <= _logegers[id]._level)
    {
        _logegers[id]._level = level;
        return true;
    }
    return hotChange(id, LDT_SET_LOGEGER_LEVEL, level, "");
}
bool LogeerManager::setLogegerDisplay(LogegerId id, bool enable) { return hotChange(id, LDT_SET_LOGEGER_DISPLAY, enable, ""); }
bool LogeerManager::setLogegerOutFile(LogegerId id, bool enable) { return hotChange(id, LDT_SET_LOGEGER_OUTFILE, enable, ""); }
bool LogeerManager::setLogegerMonthdir(LogegerId id, bool enable) { return hotChange(id, LDT_SET_LOGEGER_MONTHDIR, enable, ""); }
bool LogeerManager::setLogegerFileLine(LogegerId id, bool enable) { return hotChange(id, LDT_SET_LOGEGER_FILELINE, enable, ""); }

bool LogeerManager::setLogegerLimitsize(LogegerId id, unsigned int limitsize)
{
    if (limitsize == 0)
    {
        limitsize = (unsigned int)-1;
    }
    return hotChange(id, LDT_SET_LOGEGER_LIMITSIZE, limitsize, "");
}

bool LogeerManager::setLogegerName(LogegerId id, const char *name)
{
    if (id < 0 || id > _lastId)
        return false;
    // the name by main logeger is the process name and it's can't change.
    //     if (id == LOGE4Z_MAIN_LOGEGER_ID) return false;

    if (name == NULL || strlen(name) == 0)
    {
        return false;
    }
    return hotChange(id, LDT_SET_LOGEGER_NAME, 0, name);
}

bool LogeerManager::setLogegerPath(LogegerId id, const char *path)
{
    if (id < 0 || id > _lastId)
        return false;
    if (path == NULL || strlen(path) == 0)
        return false;
    std::string copyPath = path;
    {
        char ch = copyPath.at(copyPath.length() - 1);
        if (ch != '\\' && ch != '/')
        {
            copyPath.append("/");
        }
    }
    return hotChange(id, LDT_SET_LOGEGER_PATH, 0, copyPath);
}
bool LogeerManager::setAutoUpdate(int interval)
{
    _hotUpdateInterval = interval;
    return true;
}
bool LogeerManager::updateConfig()
{
    if (_configFile.empty())
    {
        // LOGEW("loge4z update config file error. filename is empty.");
        return false;
    }
    Loge4zFileHandler f;
    f.open(_configFile.c_str(), "rb");
    if (!f.isOpen())
    {
        std::cout << " !!! !!! !!! !!!" << std::endl;
        std::cout << " !!! !!! loge4z load config file error. filename=" << _configFile << " !!! !!! " << std::endl;
        std::cout << " !!! !!! !!! !!!" << std::endl;
        return false;
    }
    return configFromStringImpl(f.readContent().c_str(), true);
}

bool LogeerManager::isLogegerEnable(LogegerId id)
{
    if (id < 0 || id > _lastId)
        return false;
    return _logegers[id]._enable;
}

unsigned int LogeerManager::getStatusActiveLogegers()
{
    unsigned int actives = 0;
    for (int i = 0; i <= _lastId; i++)
    {
        if (_logegers[i]._enable)
        {
            actives++;
        }
    }
    return actives;
}

bool LogeerManager::openLogeger(LogeData *pLoge)
{
    int id = pLoge->_id;
    if (id < 0 || id > _lastId)
    {
        showColorText("loge4z: openLogeger can not open, invalide logeger id! \r\n", LOGE_LEVEL_FATAL);
        return false;
    }

    LogegerInfo *pLogeger = &_logegers[id];
    if (!pLogeger->_enable || !pLogeger->_outfile || pLoge->_level < pLogeger->_level)
    {
        return false;
    }

    bool sameday = isSameDay(pLoge->_time, pLogeger->_curFileCreateTime);
    bool needChageFile = pLogeger->_curWriteLen > pLogeger->_limitsize * 1024 * 1024;
    if (!sameday || needChageFile)
    {
        if (!sameday)
        {
            pLogeger->_curFileIndex = 0;
        }
        else
        {
            pLogeger->_curFileIndex++;
        }
        if (pLogeger->_handle.isOpen())
        {
            pLogeger->_handle.close();
        }
    }
    if (!pLogeger->_handle.isOpen())
    {
        pLogeger->_curFileCreateTime = pLoge->_time;
        pLogeger->_curWriteLen = 0;

        tm t = timeToTm(pLogeger->_curFileCreateTime);
        std::string name;
        std::string path;
        _hotLock.lock();
        name = pLogeger->_name;
        path = pLogeger->_path;
        _hotLock.unLock();

        char buf[100] = {0};
        if (pLogeger->_monthdir)
        {
            sprintf(buf, "%04d_%02d_%02d/", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
            path += buf;
        }

        if (!isDirectory(path))
        {
            createRecursionDir(path);
        }

        // sprintf(buf, "%s_%s_%04d%02d%02d%02d%02d_%s_%03u.loge",
        //     _proName.c_str(), name.c_str(), t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
        //     t.tm_hour, t.tm_min, _pid.c_str(), pLogeger->_curFileIndex);
        sprintf(buf, "%s_%s_%03u.log", _proName.c_str(), name.c_str(), pLogeger->_curFileIndex);
        path += buf;
        pLogeger->_handle.open(path.c_str(), "ab");
        if (!pLogeger->_handle.isOpen())
        {
            std::stringstream ss;
            ss << "loge4z: can not open loge file " << path << " . \r\n";
            showColorText("!!!!!!!!!!!!!!!!!!!!!!!!!! \r\n", LOGE_LEVEL_FATAL);
            showColorText(ss.str().c_str(), LOGE_LEVEL_FATAL);
            showColorText("!!!!!!!!!!!!!!!!!!!!!!!!!! \r\n", LOGE_LEVEL_FATAL);
            pLogeger->_outfile = false;
            return false;
        }
        return true;
    }
    return true;
}
bool LogeerManager::closeLogeger(LogegerId id)
{
    if (id < 0 || id > _lastId)
    {
        showColorText("loge4z: closeLogeger can not close, invalide logeger id! \r\n", LOGE_LEVEL_FATAL);
        return false;
    }
    LogegerInfo *pLogeger = &_logegers[id];
    if (pLogeger->_handle.isOpen())
    {
        pLogeger->_handle.close();
        return true;
    }
    return false;
}
bool LogeerManager::popLoge(LogeData *&loge)
{
    AutoLock l(_logeLock);
    if (_loges.empty())
    {
        return false;
    }
    loge = _loges.front();
    _loges.pop_front();
    return true;
}

void LogeerManager::run()
{
    _runing = true;
    LOGA("-----------------  loge4z thread started!   ----------------------------");
    for (int i = 0; i <= _lastId; i++)
    {
        if (_logegers[i]._enable)
        {
            LOGA("logeger id=" << i
                               << " key=" << _logegers[i]._key
                               << " name=" << _logegers[i]._name
                               << " path=" << _logegers[i]._path
                               << " level=" << _logegers[i]._level
                               << " display=" << _logegers[i]._display);
        }
    }

    _semaphore.post();

    LogeData *pLoge = NULL;
    int needFlush[LOGE4Z_LOGEGER_MAX] = {0};
    time_t lastCheckUpdate = time(NULL);
    while (true)
    {
        while (popLoge(pLoge))
        {
            if (pLoge->_id < 0 || pLoge->_id > _lastId)
            {
                freeLogeData(pLoge);
                continue;
            }
            LogegerInfo &curLogeger = _logegers[pLoge->_id];

            if (pLoge->_type != LDT_GENERAL)
            {
                onHotChange(pLoge->_id, (LogeDataType)pLoge->_type, pLoge->_typeval, std::string(pLoge->_content, pLoge->_contentLen));
                curLogeger._handle.close();
                freeLogeData(pLoge);
                continue;
            }

            //
            _ullStatusTotalPopLoge++;
            // discard

            if (!curLogeger._enable || pLoge->_level < curLogeger._level)
            {
                freeLogeData(pLoge);
                continue;
            }

            if (curLogeger._display && !LOGE4Z_ALL_SYNCHRONOUS_OUTPUT)
            {
                showColorText(pLoge->_content, pLoge->_level);
            }
            if (LOGE4Z_ALL_DEBUGOUTPUT_DISPLAY && !LOGE4Z_ALL_SYNCHRONOUS_OUTPUT)
            {
#ifdef WIN32
                OutputDebugStringA(pLoge->_content);
#endif
            }

            if (curLogeger._outfile && !LOGE4Z_ALL_SYNCHRONOUS_OUTPUT)
            {
                if (!openLogeger(pLoge))
                {
                    freeLogeData(pLoge);
                    continue;
                }

                curLogeger._handle.write(pLoge->_content, pLoge->_contentLen);
                curLogeger._curWriteLen += (unsigned int)pLoge->_contentLen;
                needFlush[pLoge->_id]++;
                _ullStatusTotalWriteFileCount++;
                _ullStatusTotalWriteFileBytes += pLoge->_contentLen;
            }
            else if (!LOGE4Z_ALL_SYNCHRONOUS_OUTPUT)
            {
                _ullStatusTotalWriteFileCount++;
                _ullStatusTotalWriteFileBytes += pLoge->_contentLen;
            }

            freeLogeData(pLoge);
        }

        for (int i = 0; i <= _lastId; i++)
        {
            if (_logegers[i]._enable && needFlush[i] > 0)
            {
                _logegers[i]._handle.flush();
                needFlush[i] = 0;
            }
            if (!_logegers[i]._enable && _logegers[i]._handle.isOpen())
            {
                _logegers[i]._handle.close();
            }
        }

        //! delay.
        sleepMillisecond(100);

        //! quit
        if (!_runing && _loges.empty())
        {
            break;
        }

        if (_hotUpdateInterval != 0 && time(NULL) - lastCheckUpdate > _hotUpdateInterval)
        {
            updateConfig();
            lastCheckUpdate = time(NULL);
        }
    }

    for (int i = 0; i <= _lastId; i++)
    {
        if (_logegers[i]._enable)
        {
            _logegers[i]._enable = false;
            closeLogeger(i);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// ILoge4zManager::getInstance
//////////////////////////////////////////////////////////////////////////
ILoge4zManager *ILoge4zManager::getInstance()
{
    static LogeerManager m;
    return &m;
}

_ZSUMMER_E_LOGE4Z_END
_ZSUMMER_E_END
