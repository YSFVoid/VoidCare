#include "voidcare/platform/windows/admin_utils.h"

#include <Windows.h>
#include <sddl.h>

namespace voidcare::platform::windows {

bool isRunningAsAdmin() {
    BOOL isMember = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup = nullptr;

    if (!AllocateAndInitializeSid(&ntAuthority,
                                  2,
                                  SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  &administratorsGroup)) {
        return false;
    }

    const BOOL checkResult = CheckTokenMembership(nullptr, administratorsGroup, &isMember);
    FreeSid(administratorsGroup);
    return checkResult == TRUE && isMember == TRUE;
}

QString formatWin32Error(const unsigned long errorCode) {
    LPWSTR buffer = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                          FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr,
                                      static_cast<DWORD>(errorCode),
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPWSTR>(&buffer),
                                      0,
                                      nullptr);
    if (size == 0 || buffer == nullptr) {
        return QStringLiteral("Win32 error %1").arg(errorCode);
    }

    QString message = QString::fromWCharArray(buffer, static_cast<int>(size)).trimmed();
    LocalFree(buffer);
    return message;
}

}  // namespace voidcare::platform::windows
