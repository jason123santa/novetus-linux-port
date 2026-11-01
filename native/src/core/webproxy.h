#pragma once
#include <QString>

// Port of Novetus.Core.WebProxy: an explicit HTTP proxy on port 6171 that
// intercepts traffic to Roblox hosts and serves it through the extension
// manager (data/extensions/webproxy/*.js). HTTPS (CONNECT) tunnels are
// passed through without decryption; plain HTTP requests to valid Roblox
// hosts get the extension/default response like the original's OnRequest.
namespace WebProxy {

// Loads extensions, starts listening on 6171 and (when asked to) sets the
// Wine prefix's system proxy so Windows clients route through it.
bool start(bool setSystemProxy);

bool stop(bool restoreSystemProxy);

bool running();

// Proxy DoSetup parity: opt-in dialog state machine driven by the config.
// Returns true when the proxy was started. `askCallback` shows the opt-in
// prompt and returns the user's choice.
bool doSetup(bool (*askCallback)(), bool *setupDone);

} // namespace WebProxy
