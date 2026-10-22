#include "Win.h"

// winsock2 before windows.h (WIN32_LEAN_AND_MEAN drops it); needed for AF_INET6 / MIB_TCP6TABLE_OWNER_PID
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <sddl.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <QDir>
#include <QFileInfo>
#include <vector>

namespace win {

namespace {

std::wstring w(const QString& s) { return s.toStdWString(); }

QString quoteArg(const QString& a)
{
    if (!a.isEmpty() && !a.contains(' ') && !a.contains('\t') && !a.contains('"'))
        return a;
    QString q = a;
    q.replace("\"", "\\\"");
    return "\"" + q + "\"";
}

struct ScHandle {
    SC_HANDLE h = nullptr;
    explicit ScHandle(SC_HANDLE v) : h(v) {}
    ~ScHandle() { if (h) CloseServiceHandle(h); }
    ScHandle(const ScHandle&) = delete;
    ScHandle& operator=(const ScHandle&) = delete;
    explicit operator bool() const { return h != nullptr; }
};

void setErr(QString* err, const QString& what)
{
    if (err) *err = what + ": " + lastErrorText(GetLastError());
}

} // namespace

QString lastErrorText(unsigned long code)
{
    wchar_t* buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    QString s = buf ? QString::fromWCharArray(buf).trimmed() : QString("error %1").arg(code);
    if (buf) LocalFree(buf);
    return s;
}

QList<Proc> processes(const QStringList& names)
{
    QList<Proc> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        const QString exe = QString::fromWCharArray(pe.szExeFile);
        if (!names.contains(exe, Qt::CaseInsensitive)) continue;
        const QString path = processPath(pe.th32ProcessID);
        if (!path.isEmpty()) out.append({pe.th32ProcessID, pe.th32ParentProcessID, path});
    }
    CloseHandle(snap);
    return out;
}

QString processPath(quint32 pid)
{
    if (pid == 4) return "System";
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return {};
    wchar_t buf[1024];
    DWORD n = DWORD(std::size(buf));
    QString out;
    if (QueryFullProcessImageNameW(h, 0, buf, &n)) out = QDir::fromNativeSeparators(QString::fromWCharArray(buf, int(n)));
    CloseHandle(h);
    return out;
}

bool terminate(quint32 pid)
{
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    const bool ok = TerminateProcess(h, 1);
    CloseHandle(h);
    return ok;
}

bool signalEvent(const QString& name)
{
    HANDLE e = OpenEventW(EVENT_MODIFY_STATE, FALSE, w(name).c_str());
    if (!e) return false;
    const bool ok = SetEvent(e);
    CloseHandle(e);
    return ok;
}

bool startHidden(const QString& exe, const QStringList& args, const QString& workDir, quint32* pid)
{
    QString cmd = quoteArg(QDir::toNativeSeparators(exe));
    for (const QString& a : args) cmd += " " + quoteArg(a);
    std::wstring cmdW = w(cmd);
    const std::wstring dir = w(QDir::toNativeSeparators(workDir));
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    // break away from our job (if any) so servers outlive the manager; retry if the job forbids it
    DWORD flags = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB;
    BOOL ok = CreateProcessW(nullptr, cmdW.data(), nullptr, nullptr, FALSE, flags, nullptr,
                             dir.empty() ? nullptr : dir.c_str(), &si, &pi);
    if (!ok && GetLastError() == ERROR_ACCESS_DENIED) {
        flags &= ~DWORD(CREATE_BREAKAWAY_FROM_JOB);
        ok = CreateProcessW(nullptr, cmdW.data(), nullptr, nullptr, FALSE, flags, nullptr,
                            dir.empty() ? nullptr : dir.c_str(), &si, &pi);
    }
    if (!ok) return false;
    if (pid) *pid = pi.dwProcessId;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

QMultiMap<quint16, quint32> listeningPorts()
{
    QMultiMap<quint16, quint32> out;
    auto swap16 = [](DWORD p) { return quint16(((p & 0xff) << 8) | ((p >> 8) & 0xff)); };
    for (ULONG af : {ULONG(AF_INET), ULONG(AF_INET6)}) {
        DWORD size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, af, TCP_TABLE_OWNER_PID_LISTENER, 0);
        std::vector<char> buf(size + 1024);
        size = DWORD(buf.size());
        if (GetExtendedTcpTable(buf.data(), &size, FALSE, af, TCP_TABLE_OWNER_PID_LISTENER, 0) != NO_ERROR) continue;
        if (af == AF_INET) {
            auto* t = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buf.data());
            for (DWORD i = 0; i < t->dwNumEntries; ++i) out.insert(swap16(t->table[i].dwLocalPort), t->table[i].dwOwningPid);
        } else {
            auto* t = reinterpret_cast<MIB_TCP6TABLE_OWNER_PID*>(buf.data());
            for (DWORD i = 0; i < t->dwNumEntries; ++i) out.insert(swap16(t->table[i].dwLocalPort), t->table[i].dwOwningPid);
        }
    }
    return out;
}

