#pragma once

#include <QString>

namespace voidcare::platform::windows {

bool isRunningAsAdmin();
QString formatWin32Error(unsigned long errorCode);

}  // namespace voidcare::platform::windows
