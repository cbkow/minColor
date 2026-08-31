/* Shell-args seam — the M3 shell passes per-invocation arguments to menu commands via
   settings/shell-args.json: { "command": "<menu label>", ...args }. Consume-if-match:
   the file is DELETED on a successful match (stale-args protection — a crash between
   write and dispatch leaves a file that only the matching command will ever consume);
   a non-matching or absent file returns null and the caller falls back to its current
   behavior (NSAlert picker / dropdown defaults) — menu-direct invocation keeps working
   forever.                                                                             */
#pragma once
#include <string>
#include "MincJson.h"

MincJsonPtr MincArgsConsume(const char *commandLabel);   /* null = no args for this command */