bool isElevated()
{
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    TOKEN_ELEVATION e{};
    DWORD n = 0;
    const bool ok = GetTokenInformation(tok, TokenElevation, &e, sizeof(e), &n) && e.TokenIsElevated;
    CloseHandle(tok);
    return ok;
}

QString currentUserSid()
{
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return {};
    DWORD n = 0;
    GetTokenInformation(tok, TokenUser, nullptr, 0, &n);
    std::vector<char> buf(n);
    QString out;
    if (GetTokenInformation(tok, TokenUser, buf.data(), n, &n)) {
        LPWSTR s = nullptr;
        if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buf.data())->User.Sid, &s)) {
            out = QString::fromWCharArray(s);
            LocalFree(s);
        }
    }
    CloseHandle(tok);
    return out;
}

int runElevated(const QString& exe, const QStringList& args)
{
    QStringList quoted;
    for (const QString& a : args) quoted << quoteArg(a);
    const std::wstring file = w(QDir::toNativeSeparators(exe));
    const std::wstring params = w(quoted.join(' '));
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = file.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) return -1;
    WaitForSingleObject(sei.hProcess, INFINITE);
    DWORD code = DWORD(-1);
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return int(code);
}

// --- services --------------------------------------------------------------------------------

SvcInfo queryService(const QString& name)
{
    SvcInfo info;
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm) return info;
    ScHandle svc(OpenServiceW(scm.h, w(name).c_str(), SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG));
    if (!svc) {
        if (GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST) info.state = SvcState::Other;
        return info;
    }
    SERVICE_STATUS_PROCESS ssp{};
    DWORD n = 0;
    if (QueryServiceStatusEx(svc.h, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &n)) {
        info.pid = ssp.dwProcessId;
        switch (ssp.dwCurrentState) {
        case SERVICE_RUNNING:       info.state = SvcState::Running; break;
        case SERVICE_STOPPED:       info.state = SvcState::Stopped; break;
        case SERVICE_START_PENDING: info.state = SvcState::StartPending; break;
        case SERVICE_STOP_PENDING:  info.state = SvcState::StopPending; break;
        default:                    info.state = SvcState::Other; break;
        }
    }
    QueryServiceConfigW(svc.h, nullptr, 0, &n);
    std::vector<char> buf(n);
    if (n && QueryServiceConfigW(svc.h, reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buf.data()), n, &n))
        info.autoStart = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buf.data())->dwStartType == SERVICE_AUTO_START;
    return info;
}

bool startService(const QString& name, QString* err)
{
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    ScHandle svc(scm ? OpenServiceW(scm.h, w(name).c_str(), SERVICE_START) : nullptr);
    if (!svc) { setErr(err, "open service " + name); return false; }
    if (!StartServiceW(svc.h, 0, nullptr) && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
        setErr(err, "start " + name);
        return false;
    }
    return true;
}

bool stopService(const QString& name, QString* err)
{
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    ScHandle svc(scm ? OpenServiceW(scm.h, w(name).c_str(), SERVICE_STOP) : nullptr);
    if (!svc) { setErr(err, "open service " + name); return false; }
    SERVICE_STATUS st{};
    if (!ControlService(svc.h, SERVICE_CONTROL_STOP, &st) && GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
        setErr(err, "stop " + name);
        return false;
    }
    return true;
}

