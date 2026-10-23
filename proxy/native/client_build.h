// Verified 32-bit classic clients. New Engine deliberately has no profile.
#pragma once
#include "native/native.h"

namespace rnative {
enum class ClientModule { Randy, DisplaySystem, Gamecode, Gui, N3, Fxs };
enum class ClientBuild { Unknown, Prk, Live };
ClientBuild IdentifyClientModule(HMODULE module, ClientModule kind);
}