bool configureService(const QString& name, const QString& display, const QString& description,
                      bool autoStart, const QString& grantSid, QString* err)
{
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    ScHandle svc(scm ? OpenServiceW(scm.h, w(name).c_str(), SERVICE_ALL_ACCESS) : nullptr);
    if (!svc) { setErr(err, "open service " + name); return false; }

    // low-privilege account: a compromised PHP app must not run as LocalSystem
    const std::wstring disp = w(display);
    if (!ChangeServiceConfigW(svc.h, SERVICE_NO_CHANGE, autoStart ? SERVICE_AUTO_START : SERVICE_DEMAND_START,
                              SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr,
                              L"NT AUTHORITY\\LocalService", L"", disp.c_str())) {
        setErr(err, "configure " + name);
        return false;
    }
    std::wstring descW = w(description);
    SERVICE_DESCRIPTIONW d{descW.data()};
    ChangeServiceConfig2W(svc.h, SERVICE_CONFIG_DESCRIPTION, &d);

    if (grantSid.isEmpty()) return true;
    // let the signed-in user start/stop/query without elevation (no config change rights)
    DWORD n = 0;
    QueryServiceObjectSecurity(svc.h, DACL_SECURITY_INFORMATION, nullptr, 0, &n);
    std::vector<char> sd(n);
    if (!QueryServiceObjectSecurity(svc.h, DACL_SECURITY_INFORMATION, sd.data(), n, &n)) { setErr(err, "read service ACL"); return false; }
    LPWSTR sddl = nullptr;
    if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(sd.data(), SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &sddl, nullptr)) {
        setErr(err, "convert service ACL");
        return false;
    }
    QString s = QString::fromWCharArray(sddl);
    LocalFree(sddl);
    const QString ace = QString("(A;;CCLCSWRPWPLOCRRC;;;%1)").arg(grantSid);
    if (!s.contains(ace)) {
        const int sacl = s.indexOf("S:");
        if (sacl >= 0) s.insert(sacl, ace); else s += ace;
    }
    PSECURITY_DESCRIPTOR newSd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(w(s).c_str(), SDDL_REVISION_1, &newSd, nullptr)) {
        setErr(err, "build service ACL");
        return false;
    }
    const bool ok = SetServiceObjectSecurity(svc.h, DACL_SECURITY_INFORMATION, newSd);
    if (!ok) setErr(err, "set service ACL");
    LocalFree(newSd);
    return ok;
}

bool setServiceAutoStart(const QString& name, bool autoStart, QString* err)
{
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    ScHandle svc(scm ? OpenServiceW(scm.h, w(name).c_str(), SERVICE_CHANGE_CONFIG) : nullptr);
    if (!svc) { setErr(err, "open service " + name); return false; }
    if (!ChangeServiceConfigW(svc.h, SERVICE_NO_CHANGE, autoStart ? SERVICE_AUTO_START : SERVICE_DEMAND_START,
                              SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
        setErr(err, "change start type of " + name);
        return false;
    }
    return true;
}

bool deleteService(const QString& name, QString* err)
{
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    ScHandle svc(scm ? OpenServiceW(scm.h, w(name).c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE) : nullptr);
    if (!svc) {
        if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) return true;
        setErr(err, "open service " + name);
        return false;
    }
    SERVICE_STATUS st{};
    ControlService(svc.h, SERVICE_CONTROL_STOP, &st);
    for (int i = 0; i < 60; ++i) {
        if (!QueryServiceStatus(svc.h, &st) || st.dwCurrentState == SERVICE_STOPPED) break;
        Sleep(500);
    }
    if (!DeleteService(svc.h) && GetLastError() != ERROR_SERVICE_MARKED_FOR_DELETE) {
        setErr(err, "delete " + name);
        return false;
    }
    return true;
}

// --- PATH ------------------------------------------------------------------------------------

namespace {
const wchar_t* kMachineEnv = L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
const wchar_t* kUserEnv = L"Environment";
}

QString readPath(bool machine)
{
    HKEY root = machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* sub = machine ? kMachineEnv : kUserEnv;
    DWORD size = 0;
    const DWORD flags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND;
    if (RegGetValueW(root, sub, L"Path", flags, nullptr, nullptr, &size) != ERROR_SUCCESS) return {};
    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1);
    if (RegGetValueW(root, sub, L"Path", flags, nullptr, buf.data(), &size) != ERROR_SUCCESS) return {};
    return QString::fromWCharArray(buf.data());
}

bool writePath(bool machine, const QString& value, QString* err)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, machine ? kMachineEnv : kUserEnv,
                      0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        setErr(err, "open environment key");
        return false;
    }
    const std::wstring v = w(value);
    const LSTATUS st = RegSetValueExW(key, L"Path", 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE*>(v.c_str()),
                                      DWORD((v.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    if (st != ERROR_SUCCESS) {
        if (err) *err = "write PATH: " + lastErrorText(DWORD(st));
        return false;
    }
    broadcastEnvironmentChange();
    return true;
}

void broadcastEnvironmentChange()
{
    DWORD_PTR r = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Environment"),
                        SMTO_ABORTIFHUNG, 5000, &r);
}

} // namespace win
